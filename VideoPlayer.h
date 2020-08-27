#pragma once

#include "main.h"

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)


class VideoPlayer {
public:
	//VideoPlayer(player_stat_t* input_is);
	int video_player_init(char* filepath, int input_fps);
	int video_playing();

public:
	player_stat_t* is;

private:
	static int sfp_refresh_thread(void* opaque);
	int video_play_thread();
	int video_refresh(double* remaining_time);
	void display_one_frame();
	int video_decode_thread();
	int decode_frame(AVFrame* pFrame);
	int destroy();

	double vp_duration(frame_t* vp, frame_t* nextvp);
	double compute_target_delay(double delay);
	void update_video_pts(player_stat_t* is, double pts, int64_t pos, int serial);

private:
	std::shared_ptr<std::thread> m_pDecode1;
	std::shared_ptr<std::thread> m_pDecode2;

	AVFormatContext* pFormatCtx; //���װ
	AVCodecContext* pCodecCtx; //����
	AVStream* pStream;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV; //֡����
	AVPacket* packet;	//����ǰ��ѹ�����ݣ������ݣ�
	int index;
	unsigned char* out_buffer;	//���ݻ�����
	struct SwsContext* img_convert_ctx;
	frame_queue_t fq;

	int screen_w = 0, screen_h = 0;
	SDL_Window* screen; //SDL�����Ĵ���
	SDL_Renderer* sdlRenderer; //��Ⱦ������ȾSDL_Texture��SDL_Window
	SDL_Texture* sdlTexture; //����
	SDL_Rect sdlRect;
	SDL_Thread* video_tid; //���̣߳�ͬ��ˢ��ʱ��
	SDL_Event event; //�߳�״̬

	int fps;
	//flag
	//int thread_exit = 0;
	//int thread_pause = 0;


};

//VideoPlayer::VideoPlayer(player_stat_t* input_is) {
//	this->is = input_is;
//}