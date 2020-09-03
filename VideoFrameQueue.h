#pragma once

#include "main.h"

class VideoFrameQueue {
public:
	VideoFrameQueue(int max_size, int keep_last);
	~VideoFrameQueue();
	frame_t* peek();
	frame_t* peek_next();
	frame_t* peek_last();
	frame_t* peek_writable();
	frame_t* peek_readable();
	void push();
	void pop();
	int nb_remaining();
	int queue_picture(AVFrame* src_frame, double pts, double duration, int64_t pos);

public:
	SDL_mutex* mutex;
	SDL_cond* cond;


private:
	frame_t queue[FRAME_QUEUE_SIZE];
	int rindex = 0;                     // ��������������ʱ��ȡ��֡���в��ţ����ź��֡��Ϊ��һ֡
	int windex = 0;                     // д����
	int size = 0;                       // ��֡��
	int max_size;                   // ���пɴ洢���֡��
	int keep_last;
	int rindex_shown = 0;               // ��ǰ�Ƿ���֡����ʾ


};

