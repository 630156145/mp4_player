#include "main.h"
#include "VideoPlayer.h"
#include "VideoFrameQueue.h"
#include "AudioPlayer.h"


//������һ֡��pts����ֵ(��һ֡pts+���ŵ�ʱ��)
double get_clock(play_clock_t* c) {
    if (c->paused) {
        return c->pts;
    }
    else {
        return (c->pts_drift + av_gettime_relative() / 1000000.0);   // չ���ã� c->pts + (time - c->last_updated)
    }
}

void set_clock(play_clock_t* c, double pts) {
    double time = av_gettime_relative() / 1000000.0;
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
}


void init_clock(play_clock_t* c) {
    c->paused = 0;
    set_clock(c, NAN);
}


int main(int argc, char* argv[]) {
	//��ȡ�ļ�·��
	//char filepath[] = "F:/bupt/���б���/player/Debug/Audio_Video_Sync_Test.mp4";		
    char filepath[] = "F:/bupt/���б���/player/Debug/jojo.mp4";

	/*char* filepath;
	if (argc==2) {
		filepath = argv[1];
	}
	else {
		printf("Usage: player <filename>");
		return -1;
	}*/

	player_stat_t* is;
    is = (player_stat_t*)av_mallocz(sizeof(player_stat_t));
    if (!is) {
        return -1;
    }
    init_clock(&is->video_clk);
    init_clock(&is->audio_clk);

	AudioPlayer* audioPlayer = new AudioPlayer(filepath, is);    
    VideoPlayer* videoPlayer = new VideoPlayer(filepath, is);

    audioPlayer->decode();
    audioPlayer->audio_playing();
    videoPlayer->video_playing();

    SDL_Event event;
    while (!is->flag_exit) {
        if (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                is->flag_exit = 1;
            }
            if (event.type == SDL_KEYDOWN) {//�����¼�
                switch (event.key.keysym.sym) {
                    case SDLK_p:    //��p��ͣ���ٰ�p�ָ�����
                        printf("pause\n");
                        is->flag_pause = !is->flag_pause;
                        break;
                    case SDLK_1:    //ǰ��10��
                        printf("forward 10s\n");
                        is->forward = 10;
                        break;
                    case SDLK_3:    //ǰ��10��
                        printf("forward 30s\n");
                        is->forward = 30;
                        break;
                }
            }
        }
    }

    //system("pause");
    return 0;
}