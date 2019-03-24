// 包含头文件
#include <pthread.h>
#include "ffplayer.h"
#include "ffrender.h"
#include "snapshot.h"
#include "veffect.h"
#include "adev.h"
#include "vdev.h"

#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"

#if CONFIG_ENABLE_SOUNDTOUCH
#include <soundtouch.h>
#endif

#ifdef ANDROID
#include "fanplayer_jni.h"
#endif

#ifdef WIN32
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#endif

// 内部类型定义
typedef struct
{
    int                adev_type;
    int                sample_rate;
    int                sample_fmt;
    int64_t            chan_layout;

    int                adev_buf_avail;
    uint8_t           *adev_buf_cur;
    AUDIOBUF          *adev_hdr_cur;

    int                vdev_type;
    int                video_width;
    int                video_height;
    AVRational         frame_rate;
    int                pixel_fmt;
    void              *surface;

    // video render rect
    int                rect_x;
    int                rect_y;
    int                rect_w;
    int                rect_h;

    // playback speed
    int                speed_value;
    int                speed_type;
    CMNVARS           *cmnvars;

    // adev & vdev
    void              *adev;
    void              *vdev;

    // swresampler & swscaler
    struct SwrContext *swr_context;
    struct SwsContext *sws_context;

    // render status
    #define RENDER_CLOSE            (1 << 0)
    #define RENDER_PAUSE            (1 << 1)
    #define RENDER_SNAPSHOT         (1 << 2)  // take snapshot
    #define RENDER_STEPFORWARD      (1 << 3)  // step forward
    #define RENDER_UDPATE_RECT      (1 << 4)  // update rect
    #define RENDER_AINITED          (1 << 5)
    #define RENDER_VINITED          (1 << 6)
    #define RENDER_DEFINITION_EVAL  (1 << 7)
    int                status;
    float              definitionval;

#if CONFIG_ENABLE_SOUNDTOUCH
    void              *stcontext;
#endif

#if CONFIG_ENABLE_VEFFECT
    // visual effect
    void              *veffect_context;
    int                veffect_type;
    int                veffect_x;
    int                veffect_y;
    int                veffect_w;
    int                veffect_h;
#endif

#if CONFIG_ENABLE_SNAPSHOT
    // snapshot
    char               snapfile[PATH_MAX];
    int                snapwidth;
    int                snapheight;
#endif
} RENDER;

// 内部函数实现
static void render_setspeed(RENDER *render, int speed)
{
    if (speed > 0) {
        // set vdev playback speed
        vdev_setparam(render->vdev, PARAM_PLAY_SPEED_VALUE, &speed);

        // set speed_value_new to triger swr_context re-create
        render->speed_value = speed;
        render->status     &= ~RENDER_AINITED;
    }
}

// 函数实现
void* render_open(int adevtype, int srate, int sndfmt, int64_t ch_layout,
                  int vdevtype, void *surface, struct AVRational frate, int pixfmt, int w, int h,
                  CMNVARS *cmnvars)
{
    RENDER  *render = NULL;
    int64_t *papts  = NULL;

    render = (RENDER*)calloc(1, sizeof(RENDER));
    if (!render) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate render context !\n");
        exit(0);
    }

    // init for audio
    render->adev_type    = adevtype;
    render->sample_rate  = srate;
    render->sample_fmt   = sndfmt;
    render->chan_layout  = ch_layout;

    // init for video
    render->vdev_type    = vdevtype;
    render->video_width  = w;
    render->video_height = h;
    render->rect_w       = w;
    render->rect_h       = h;
    render->frame_rate   = frate;
    render->pixel_fmt    = pixfmt;
#ifdef WIN32
    render->surface      = surface;
#endif
    if (render->pixel_fmt == AV_PIX_FMT_NONE) {
        render->pixel_fmt = AV_PIX_FMT_YUV420P;
    }

    // init for cmnvars
    render->cmnvars = cmnvars;

    // init for visual effect
#if CONFIG_ENABLE_VEFFECT
    render->veffect_context = veffect_create(surface);
#endif

