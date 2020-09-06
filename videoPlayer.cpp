#include "VideoPlayer.h"
#include "main.h"

VideoPlayer::VideoPlayer(char* filepath, player_stat_t* is1) {
	is = is1;
	if (!(display_mutex = SDL_CreateMutex())) {
		printf("SDL_CreateMutex(): %s\n", SDL_GetError());
		return;
	}
	av_register_all();	//ע���
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//��ʼ��SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return;
	}

	//����Ƶ�ļ�����ʼ��pFormatCtx
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return;
	}
	//��ȡ�ļ���Ϣ
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Couldn't find stream information.\n");
		return;
	}
	//��ȡ����ý�����ı�������Ϣ���ҵ���Ӧ��type���ڵ�pFormatCtx->streams������λ�ã���ʼ��������
	index = -1;
	for (int i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			index = i;
			break;
		}
	if (index == -1) {
		printf("Didn't find a video stream.\n");
		return;
	}
	//��ȡ������
	pStream = pFormatCtx->streams[index];
	pCodecCtx = pFormatCtx->streams[index]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return;
	}
	//�򿪽�����
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		printf("Could not open codec.\n");
		return;
	}

	//�ڴ����
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	//�������ڴ����
	out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	//�������󶨵������AVFrame��
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
	//��ʼ��img_convert_ctx
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


	//�����Ƶ�ļ���Ϣ
	printf("---------------- File Information ---------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");

	//����SDL����
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow(filepath, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;
}


VideoPlayer::~VideoPlayer() {
	sws_freeContext(img_convert_ctx);
	av_free(out_buffer);
	av_free_packet(packet);
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	SDL_Quit();
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
}


void VideoPlayer::display_one_frame() {
	frame_t* pFrame_t = fq.peek_last();
	AVFrame* pFrame = pFrame_t->frame;
	//pFrameYUV = av_frame_alloc();
	//�������䣬�������������ݴ���pFrameYUV->data��
	sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
		pFrameYUV->data, pFrameYUV->linesize);
	//printf("=======================================\n");
	//SDL��ʾ��Ƶ
	SDL_LockMutex(display_mutex);
	SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
	SDL_RenderPresent(sdlRenderer);
	SDL_UnlockMutex(display_mutex);
}


int VideoPlayer::video_refresh(double* remaining_time) {
	double time;
	bool flag = false;

	do {
		if (fq.nb_remaining() == 0) { //����֡������ʾ
			return 0;
		}

		double last_duration, duration, delay;
		frame_t* vp, * lastvp;

		lastvp = fq.peek_last();//��һ������ʾ��֡
		vp = fq.peek();         //����ʾ��֡

		last_duration = vp->pts - lastvp->pts;	//��һ֡����ʱ����������֡�Ĳ���ʱ������һ֡��ʱ��ͨ�����ڴ�ֵ�����ڵ�ǰ֡���ſ���
		delay = compute_target_delay(last_duration);

		time = av_gettime_relative()/1000000.0;
		//��ǰϵͳʱ�� < ��ǰ֡����ʱ�̣���ʾ����ʱ��δ���������ţ�ֱ�ӷ���
		if (time < is->frame_timer + delay) {
			//����ˢ��ʱ��remaining_timeΪ��ǰʱ�̵���һ����ʱ�̵�ʱ��
			*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
			return 0;
		}

		//����frame_timerֵ
		is->frame_timer += delay;
		//У��frame_timerֵ����frame_timer����ڵ�ǰϵͳʱ��̫��(�������ͬ����ֵ)�������Ϊ��ǰϵͳʱ��
		if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX) {
			is->frame_timer = time;
		}

		SDL_LockMutex(fq.mutex);
		if (!isnan(vp->pts)) {
			set_clock(&is->video_clk, vp->pts);	//������Ƶʱ��
		}
		SDL_UnlockMutex(fq.mutex);

		//���������ֻʣһ֡��������û���ü����ŵ�֡
		if (fq.nb_remaining() > 1) {
			frame_t* nextvp = fq.peek_next();  //��һ������ʾ��֡
			duration = nextvp->pts - vp->pts;  //��ǰ֡vp����ʱ��
			//��ǰϵͳʱ�� > ��һ֡����ʱ�̣���ǰ֡vpδ�ܼ�ʱ����
			if (time > is->frame_timer + duration) {
				fq.pop();   //ɾ����һ֡����ʾ֡(��lastvp���µ�vp)
				flag = true;
			}
		}
	} while (flag);

	//ɾ����ǰ��ָ��Ԫ�أ���ָ��+1����δ��֡����ָ���lastvp���µ�vp�����ж�֡����ָ���vp���µ�nextvp
	fq.pop();
	//����
	display_one_frame();

	return 0;
}


