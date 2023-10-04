#include <pthread.h>
#include "pktqueue.h"
#include "libavutil/log.h"
#include "libavutil/time.h"

#define DEF_PKT_QUEUE_SIZE 256 // important!! size must be a power of 2

typedef struct {
    int        fsize;
    int        asize;
    int        vsize;
    int        fncur;
    int        ancur;
    int        vncur;
    int        fhead;
    int        ftail;
    int        ahead;
    int        atail;
    int        vhead;
    int        vtail;
    #define TS_STOP (1 << 0)
    int        status;
    AVPacket  *bpkts; // packet buffers
    AVPacket **fpkts; // free packets
    AVPacket **apkts; // audio packets
    AVPacket **vpkts; // video packets
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} PKTQUEUE;

void* pktqueue_create(int size)
{
    PKTQUEUE *ppq;
    int       i;

    size = size ? size : DEF_PKT_QUEUE_SIZE;
    ppq  = (PKTQUEUE*)calloc(1, sizeof(PKTQUEUE) + size * sizeof(AVPacket) + 3 * size * sizeof(AVPacket*));
    if (!ppq) return NULL;

    ppq->fncur  = ppq->asize = ppq->vsize = ppq->fsize = size;
    ppq->bpkts  = (AVPacket* )((uint8_t*)ppq + sizeof(PKTQUEUE));
    ppq->fpkts  = (AVPacket**)((uint8_t*)ppq->bpkts + size * sizeof(AVPacket ));
    ppq->apkts  = (AVPacket**)((uint8_t*)ppq->fpkts + size * sizeof(AVPacket*));
    ppq->vpkts  = (AVPacket**)((uint8_t*)ppq->apkts + size * sizeof(AVPacket*));
    pthread_mutex_init(&ppq->lock, NULL);
    pthread_cond_init (&ppq->cond, NULL);
    for (i = 0; i < ppq->fsize; i++) ppq->fpkts[i] = &ppq->bpkts[i];
    return ppq;
}

void pktqueue_destroy(void *ctx)
{
    if (!ctx) return;
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    int       i;
    for (i = 0; i < ppq->fsize; i++) av_packet_unref(&ppq->bpkts[i]);
    pthread_mutex_destroy(&ppq->lock);
    pthread_cond_destroy (&ppq->cond);
    free(ppq);
}

void pktqueue_reset(void *ctx)
{
    if (!ctx) return;
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    int       i;
    pthread_mutex_lock(&ppq->lock);
    for (i = 0; i < ppq->fsize; i++) {
        ppq->fpkts[i] = &ppq->bpkts[i];
        ppq->apkts[i] = NULL;
        ppq->vpkts[i] = NULL;
    }
    ppq->fncur = ppq->asize;
    ppq->ancur = ppq->vncur = 0;
    ppq->fhead = ppq->ftail = 0;
    ppq->ahead = ppq->atail = 0;
    ppq->vhead = ppq->vtail = 0;
    pthread_cond_signal(&ppq->cond);
    pthread_mutex_unlock(&ppq->lock);
}

AVPacket* pktqueue_request_packet(void *ctx)
{
    if (!ctx) { av_usleep(20 * 1000); return NULL; }
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    AVPacket *pkt = NULL;
    struct timespec ts;
    int ret = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    pthread_mutex_lock(&ppq->lock);
    while (ppq->fncur == 0 && (ppq->status & TS_STOP) == 0 && ret != ETIMEDOUT) ret = pthread_cond_timedwait(&ppq->cond, &ppq->lock, &ts);
    if (ppq->fncur != 0) {
        ppq->fncur--;
        pkt = ppq->fpkts[ppq->fhead++ & (ppq->fsize - 1)];
        av_packet_unref(pkt);
        pthread_cond_signal(&ppq->cond);
    }
    pthread_mutex_unlock(&ppq->lock);
    return pkt;
}

