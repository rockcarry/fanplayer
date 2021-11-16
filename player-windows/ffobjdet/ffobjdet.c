#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <yolodet.h>
#include <libswscale/swscale.h>
#include "ffobjdet.h"

#pragma warning(disable:4996) // disable warnings
#define ALIGN(a, b) ((a) + ((b) - 1) & ~((b) - 1))

typedef struct {
    void *yolodet;
    int   precision;

    #define FLAG_EXIT (1 << 0)
    uint32_t flags;

    #define MAX_BBOX_NUM 100
    BBOX  bboxlist[MAX_BBOX_NUM];

    #define RGBBUF_SIZE  256
    uint8_t rgbbuf[RGBBUF_SIZE * RGBBUF_SIZE * 3];
    int     numbuf;
    int     srcw, srch;
    int     dstw, dsth;

    struct SwsContext *sws_context;

    pthread_t     thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} OBJDET;

static void* detect_thread_proc(void *param)
{
    OBJDET *det = (OBJDET*)param;
    BBOX    boxes[MAX_BBOX_NUM];
    int     n, i;

    while (!(det->flags & FLAG_EXIT)) {
        pthread_mutex_lock(&det->lock);
        while (det->numbuf == 0 && (det->flags & FLAG_EXIT) == 0) pthread_cond_wait(&det->cond, &det->lock);
        n = yolodet_detect(det->yolodet, boxes, MAX_BBOX_NUM - 1, det->rgbbuf, det->dstw, det->dsth);
        det->numbuf = 0;
        pthread_mutex_unlock(&det->lock);
        for (i=0; i<n; i++) {
            det->bboxlist[i].score = boxes[i].score;
            det->bboxlist[i].x1    = boxes[i].x1 / det->dstw;
            det->bboxlist[i].y1    = boxes[i].y1 / det->dsth;
            det->bboxlist[i].x2    = boxes[i].x2 / det->dstw;
            det->bboxlist[i].y2    = boxes[i].y2 / det->dsth;
        }
        det->bboxlist[n].score = 0;
    }
    return NULL;
}

static void get_app_dir(char *path, int size)
{
    HMODULE handle = GetModuleHandle(NULL);
    char   *str;
    GetModuleFileNameA(handle, path, size);
    str = path + strlen(path);
    while (*--str != '\\');
    *str = '\0';
}

void* ffobjdet_init(void)
{
    char paramfile[MAX_PATH];
    char binfile  [MAX_PATH];

    OBJDET *det = calloc(1, sizeof(OBJDET));
    if (!det) return NULL;

    get_app_dir(paramfile, sizeof(paramfile)); strncat(paramfile, "\\yolo-fastest-1.1_body.param", sizeof(paramfile));
    get_app_dir(binfile  , sizeof(binfile  )); strncat(binfile  , "\\yolo-fastest-1.1_body.bin"  , sizeof(binfile  ));
    det->yolodet = yolodet_init(paramfile, binfile);
    if (!det->yolodet) {
        av_log(NULL, AV_LOG_ERROR, "ffobjdet_init failed to init yolodet !\n");
        free(det); return NULL;
    }

    pthread_mutex_init(&det->lock, NULL);
    pthread_cond_init (&det->cond, NULL);
    pthread_create(&det->thread, NULL, detect_thread_proc, det);
    return det;
}

void ffobjdet_data(void *ctx, struct AVFrame *video)
{
    OBJDET *det = (OBJDET*)ctx;
    if (!det) return;
    if (det->precision == 0 || video->width == 0 || video->height == 0) {
        det->bboxlist[0].score = 0;
        return;
    }
    pthread_mutex_lock(&det->lock);
    if (det->numbuf == 0) {
        if (det->srcw != video->width || det->srch != video->height) {
            det->srcw = video->width;
            det->srch = video->height;
            if (det->srcw > det->srch) {
                det->dstw = det->precision;
                det->dsth = det->dstw * det->srch / det->srcw;
            } else {
                det->dsth = det->precision;
                det->dstw = det->dsth * det->srcw / det->srch;
                det->dstw = ALIGN(det->dstw, 4);
            }
            if (det->sws_context) sws_freeContext(det->sws_context);
            det->sws_context = sws_getContext(det->srcw, det->srch, video->format, det->dstw, det->dsth, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, 0, 0, 0);
        }
        if (det->sws_context) {
            int     linesize[1] = { det->dstw * 3 };
            uint8_t *dstdata[1] = { det->rgbbuf   };
            sws_scale(det->sws_context, video->data, video->linesize, 0, det->srch, dstdata, linesize);
            det->numbuf = 1;
            pthread_cond_signal(&det->cond);
        }
    }
    pthread_mutex_unlock(&det->lock);
}

BBOX* ffobjdet_bbox(void *ctx)
{
    OBJDET *det = (OBJDET*)ctx;
    return det ? det->bboxlist : NULL;
}

void ffobjdet_enable(void *ctx, int precision)
{
    OBJDET *det = (OBJDET*)ctx;
    if (det) det->precision = precision < RGBBUF_SIZE ? precision : RGBBUF_SIZE;
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
        if (det->sws_context) sws_freeContext(det->sws_context);
        yolodet_free(det->yolodet);
        free(det);
    }
}

