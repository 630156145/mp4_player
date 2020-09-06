#pragma once

#include "main.h"
#include "VideoFrameQueue.h"


class VideoPlayer {
public:
	VideoPlayer(char* filepath, player_stat_t* is);
	~VideoPlayer();
	int video_playing();
	int do_fullscreen();
	int resize_window(int, int);
public:
	player_stat_t* is;
	SDL_Renderer* sdlRenderer; //��Ⱦ������ȾSDL_Texture��SDL_Window

private:
	int video_play_thread();
	int video_refresh(double* remaining_time);
	void display_one_frame();
	int video_decode_thread();
	int decode_frame(AVFrame* pFrame);
	double compute_target_delay(double delay);
	int render_refresh();
	//void forward_func(int second);

private:
	std::shared_ptr<std::thread> m_pPlay;
	std::shared_ptr<std::thread> m_pDecode;
	std::shared_ptr<std::thread> m_pControl;

	AVFormatContext* pFormatCtx; //���װ
	AVCodecContext* pCodecCtx; //����
	AVStream* pStream;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV; //֡����
	AVPacket* packet;	//����ǰ��ѹ�����ݣ������ݣ�
	int index;
	unsigned char* out_buffer;	//���ݻ�����
	struct SwsContext* img_convert_ctx;
	VideoFrameQueue fq = VideoFrameQueue();

	int screen_w = 0, screen_h = 0;
	SDL_Window* screen; //SDL�����Ĵ���
	SDL_Texture* sdlTexture; //����
	SDL_Rect sdlRect;
	SDL_mutex* display_mutex;

	double refresh_rate;	//ˢ����=ÿ�����֡

};