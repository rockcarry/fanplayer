#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <yolodet.h>
#include <libswscale/swscale.h>
#include "ffobjdet.h"

typedef struct {
    void *yolodet;

    #define FLAG_EXIT (1 << 0)
    uint32_t flags;

    #define MAX_BBOX_NUM 100
    BBOX  bboxlist[MAX_BBOX_NUM];

    uint8_t rgbbuf[320 * 320 * 3];
    int     numbuf;
    int     width;
    int     height;

    pthread_t       thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} OBJDET;

static void* detect_thread_proc(void *param)
{
    OBJDET *det = (OBJDET*)param;
    int     n;
    while (!(det->flags & FLAG_EXIT)) {
        pthread_mutex_lock(&det->lock);
        while (det->numbuf == 0 && (det->flags & FLAG_EXIT) == 0) pthread_cond_wait(&det->cond, &det->lock);
        n = yolodet_detect(det->yolodet, det->bboxlist, MAX_BBOX_NUM - 1, det->rgbbuf, det->width, det->height);
        det->bboxlist[n].score = 0;
        pthread_mutex_unlock(&det->lock);
    }
    return NULL;
}

void* ffobjdet_init(void)
{
    OBJDET *det = calloc(1, sizeof(OBJDET));
    if (!det) return NULL;

    det->yolodet = yolodet_init("yolo-fastest-1.1.param", "yolo-fastest-1.1.bin");
    if (!det->yolodet) {
        av_log(NULL, AV_LOG_ERROR, "ffobjdet_init failed to init yolodet !\n");
        free(det); return NULL;
    }

    pthread_mutex_init(&det->lock, NULL);
    pthread_cond_init (&det->cond, NULL);
    pthread_create(&det->thread, NULL, detect_thread_proc, det);
    return det;
}

void ffobjdet_data(void *ctx, void *rgb, int w, int h)
{
    OBJDET *det = (OBJDET*)ctx;
    if (!det) return;
}

BBOX* ffobjdet_bbox(void *ctx)
{
    OBJDET *det = (OBJDET*)ctx;
    return det ? det->bboxlist : NULL;
    pthread_mutex_lock(&det->lock);
    if (det->numbuf == 0) {
    }
    pthread_mutex_unlock(&det->lock);
}

void ffobjdet_free(void *ctx)
{
    OBJDET *det = (OBJDET*)ctx;
    if (det) {
        pthread_mutex_lock(&det->lock);
        det->flags |= FLAG_EXIT;
        pthread_cond_signal(&det->cond);
        pthread_mutex_unlock(&det->lock);

        pthread_join(det->thread, NULL);
        pthread_mutex_destroy(&det->lock);
        pthread_cond_destroy (&det->cond);
        yolodet_free(det->yolodet);
        free(det);
    }
}

