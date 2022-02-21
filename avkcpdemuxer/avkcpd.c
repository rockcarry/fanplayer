#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "ringbuf.h"
#include "avkcpc.h"
#include "avkcpd.h"

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
    void            *avkcpc;
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
    void*          (*render_open )(int, int, void*, struct AVRational, int, int, CMNVARS*);
    void           (*render_getparam        )(void*, int, void*);
    AVPacket*      (*pktqueue_request_packet)(void*);
    void           (*pktqueue_audio_enqueue )(void*, AVPacket*);
    void           (*pktqueue_video_enqueue )(void*, AVPacket*);
    void           (*player_send_message    )(void*, int32_t, void*);
    int            (*dxva2hwa_init          )(AVCodecContext*, void*, void*);
    int             *playerstatus;
    int              inited;
    uint32_t         curtimestamp;
    int              vpsspsppslen;
    uint8_t          vpsspsppsbuf[256];
    uint8_t          buffer[1024*1024];
} AVKCPDEMUXER;

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
    val[i < len ? i : len - 1] = '\0';
    return val;
}

static int hexstr2buf(uint8_t *buf, int size, char *str)
{
    int  len = (int)strlen(str), hex, i;
    char tmp[3];
    for (i=0; i<size && i*2+1<len; i++) {
        tmp[0] = str[i * 2 + 0];
        tmp[1] = str[i * 2 + 1];
        tmp[2] = '\0';
        sscanf(tmp, "%x", &hex);
        buf[i] = hex;
    }
    return i;
}