// ������Ƶʱ������Ƶʱ�ӵĲ�ֵ��У��delayֵ��ʹ��Ƶʱ��׷�ϣ�����֡����ȴ����ظ�����֡����Ƶʱ��
double VideoPlayer::compute_target_delay(double delay) {
	// ��delay < AV_SYNC_THRESHOLD_MIN��ͬ����ֵΪAV_SYNC_THRESHOLD_MIN
	// ��delay > AV_SYNC_THRESHOLD_MAX��ͬ����ֵΪAV_SYNC_THRESHOLD_MAX
	// ��AV_SYNC_THRESHOLD_MIN < delay < AV_SYNC_THRESHOLD_MAX����ͬ����ֵΪdelay
	double sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
	//��Ƶ����Ƶʱ�ӵĲ�ֵ��ʱ��ֵ����һ֡ptsֵ(ʵΪ����һ֡pts + ��һ֡�������ŵ�ʱ���)
	double diff = get_clock(&is->video_clk) - get_clock(&is->audio_clk);	

	//printf("video_clk=%.2f, audio_clk=%.2f, diff=%.2f\n", get_clock(&is->video_clk), get_clock(&is->audio_clk), diff);

	if (!isnan(diff)) {
		//��Ƶʱ�������Ƶʱ�ӣ��ҳ���ͬ����ֵ
		if (diff <= -sync_threshold)        
			delay = FFMAX(0, delay + diff); // ��ǰ֡����ʱ���������Ƶʱ��(delay+diff<0)��delay=0(��Ƶ׷�ϣ���������)������delay=delay+diff
		//��Ƶʱ�ӳ�ǰ��Ƶʱ�ӣ��ҳ���ͬ����ֵ��������Ƶ���ţ��Ӵ���ʱ
		else if (diff >= sync_threshold)
			delay = 2 * delay;
	}	

	return delay;
}


int VideoPlayer::video_play_thread() {
	double remaining_time = 0.0;

	while (!is->flag_exit) {
		if (is->flag_pause) continue;

		if (remaining_time > 0.0) {
			av_usleep((unsigned)(remaining_time * 1000000.0));
		}
		remaining_time = 1/refresh_rate;
		//�ж���ʱʱ�䲢��ʾһ֡
		video_refresh(&remaining_time);
	}

	return 0;
}


int VideoPlayer::decode_frame(AVFrame* pFrame) {
	int ret, got_picture;

	while (1) {
		while (1) {
			if (av_read_frame(pFormatCtx, packet) < 0) {//���װý���ļ�
				//is->flag_exit = 1;
				//printf("av_read_frame() error.\n");
				return 0;
			}
			if (packet->stream_index == index) {
				break;
			}
		}

		// ����packet��pFrame
		ret = avcodec_send_packet(pCodecCtx, packet);	//��AVPacket���ݷ�������������
		if (ret < 0) {
			printf("Decode Error.\n");
		}		
		got_picture = avcodec_receive_frame(pCodecCtx, pFrame);	//�ӽ�����ɶ�����ȡ��һ��AVFrame����
		if (got_picture==0) {
			pFrame->pts = pFrame->best_effort_timestamp;
			return 1;
		}
		av_free_packet(packet);
	}
	
	return 0;
}


int VideoPlayer::video_decode_thread() {
	AVFrame* p_frame = av_frame_alloc();
	double pts;
	double duration;
	int ret, got_picture;
	AVRational tb = pStream->time_base;
	AVRational frame_rate = av_guess_frame_rate(pFormatCtx, pStream, NULL);
	refresh_rate = frame_rate.num / frame_rate.den;
	//printf("refresh=%f\n", refresh_rate);

	if (p_frame == NULL) {
		printf("av_frame_alloc() for p_frame failed\n");
		return AVERROR(ENOMEM);
	}

	while (1) {
		got_picture = decode_frame(p_frame);
		if (got_picture < 0) {
			av_frame_free(&p_frame);
			return 0;
		}
		AVRational avr = {frame_rate.den, frame_rate.num};
		duration = (frame_rate.num && frame_rate.den ? av_q2d(avr) : 0);   // ��ǰ֡����ʱ��
		pts = (p_frame->pts == AV_NOPTS_VALUE) ? NAN : p_frame->pts * av_q2d(tb);   // ��ǰ֡��ʾʱ���
		ret = fq.queue_picture(p_frame, pts, duration, p_frame->pkt_pos);   // ����ǰ֡ѹ��frame_queue
		av_frame_unref(p_frame);

		if (ret < 0) {
			av_frame_free(&p_frame);
		}
	}

	return 0;
}


int VideoPlayer::render_refresh() {
	SDL_LockMutex(display_mutex);	//��������ʱ���ͻ	
	SDL_RenderClear(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	SDL_UnlockMutex(display_mutex);
	return 0;
}


int VideoPlayer::do_fullscreen() {
	if (is->flag_fullscreen) {
		SDL_SetWindowFullscreen(screen, SDL_TRUE);
	}
	else {
		SDL_SetWindowFullscreen(screen, SDL_FALSE);
	}
	render_refresh();
	return 0;
}


int VideoPlayer::resize_window(int width, int height) {
	//����ԭʼ�������
	double tmp = double(screen_h) / screen_w * width;
	if (tmp < height) {
		sdlRect.x = 0;
		sdlRect.y = (height - tmp) / 2;
		sdlRect.w = width;
		sdlRect.h = tmp;
	}
	else {
		tmp = double(screen_w) / screen_h * height;
		sdlRect.x = (width - tmp) / 2;
		sdlRect.y = 0;
		sdlRect.w = tmp;
		sdlRect.h = height;
	}

	render_refresh();
	
	return 0;
}

void VideoPlayer::video_playing() {
	m_pPlay = std::move(std::make_shared<std::thread>(&VideoPlayer::video_play_thread, this));
	m_pDecode = std::move(std::make_shared<std::thread>(&VideoPlayer::video_decode_thread, this));
}