#include <stdlib.h>
#include <pthread.h>
#include "snapshot.h"
#include "ffrender.h"

#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"

#define DEF_FRAME_RATE 20
#define DEF_PLAY_SPEED 100

enum {
    AVSYNC_MODE_AUTO,  // auto
    AVSYNC_MODE_FILE,  // file mode
    AVSYNC_MODE_LIVE_SYNC0, // live mode, without avts sync
    AVSYNC_MODE_LIVE_SYNC1, // live mode, with avts sync
};

typedef struct {
    #define FLAG_STRETCH  (1 << 0)
    #define FLAG_UPDATE   (1 << 1)
    #define FLAG_SNAPSHOT (1 << 2)
    int                flags;

    // swresampler & swscaler
    struct SwrContext *swr_context;
    struct SwsContext *sws_context;

    int                avts_sync_mode;
    int                audio_buf_npkt;
    int                video_buf_npkt;
    int                cur_speed_value;
    int                new_speed_value;

    int                swr_src_format;
    int                swr_src_samprate;
    int                swr_src_chlayout;
    int                swr_dst_samprate;
    int16_t            swr_dst_buf[48000 * 2 / DEF_FRAME_RATE];
    int                adev_samprate;
    int                adev_channels;

    int                sws_src_pixfmt;
    int                sws_src_width;
    int                sws_src_height;
    int                sws_dst_pixfmt;
    int                sws_dst_width;
    int                sws_dst_height;
    int                sws_dst_offset;
    int                sws_scale_type;

    int64_t            frate_num, frate_den, apts, vpts, tick_start, tick_adjust, frame_count;
    PFN_PLAYER_CB      callback;
    void              *cbctx;

    char snapshot[PATH_MAX];
} RENDER;

void* render_init(char *type, PFN_PLAYER_CB callback, void *cbctx)
{
    RENDER *render = (RENDER*)calloc(1, sizeof(RENDER));
    if (!render) return NULL;
    render->sws_scale_type  = SWS_FAST_BILINEAR;
    render->new_speed_value = DEF_PLAY_SPEED;
    render->frate_num       = 1000;
    render->frate_den       = 1000 / DEF_FRAME_RATE;
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

void render_audio(void *ctx, AVFrame *audio, int npkt)
{
    RENDER *render = (RENDER*)ctx;
    if (!render) return;

    int16_t *out = render->swr_dst_buf;
    int adev_samprate = render->callback(render->cbctx, PLAYER_ADEV_SAMPRATE, NULL, 0);
    int adev_channels = render->callback(render->cbctx, PLAYER_ADEV_CHANNELS, NULL, 0);
    int avsync_delta  = render->callback(render->cbctx, PLAYER_AVSYNC_DELTA , NULL, 0) * DEF_FRAME_RATE / render->new_speed_value;
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
        frate   = (render->frate_num * render->cur_speed_value) / (render->frate_den * DEF_PLAY_SPEED);
        frate   = frate > DEF_FRAME_RATE ? frate : DEF_FRAME_RATE;
        sampnum = render->adev_samprate / frate;
        do {
            sampnum = swr_convert(render->swr_context, (uint8_t**)&out, sampnum, (const uint8_t**)audio->extended_data, audio->nb_samples);
            audio->extended_data = NULL, audio->nb_samples = 0;
            if (sampnum) {
                render->apts = audio->pts + 1000 * samptotal * render->cur_speed_value / (render->adev_samprate * DEF_PLAY_SPEED) - avsync_delta;
                if (render->avts_sync_mode < AVSYNC_MODE_LIVE_SYNC0 || render->audio_buf_npkt >= npkt) {
                    render->callback(render->cbctx, PLAYER_ADEV_WRITE, out, sampnum * adev_channels * sizeof(int16_t));
                }
                samptotal += sampnum;
            }
        } while (sampnum);
    }
}