static int avkcpc_callback(void *ctxt, int type, char *rbuf, int rbsize, int rbhead, int fsize)
{
    AVKCPDEMUXER    *avkcpd = (AVKCPDEMUXER*)ctxt;
    AVCodec         *acodec = NULL;
    AVCodec         *vcodec = NULL;
    AVPacket        *packet = NULL;
    AVCodec         *hwdec  = NULL;
    AVCodecContext  *hwctxt = NULL;
    struct AVRational vrate;
    char avinfo[512], temp[256];
    int  ret = -1;

    switch ((char)type) {
    case 'I':
        ret = ringbuf_read((uint8_t*)rbuf, rbsize, rbhead, (uint8_t*)avinfo, fsize);
        if (avkcpd->inited) break;
        parse_params(avinfo, "aenc", avkcpd->aenctype, sizeof(avkcpd->aenctype));
        parse_params(avinfo, "venc", avkcpd->venctype, sizeof(avkcpd->venctype));
        parse_params(avinfo, "channels", temp, sizeof(temp)); avkcpd->channels = atoi(temp);
        parse_params(avinfo, "samprate", temp, sizeof(temp)); avkcpd->samprate = atoi(temp);
        parse_params(avinfo, "width"   , temp, sizeof(temp)); avkcpd->vwidth   = atoi(temp);
        parse_params(avinfo, "height"  , temp, sizeof(temp)); avkcpd->vheight  = atoi(temp);
        parse_params(avinfo, "frate"   , temp, sizeof(temp)); avkcpd->frate    = atoi(temp);
        parse_params(avinfo, "vps"     , temp, sizeof(temp)); avkcpd->vpsspsppslen  = hexstr2buf(avkcpd->vpsspsppsbuf, sizeof(avkcpd->vpsspsppsbuf), temp);
        parse_params(avinfo, "sps"     , temp, sizeof(temp)); avkcpd->vpsspsppslen += hexstr2buf(avkcpd->vpsspsppsbuf + avkcpd->vpsspsppslen, sizeof(avkcpd->vpsspsppsbuf) - avkcpd->vpsspsppslen, temp);
        parse_params(avinfo, "pps"     , temp, sizeof(temp)); avkcpd->vpsspsppslen += hexstr2buf(avkcpd->vpsspsppsbuf + avkcpd->vpsspsppslen, sizeof(avkcpd->vpsspsppsbuf) - avkcpd->vpsspsppslen, temp);
        if (strstr(avkcpd->aenctype, "alaw") == avkcpd->aenctype) { avkcpd->channels = 1; avkcpd->samprate = 8000; }
        if (strstr(avkcpd->aenctype, "aac" ) == avkcpd->aenctype) acodec = avcodec_find_decoder(AV_CODEC_ID_AAC     );
        if (strstr(avkcpd->aenctype, "alaw") == avkcpd->aenctype) acodec = avcodec_find_decoder(AV_CODEC_ID_PCM_ALAW);
        if (strstr(avkcpd->venctype, "h264") == avkcpd->venctype) vcodec = avcodec_find_decoder(AV_CODEC_ID_H264    );
        if (strstr(avkcpd->venctype, "h265") == avkcpd->venctype) vcodec = avcodec_find_decoder(AV_CODEC_ID_H265    );
        vrate.num = avkcpd->frate; vrate.den = 1;
        if (acodec) {
            *avkcpd->acodec_context = avcodec_alloc_context3(acodec);
            if (acodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                (*avkcpd->acodec_context)->flags |= AV_CODEC_FLAG_TRUNCATED;
            }
            (*avkcpd->acodec_context)->channels       = avkcpd->channels;
            (*avkcpd->acodec_context)->channel_layout = avkcpd->channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
            (*avkcpd->acodec_context)->sample_rate    = avkcpd->samprate;
            (*avkcpd->acodec_context)->sample_fmt     = acodec->id == AV_CODEC_ID_AAC ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
            if (avcodec_open2(*avkcpd->acodec_context, acodec, NULL) < 0) {
                avcodec_close(*avkcpd->acodec_context);
                avcodec_free_context(avkcpd->acodec_context);
            }
        }
        if (vcodec) {
#ifdef ANDROID
            if (avkcpd->cmnvars->init_params->video_hwaccel) {
                switch (vcodec->id) {
                case AV_CODEC_ID_H264: hwdec = avcodec_find_decoder_by_name("h264_mediacodec" ); break;
                case AV_CODEC_ID_HEVC: hwdec = avcodec_find_decoder_by_name("hevc_mediacodec" ); break;
                default: break;
                }
            }
            if (hwdec) {
                hwctxt = avcodec_alloc_context3(hwdec);
                hwctxt->width          = avkcpd->vwidth;
                hwctxt->height         = avkcpd->vheight;
                hwctxt->framerate      = vrate;
                hwctxt->extradata_size = avkcpd->vpsspsppslen;
                hwctxt->extradata      = avkcpd->vpsspsppsbuf;
                if (hwdec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                    hwctxt->flags |= AV_CODEC_FLAG_TRUNCATED;
                }
                if (avcodec_open2(hwctxt, hwdec, NULL) == 0) {
                   *avkcpd->vcodec_context = hwctxt;
                } else {
                    hwctxt->extradata_size = 0;
                    hwctxt->extradata      = NULL;
                    avkcpd->cmnvars->init_params->video_hwaccel = 0;
                    avcodec_close(hwctxt);
                    avcodec_free_context(&hwctxt);
                    hwdec = NULL;
                }
            } else avkcpd->cmnvars->init_params->video_hwaccel = 0;
#endif
            if (!hwctxt) {
                *avkcpd->vcodec_context = avcodec_alloc_context3(vcodec);
                if (vcodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                    (*avkcpd->vcodec_context)->flags |= AV_CODEC_FLAG_TRUNCATED;
                }
                (*avkcpd->vcodec_context)->width          = avkcpd->vwidth;
                (*avkcpd->vcodec_context)->height         = avkcpd->vheight;
                (*avkcpd->vcodec_context)->framerate      = vrate;
                (*avkcpd->vcodec_context)->extradata_size = avkcpd->vpsspsppslen;
                (*avkcpd->vcodec_context)->extradata      = avkcpd->vpsspsppsbuf;
                (*avkcpd->vcodec_context)->pix_fmt        = AV_PIX_FMT_YUV420P;
                (*avkcpd->vcodec_context)->thread_count   = avkcpd->cmnvars->init_params->video_thread_count;
                if (avcodec_open2(*avkcpd->vcodec_context, vcodec, NULL) < 0) {
                    avcodec_close(*avkcpd->vcodec_context);
                    avcodec_free_context(avkcpd->vcodec_context);
                }
            }
        }
       *avkcpd->render = avkcpd->render_open(avkcpd->adevtype, avkcpd->vdevtype, avkcpd->cmnvars->winmsg, vrate, avkcpd->vwidth, avkcpd->vheight, avkcpd->cmnvars);
#ifdef WIN32
        if (vcodec && avkcpd->cmnvars->init_params->video_hwaccel) {
            void *d3ddev = NULL;
            if (avkcpd->cmnvars->init_params->video_hwaccel == 1) avkcpd->render_getparam(*avkcpd->render, PARAM_VDEV_GET_D3DDEV, &d3ddev);
            if (avkcpd->dxva2hwa_init(*avkcpd->vcodec_context, d3ddev, avkcpd->cmnvars->winmsg) != 0) {
                avkcpd->cmnvars->init_params->video_hwaccel = 0;
            }
        }
#endif
        if (avkcpd->cmnvars->init_params->avts_syncmode == 0) avkcpd->cmnvars->init_params->avts_syncmode = AVSYNC_MODE_LIVE_SYNC0;
        avkcpd->cmnvars->init_params->video_vwidth  = avkcpd->cmnvars->init_params->video_owidth  = avkcpd->vwidth ;
        avkcpd->cmnvars->init_params->video_vheight = avkcpd->cmnvars->init_params->video_oheight = avkcpd->vheight;
       *avkcpd->playerstatus = 0;
        avkcpd->inited       = 1;
        avkcpd->player_send_message(avkcpd->cmnvars->winmsg, MSG_OPEN_DONE, avkcpd->player);
        break;
    case 'A': case 'V':
        if (!avkcpd->inited) { ret = ringbuf_read((uint8_t*)rbuf, rbsize, rbhead, NULL, fsize); break; }
        packet = avkcpd->pktqueue_request_packet(avkcpd->pktqueue);
        if (packet == NULL) return -1;
        av_new_packet(packet, fsize);
        packet->pts = packet->dts = avkcpd->curtimestamp ? avkcpd->curtimestamp : get_tick_count();
        ret = ringbuf_read((uint8_t*)rbuf, rbsize, rbhead, packet->data, fsize);
        if ((char)type == 'A') avkcpd->pktqueue_audio_enqueue(avkcpd->pktqueue, packet);
        else                   avkcpd->pktqueue_video_enqueue(avkcpd->pktqueue, packet);
        break;
    case 'T':
        avkcpd->curtimestamp = (uint32_t)type >> 8;
        ret = rbhead;
        break;
    }
    return ret;
}