void pktqueue_release_packet(void *ctx, AVPacket *pkt)
{
    if (!ctx) return;
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    struct timespec ts;
    int ret = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    pthread_mutex_lock(&ppq->lock);
    while (ppq->fncur == ppq->fsize && (ppq->status & TS_STOP) == 0 && ret != ETIMEDOUT) ret = pthread_cond_timedwait(&ppq->cond, &ppq->lock, &ts);
    if (ppq->fncur != ppq->fsize) {
        ppq->fncur++;
        ppq->fpkts[ppq->ftail++ & (ppq->fsize - 1)] = pkt;
        pthread_cond_signal(&ppq->cond);
    }
    pthread_mutex_unlock(&ppq->lock);
}

void pktqueue_audio_enqueue(void *ctx, AVPacket *pkt)
{
    if (!ctx) return;
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    pthread_mutex_lock(&ppq->lock);
    while (ppq->ancur == ppq->asize && (ppq->status & TS_STOP) == 0) pthread_cond_wait(&ppq->cond, &ppq->lock);
    if (ppq->ancur != ppq->asize) {
        ppq->ancur++;
        ppq->apkts[ppq->atail++ & (ppq->asize - 1)] = pkt;
        pthread_cond_signal(&ppq->cond);
        av_log(NULL, AV_LOG_INFO, "apktn: %d\n", ppq->ancur);
    }
    pthread_mutex_unlock(&ppq->lock);
}

AVPacket* pktqueue_audio_dequeue(void *ctx)
{
    if (!ctx) { av_usleep(20 * 1000); return NULL; }
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    AVPacket *pkt = NULL;
    struct timespec ts;
    int ret = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    pthread_mutex_lock(&ppq->lock);
    while (ppq->ancur == 0 && (ppq->status & TS_STOP) == 0 && ret != ETIMEDOUT) ret = pthread_cond_timedwait(&ppq->cond, &ppq->lock, &ts);
    if (ppq->ancur != 0) {
        ppq->ancur--;
        pkt = ppq->apkts[ppq->ahead++ & (ppq->asize - 1)];
        pthread_cond_signal(&ppq->cond);
        av_log(NULL, AV_LOG_INFO, "apktn: %d\n", ppq->ancur);
    }
    pthread_mutex_unlock(&ppq->lock);
    return pkt;
}

void pktqueue_video_enqueue(void *ctx, AVPacket *pkt)
{
    if (!ctx) return;
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    pthread_mutex_lock(&ppq->lock);
    while (ppq->vncur == ppq->vsize && (ppq->status & TS_STOP) == 0) pthread_cond_wait(&ppq->cond, &ppq->lock);
    if (ppq->vncur != ppq->vsize) {
        ppq->vncur++;
        ppq->vpkts[ppq->vtail++ & (ppq->vsize - 1)] = pkt;
        pthread_cond_signal(&ppq->cond);
        av_log(NULL, AV_LOG_INFO, "vpktn: %d\n", ppq->vncur);
    }
    pthread_mutex_unlock(&ppq->lock);
}

AVPacket* pktqueue_video_dequeue(void *ctx)
{
    if (!ctx) { av_usleep(20 * 1000); return NULL; }
    PKTQUEUE *ppq = (PKTQUEUE*)ctx;
    AVPacket *pkt = NULL;
    struct timespec ts;
    int ret = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    pthread_mutex_lock(&ppq->lock);
    while (ppq->vncur == 0 && (ppq->status & TS_STOP) == 0 && ret != ETIMEDOUT) ret = pthread_cond_timedwait(&ppq->cond, &ppq->lock, &ts);
    if (ppq->vncur != 0) {
        ppq->vncur--;
        pkt = ppq->vpkts[ppq->vhead++ & (ppq->vsize - 1)];
        pthread_cond_signal(&ppq->cond);
        av_log(NULL, AV_LOG_INFO, "vpktn: %d\n", ppq->vncur);
    }
    pthread_mutex_unlock(&ppq->lock);
    return pkt;
}
