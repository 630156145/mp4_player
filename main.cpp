#include "main.h"
#include "VideoPlayer.h"
#include "VideoFrameQueue.h"
#include "AudioPlayer.h"


//������һ֡��pts����ֵ(��һ֡pts+���ŵ�ʱ��)
double get_clock(play_clock_t* c) {
    return (c->pts_drift + av_gettime_relative() / 1000000.0);   // չ���ã� c->pts + (time - c->last_updated)
}


void set_clock(play_clock_t* c, double pts) {
    double time = av_gettime_relative() / 1000000.0;
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
}


void init_clock(play_clock_t* c) {
    set_clock(c, NAN);
}


int main(int argc, char* argv[]) {
	//��ȡ�ļ�·��
	//char filepath[] = "./Debug/Audio_Video_Sync_Test.mp4";
    //char filepath[] = "./Debug/snow.mp4";

	char* filepath;
	if (argc==2) {
		filepath = argv[1];
	}
	else {
		printf("Usage: player <filename>");
		return -1;
	}

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
            if (event.type == SDL_QUIT) {   //�رմ���
                printf("[quit]\n");
                is->flag_exit = 1;
            }
            else if (event.type == SDL_KEYDOWN) {//�����¼�
                switch (event.key.keysym.sym) {
                    case SDLK_p:    //��p��ͣ���ٰ�p�ָ�����
                        if (is->flag_pause) printf("[continue]\n");
                        else printf("[pause]\n");
                        is->flag_pause = !is->flag_pause;
                        break;

                    case SDLK_1:    //��1ǰ��10��
                        printf("[forward 10s]\n");
                        is->forward = 10;
                        break;

                    case SDLK_3:    //��3ǰ��30��
                        printf("[forward 30s]\n");
                        is->forward = 30;
                        break;

                    case SDLK_u:    //��u����+
                        printf("[volume up]\n");
                        audioPlayer->adjust_volume(VOLUME_UP);
                        break;

                    case SDLK_d:    //��d����-
                        printf("[volume down]\n");
                        audioPlayer->adjust_volume(VOLUME_DOWN);
                        break;

                    case SDLK_f:    //��fȫ�����ٰ�ȡ��
                        if (is->flag_fullscreen) printf("[window]\n");
                        else printf("[full screen]\n");
                        is->flag_fullscreen = !is->flag_fullscreen;
                        videoPlayer->do_fullscreen();
                        break;

                    case SDLK_q:    //��q(quick)����
                        printf("[speed up]\n");
                        audioPlayer->adjust_speed(SPEED_UP);
                        break;

                    case SDLK_s:    //��s(slow)����
                        printf("[speed down]\n");
                        audioPlayer->adjust_speed(SPEED_DOWN);
                        break;
                }
            }
            else if (event.type == SDL_WINDOWEVENT) {   //���Ŵ���
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    printf("[resize window]\n");
                    videoPlayer->resize_window(event.window.data1, event.window.data2);
                }
            }

        }
    }

    //system("pause");
    return 0;
}