void* avkcpdemuxer_init(char *url, void *player, void *pktqueue, AVCodecContext **acodec_context, AVCodecContext **vcodec_context, int *playerstatus,
                        AVRational *astream_timebase, AVRational *vstream_timebase, void **render, int adevtype, int vdevtype, CMNVARS *cmnvars,
                        void *pfnrenderopen, void *pfnrendergetparam, void *pfnpktqrequest, void *pfnpktqaenqueue, void *pfnpktqvenqueue, void *pfnplayermsg, void *pfndxva2hwinit)
{
    AVKCPDEMUXER *avkcpd = NULL;
    char         *str    = NULL;
    char          ipaddr[16];
    int           port;

    if (strstr(url, "avkcp://") != url) return NULL;
    avkcpd = calloc(1, sizeof(AVKCPDEMUXER));
    if (!avkcpd) return NULL;

    str = strstr(url + 8, ":");
    port = str ? atoi(str + 1) : 8000;
    strncpy(ipaddr, url + 8, sizeof(ipaddr));
    str = strstr(ipaddr, ":");
    if (str) *str = '\0';

    astream_timebase->num  = 1;
    astream_timebase->den  = 1000;
    vstream_timebase->num  = 1;
    vstream_timebase->den  = 1000;

    avkcpd->player                  = player;
    avkcpd->pktqueue                = pktqueue;
    avkcpd->acodec_context          = acodec_context;
    avkcpd->vcodec_context          = vcodec_context;
    avkcpd->playerstatus            = playerstatus;
    avkcpd->render                  = render;
    avkcpd->adevtype                = adevtype;
    avkcpd->vdevtype                = vdevtype;
    avkcpd->cmnvars                 = cmnvars;
    avkcpd->render_open             = pfnrenderopen;
    avkcpd->render_getparam         = pfnrendergetparam;
    avkcpd->pktqueue_request_packet = pfnpktqrequest;
    avkcpd->pktqueue_audio_enqueue  = pfnpktqaenqueue;
    avkcpd->pktqueue_video_enqueue  = pfnpktqvenqueue;
    avkcpd->player_send_message     = pfnplayermsg;
    avkcpd->dxva2hwa_init           = pfndxva2hwinit;

    avkcpd->avkcpc = avkcpc_init(ipaddr, port, avkcpc_callback, avkcpd);
    if (!avkcpd->avkcpc) { free(avkcpd); avkcpd = NULL; }
    return avkcpd;
}

void avkcpdemuxer_exit(void *ctxt)
{
    AVKCPDEMUXER *avkcpd = (AVKCPDEMUXER*)ctxt;
    if (avkcpd) {
        avkcpc_exit(avkcpd->avkcpc);
        if (*avkcpd->vcodec_context) {
            (*avkcpd->vcodec_context)->extradata_size = 0;
            (*avkcpd->vcodec_context)->extradata      = NULL;
        }
        avcodec_close(*avkcpd->acodec_context);
        avcodec_close(*avkcpd->vcodec_context);
        avcodec_free_context(avkcpd->acodec_context);
        avcodec_free_context(avkcpd->vcodec_context);
        free(avkcpd);
    }
}