#if CONFIG_ENABLE_SOUNDTOUCH
    render->stcontext = soundtouch_createInstance();
    soundtouch_setSampleRate(render->stcontext, ADEV_SAMPLE_RATE);
    soundtouch_setChannels  (render->stcontext, 2);
#endif

#ifdef WIN32
    if (1) {
        RECT rect; GetClientRect((HWND)surface, &rect);
        render_setrect(render, 0, rect.left, rect.top, rect.right, rect.bottom);
        render_setrect(render, 1, rect.left, rect.top, rect.right, rect.bottom);
    }
#endif

    // set default playback speed
    render_setspeed(render, 100);
    return render;
}

void render_close(void *hrender)
{
    RENDER *render = (RENDER*)hrender;

    // wait visual effect thread exit
    render->status = RENDER_CLOSE;

#if CONFIG_ENABLE_VEFFECT
    veffect_destroy(render->veffect_context);
#endif

    //++ audio ++//
    // destroy adev
    adev_destroy(render->adev);

    // free swr context
    swr_free(&render->swr_context);
    //-- audio --//

    //++ video ++//
    // destroy vdev
    vdev_destroy(render->vdev);

    // free sws context
    if (render->sws_context) {
        sws_freeContext(render->sws_context);
    }
    //-- video --//

#if CONFIG_ENABLE_SOUNDTOUCH
    soundtouch_destroyInstance(render->stcontext);
#endif

#ifdef ANDROID
    JniReleaseWinObj(render->surface);
#endif

    // free context
    free(render);
}

#if CONFIG_ENABLE_SOUNDTOUCH
static int render_audio_soundtouch(RENDER *render, AVFrame *audio)
{
    int16_t  buf[1024];
    int16_t *out = buf;
    int      num_samp;
    int      num_st;

    num_samp = swr_convert(render->swr_context,
        (uint8_t**)&out, 1024 / 2,
        (const uint8_t**)audio->extended_data, audio->nb_samples);
    audio->extended_data = NULL;
    audio->nb_samples    = 0;

    soundtouch_putSamples_i16(render->stcontext, out, num_samp);
    do {
        if (render->adev_buf_avail == 0) {
            adev_lock(render->adev, &render->adev_hdr_cur);
            if (render->adev_hdr_cur) {
                render->adev_buf_avail = (int     )render->adev_hdr_cur->size;
                render->adev_buf_cur   = (uint8_t*)render->adev_hdr_cur->data;
            }
#if CONFIG_ENABLE_VEFFECT
            if (render->veffect_type != VISUAL_EFFECT_DISABLE) {
                veffect_render(render->veffect_context, render->veffect_x, render->veffect_y,
                    render->veffect_w, render->veffect_h, render->veffect_type, render->adev);
            }
#endif
        }
        num_st = soundtouch_receiveSamples_i16(render->stcontext, (int16_t*)render->adev_buf_cur, render->adev_buf_avail / 4);
        render->adev_buf_avail -= num_st * 4;
        render->adev_buf_cur   += num_st * 4;
        if (render->adev_buf_avail == 0) {
            audio->pts += 10 * render->speed_value * render->frame_rate.den / render->frame_rate.num;
            adev_unlock(render->adev, audio->pts);
        }
    } while (num_st != 0);

    return num_samp;
}
#endif

static int render_audio_swresample(RENDER *render, AVFrame *audio)
{
    int num_samp;

    if (render->adev_buf_avail == 0) {
        adev_lock(render->adev, &render->adev_hdr_cur);
        if (render->adev_hdr_cur) {
            render->adev_buf_avail = (int     )render->adev_hdr_cur->size;
            render->adev_buf_cur   = (uint8_t*)render->adev_hdr_cur->data;
        }
#if CONFIG_ENABLE_VEFFECT
        if (render->veffect_type != VISUAL_EFFECT_DISABLE) {
            veffect_render(render->veffect_context, render->veffect_x, render->veffect_y,
                render->veffect_w, render->veffect_h, render->veffect_type, render->adev);
        }
#endif
    }

    //++ do resample audio data ++//
    num_samp = swr_convert(render->swr_context,
        (uint8_t**)&render->adev_buf_cur, render->adev_buf_avail / 4,
        (const uint8_t**)audio->extended_data, audio->nb_samples);
    audio->extended_data    = NULL;
    audio->nb_samples       = 0;
    render->adev_buf_avail -= num_samp * 4;
    render->adev_buf_cur   += num_samp * 4;
    //-- do resample audio data --//

    if (render->adev_buf_avail == 0) {
        audio->pts += 10 * render->speed_value * render->frame_rate.den / render->frame_rate.num;
        adev_unlock(render->adev, audio->pts);
    }

    return num_samp;
}

