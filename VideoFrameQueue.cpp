#include "VideoFrameQueue.h"
#include "main.h"


VideoFrameQueue::VideoFrameQueue() {
    memset(queue, 0, sizeof(queue));
    if (!(mutex = SDL_CreateMutex())) {
        printf("SDL_CreateMutex(): %s\n", SDL_GetError());
        return;
    }
    if (!(cond = SDL_CreateCond())) {
        printf("SDL_CreateCond(): %s\n", SDL_GetError());
        return;
    }
    max_size = FRAME_QUEUE_SIZE;
    for (int i = 0; i < max_size; i++) {
        if (!(queue[i].frame = av_frame_alloc())) {
            printf("alloc() error\n");
            return;
        }
    }
}

VideoFrameQueue::~VideoFrameQueue() {
    for (int i = 0; i < max_size; i++) {
        frame_t* vp = &queue[i];
        av_frame_unref(vp->frame);
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(mutex);
    SDL_DestroyCond(cond);
}

//��ǰ֡
frame_t* VideoFrameQueue::peek() {
    return &queue[(rindex + rindex_shown) % max_size];
}

//��һ֡
frame_t* VideoFrameQueue::peek_next() {
    return &queue[(rindex + rindex_shown + 1) % max_size];
}

//��һ֡
frame_t* VideoFrameQueue::peek_last() {
    return &queue[rindex];
}

//�����β������һ����д��֡�ռ䣬���޿ռ��д����ȴ�
frame_t* VideoFrameQueue::peek_writable() {
    SDL_LockMutex(mutex);
    while (size >= max_size) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    return &queue[windex];
}

//ֻ��ȡ��ɾ��������֡�ɶ���ȴ�
frame_t* VideoFrameQueue::peek_readable() {
    SDL_LockMutex(mutex);
    while (size - rindex_shown <= 0) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    return &queue[(rindex + rindex_shown) % max_size];
}

//ѹ��һ֡��ֻ���¼�����дָ��
void VideoFrameQueue::push() {
    if (++windex == max_size)
        windex = 0;
    SDL_LockMutex(mutex);
    size++;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

//ɾ��һ֡����ָ���1
void VideoFrameQueue::pop() {
    if (!rindex_shown) {
        rindex_shown = 1;
        return;
    }
    av_frame_unref(queue[rindex].frame);
    if (++rindex == max_size)
        rindex = 0;
    SDL_LockMutex(mutex);
    size--;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

//���ض�����δ��ʾ��֡��
int VideoFrameQueue::nb_remaining() {
    return size - rindex_shown;
}

//�������ѹ��һ֡��ͬʱ����֡����
int VideoFrameQueue::queue_picture(AVFrame* src_frame, double pts, double duration, int64_t pos) {
    //����ռ�
    frame_t* vp = peek_writable();
    if (!vp)
        return -1;

    vp->pts = pts;
    vp->duration = duration;

    // ��AVFrame���������Ӧλ��
    av_frame_move_ref(vp->frame, src_frame);

    // ���¶��м�����д����
    push();
    return 0;
}