#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "ringbuf.h"
#include "ffrdpc.h"
#include "ffrdpd.h"

#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4996) // disable warnings
#define get_tick_count GetTickCount
#else
static uint32_t get_tick_count()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

typedef struct {
    AVCodecContext **acodec_context;
    AVCodecContext **vcodec_context;
    void            *player;
    void            *pktqueue;
    void           **render;
    void            *ffrdpc;
    char             aenctype[8];
    char             venctype[8];
    int              vwidth;
    int              vheight;
    int              frate;
    int              channels;
    int              samprate;
    int              adevtype;
    int              vdevtype;
    CMNVARS         *cmnvars;
    void*          (*render_open )(int, int, int, int64_t, int, void*, struct AVRational, int, int, int, CMNVARS*);
    void           (*render_getparam        )(void*, int, void*);
    AVPacket*      (*pktqueue_request_packet)(void*);
    void           (*pktqueue_audio_enqueue )(void*, AVPacket*);
    void           (*pktqueue_video_enqueue )(void*, AVPacket*);
    void           (*player_send_message    )(void*, int32_t, int64_t);
    int            (*dxva2hwa_init          )(AVCodecContext*, void*);
    int             *playerstatus;
    int              inited;
    uint8_t          buffer[1024*1024];
} FFRDPDEMUXER;

static char* parse_params(const char *str, const char *key, char *val, int len)
{
    char *p = (char*)strstr(str, key);
    int   i;

    if (!p) return NULL;
    p += strlen(key);
    if (*p == '\0') return NULL;

    while (1) {
        if (*p != ' ' && *p != '=' && *p != ':') break;
        else p++;
    }

    for (i=0; i<len; i++) {
        if (*p == '\\') {
            p++;
        } else if (*p == ',' || *p == ';' || *p == '\n' || *p == '\0') {
            break;
        }
        val[i] = *p++;
    }
    val[i] = val[len-1] = '\0';
    return val;
}

