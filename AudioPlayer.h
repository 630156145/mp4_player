#pragma once

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


typedef struct _tFrame {
    void* data;
    int size;
    int samplerate;
}TFRAME, * PTFRAME;


class AudioPlayer {
public:
    int audio_player_init(char* filepath);
    int decode();
    int audio_playing();

public:
    player_stat_t* is;

private:
    int OpenAL_init();
    int SoundCallback(ALuint& bufferID);
    int Play();
    int destory();
    //int start_playing();
    int audio_play_thread();

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
};


