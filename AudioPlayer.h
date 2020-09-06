#pragma once

#include <stdio.h>
#include <cstdlib>
#include <queue>
#include "al.h"
#include "alc.h"
#include "main.h"

extern "C" {
#include "libavformat/avformat.h"	//��װ��ʽ
#include "libavcodec/avcodec.h"	//����
#include "libswresample/swresample.h"
};


#define MAX_AUDIO_FARME_SIZE 48000 * 2
#define NUMBUFFERS (4)
#define VOLUME_UP 0.2
#define VOLUME_DOWN -0.2
#define SPEED_UP 1
#define SPEED_DOWN -1
#define SPEED_NUM 5 //speeds����Ĵ�С���м��ֿɵ����ٶȣ�


typedef struct _tFrame {
    void* data;
    int size;
    int samplerate;
    double pts;
}TFRAME, * PTFRAME;


class AudioPlayer {
public:
    AudioPlayer(char* filepath, player_stat_t* is);
    ~AudioPlayer();
    int decode();
    int audio_playing();
    void adjust_volume(double v); 
    void adjust_speed(int v);

private:
    int OpenAL_init();
    int SoundCallback(ALuint& bufferID);
    int Play();
    int audio_play_thread();
    void forward_func(int second);

public:
    player_stat_t* is;

private:
    std::shared_ptr<std::thread> m_pAudio;

    AVFormatContext* pFormatCtx; //���װ
    AVCodecContext* pCodecCtx; //����
    AVCodec* pCodec;
    AVFrame* pFrame, * pFrameYUV; //֡����
    AVPacket* packet;	//����ǰ��ѹ�����ݣ������ݣ�
    int index; //����������λ��
    uint8_t* out_buffer;	//���ݻ�����
    int out_buffer_size;    //��������С
    SwrContext* swrCtx;

    enum AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    int out_channel_nb;

    std::queue<PTFRAME> queueData; //������������
    ALuint m_source;
    ALuint m_buffers[NUMBUFFERS];

    double volume;    //����
    const double speeds[SPEED_NUM] = { 0.5, 0.75, 1.0, 1.5, 2.0 };
    int speed_idx;    //�����ٶȵ�index����speeds�����еģ�
};