void render_video(void *ctx, AVFrame *video, int npkt)
{
    RENDER *render = (RENDER*)ctx;
    if (!render) return;
    int drop = render->avts_sync_mode >= AVSYNC_MODE_LIVE_SYNC0 && render->video_buf_npkt < npkt;
    if (drop) goto handle_avts_sync;

    SURFACE surface;
    render->callback(render->cbctx, PLAYER_VDEV_LOCK, &surface, sizeof(surface));

    if ( (render->flags & FLAG_UPDATE) || render->sws_src_width != video->width || render->sws_src_height != video->height || render->sws_src_pixfmt != video->format
       || render->sws_dst_width != surface.w || render->sws_dst_height != surface.h || render->sws_dst_pixfmt != surface.format || render->sws_context == NULL)
    {
        render->sws_src_width  = video->width;
        render->sws_src_height = video->height;
        render->sws_src_pixfmt = video->format;
        render->sws_dst_width  = surface.w;
        render->sws_dst_height = surface.h;
        render->sws_dst_pixfmt = surface.format;
        if (render->sws_context) { sws_freeContext(render->sws_context); render->sws_context = NULL; }

        if (video->width && video->height) {
            int dstw, dsth, dstx, dsty;
            if (render->flags & FLAG_STRETCH) {
                dstw = surface.w, dsth = surface.h;
            } else {
                if (video->width * surface.h > video->height * surface.w) {
                    dstw = surface.w & ~7, dsth = (surface.w * video->height / video->width ) & ~7;
                } else {
                    dsth = surface.h & ~7, dstw = (surface.h * video->width  / video->height) & ~7;
                }
            }
            dstx = (surface.w - dstw) / 2;
            dsty = (surface.h - dsth) / 2;
            render->sws_dst_offset = dsty * surface.stride + dstx * (surface.cdepth / 8);
            render->sws_context    = sws_getContext(render->sws_src_width, render->sws_src_height, render->sws_src_pixfmt,
                dstw, dsth, render->sws_dst_pixfmt, render->sws_scale_type, 0, 0, 0);
            if (render->flags & FLAG_UPDATE) {
                render->flags &= ~FLAG_UPDATE;
                memset(surface.data, 0, surface.stride * surface.h);
            }
        }
    }

    if (render->flags & FLAG_SNAPSHOT) {
        render->flags &= ~FLAG_SNAPSHOT;
        take_snapshot(render->snapshot, 0, 0, video);
    }

    if (render->sws_context && surface.data) {
        AVFrame dstpic = { .data[0] = (uint8_t*)surface.data + render->sws_dst_offset, .linesize[0] = surface.stride };
        sws_scale(render->sws_context, (const uint8_t**)video->data, video->linesize, 0, render->sws_src_height, dstpic.data, dstpic.linesize);
    }
    render->callback(render->cbctx, PLAYER_VDEV_UNLOCK, NULL, 0);

handle_avts_sync:
    int64_t tick_cur   = av_gettime_relative() / 1000;
    int64_t tick_avdiff= (render->apts && render->vpts != video->pts) ? render->apts - video->pts : 0;
    render->tick_start =  render->tick_start ? render->tick_start : tick_cur;
    render->frate_num += (render->vpts && video->pts > render->vpts) ? 1000 : 0;
    render->frate_den += (render->vpts && video->pts > render->vpts) ? video->pts - render->vpts : 0;
    render->vpts       = video->pts > 0 ? video->pts : 0;
    render->frame_count= render->frame_count + !drop;
    int64_t tick_next  = render->tick_start + (render->frame_count * DEF_PLAY_SPEED * 1000 * render->frate_den) / (render->new_speed_value * render->frate_num);
    int64_t tick_sleep = tick_next - tick_cur;

    if (render->avts_sync_mode == AVSYNC_MODE_LIVE_SYNC0) render->tick_adjust = 0;
    else if (tick_avdiff > 50 ) render->tick_adjust -= 2;
    else if (tick_avdiff > 20 ) render->tick_adjust -= 1;
    else if (tick_avdiff <-50 ) render->tick_adjust += 2;
    else if (tick_avdiff <-20 ) render->tick_adjust += 1;
    tick_sleep += render->tick_adjust;
    if (!drop && tick_sleep > 0) av_usleep(tick_sleep * 1000);
//  printf("tick_avdiff: %lld\n", tick_avdiff);
}

void render_set(void *ctx, char *key, void *val)
{
    RENDER *render = (RENDER*)ctx;
    if (!ctx || !key) return;
    if (strcmp(key, "speed") == 0 || strcmp(key, "reset") == 0) {
        int n = (intptr_t)val;
        n = n < 300 ? n : 300;
        n = n > 10  ? n : 10;
        if ((intptr_t)val != -1) render->new_speed_value = n;
        render->vpts = render->frame_count = render->tick_start = render->tick_adjust = 0;
    }
    else if (strcmp(key, "stretch") == 0) {
        if ((intptr_t)val) render->flags |= FLAG_STRETCH;
        else render->flags &= ~FLAG_STRETCH;
        render->flags |= FLAG_UPDATE;
    }
    else if (strcmp(key, "snapshot") == 0 && val) {
        strncpy(render->snapshot, val, sizeof(render->snapshot) - 1);
        render->flags |= FLAG_SNAPSHOT;
    }
    else if (strcmp(key, "avts_sync_mode") == 0) render->avts_sync_mode = (intptr_t)val;
    else if (strcmp(key, "audio_buf_npkt") == 0) render->audio_buf_npkt = (intptr_t)val;
    else if (strcmp(key, "video_buf_npkt") == 0) render->video_buf_npkt = (intptr_t)val;
}

long render_get(void *ctx, char *key, void *val)
{
    RENDER *render = (RENDER*)ctx;
    if (!ctx || !key) return 0;
    if (key == PARAM_MEDIA_POSITION) return (render->apts > render->vpts ? render->apts : render->vpts);
    if (strcmp(key, "speed"  ) == 0) return render->cur_speed_value;
    if (strcmp(key, "stretch") == 0) return !!(render->flags & FLAG_STRETCH);
    if (strcmp(key, "avts_sync_mode") == 0) return render->avts_sync_mode;
    if (strcmp(key, "audio_buf_npkt") == 0) return render->audio_buf_npkt;
    if (strcmp(key, "video_buf_npkt") == 0) return render->video_buf_npkt;
    return 0;
}
