#include "VideoPlayer.h"
#include "main.h"
#include "VideoFrameQueue.h"

VideoPlayer::VideoPlayer(char* filepath, player_stat_t* is1) {
	is = is1;

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
	//frame_queue_init(&fq, VIDEO_PICTURE_QUEUE_SIZE, 1);
	frame_queue_init(&fq, 100, 1);
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
		screen_w, screen_h, SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		//return -1;
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


//������һ��SDL�̣߳�ÿ���̶�ʱ�䣨=ˢ�¼��������һ���Զ������Ϣ����֪���������н�����ʾ��ʹ����ˢ�¼��������40����
int VideoPlayer::sfp_refresh_thread() {
	//while (!thread_exit) {
	while (1) {
		SDL_Event event;
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(1000/refresh_rate);
	}
	//thread_exit = 0;

	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


void VideoPlayer::display_one_frame() {
	frame_t* pFrame_t = frame_queue_peek_last(&fq);
	AVFrame* pFrame = pFrame_t->frame;
	//pFrameYUV = av_frame_alloc();
	//�������䣬�������������ݴ���pFrameYUV->data��
	sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
		pFrameYUV->data, pFrameYUV->linesize);
	printf("=======================================\n");
	//SDL��ʾ��Ƶ
	SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
}


int VideoPlayer::video_refresh(double* remaining_time) {
	double time;
	static bool first_frame = true;
	bool flag = false;

	do {
		if (frame_queue_nb_remaining(&fq) == 0) { // ����֡����ʾ
			return 0;
		}

		double last_duration, duration, delay;
		frame_t* vp, * lastvp;

		/* dequeue the picture */
		lastvp = frame_queue_peek_last(&fq);     // ��һ֡���ϴ�����ʾ��֡
		vp = frame_queue_peek(&fq);              // ��ǰ֡����ǰ����ʾ��֡

		// lastvp��vp����ͬһ��������(һ��seek�Ὺʼһ���²�������)����frame_timer����Ϊ��ǰʱ��
		if (first_frame) {
			is->frame_timer = av_gettime_relative() / 1000000.0;
			first_frame = false;
		}

		// ��ͣ������ͣ������һ֡ͼ��
		if (flag_pause)
			display_one_frame();

		/* compute nominal last_duration */
		last_duration = vp_duration(lastvp, vp);        // ��һ֡����ʱ����vp->pts - lastvp->pts
		delay = compute_target_delay(last_duration);    // ������Ƶʱ�Ӻ�ͬ��ʱ�ӵĲ�ֵ������delayֵ

		time = av_gettime_relative()/1000000.0;
		// ��ǰ֡����ʱ��(is->frame_timer+delay)���ڵ�ǰʱ��(time)����ʾ����ʱ��δ��
		if (time < is->frame_timer + delay) {
			// ����ʱ��δ���������ˢ��ʱ��remaining_timeΪ��ǰʱ�̵���һ����ʱ�̵�ʱ���
			*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
			// ����ʱ��δ�����򲻲��ţ�ֱ�ӷ���
			return 0;
		}

		// ����frame_timerֵ
		is->frame_timer += delay;
		// У��frame_timerֵ����frame_timer����ڵ�ǰϵͳʱ��̫��(�������ͬ����ֵ)�������Ϊ��ǰϵͳʱ��
		if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX) {
			is->frame_timer = time;
		}

		SDL_LockMutex(fq.mutex);
		if (!isnan(vp->pts)) {
			update_video_pts(is, vp->pts, vp->pos, vp->serial); // ������Ƶʱ�ӣ�ʱ�����ʱ��ʱ��
		}
		SDL_UnlockMutex(fq.mutex);

		// �Ƿ�Ҫ����δ�ܼ�ʱ���ŵ���Ƶ֡
		if (frame_queue_nb_remaining(&fq) > 1) { // ������δ��ʾ֡��>1(ֻ��һ֡�򲻿��Ƕ�֡)
			frame_t* nextvp = frame_queue_peek_next(&fq);  // ��һ֡����һ����ʾ��֡
			duration = vp_duration(vp, nextvp);             // ��ǰ֡vp����ʱ�� = nextvp->pts - vp->pts
			// ��ǰ֡vpδ�ܼ�ʱ���ţ�����һ֡����ʱ��(is->frame_timer+duration)С�ڵ�ǰϵͳʱ��(time)
			if (time > is->frame_timer + duration) {
				frame_queue_next(&fq);   // ɾ����һ֡����ʾ֡����ɾ��lastvp����ָ���1(��lastvp���µ�vp)
				flag = true;
			}
		}
	} while (flag);

	// ɾ����ǰ��ָ��Ԫ�أ���ָ��+1����δ��֡����ָ���lastvp���µ�vp�����ж�֡����ָ���vp���µ�nextvp
	frame_queue_next(&fq);

//display:
	display_one_frame();   // ȡ����ǰ֡vp(���ж�֡��nextvp)���в���
	return 0;
}


double VideoPlayer::vp_duration(frame_t* vp, frame_t* nextvp) {
	if (vp->serial == nextvp->serial) {
		double duration = nextvp->pts - vp->pts;
		if (isnan(duration) || duration <= 0)
			return vp->duration;
		else
			return duration;
	}
	else {
		return 0.0;
	}
}


