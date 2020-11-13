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
    uint8_t           *adev_buf_data ;
    uint8_t           *adev_buf_cur  ;
    int                adev_buf_size ;
    int                adev_buf_avail;

    void              *surface;
    AVRational         frmrate;

    // cmnvars & adev & vdev
    CMNVARS           *cmnvars;
    void              *adev;
    void              *vdev;

    // swresampler & swscaler
    struct SwrContext *swr_context;
    struct SwsContext *sws_context;

    // playback speed
    int                cur_speed_type;
    int                cur_speed_value;
    int                new_speed_type;
    int                new_speed_value;

    int                swr_src_format;
    int                swr_src_samprate;
    int                swr_src_chlayout;

    int                sws_src_pixfmt;
    int                sws_src_width;
    int                sws_src_height;
    int                sws_dst_pixfmt;
    int                sws_dst_width;
    int                sws_dst_height;

    /* software volume */
    #define SW_VOLUME_MINDB  -30
    #define SW_VOLUME_MAXDB  +12
    int                vol_scaler[256];
    int                vol_zerodb;
    int                vol_curvol;

    // render status
    #define RENDER_CLOSE                  (1 << 0)
    #define RENDER_PAUSE                  (1 << 1)
    #define RENDER_SNAPSHOT               (1 << 2)  // take snapshot
    #define RENDER_STEPFORWARD            (1 << 3)  // step forward
    #define RENDER_DEFINITION_EVAL        (1 << 4)
    #define RENDER_WORKAROUND_MEDIACODEC  (1 << 5)
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

// 函数实现
int swvol_scaler_init(int *scaler, int mindb, int maxdb)
{
    double tabdb[256];
    double tabf [256];
    int    z, i;

    for (i=0; i<256; i++) {
        tabdb[i]  = mindb + (maxdb - mindb) * i / 256.0;
        tabf [i]  = pow(10.0, tabdb[i] / 20.0);
        scaler[i] = (int)((1 << 14) * tabf[i]); // Q14 fix point
    }

    z = -mindb * 256 / (maxdb - mindb);
    z = MAX(z, 0  );
    z = MIN(z, 255);
    scaler[0] = 0;        // mute
    scaler[z] = (1 << 14);// 0db
    return z;
}

void swvol_scaler_run(int16_t *buf, int n, int multiplier)
{
    if (multiplier > (1 << 14)) {
        int64_t v;
        while (n--) {
            v = ((int32_t)*buf * multiplier) >> 14;
            v = MAX(v,-0x7fff);
            v = MIN(v, 0x7fff);
            *buf++ = (int16_t)v;
        }
    } else if (multiplier < (1 << 14)) {
        while (n--) { *buf = ((int32_t)*buf * multiplier) >> 14; buf++; }
    }
}

static void render_setspeed(RENDER *render, int speed)
{
    if (speed <= 0) return;
    vdev_setparam(render->vdev, PARAM_PLAY_SPEED_VALUE, &speed); // set vdev playback speed
    render->new_speed_value = speed; // set speed_value_new to triger swr_context re-create
}

// 函数实现
void* render_open(int adevtype, int vdevtype, void *surface, struct AVRational frate, int w, int h, CMNVARS *cmnvars)
{
    RENDER  *render = NULL;
    int64_t *papts  = NULL;

    render = (RENDER*)calloc(1, sizeof(RENDER));
    if (!render) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate render context !\n");
        exit(0);
    }

#ifdef WIN32
    render->surface = surface;
#endif
    render->frmrate = frate;
    render->cmnvars = cmnvars;

    render->adev_buf_avail = render->adev_buf_size = (int)((double)ADEV_SAMPLE_RATE / 60 + 0.5) * 4;
    render->adev_buf_cur   = render->adev_buf_data = malloc(render->adev_buf_size);

    // init for cmnvars
    render->adev = adev_create(adevtype, 5, render->adev_buf_size, cmnvars);
    render->vdev = vdev_create(vdevtype, surface, 0, w, h, 1000 * frate.den / frate.num, cmnvars);

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

    // init software volume scaler
    render->vol_zerodb = swvol_scaler_init(render->vol_scaler, SW_VOLUME_MINDB, SW_VOLUME_MAXDB);
    render->vol_curvol = render->vol_zerodb;
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
    free(render->adev_buf_data);
    free(render);
}

