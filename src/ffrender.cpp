// 包含头文件
#include <pthread.h>
#include "ffrender.h"
#include "snapshot.h"
#include "veffect.h"
#include "adev.h"
#include "vdev.h"

extern "C" {
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

#ifdef WIN32
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#endif

// 内部类型定义
typedef struct
{
    // adev & vdev
    void          *adev;
    void          *vdev;

    // swresampler & swscaler
    SwrContext    *swr_context;
    SwsContext    *sws_context;

    int            sample_rate;
    AVSampleFormat sample_fmt;
    int64_t        chan_layout;

    int            video_width;
    int            video_height;
    AVRational     frame_rate;
    AVPixelFormat  pixel_fmt;

    int            adev_buf_avail;
    uint8_t       *adev_buf_cur;
    AUDIOBUF      *adev_hdr_cur;

    // video render rect
    int            render_xcur;
    int            render_ycur;
    int            render_xnew;
    int            render_ynew;
    int            render_wcur;
    int            render_hcur;
    int            render_wnew;
    int            render_hnew;

    // playback speed
    int            render_speed_cur;
    int            render_speed_new;

#if CONFIG_ENABLE_VEFFECT
    // visual effect
    void          *veffect_context;
    int            veffect_type;
    int            veffect_x;
    int            veffect_y;
    int            veffect_w;
    int            veffect_h;
    pthread_t      veffect_thread;
#endif

    // render status
    #define RENDER_CLOSE    (1 << 0)
    #define RENDER_PAUSE    (1 << 1)
    #define RENDER_SNAPSHOT (1 << 2)  // take snapshot
    #define RENDER_SEEKSTEP (1 << 3)  // seek step
    int            render_status;

#if CONFIG_ENABLE_SNAPSHOT
    // snapshot
    char           snapfile[MAX_PATH];
    int            snapwidth;
    int            snapheight;
#endif
} RENDER;

// 内部函数实现
static void render_setspeed(RENDER *render, int speed)
{
    if (speed > 0) {
        // set vdev frame rate
        int framerate = (int)((render->frame_rate.num * speed) / (render->frame_rate.den * 100.0) + 0.5);
        vdev_setparam(render->vdev, PARAM_VDEV_FRAME_RATE, &framerate);

        // set render_speed_new to triger swr_context re-create
        render->render_speed_new = speed;
    }
}

#if CONFIG_ENABLE_VEFFECT
static void* render_veffect_thread(void *param)
{
    RENDER *render = (RENDER*)param;
    int     timeus = 1000000LL * render->frame_rate.den / render->frame_rate.num;
    while (!(render->render_status & RENDER_CLOSE)) {
        if (render->veffect_type != VISUAL_EFFECT_DISABLE) {
            void *buf = NULL;
            int   len = 0;
            adev_curdata  (render->adev, &buf, &len);
            veffect_render(render->veffect_context,
                render->veffect_x, render->veffect_y,
                render->veffect_w, render->veffect_h,
                render->veffect_type, buf, len);
        }
        av_usleep(timeus);
    }
    return NULL;
}
#endif

// 函数实现
void* render_open(int adevtype, int srate, AVSampleFormat sndfmt, int64_t ch_layout,
                  int vdevtype, void *surface, AVRational frate, AVPixelFormat pixfmt, int w, int h)
{
    RENDER *render = (RENDER*)calloc(1, sizeof(RENDER));
    if (!render) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate render context !\n");
        exit(0);
    }

    // init for video
    render->video_width  = w;
    render->video_height = h;
    render->render_wnew  = w;
    render->render_hnew  = h;
    render->frame_rate   = frate;
    render->pixel_fmt    = pixfmt;
    if (render->pixel_fmt == AV_PIX_FMT_NONE) {
        render->pixel_fmt = AV_PIX_FMT_YUV420P;
    }

    // init for audio
    render->sample_rate  = srate;
    render->sample_fmt   = sndfmt;
    render->chan_layout  = ch_layout;

    // init for visual effect
#if CONFIG_ENABLE_VEFFECT
    render->veffect_context = veffect_create(surface);
    pthread_create(&render->veffect_thread, NULL, render_veffect_thread, render);
#endif

    // create adev & vdev
    render->adev = adev_create(adevtype, 0, (int)((double)ADEV_SAMPLE_RATE * frate.den / frate.num + 0.5) * 4);
    render->vdev = vdev_create(vdevtype, surface, 0, w, h, (int)((double)frate.num / frate.den + 0.5));

    // make adev & vdev sync together
    int64_t *papts = NULL;
    vdev_getavpts(render->vdev, &papts, NULL);
    adev_syncapts(render->adev,  papts);

#ifdef WIN32
    RECT rect; GetClientRect((HWND)surface, &rect);
    render_setrect(render, 0, rect.left, rect.top, rect.right, rect.bottom);
    render_setrect(render, 1, rect.left, rect.top, rect.right, rect.bottom);
#endif

    // set default playback speed
    render_setspeed(render, 100);
    return render;
}