void render_audio(void *hrender, AVFrame *audio)
{
    RENDER *render  = (RENDER*)hrender;
    int     sampnum;
    if (!hrender) return;

    if (render->cmnvars->init_params->avts_syncmode == AVSYNC_MODE_LIVE && render->cmnvars->apktn > 0) return;
    if (render->adev == NULL) render->adev = adev_create(render->adev_type, 0, (int)((double)ADEV_SAMPLE_RATE * render->frame_rate.den / render->frame_rate.num + 0.5) * 4, render->cmnvars);
    do {
        if (!(render->status & RENDER_AINITED)) {
            //++ allocate & init swr context
            int samprate = render->speed_type ? ADEV_SAMPLE_RATE : (int)(ADEV_SAMPLE_RATE * 100.0 / render->speed_value);
            if (render->swr_context) swr_free(&render->swr_context);
            render->swr_context = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, samprate,
                render->chan_layout, render->sample_fmt, render->sample_rate, 0, NULL);
            swr_init(render->swr_context);
            //-- allocate & init swr context

#if CONFIG_ENABLE_SOUNDTOUCH
            if (render->speed_type) {
                soundtouch_setTempo(render->stcontext, (float)render->speed_value / 100);
            }
#endif
            render->status |= RENDER_AINITED;
        }

#if CONFIG_ENABLE_SOUNDTOUCH
        if (render->speed_type && render->speed_value != 100) {
            sampnum = render_audio_soundtouch(render, audio);
        } else
#endif
        {
            sampnum = render_audio_swresample(render, audio);
        }
    } while (sampnum > 0);
}

static float definition_evaluation(uint8_t *img, int w, int h, int stride)
{
    uint8_t *cur, *pre, *nxt;
    int     i, j, l;
    int64_t s = 0;

    if (!img || !w || !h || !stride) return 0;
    pre = img + 1;
    cur = img + 1 + stride * 1;
    nxt = img + 1 + stride * 2;

    for (i=1; i<h-1; i++) {
        for (j=1; j<w-1; j++) {
            l  = 1 * pre[-1] +  4 * pre[0] + 1 * pre[1];
            l += 4 * cur[-1] - 20 * cur[0] + 4 * cur[1];
            l += 1 * nxt[-1] +  4 * nxt[0] + 1 * nxt[1];
            s += abs(l);
            pre++; cur++; nxt++;
        }
        pre += stride - (w - 2);
        cur += stride - (w - 2);
        nxt += stride - (w - 2);
    }
    return (float)s / ((w - 2) * (h - 2));
}