static int ffrdpc_callback(void *ctxt, int type, char *rbuf, int rbsize, int rbhead, int fsize)
{
    FFRDPDEMUXER    *ffrdpd = (FFRDPDEMUXER*)ctxt;
    AVCodec         *acodec = NULL;
    AVCodec         *vcodec = NULL;
    AVPacket        *packet = NULL;
    AVCodec         *hwdec  = NULL;
    AVCodecContext  *hwctxt = NULL;
    struct AVRational vrate;
    char avinfo[256], temp[256];
    int  ret = -1;

    switch (type) {
    case 'I':
        ret = ringbuf_read((uint8_t*)rbuf, rbsize, rbhead, (uint8_t*)avinfo, fsize);
        if (ffrdpd->inited) break;
        parse_params(avinfo, "aenc", ffrdpd->aenctype, sizeof(ffrdpd->aenctype));
        parse_params(avinfo, "venc", ffrdpd->venctype, sizeof(ffrdpd->venctype));
        parse_params(avinfo, "channels", temp, sizeof(temp)); ffrdpd->channels = atoi(temp);
        parse_params(avinfo, "samprate", temp, sizeof(temp)); ffrdpd->samprate = atoi(temp);
        parse_params(avinfo, "width"   , temp, sizeof(temp)); ffrdpd->vwidth   = atoi(temp);
        parse_params(avinfo, "height"  , temp, sizeof(temp)); ffrdpd->vheight  = atoi(temp);
        parse_params(avinfo, "frate"   , temp, sizeof(temp)); ffrdpd->frate    = atoi(temp);
        if (strstr(ffrdpd->aenctype, "alaw") == ffrdpd->aenctype) { ffrdpd->channels = 1; ffrdpd->samprate = 8000; }
        if (strstr(ffrdpd->aenctype, "aac" ) == ffrdpd->aenctype) acodec = avcodec_find_decoder(AV_CODEC_ID_AAC     );
        if (strstr(ffrdpd->aenctype, "alaw") == ffrdpd->aenctype) acodec = avcodec_find_decoder(AV_CODEC_ID_PCM_ALAW);
        if (strstr(ffrdpd->venctype, "h264") == ffrdpd->venctype) vcodec = avcodec_find_decoder(AV_CODEC_ID_H264    );
        if (strstr(ffrdpd->venctype, "h265") == ffrdpd->venctype) vcodec = avcodec_find_decoder(AV_CODEC_ID_H265    );
        vrate.num = ffrdpd->frate; vrate.den = 1;
        if (acodec) {
            *ffrdpd->acodec_context = avcodec_alloc_context3(acodec);
            if (acodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                (*ffrdpd->acodec_context)->flags |= AV_CODEC_FLAG_TRUNCATED;
            }
            (*ffrdpd->acodec_context)->channels       = ffrdpd->channels;
            (*ffrdpd->acodec_context)->channel_layout = ffrdpd->channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
            (*ffrdpd->acodec_context)->sample_rate    = ffrdpd->samprate;
            (*ffrdpd->acodec_context)->sample_fmt     = acodec->id == AV_CODEC_ID_AAC ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
            if (avcodec_open2(*ffrdpd->acodec_context, acodec, NULL) < 0) {
                avcodec_close(*ffrdpd->acodec_context);
                avcodec_free_context(ffrdpd->acodec_context);
            }
        }
        if (vcodec) {
#ifdef ANDROID
            if (ffrdpd->cmnvars->init_params->video_hwaccel) {
                switch (vcodec->id) {
                case AV_CODEC_ID_H264: hwdec = avcodec_find_decoder_by_name("h264_mediacodec" ); break;
                case AV_CODEC_ID_HEVC: hwdec = avcodec_find_decoder_by_name("hevc_mediacodec" ); break;
                default: break;
                }
            }
            if (hwdec) {
                hwctxt = avcodec_alloc_context3(hwdec);
                if (hwdec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                    hwctxt->flags |= AV_CODEC_FLAG_TRUNCATED;
                }
                if (avcodec_open2(hwctxt, hwdec, NULL) == 0) {
                   *ffrdpd->vcodec_context = hwctxt;
                } else {
                    ffrdpd->cmnvars->init_params->video_hwaccel = 0;
                    avcodec_close(hwctxt);
                    avcodec_free_context(&hwctxt);
                }
            } else ffrdpd->cmnvars->init_params->video_hwaccel = 0;
#endif
            if (ffrdpd->cmnvars->init_params->video_hwaccel == 0) {
                *ffrdpd->vcodec_context = avcodec_alloc_context3(vcodec);
                if (vcodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                    (*ffrdpd->vcodec_context)->flags |= AV_CODEC_FLAG_TRUNCATED;
                }
                (*ffrdpd->vcodec_context)->width     = ffrdpd->vwidth;
                (*ffrdpd->vcodec_context)->height    = ffrdpd->vheight;
                (*ffrdpd->vcodec_context)->framerate = vrate;
                (*ffrdpd->vcodec_context)->pix_fmt   = AV_PIX_FMT_YUV420P;
                if (avcodec_open2(*ffrdpd->vcodec_context, vcodec, NULL) < 0) {
                    avcodec_close(*ffrdpd->vcodec_context);
                    avcodec_free_context(ffrdpd->vcodec_context);
                }
            }
#ifdef WIN32
            if (1) {
                void *d3ddev = NULL; ffrdpd->render_getparam(ffrdpd->render, PARAM_VDEV_GET_D3DDEV, &d3ddev);
                if (ffrdpd->dxva2hwa_init(*ffrdpd->vcodec_context, d3ddev) != 0) {
                    ffrdpd->cmnvars->init_params->video_hwaccel = 0;
                }
            }
#endif
        }
       *ffrdpd->render = ffrdpd->render_open(
            ffrdpd->adevtype, ffrdpd->samprate, (*ffrdpd->acodec_context)->sample_fmt, (*ffrdpd->acodec_context)->channel_layout,
            ffrdpd->vdevtype, ffrdpd->cmnvars->winmsg, vrate, (*ffrdpd->vcodec_context)->pix_fmt, ffrdpd->vwidth, ffrdpd->vheight, ffrdpd->cmnvars);
        ffrdpd->cmnvars->init_params->avts_syncmode = AVSYNC_MODE_LIVE_SYNC0;
        ffrdpd->cmnvars->init_params->video_vwidth  = ffrdpd->cmnvars->init_params->video_owidth  = ffrdpd->vwidth ;
        ffrdpd->cmnvars->init_params->video_vheight = ffrdpd->cmnvars->init_params->video_oheight = ffrdpd->vheight;
       *ffrdpd->playerstatus = 0;
        ffrdpd->inited       = 1;
        ffrdpd->player_send_message(ffrdpd->cmnvars->winmsg, MSG_OPEN_DONE, (int64_t)ffrdpd->player);
        break;
    case 'A': case 'V':
        packet = ffrdpd->pktqueue_request_packet(ffrdpd->pktqueue);
        if (packet == NULL) return -1;
        av_new_packet(packet, fsize);
        packet->pts = packet->dts = get_tick_count();
        ret = ringbuf_read((uint8_t*)rbuf, rbsize, rbhead, packet->data, fsize);
        if (type == 'A') ffrdpd->pktqueue_audio_enqueue(ffrdpd->pktqueue, packet);
        else             ffrdpd->pktqueue_video_enqueue(ffrdpd->pktqueue, packet);
        break;
    }
    return ret;
}

