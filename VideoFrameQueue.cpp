#include "VideoFrameQueue.h"
#include "main.h"


VideoFrameQueue::VideoFrameQueue(int maxs, int keep_last) {
    int i;
    memset(queue, 0, sizeof(queue));
    if (!(mutex = SDL_CreateMutex())) {
        printf("SDL_CreateMutex(): %s\n", SDL_GetError());
        return;
    }
    if (!(cond = SDL_CreateCond())) {
        printf("SDL_CreateCond(): %s\n", SDL_GetError());
        return;
    }
    max_size = FFMIN(maxs, FRAME_QUEUE_SIZE);
    keep_last = !!keep_last;
    for (i = 0; i < max_size; i++) {
        if (!(queue[i].frame = av_frame_alloc())) {
            printf("alloc() error\n");
            return;
        }
    }
}

VideoFrameQueue::~VideoFrameQueue() {
    int i;
    for (i = 0; i < max_size; i++) {
        frame_t* vp = &queue[i];
        av_frame_unref(vp->frame);
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(mutex);
    SDL_DestroyCond(cond);
}

frame_t* VideoFrameQueue::peek() {
    return &queue[(rindex + rindex_shown) % max_size];
}

frame_t* VideoFrameQueue::peek_next() {
    return &queue[(rindex + rindex_shown + 1) % max_size];
}

// ȡ����֡���в��ţ�ֻ��ȡ��ɾ������ɾ������Ϊ��֡��Ҫ������������һ��ʹ�á����ź󣬴�֡��Ϊ��һ֡
frame_t* VideoFrameQueue::peek_last() {
    return &queue[rindex];
}

// �����β������һ����д��֡�ռ䣬���޿ռ��д����ȴ�
frame_t* VideoFrameQueue::peek_writable() {
    /* wait until we have space to put a new frame */
    SDL_LockMutex(mutex);
    while (size >= max_size) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    //if (f->pktq->abort_request)
    //    return NULL;

    return &queue[windex];
}

// �Ӷ���ͷ����ȡһ֡��ֻ��ȡ��ɾ��������֡�ɶ���ȴ�
frame_t* VideoFrameQueue::peek_readable() {
    /* wait until we have a readable a new frame */
    SDL_LockMutex(mutex);
    while (size - rindex_shown <= 0 /* &&!f->pktq->abort_request*/) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    //if (f->pktq->abort_request)
    //    return NULL;

    return &queue[(rindex + rindex_shown) % max_size];
}

// �����β��ѹ��һ֡��ֻ���¼�����дָ�룬��˵��ô˺���ǰӦ��֡����д�������Ӧλ��
void VideoFrameQueue::push() {
    if (++windex == max_size)
        windex = 0;
    SDL_LockMutex(mutex);
    size++;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

// ��ָ��(rindex)ָ���֡����ʾ��ɾ����֡��ע�ⲻ��ȡֱ��ɾ������ָ���1
void VideoFrameQueue::pop() {
    if (keep_last && !rindex_shown) {
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

// ������δ��ʾ��֡��
int VideoFrameQueue::nb_remaining() {
    return size - rindex_shown;
}


int VideoFrameQueue::queue_picture(AVFrame* src_frame, double pts, double duration, int64_t pos) {
    
    frame_t* vp = peek_writable();
    if (!(vp))
        return -1;

    //vp->sar = src_frame->sample_aspect_ratio;
    //vp->uploaded = 0;

    //vp->width = src_frame->width;
    //vp->height = src_frame->height;
    //vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    //vp->pos = pos;

    //set_default_window_size(vp->width, vp->height, vp->sar);

    // ��AVFrame���������Ӧλ��
    av_frame_move_ref(vp->frame, src_frame);

    // ���¶��м�����д����
    push();
    return 0;
}