// ������Ƶʱ������Ƶʱ�ӵĲ�ֵ��У��delayֵ��ʹ��Ƶʱ��׷�ϣ�����֡����ȴ����ظ�����֡����Ƶʱ��
// �������delay����һ֡����ʱ��������һ֡���ź�Ӧ��ʱ�೤ʱ����ٲ��ŵ�ǰ֡��ͨ�����ڴ�ֵ�����ڵ�ǰ֡���ſ���
// ����У�����delayֵ
double VideoPlayer::compute_target_delay(double delay) {
	double sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
	//��Ƶ����Ƶʱ�ӵĲ�ֵ��ʱ��ֵ����һ֡ptsֵ(ʵΪ����һ֡pts + ��һ֡�������ŵ�ʱ���)
	double diff = get_clock(&is->video_clk) - get_clock(&is->audio_clk);	

	printf("video_clk=%.2f, audio_clk=%.2f, diff=%.2f\n", get_clock(&is->video_clk), is->audio_clk.pts, diff);

	// delay����һ֡����ʱ������ǰ֡(�����ŵ�֡)����ʱ������һ֡����ʱ�������ֵ
	// ��delay < AV_SYNC_THRESHOLD_MIN����ͬ����ֵΪAV_SYNC_THRESHOLD_MIN
	// ��delay > AV_SYNC_THRESHOLD_MAX����ͬ����ֵΪAV_SYNC_THRESHOLD_MAX
	// ��AV_SYNC_THRESHOLD_MIN < delay < AV_SYNC_THRESHOLD_MAX����ͬ����ֵΪdelay
	if (!isnan(diff)) {
		if (diff <= -sync_threshold)        // ��Ƶʱ�������ͬ��ʱ�ӣ��ҳ���ͬ����ֵ
			delay = FFMAX(0, delay + diff); // ��ǰ֡����ʱ�������ͬ��ʱ��(delay+diff<0)��delay=0(��Ƶ׷�ϣ���������)������delay=delay+diff
		else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)  // ��Ƶʱ�ӳ�ǰ��ͬ��ʱ�ӣ��ҳ���ͬ����ֵ������һ֡����ʱ������
			delay = delay + diff;           // ����У��Ϊdelay=delay+diff����Ҫ��AV_SYNC_FRAMEDUP_THRESHOLD����������
		else if (diff >= sync_threshold)    // ��Ƶʱ�ӳ�ǰ��ͬ��ʱ�ӣ��ҳ���ͬ����ֵ
			delay = 2 * delay;              // ��Ƶ����Ҫ�����Ų���delay������2��

		//if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
		//	if (diff <= -sync_threshold)
		//		delay = 0;
		//	//���統ǰ֡�Ĳ���ʱ�䣬Ҳ����pts����ǰ����ʱ�ӣ��Ǿ���Ҫ�Ӵ���ʱ
		//	else if (diff >= sync_threshold)
		//		delay = 2 * delay;
		//}
	}	

	//printf("thresh=%0.3f delay=%0.3f A-V=%f\n", sync_threshold, delay, -diff);

	return delay;
}


void VideoPlayer::update_video_pts(player_stat_t* is, double pts, int64_t pos, int serial) {
	/* update current video pts */
	set_clock(&is->video_clk, pts);            // ����vidclock
	//-sync_clock_to_slave(&is->extclk, &is->vidclk);  // ��extclockͬ����vidclock
}


int VideoPlayer::video_play_thread() {
	double remaining_time = 0.0;

	while (!flag_exit) {
		if (remaining_time > 0.0) {
			av_usleep((unsigned)(remaining_time * 1000000.0));
		}
		remaining_time = 1/refresh_rate;
		// ������ʾ��ǰ֡������ʱremaining_time������ʾ
		video_refresh(&remaining_time);
	}

	return 0;
}

int VideoPlayer::decode_frame(AVFrame* pFrame) {
	int ret, got_picture;

	while (1) {
		while (1) {
			if (av_read_frame(pFormatCtx, packet) < 0) {//���װý���ļ�
				//thread_exit = 1;
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

	while (!flag_exit) {
		got_picture = decode_frame(p_frame);
		if (got_picture < 0) {
			av_frame_free(&p_frame);
			return 0;
		}
		AVRational avr = {frame_rate.den, frame_rate.num};
		duration = (frame_rate.num && frame_rate.den ? av_q2d(avr) : 0);   // ��ǰ֡����ʱ��
		pts = (p_frame->pts == AV_NOPTS_VALUE) ? NAN : p_frame->pts * av_q2d(tb);   // ��ǰ֡��ʾʱ���
		ret = queue_picture(&fq, p_frame, pts, duration, p_frame->pkt_pos);   // ����ǰ֡ѹ��frame_queue
		av_frame_unref(p_frame);

		if (ret < 0) {
			av_frame_free(&p_frame);
		}
	}

	return 0;
}



int VideoPlayer::video_playing() {
	m_pPlay = std::move(std::make_shared<std::thread>(&VideoPlayer::video_play_thread, this));
	m_pDecode = std::move(std::make_shared<std::thread>(&VideoPlayer::video_decode_thread, this));

	//printf("video_playing() return\n");
	return 0;
}