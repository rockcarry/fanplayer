// 包含头文件
#include <pthread.h>
#include <semaphore.h>
#include "pktqueue.h"

// 内部常量定义
#define DEF_PKT_QUEUE_SIZE 256 // important!! size must be a power of 2

// 内部类型定义
typedef struct {
    int        fsize;
    int        asize;
    int        vsize;
    int        fhead;
    int        ftail;
    int        ahead;
    int        atail;
    int        vhead;
    int        vtail;
    sem_t      fsem;
    sem_t      asem;
    sem_t      vsem;
    AVPacket  *bpkts; // packet buffers
    AVPacket **fpkts; // free packets
    AVPacket **apkts; // audio packets
    AVPacket **vpkts; // video packets
    CMNVARS   *cmnvars;
    pthread_mutex_t lock;
} PKTQUEUE;

// 函数实现
void* pktqueue_create(int size, CMNVARS *cmnvars)
{
    PKTQUEUE *ppq;
    int       i  ;

    ppq = (PKTQUEUE*)calloc(1, sizeof(PKTQUEUE));
    if (!ppq) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate pktqueue context !\n");
        exit(0);
    }

    ppq->fsize = size ? size : DEF_PKT_QUEUE_SIZE;
    ppq->asize = ppq->vsize = ppq->fsize;

    // alloc buffer & semaphore
    ppq->bpkts  = (AVPacket* )calloc(ppq->fsize, sizeof(AVPacket ));
    ppq->fpkts  = (AVPacket**)calloc(ppq->fsize, sizeof(AVPacket*));
    ppq->apkts  = (AVPacket**)calloc(ppq->asize, sizeof(AVPacket*));
    ppq->vpkts  = (AVPacket**)calloc(ppq->vsize, sizeof(AVPacket*));
    ppq->cmnvars= cmnvars;
    sem_init(&ppq->fsem, 0, ppq->fsize );
    sem_init(&ppq->asem, 0, 0          );
    sem_init(&ppq->vsem, 0, 0          );
    pthread_mutex_init(&ppq->lock, NULL);

    // check invalid
    if (!ppq->bpkts || !ppq->fpkts || !ppq->apkts || !ppq->vpkts) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for pktqueue !\n");
        exit(0);
    }

    // init fpkts
    for (i=0; i<ppq->fsize; i++) {
        ppq->fpkts[i] = &ppq->bpkts[i];
    }

    return ppq;
}

void pktqueue_destroy(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;

    // reset to unref packets
    pktqueue_reset(ctxt);

    // close
    sem_destroy(&ppq->fsem);
    sem_destroy(&ppq->asem);
    sem_destroy(&ppq->vsem);
    pthread_mutex_destroy(&ppq->lock);

    // free
    free(ppq->bpkts);
    free(ppq->fpkts);
    free(ppq->apkts);
    free(ppq->vpkts);
    free(ppq);
}

void pktqueue_reset(void *ctxt)
{
    PKTQUEUE *ppq    = (PKTQUEUE*)ctxt;
    AVPacket *packet = NULL;

    while (NULL != (packet = pktqueue_audio_dequeue(ctxt))) {
        pktqueue_release_packet(ctxt, packet);
    }

    while (NULL != (packet = pktqueue_video_dequeue(ctxt))) {
        pktqueue_release_packet(ctxt, packet);
    }

    ppq->fhead = ppq->ftail = 0;
    ppq->ahead = ppq->atail = 0;
    ppq->vhead = ppq->vtail = 0;
}

AVPacket* pktqueue_request_packet(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    if (0 != sem_trywait(&ppq->fsem)) return NULL;
    return ppq->fpkts[ppq->fhead++ & (ppq->fsize - 1)];
}

void pktqueue_release_packet(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    av_packet_unref(pkt);
    pthread_mutex_lock(&ppq->lock);
    ppq->fpkts[ppq->ftail++ & (ppq->fsize - 1)] = pkt;
    pthread_mutex_unlock(&ppq->lock);
    sem_post(&ppq->fsem);
}

void pktqueue_audio_enqueue(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->apkts[ppq->atail++ & (ppq->asize - 1)] = pkt;
    sem_post(&ppq->asem);
}

AVPacket* pktqueue_audio_dequeue(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    if (0 != sem_timedwait(&ppq->asem, &ts)) return NULL;
    sem_getvalue(&ppq->asem, &ppq->cmnvars->apktn);
    av_log(NULL, AV_LOG_INFO, "apktn: %d\n", ppq->cmnvars->apktn);
    return ppq->apkts[ppq->ahead++ & (ppq->asize - 1)];
}

void pktqueue_video_enqueue(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->vpkts[ppq->vtail++ & (ppq->vsize - 1)] = pkt;
    sem_post(&ppq->vsem);
}

AVPacket* pktqueue_video_dequeue(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    if (0 != sem_timedwait(&ppq->vsem, &ts)) return NULL;
    sem_getvalue(&ppq->vsem, &ppq->cmnvars->vpktn);
    av_log(NULL, AV_LOG_INFO, "vpktn: %d\n", ppq->cmnvars->vpktn);
    return ppq->vpkts[ppq->vhead++ & (ppq->vsize - 1)];
}




