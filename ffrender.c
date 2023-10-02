#include <stdlib.h>
#include <pthread.h>
#include "ffrender.h"

#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"

#define DEF_FRAME_RATE 25
#define DEF_PLAY_SPEED 100

typedef struct {
    // swresampler & swscaler
    struct SwrContext *swr_context;
    struct SwsContext *sws_context;

    AVRational         frate;
    int                cur_speed_value;
    int                new_speed_value;

    int                swr_src_format;
    int                swr_src_samprate;
    int                swr_src_chlayout;
    int                swr_dst_samprate;
    int                adev_samprate;
    int                adev_channels;
    int16_t            adev_buf[48000 * 2 / DEF_FRAME_RATE];

    int                sws_src_pixfmt;
    int                sws_src_width;
    int                sws_src_height;
    int                sws_dst_pixfmt;
    int                sws_dst_width;
    int                sws_dst_height;
    int                sws_scale_type;

    int64_t            apts, vpts, tick_start, tick_adjust, frame_count;

    PFN_PLAYER_CB      callback;
    void              *cbctx;
} RENDER;

void* render_init(char *type, PFN_PLAYER_CB callback, void *cbctx)
{
    RENDER *render = (RENDER*)calloc(1, sizeof(RENDER));
    if (!render) return NULL;
    render->sws_scale_type  = SWS_FAST_BILINEAR;
    render->new_speed_value = DEF_PLAY_SPEED;
    render->frate.num       = DEF_FRAME_RATE;
    render->frate.den       = 1;
    render->callback        = callback;
    render->cbctx           = cbctx;
    return render;
}

void render_exit(void *ctx)
{
    RENDER *render = (RENDER*)ctx;
    if (!render) return;
    if (render->swr_context) swr_free(&render->swr_context);
    if (render->sws_context) sws_freeContext(render->sws_context);
    free(render);
}

void render_audio(void *ctx, AVFrame *audio)
{
    RENDER *render = (RENDER*)ctx;
    if (!render) return;

    int16_t *out = render->adev_buf;
    int adev_samprate = render->callback(render->cbctx, PLAYER_ADEV_SAMPRATE, NULL, 0);
    int adev_channels = render->callback(render->cbctx, PLAYER_ADEV_CHANNELS, NULL, 0);
    int frate, sampnum, samptotal = 0;

    if (  render->swr_src_format != audio->format || render->swr_src_samprate != audio->sample_rate || render->swr_src_chlayout != audio->channel_layout
       || render->adev_samprate != adev_samprate || render->adev_channels != adev_channels || render->cur_speed_value != render->new_speed_value || !render->swr_context) {
        render->swr_src_format   = (int)audio->format;
        render->swr_src_samprate = (int)audio->sample_rate;
        render->swr_src_chlayout = (int)audio->channel_layout;
        render->adev_samprate    = adev_samprate;
        render->adev_channels    = adev_channels;
        render->cur_speed_value  = render->new_speed_value;
        render->swr_dst_samprate = (int)(render->adev_samprate * DEF_PLAY_SPEED / render->cur_speed_value);
        if (render->swr_context) swr_free(&render->swr_context);
        render->swr_context = swr_alloc_set_opts(NULL, render->adev_channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16,
            render->swr_dst_samprate, render->swr_src_chlayout, render->swr_src_format, render->swr_src_samprate, 0, NULL);
        swr_init(render->swr_context);
    }

    if (render->swr_context) {
        frate   = render->frate.num / render->frate.den;
        frate   = frate > DEF_FRAME_RATE ? frate : DEF_FRAME_RATE;
        sampnum = (render->adev_samprate * DEF_PLAY_SPEED) / (frate * render->cur_speed_value);
        do {
            sampnum = swr_convert(render->swr_context, (uint8_t**)&out, sampnum, (const uint8_t**)audio->extended_data, audio->nb_samples);
            audio->extended_data = NULL;
            if (sampnum) {
                render->apts = audio->pts + 1000 * samptotal * render->cur_speed_value / (render->adev_samprate * DEF_PLAY_SPEED);
                render->callback(render->cbctx, PLAYER_ADEV_BUFFER, render->adev_buf, sampnum * adev_channels * sizeof(int16_t));
                samptotal += sampnum;
            }
        } while (sampnum);
    }
}

void render_video(void *ctx, AVFrame *video)
{
    RENDER *render = (RENDER*)ctx;
    if (!render) return;

    SURFACE surface;
    render->callback(render->cbctx, PLAYER_VDEV_LOCK, &surface, sizeof(surface));

    if (  render->sws_src_width != video->width || render->sws_src_height != video->height || render->sws_src_pixfmt != video->format
       || render->sws_dst_width != surface.w || render->sws_dst_height != surface.h || render->sws_dst_pixfmt != surface.format || render->sws_context == NULL)
    {
        render->sws_src_width  = video->width;
        render->sws_src_height = video->height;
        render->sws_src_pixfmt = video->format;
        render->sws_dst_width  = surface.w;
        render->sws_dst_height = surface.h;
        render->sws_dst_pixfmt = surface.format;
        if (render->sws_context) sws_freeContext(render->sws_context);
        render->sws_context = sws_getContext(render->sws_src_width, render->sws_src_height, render->sws_src_pixfmt,
                render->sws_dst_width, render->sws_dst_height, render->sws_dst_pixfmt, render->sws_scale_type, 0, 0, 0);
    }

    if (render->sws_context && surface.data) {
        AVFrame dstpic = { .data[0] = surface.data, .linesize[0] = surface.stride };
        sws_scale(render->sws_context, (const uint8_t**)video->data, video->linesize, 0, render->sws_src_height, dstpic.data, dstpic.linesize);
    }
    render->callback(render->cbctx, PLAYER_VDEV_UNLOCK, NULL, 0);
    render->vpts = video->pts;

    int64_t tick_cur   = av_gettime_relative() / 1000;
    int64_t tick_avdiff= render->apts - render->vpts;
    render->tick_start = render->tick_start ? render->tick_start : tick_cur;
    int64_t tick_next  = render->tick_start + render->frame_count * 1000 * DEF_PLAY_SPEED * render->frate.den / ((int64_t)render->frate.num * render->new_speed_value);
    int64_t tick_sleep = tick_next - tick_cur;
    render->frame_count++;

    if      (tick_avdiff > 50 ) render->tick_adjust -= 2;
    else if (tick_avdiff > 20 ) render->tick_adjust -= 1;
    else if (tick_avdiff <-50 ) render->tick_adjust += 2;
    else if (tick_avdiff <-20 ) render->tick_adjust += 1;
    tick_sleep += render->tick_adjust;
    if (tick_sleep > 0) av_usleep(tick_sleep * 1000);
//  printf("tick_avdiff: %lld\n", tick_avdiff);
}

void render_set(void *ctx, char *key, void *val)
{
    RENDER *render = (RENDER*)ctx;
    if (!ctx) return;
    if      (strcmp(key, "speed") == 0) render->new_speed_value = (long)val;
    else if (strcmp(key, "frate") == 0) render->frate = *(AVRational*)val;
}

long render_get(void *ctx, char *key, void *val)
{
    RENDER *render = (RENDER*)ctx;
    if (!ctx) return 0;
    if (strcmp(key, "speed") == 0) return (long)render->cur_speed_value;
    if (strcmp(key, "frate") == 0) return (long)&render->frate;
    return 0;
}