void render_video(void *hrender, AVFrame *video)
{
    RENDER  *render = (RENDER*)hrender;
    if (!hrender) return;

    if (render->status & RENDER_DEFINITION_EVAL) {
        render->definitionval =  definition_evaluation(video->data[0], video->width, video->height, video->linesize[0]);
        render->status       &= ~RENDER_DEFINITION_EVAL;
    }

    if (render->cmnvars->init_params->avts_syncmode == AVSYNC_MODE_LIVE && render->cmnvars->vpktn > 0) return;
    if (!(render->status & RENDER_VINITED)) {
        if (render->vdev) vdev_destroy(render->vdev);
        render->vdev = vdev_create(render->vdev_type, render->surface, 0, render->video_width, render->video_height, 1000 * render->frame_rate.den / render->frame_rate.num, render->cmnvars);
        vdev_setparam(render->vdev, PARAM_PLAY_SPEED_VALUE, &render->speed_value);
        render->status |= render->vdev ? (RENDER_UDPATE_RECT|RENDER_VINITED) : 0;
    }
    do {
        VDEV_COMMON_CTXT *vdev = (VDEV_COMMON_CTXT*)render->vdev;
        if (render->status & RENDER_UDPATE_RECT) {
            render->status &= ~RENDER_UDPATE_RECT;
            // vdev set rect
            vdev_setrect(render->vdev, render->rect_x, render->rect_y, render->rect_w, render->rect_h);
            // we need recreate sws
            if (render->sws_context) sws_freeContext(render->sws_context);
            render->sws_context = sws_getContext(render->video_width, render->video_height, render->pixel_fmt,
                vdev->sw, vdev->sh, vdev->pixfmt, SWS_FAST_BILINEAR, 0, 0, 0);
        }

        if (video->format == AV_PIX_FMT_DXVA2_VLD) {
            vdev_setparam(render->vdev, PARAM_VDEV_POST_SURFACE, video);
        } else {
            AVFrame picture;
            picture.data[0]     = NULL;
            picture.linesize[0] = 0;
            vdev_lock(render->vdev, picture.data, picture.linesize);
            if (picture.data[0] && video->pts != -1) {
                if (render->sws_context && 0 == sws_scale(render->sws_context, (const uint8_t**)video->data, video->linesize, 0, render->video_height, picture.data, picture.linesize)) {
                    //++ on some android device, output of h264 mediacodec decoder is NV12
                    if (  (render->pixel_fmt == AV_PIX_FMT_YUV420P || render->pixel_fmt == AV_PIX_FMT_YUVJ420P)
                        && video->data[0] && video->data[1] && !video->data[2]
                        && video->linesize[0] == video->linesize[1] && video->linesize[2] == 0) {
                        render->pixel_fmt = AV_PIX_FMT_NV12;
                        render->status |= RENDER_UDPATE_RECT;
                    }
                    //-- on some android device, output of h264 mediacodec decoder is NV12
                }
            }
            vdev_unlock(render->vdev, video->pts);
        }

#if CONFIG_ENABLE_SNAPSHOT
        if (render->status & RENDER_SNAPSHOT) {
            int ret = take_snapshot(render->snapfile, render->snapwidth, render->snapheight, video);
            player_send_message(render->cmnvars->winmsg, MSG_TAKE_SNAPSHOT, 0);
            render->status &= ~RENDER_SNAPSHOT;
        }
#endif
    } while ((render->status & RENDER_PAUSE) && !(render->status & RENDER_STEPFORWARD));

    // clear step forward flag
    render->status &= ~RENDER_STEPFORWARD;
}

void render_setrect(void *hrender, int type, int x, int y, int w, int h)
{
    RENDER *render = (RENDER*)hrender;
    if (!hrender) return;
    switch (type) {
    case 0:
        render->rect_x = x;
        render->rect_y = y;
        render->rect_w = MAX(w, 1);
        render->rect_h = MAX(h, 1);
        render->status|= RENDER_UDPATE_RECT;
        break;
#if CONFIG_ENABLE_VEFFECT
    case 1:
        render->veffect_x = x;
        render->veffect_y = y;
        render->veffect_w = MAX(w, 1);
        render->veffect_h = MAX(h, 1);
        break;
#endif
    }
}

void render_pause(void *hrender, int pause)
{
    RENDER *render = (RENDER*)hrender;
    if (!hrender) return;
    if (pause) render->status |= RENDER_PAUSE;
    else       render->status &=~RENDER_PAUSE;
    adev_pause(render->adev, pause);
    vdev_pause(render->vdev, pause);
    render->cmnvars->start_tick= av_gettime_relative() / 1000;
    render->cmnvars->start_pts = MAX(render->cmnvars->apts, render->cmnvars->vpts);
}

void render_reset(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    if (!hrender) return;
    adev_reset(render->adev);
    vdev_reset(render->vdev);
    render->status = 0;
}

