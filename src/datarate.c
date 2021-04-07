// 包含头文件
#include <stdint.h>
#include "datarate.h"
#include "libavutil/time.h"

typedef struct {
    uint64_t tick_start;
    uint32_t audio_bytes;
    uint32_t video_bytes;
} CONTEXT;

void* datarate_create(void)
{
    void *ctxt = calloc(1, sizeof(CONTEXT));
    datarate_reset(ctxt);
    return ctxt;
}

void datarate_destroy(void *ctxt)
{
    free(ctxt);
}

void datarate_reset(void *ctxt)
{
    CONTEXT *context = (CONTEXT*)ctxt;
    if (context) {
        context->tick_start  = av_gettime_relative();
        context->audio_bytes = context->video_bytes = 0;
    }
}

void datarate_result(void *ctxt, int *arate, int *vrate, int *drate)
{
    CONTEXT *context = (CONTEXT*)ctxt;
    if (context) {
        uint64_t tickcur  = av_gettime_relative();
        int64_t  tickdiff = (int64_t)tickcur - (int64_t)context->tick_start;
        if (tickdiff == 0) tickdiff = 1;
        if (arate) *arate = (int)(context->audio_bytes * 1000000.0 / tickdiff);
        if (vrate) *vrate = (int)(context->video_bytes * 1000000.0 / tickdiff);
        if (drate) *drate = (int)((context->audio_bytes + context->video_bytes) * 1000000.0 / tickdiff);
        context->tick_start  += tickdiff / 2;
        context->audio_bytes /= 2;
        context->video_bytes /= 2;
    }
}

void datarate_audio_packet(void *ctxt, AVPacket *pkt)
{
    CONTEXT *context = (CONTEXT*)ctxt;
    if (context) context->audio_bytes += pkt->size;
}

void datarate_video_packet(void *ctxt, AVPacket *pkt)
{
    CONTEXT *context = (CONTEXT*)ctxt;
    if (context) context->video_bytes += pkt->size;
}
