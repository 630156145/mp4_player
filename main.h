#pragma once

#include <stdio.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <cstdlib>
#include <queue>
#include "al.h"
#include "alc.h"

extern "C" {
#include "libavformat/avformat.h"	//��װ��ʽ
#include "libavcodec/avcodec.h"	//����
#include "libswscale/swscale.h"	//����
#include "libswresample/swresample.h" //�ز���
#include "libavutil/imgutils.h" //����ͼ��ʹ��
#include "libavutil/time.h"
#include "SDL2/SDL.h"	//����
};


//ͬ����ֵ����Сֵ�����С����������У��
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
//ͬ����ֵ�����ֵ
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
//��Ƶ֡���еĴ�С
#define FRAME_QUEUE_SIZE 16


typedef struct {
    AVFrame* frame;
    double pts;  //presentation timestamp
    double duration;
} frame_t;


typedef struct {
    double pts;         // ��ǰ֡(������)��ʾʱ��������ź󣬵�ǰ֡�����һ֡
    double pts_drift;   // ��ǰ֡��ʾʱ����뵱ǰϵͳʱ��ʱ��Ĳ�ֵ
    double last_updated;// ��ǰʱ��(����Ƶʱ��)���һ�θ���ʱ�䣬Ҳ�ɳƵ�ǰʱ��ʱ��
} play_clock_t;


typedef struct { 
    play_clock_t audio_clk; // ��Ƶʱ��
    play_clock_t video_clk; // ��Ƶʱ��
    double frame_timer;
    //flag
    int flag_exit;
    int flag_pause;
    int flag_fullscreen;
    int flag_resize;
    int forward;
}   player_stat_t;


double get_clock(play_clock_t* c);
void set_clock(play_clock_t* c, double pts);