void render_close(void *hrender)
{
    RENDER *render = (RENDER*)hrender;

    // wait visual effect thread exit
    render->render_status = RENDER_CLOSE;

#if CONFIG_ENABLE_VEFFECT
    pthread_join(render->veffect_thread, NULL);
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

    // free context
    free(render);
}

void render_audio(void *hrender, AVFrame *audio)
{
    RENDER *render  = (RENDER*)hrender;
    int     sampnum = 0;
    int64_t apts    = audio->pts;

    if (!render || !render->adev) return;
    do {
        if (render->adev_buf_avail == 0) {
            adev_lock(render->adev, &render->adev_hdr_cur);
            apts += 10 * render->render_speed_cur * render->frame_rate.den / render->frame_rate.num;
            render->adev_buf_avail = (int     )render->adev_hdr_cur->size;
            render->adev_buf_cur   = (uint8_t*)render->adev_hdr_cur->data;
        }

        if (render->render_speed_cur != render->render_speed_new) {
            render->render_speed_cur = render->render_speed_new;
            //++ allocate & init swr context
            if (render->swr_context) {
                swr_free(&render->swr_context);
            }
            int samprate = (int)(ADEV_SAMPLE_RATE * 100.0 / render->render_speed_cur);
            render->swr_context = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, samprate,
                render->chan_layout, render->sample_fmt, render->sample_rate, 0, NULL);
            swr_init(render->swr_context);
            //-- allocate & init swr context
        }

        //++ do resample audio data ++//
        sampnum = swr_convert(render->swr_context,
            (uint8_t**)&render->adev_buf_cur, render->adev_buf_avail / 4,
            (const uint8_t**)audio->extended_data, audio->nb_samples);
        audio->extended_data    = NULL;
        audio->nb_samples       = 0;
        render->adev_buf_avail -= sampnum * 4;
        render->adev_buf_cur   += sampnum * 4;
        //-- do resample audio data --//

        if (render->adev_buf_avail == 0) {
            adev_unlock(render->adev, apts);
        }
    } while (sampnum > 0);
}

void render_video(void *hrender, AVFrame *video)
{
    RENDER  *render = (RENDER*)hrender;
    AVFrame  picture;

    // init picture
    memset(&picture, 0, sizeof(AVFrame));

    if (!render || !render->vdev) return;
    do {
        VDEV_COMMON_CTXT *vdev = (VDEV_COMMON_CTXT*)render->vdev;
        if (  render->render_xcur != render->render_xnew
           || render->render_ycur != render->render_ynew
           || render->render_wcur != render->render_wnew
           || render->render_hcur != render->render_hnew ) {
            render->render_xcur = render->render_xnew;
            render->render_ycur = render->render_ynew;
            render->render_wcur = render->render_wnew;
            render->render_hcur = render->render_hnew;

            // vdev set rect
            vdev_setrect(render->vdev, render->render_xcur, render->render_ycur,
                render->render_wcur, render->render_hcur);

            // we need recreate sws
            if (!render->sws_context) {
                sws_freeContext(render->sws_context);
            }
            render->sws_context = sws_getContext(
                render->video_width, render->video_height, render->pixel_fmt,
                vdev->sw, vdev->sh, (AVPixelFormat)vdev->pixfmt,
                SWS_FAST_BILINEAR, 0, 0, 0);
        }

        if (video->format == AV_PIX_FMT_DXVA2_VLD) {
            vdev_setparam(render->vdev, PARAM_VDEV_POST_SURFACE, video);
        } else {
            vdev_lock(render->vdev, picture.data, picture.linesize);
            if (picture.data[0] && video->pts != -1) {
                sws_scale(render->sws_context, video->data, video->linesize, 0, render->video_height, picture.data, picture.linesize);
            }
            vdev_unlock(render->vdev, video->pts);
        }

#if CONFIG_ENABLE_SNAPSHOT
        if (render->render_status & RENDER_SNAPSHOT) {
            int ret = take_snapshot(render->snapfile, render->snapwidth, render->snapheight, video);
            player_send_message(((VDEV_COMMON_CTXT*)render->vdev)->surface, MSG_TAKE_SNAPSHOT, 0);
            render->render_status &= ~RENDER_SNAPSHOT;
        }
#endif

        //++ seek step
        if (render->render_status & RENDER_SEEKSTEP) {
            render->render_status &= ~RENDER_SEEKSTEP;
            break;
        }
        //-- seek step
    } while (render->render_status & RENDER_PAUSE);
}