void* ffrdpdemuxer_init(char *url, void *player, void *pktqueue, AVCodecContext **acodec_context, AVCodecContext **vcodec_context, int *playerstatus,
                        AVRational *astream_timebase, AVRational *vstream_timebase, void **render, int adevtype, int vdevtype, CMNVARS *cmnvars,
                        void *pfnrenderopen, void *pfnrendergetparam, void *pfnpktqrequest, void *pfnpktqaenqueue, void *pfnpktqvenqueue, void *pfnplayermsg, void *pfndxva2hwinit)
{
    FFRDPDEMUXER *ffrdpd = NULL;
    char         *str    = NULL;
    char          ipaddr[16];
    int           port;

    if (strstr(url, "ffrdp://") != url) return NULL;
    ffrdpd = calloc(1, sizeof(FFRDPDEMUXER));
    if (!ffrdpd) return NULL;

    str = strstr(url + 8, ":");
    port = str ? atoi(str + 1) : 8000;
    strncpy(ipaddr, url + 8, sizeof(ipaddr));
    str = strstr(ipaddr, ":");
    if (str) *str = '\0';
 
    ffrdpd->ffrdpc = ffrdpc_init(ipaddr, port, ffrdpc_callback, ffrdpd);
    if (!ffrdpd->ffrdpc) { free(ffrdpd); return NULL; }

    astream_timebase->num  = 1;
    astream_timebase->den  = 1000;
    vstream_timebase->num  = 1;
    vstream_timebase->den  = 1000;

    ffrdpd->player         = player;
    ffrdpd->pktqueue       = pktqueue;
    ffrdpd->acodec_context = acodec_context;
    ffrdpd->vcodec_context = vcodec_context;
    ffrdpd->playerstatus   = playerstatus;
    ffrdpd->render         = render;
    ffrdpd->adevtype       = adevtype;
    ffrdpd->vdevtype       = vdevtype;
    ffrdpd->cmnvars        = cmnvars;

    ffrdpd->render_open             = pfnrenderopen;
    ffrdpd->render_getparam         = pfnrendergetparam;
    ffrdpd->pktqueue_request_packet = pfnpktqrequest;
    ffrdpd->pktqueue_audio_enqueue  = pfnpktqaenqueue;
    ffrdpd->pktqueue_video_enqueue  = pfnpktqvenqueue;
    ffrdpd->player_send_message     = pfnplayermsg;
    ffrdpd->dxva2hwa_init           = pfndxva2hwinit;
    return ffrdpd;
}

void ffrdpdemuxer_exit(void *ctxt)
{
    FFRDPDEMUXER *ffrdpd = (FFRDPDEMUXER*)ctxt;
    if (ffrdpd) {
        avcodec_close(*ffrdpd->acodec_context);
        avcodec_close(*ffrdpd->vcodec_context);
        avcodec_free_context(ffrdpd->acodec_context);
        avcodec_free_context(ffrdpd->vcodec_context);
        ffrdpc_exit(ffrdpd->ffrdpc);
        free(ffrdpd);
    }
}