#if CONFIG_ENABLE_SOUNDTOUCH
static int render_audio_soundtouch(RENDER *render, AVFrame *audio)
{
    int16_t  buf[1024], *out = buf;
    int      num_samp, num_st;

    num_samp = swr_convert(render->swr_context,
        (uint8_t**)&out, 1024 / 2,
        (const uint8_t**)audio->extended_data, audio->nb_samples);
    audio->extended_data = NULL;
    audio->nb_samples    = 0;

    soundtouch_putSamples_i16(render->stcontext, out, num_samp);
    do {
        num_st = soundtouch_receiveSamples_i16(render->stcontext, (int16_t*)render->adev_buf_cur, render->adev_buf_avail / 4);
        render->adev_buf_avail -= num_st * 4;
        render->adev_buf_cur   += num_st * 4;
        if (render->adev_buf_avail == 0) {
#if CONFIG_ENABLE_VEFFECT
            if (render->veffect_type != VISUAL_EFFECT_DISABLE) {
                veffect_render(render->veffect_context, render->veffect_x, render->veffect_y,
                    render->veffect_w, render->veffect_h, render->veffect_type, render->adev);
            }
#endif
            swvol_scaler_run((int16_t*)render->adev_buf_data, render->adev_buf_size / sizeof(int16_t), render->vol_scaler[render->vol_curvol]);
            audio->pts += 5 * render->cur_speed_value * render->adev_buf_size / (2 * ADEV_SAMPLE_RATE);
            adev_write(render->adev, render->adev_buf_data, render->adev_buf_size, audio->pts);
            render->adev_buf_avail = render->adev_buf_size;
            render->adev_buf_cur   = render->adev_buf_data;
        }
    } while (num_st);
    return num_samp;
}
#endif

static int render_audio_swresample(RENDER *render, AVFrame *audio)
{
    int num_samp;

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
#if CONFIG_ENABLE_VEFFECT
        if (render->veffect_type != VISUAL_EFFECT_DISABLE) {
            veffect_render(render->veffect_context, render->veffect_x, render->veffect_y,
                render->veffect_w, render->veffect_h, render->veffect_type, render->adev);
        }
#endif
        swvol_scaler_run((int16_t*)render->adev_buf_data, render->adev_buf_size / sizeof(int16_t), render->vol_scaler[render->vol_curvol]);
        audio->pts += 5 * render->cur_speed_value * render->adev_buf_size / (2 * ADEV_SAMPLE_RATE);
        adev_write(render->adev, render->adev_buf_data, render->adev_buf_size, audio->pts);
        render->adev_buf_avail = render->adev_buf_size;
        render->adev_buf_cur   = render->adev_buf_data;
    }
    return num_samp;
}