void render_setrect(void *hrender, int type, int x, int y, int w, int h)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    switch (type) {
    case 0:
        render->render_xnew = x;
        render->render_ynew = y;
        render->render_wnew = w > 1 ? w : 1;
        render->render_hnew = h > 1 ? h : 1;
        break;
#if CONFIG_ENABLE_VEFFECT
    case 1:
        render->veffect_x = x;
        render->veffect_y = y;
        render->veffect_w = w > 1 ? w : 1;
        render->veffect_h = h > 1 ? h : 1;
        break;
#endif
    }
}

void render_start(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    render->render_status &=~RENDER_PAUSE;
    adev_pause(render->adev, 0);
    vdev_pause(render->vdev, 0);
}

void render_pause(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    render->render_status |= RENDER_PAUSE;
    adev_pause(render->adev, 1);
    vdev_pause(render->vdev, 1);
}

void render_reset(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    adev_reset(render->adev);
    vdev_reset(render->vdev);
    render->render_status = 0;
}

int render_snapshot(void *hrender, char *file, int w, int h, int waitt)
{
    DO_USE_VAR(hrender);
    DO_USE_VAR(file);
    DO_USE_VAR(w);
    DO_USE_VAR(h);
    DO_USE_VAR(waitt);

#if CONFIG_ENABLE_SNAPSHOT
    if (!hrender) return -1;
    RENDER *render = (RENDER*)hrender;

    // if take snapshot in progress
    if (render->render_status & RENDER_SNAPSHOT) {
        return -1;
    }

    // copy snapshot file name
    strcpy(render->snapfile, file);
    render->snapwidth  = w;
    render->snapheight = h;

    // setup render flags to request snapshot
    render->render_status |= RENDER_SNAPSHOT;

    // wait take snapshot done
    if (waitt > 0) {
        int retry = waitt / 10;
        while ((render->render_status & RENDER_SNAPSHOT) && retry--) {
            av_usleep(10 * 1000);
        }
    }
#endif

    return 0;
}

void render_setparam(void *hrender, int id, void *param)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    switch (id)
    {
    case PARAM_AUDIO_VOLUME:
        adev_setparam(render->adev, id, param);
        break;
    case PARAM_PLAY_SPEED:
        render_setspeed(render, *(int*)param);
        break;
#if CONFIG_ENABLE_VEFFECT
    case PARAM_VISUAL_EFFECT:
        render->veffect_type = *(int*)param;
        if (render->veffect_type == VISUAL_EFFECT_DISABLE) {
            veffect_render(render->veffect_context,
                render->veffect_x, render->veffect_y,
                render->veffect_w, render->veffect_h,
                VISUAL_EFFECT_DISABLE, 0, 0);
        }
        break;
#endif
    case PARAM_AVSYNC_TIME_DIFF:
    case PARAM_VDEV_POST_SURFACE:
        vdev_setparam(render->vdev, id, param);
        break;
    case PARAM_RENDER_SEEK_STEP:
        render->render_status |= RENDER_SEEKSTEP;
        break;
    }
}

void render_getparam(void *hrender, int id, void *param)
{
    if (!hrender) return;
    RENDER         *render = (RENDER*)hrender;
    VDEV_COMMON_CTXT *vdev = (VDEV_COMMON_CTXT*)render->vdev;
    switch (id)
    {
    case PARAM_MEDIA_POSITION:
        if (vdev->status & VDEV_COMPLETED) {
            *(int64_t*)param  = -1; // means completed
        } else {
            *(int64_t*)param = vdev->apts > vdev->vpts ? vdev->apts : vdev->vpts;
        }
        break;
    case PARAM_AUDIO_VOLUME:
        adev_getparam(render->adev, id, param);
        break;
    case PARAM_PLAY_SPEED:
        *(int*)param = render->render_speed_cur;
        break;
#if CONFIG_ENABLE_VEFFECT
    case PARAM_VISUAL_EFFECT:
        *(int*)param = render->veffect_type;
        break;
#endif
    case PARAM_AVSYNC_TIME_DIFF:
    case PARAM_VDEV_GET_D3DDEV:
        vdev_getparam(render->vdev, id, param);
        break;
    case PARAM_ADEV_GET_CONTEXT:
        *(void**)param = render->adev;
        break;
    case PARAM_VDEV_GET_CONTEXT:
        *(void**)param = render->vdev;
        break;
    }
}