int render_snapshot(void *hrender, char *file, int w, int h, int waitt)
{
#if CONFIG_ENABLE_SNAPSHOT
    RENDER *render = (RENDER*)hrender;
    if (!hrender) return -1;

    // if take snapshot in progress
    if (render->status & RENDER_SNAPSHOT) {
        return -1;
    }

    // copy snapshot file name
    strcpy(render->snapfile, file);
    render->snapwidth  = w;
    render->snapheight = h;

    // setup render flags to request snapshot
    render->status |= RENDER_SNAPSHOT;

    // wait take snapshot done
    if (waitt > 0) {
        int retry = waitt / 10;
        while ((render->status & RENDER_SNAPSHOT) && retry--) {
            av_usleep(10 * 1000);
        }
    }
#endif

    DO_USE_VAR(hrender);
    DO_USE_VAR(file);
    DO_USE_VAR(w);
    DO_USE_VAR(h);
    DO_USE_VAR(waitt);
    return 0;
}

void render_setparam(void *hrender, int id, void *param)
{
    RENDER *render = (RENDER*)hrender;
    if (!hrender) return;
    switch (id)
    {
    case PARAM_AUDIO_VOLUME:
        adev_setparam(render->adev, id, param);
        break;
    case PARAM_PLAY_SPEED_VALUE:
        render_setspeed(render, *(int*)param);
        break;
    case PARAM_PLAY_SPEED_TYPE:
        render->speed_type = *(int*)param;
        render->status    &= ~RENDER_AINITED;
        break;
#if CONFIG_ENABLE_VEFFECT
    case PARAM_VISUAL_EFFECT:
        render->veffect_type = *(int*)param;
        if (render->veffect_type == VISUAL_EFFECT_DISABLE) {
            veffect_render(render->veffect_context,
                render->veffect_x, render->veffect_y,
                render->veffect_w, render->veffect_h,
                VISUAL_EFFECT_DISABLE, render->adev);
        }
        break;
#endif
    case PARAM_AVSYNC_TIME_DIFF:
    case PARAM_VDEV_POST_SURFACE:
    case PARAM_VDEV_D3D_ROTATE:
        vdev_setparam(render->vdev, id, param);
        break;
    case PARAM_RENDER_STEPFORWARD:
        render->status |= RENDER_STEPFORWARD;
        break;
    case PARAM_RENDER_REINIT_A:
        // todo...
        render->status &= ~RENDER_AINITED;
        break;
    case PARAM_RENDER_REINIT_V:
        render->video_width  = ((int*)param)[0];
        render->video_height = ((int*)param)[1];
        render->status      &= ~RENDER_VINITED;
        break;
    case PARAM_RENDER_VDEV_WIN:
#ifdef WIN32
        render->surface = param;
#endif
#ifdef ANDROID
        JniReleaseWinObj(render->surface);
        render->surface = JniRequestWinObj(param);
        vdev_setparam(render->vdev, id, render->surface);
#endif
    }
}

void render_getparam(void *hrender, int id, void *param)
{
    RENDER           *render = (RENDER*)hrender;
    VDEV_COMMON_CTXT *vdev   = render ? (VDEV_COMMON_CTXT*)render->vdev : NULL;
    if (!hrender) return;
    switch (id)
    {
    case PARAM_MEDIA_POSITION:
        if (vdev && vdev->status & VDEV_COMPLETED) {
            *(int64_t*)param  = -1; // means completed
        } else {
            *(int64_t*)param = MAX(render->cmnvars->apts, render->cmnvars->vpts);
        }
        break;
    case PARAM_AUDIO_VOLUME:
        adev_getparam(render->adev, id, param);
        break;
    case PARAM_PLAY_SPEED_VALUE:
        *(int*)param = render->speed_value;
        break;
    case PARAM_PLAY_SPEED_TYPE:
        *(int*)param = render->speed_type;
        break;
#if CONFIG_ENABLE_VEFFECT
    case PARAM_VISUAL_EFFECT:
        *(int*)param = render->veffect_type;
        break;
#endif
    case PARAM_AVSYNC_TIME_DIFF:
    case PARAM_VDEV_GET_D3DDEV:
    case PARAM_VDEV_D3D_ROTATE:
        vdev_getparam(vdev, id, param);
        break;
    case PARAM_ADEV_GET_CONTEXT:
        *(void**)param = render->adev;
        break;
    case PARAM_VDEV_GET_CONTEXT:
        *(void**)param = render->vdev;
        break;
    case PARAM_DEFINITION_VALUE:
        *(float*)param  = render->definitionval;
        render->status |= RENDER_DEFINITION_EVAL;
        break;
    }
}