void render_audio(void *hrender, AVFrame *audio)
{
    RENDER *render  = (RENDER*)hrender;
    int     samprate, sampnum;
    if (!hrender) return;

    if (render->cmnvars->init_params->avts_syncmode != AVSYNC_MODE_FILE && render->cmnvars->apktn > render->cmnvars->init_params->audio_bufpktn) return;
    do {
        if (  render->swr_src_format != audio->format || render->swr_src_samprate != audio->sample_rate || render->swr_src_chlayout != audio->channel_layout
           || render->cur_speed_type != render->new_speed_type || render->cur_speed_value != render->new_speed_value) {
            render->swr_src_format   = (int)audio->format;
            render->swr_src_samprate = (int)audio->sample_rate;
            render->swr_src_chlayout = (int)audio->channel_layout;
            render->cur_speed_type   = render->new_speed_type ;
            render->cur_speed_value  = render->new_speed_value;
            samprate = render->cur_speed_type ? ADEV_SAMPLE_RATE : (int)(ADEV_SAMPLE_RATE * 100.0 / render->cur_speed_value);
            if (render->swr_context) swr_free(&render->swr_context);
            render->swr_context = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, samprate,
                render->swr_src_chlayout, render->swr_src_format, render->swr_src_samprate, 0, NULL);
            swr_init(render->swr_context);
#if CONFIG_ENABLE_SOUNDTOUCH
            if (render->cur_speed_type) soundtouch_setTempo(render->stcontext, (float)render->cur_speed_value / 100);
#endif
        }
#if CONFIG_ENABLE_SOUNDTOUCH
        if (render->cur_speed_type && render->cur_speed_value != 100) {
            sampnum = render_audio_soundtouch(render, audio);
        } else
#endif
        {
            sampnum = render_audio_swresample(render, audio);
        }
    } while (sampnum);
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

    if (render->cmnvars->init_params->avts_syncmode != AVSYNC_MODE_FILE && render->cmnvars->vpktn > render->cmnvars->init_params->video_bufpktn) return;
    do {
        if (video->format == AV_PIX_FMT_DXVA2_VLD) {
            vdev_setparam(render->vdev, PARAM_VDEV_POST_SURFACE, video);
        } else {
            AVFrame           picture = {0};
            VDEV_COMMON_CTXT *vdev    = (VDEV_COMMON_CTXT*)render->vdev;
            if (render->sws_src_width != video->width || render->sws_src_height != video->height) {
                vdev->vw = video->width; vdev->vh = video->height;
                vdev_setparam(vdev, PARAM_VIDEO_MODE, &vdev->vm);
                render->status &= ~RENDER_WORKAROUND_MEDIACODEC;
            }
            if (render->status & RENDER_WORKAROUND_MEDIACODEC) video->format = AV_PIX_FMT_NV12;
            vdev_lock(render->vdev, picture.data, picture.linesize, video->pts);
            if (picture.data[0] && video->pts != -1) {
                if (  render->sws_src_pixfmt != video->format || render->sws_src_width != video->width        || render->sws_src_height != video->height
                   || render->sws_dst_pixfmt != vdev ->pixfmt || render->sws_dst_width != picture.linesize[6] || render->sws_dst_height != picture.linesize[7]) {
                    render->sws_src_pixfmt = video->format;
                    render->sws_src_width  = video->width;
                    render->sws_src_height = video->height;
                    render->sws_dst_pixfmt = vdev ->pixfmt;
                    render->sws_dst_width  = picture.linesize[6];
                    render->sws_dst_height = picture.linesize[7];
                    if (render->sws_context) sws_freeContext(render->sws_context);
                    render->sws_context = sws_getContext(render->sws_src_width, render->sws_src_height, render->sws_src_pixfmt,
                        render->sws_dst_width, render->sws_dst_height, render->sws_dst_pixfmt, SWS_FAST_BILINEAR, 0, 0, 0);
                }
                if (render->sws_context && 0 == sws_scale(render->sws_context, (const uint8_t**)video->data, video->linesize, 0, render->sws_src_height, picture.data, picture.linesize)) {
                    //++ on some android device, output of h264 mediacodec decoder is NV12
                    if (  (render->sws_src_pixfmt == AV_PIX_FMT_YUV420P || render->sws_src_pixfmt == AV_PIX_FMT_YUVJ420P)
                        && video->data[0] && video->data[1] && !video->data[2]
                        && video->linesize[0] == video->linesize[1] && video->linesize[2] == 0) {
                        render->status |= RENDER_WORKAROUND_MEDIACODEC;
                    }
                    //-- on some android device, output of h264 mediacodec decoder is NV12
                }
            }
            vdev_unlock(render->vdev);
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
    case 0: vdev_setrect(render->vdev, x, y, w, h); break;
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
}

int render_snapshot(void *hrender, char *file, int w, int h, int waitt)
{
#if CONFIG_ENABLE_SNAPSHOT
    RENDER *render = (RENDER*)hrender;
    if (!hrender) return -1;

    // if take snapshot in progress
    if (render->status & RENDER_SNAPSHOT) return -1;

    // copy snapshot file name
    strcpy(render->snapfile, file);
    render->snapwidth  = w;
    render->snapheight = h;

    // setup render flags to request snapshot
    render->status |= RENDER_SNAPSHOT;

    // wait take snapshot done
    if (waitt > 0) {
        int retry = waitt / 10;
        while ((render->status & RENDER_SNAPSHOT) && retry--) av_usleep(10 * 1000);
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
        {
            int vol = *(int*)param;
            vol += render->vol_zerodb;
            vol  = MAX(vol, 0  );
            vol  = MIN(vol, 255);;
            render->vol_curvol = vol;
        }
        break;
    case PARAM_PLAY_SPEED_VALUE: render_setspeed(render, *(int*)param);  break;
    case PARAM_PLAY_SPEED_TYPE : render->new_speed_type = *(int*)param;  break;
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
    case PARAM_VIDEO_MODE:
    case PARAM_AVSYNC_TIME_DIFF:
    case PARAM_VDEV_POST_SURFACE:
    case PARAM_VDEV_D3D_ROTATE:
    case PARAM_VDEV_SET_OVERLAY_RECT:
        vdev_setparam(render->vdev, id, param);
        break;
    case PARAM_RENDER_STEPFORWARD:
        render->status |= RENDER_STEPFORWARD;
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
    case PARAM_AUDIO_VOLUME    : *(int*)param = render->vol_curvol - render->vol_zerodb; break;
    case PARAM_PLAY_SPEED_VALUE: *(int*)param = render->cur_speed_value; break;
    case PARAM_PLAY_SPEED_TYPE : *(int*)param = render->cur_speed_type;  break;
#if CONFIG_ENABLE_VEFFECT
    case PARAM_VISUAL_EFFECT   : *(int*)param = render->veffect_type;    break;
#endif
    case PARAM_VIDEO_MODE:
    case PARAM_AVSYNC_TIME_DIFF:
    case PARAM_VDEV_GET_D3DDEV:
    case PARAM_VDEV_D3D_ROTATE:
    case PARAM_VDEV_GET_OVERLAY_HDC:
        vdev_getparam(vdev, id, param);
        break;
    case PARAM_ADEV_GET_CONTEXT: *(void**)param = render->adev; break;
    case PARAM_VDEV_GET_CONTEXT: *(void**)param = render->vdev; break;
    case PARAM_DEFINITION_VALUE:
        *(float*)param  = render->definitionval;
        render->status |= RENDER_DEFINITION_EVAL;
        break;
    }
}

