#pragma once

#include "main.h"

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)


class VideoPlayer {
public:
	VideoPlayer(char* filepath, player_stat_t* is);
	~VideoPlayer();
	int video_playing();

public:
	player_stat_t* is;

private:
	int sfp_refresh_thread();
	int video_play_thread();
	int video_refresh(double* remaining_time);
	void display_one_frame();
	int video_decode_thread();
	int decode_frame(AVFrame* pFrame);

	double vp_duration(frame_t* vp, frame_t* nextvp);
	double compute_target_delay(double delay);
	void update_video_pts(player_stat_t* is, double pts, int64_t pos, int serial);

private:
	std::shared_ptr<std::thread> m_pPlay;
	std::shared_ptr<std::thread> m_pDecode;

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
	//SDL_Thread* video_tid; //���̣߳�ͬ��ˢ��ʱ��
	//SDL_Event event; //�߳�״̬

	double refresh_rate;	//ˢ����=ÿ�����֡
	//flag
	int flag_exit = 0;
	int flag_pause = 0;


};

//VideoPlayer::VideoPlayer(player_stat_t* input_is) {
//	this->is = input_is;
//}