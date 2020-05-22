#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "ringbuf.h"
#include "avkcpc.h"
#include "avkcpd.h"

#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4996) // disable warnings
#endif

typedef struct {
    AVCodec         *acodec;
    AVCodec         *vcodec;
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
    void*          (*render_open )(int, int, int, int64_t, int, void*, struct AVRational, int, int, int, CMNVARS*);
    AVPacket*      (*pktqueue_request_packet)(void*);
    void           (*pktqueue_audio_enqueue )(void*, AVPacket*);
    void           (*pktqueue_video_enqueue )(void*, AVPacket*);
    void           (*player_send_message    )(void*, int32_t, int64_t);
    int             *playerstatus;
    int              inited;
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
    val[i] = val[len-1] = '\0';
    return val;
}

static int avkcpc_callback(void *ctxt, int type, char *rbuf, int rbsize, int rbhead, int fsize)
{
    AVKCPDEMUXER    *avkcpd = (AVKCPDEMUXER*)ctxt;
    AVPacket        *packet = NULL;
    struct AVRational vrate;
    char avinfo[256], temp[256];
    int  ret = -1;

    switch (type) {
    case 'I':
        ret = ringbuf_read(rbuf, rbsize, rbhead, avinfo, fsize);
        if (avkcpd->inited) break;
        parse_params(avinfo, "aenc", avkcpd->aenctype, sizeof(avkcpd->aenctype));
        parse_params(avinfo, "venc", avkcpd->venctype, sizeof(avkcpd->venctype));
        parse_params(avinfo, "channels", temp, sizeof(temp)); avkcpd->channels = atoi(temp);
        parse_params(avinfo, "samprate", temp, sizeof(temp)); avkcpd->samprate = atoi(temp);
        parse_params(avinfo, "width"   , temp, sizeof(temp)); avkcpd->vwidth   = atoi(temp);
        parse_params(avinfo, "height"  , temp, sizeof(temp)); avkcpd->vheight  = atoi(temp);
        parse_params(avinfo, "frate"   , temp, sizeof(temp)); avkcpd->frate    = atoi(temp);
        if (strstr(avkcpd->aenctype, "alaw") == avkcpd->aenctype) { avkcpd->channels = 1; avkcpd->samprate = 8000; }
        if (strstr(avkcpd->aenctype, "aac" ) == avkcpd->aenctype) avkcpd->acodec = avcodec_find_decoder(AV_CODEC_ID_AAC     );
        if (strstr(avkcpd->aenctype, "alaw") == avkcpd->aenctype) avkcpd->acodec = avcodec_find_decoder(AV_CODEC_ID_PCM_ALAW);
        if (strstr(avkcpd->venctype, "h264") == avkcpd->venctype) avkcpd->vcodec = avcodec_find_decoder(AV_CODEC_ID_H264    );
        if (strstr(avkcpd->venctype, "h265") == avkcpd->venctype) avkcpd->vcodec = avcodec_find_decoder(AV_CODEC_ID_H265    );
        vrate.num = avkcpd->frate; vrate.den = 1;
        if (avkcpd->acodec) {
            *avkcpd->acodec_context = avcodec_alloc_context3(avkcpd->acodec);
            if (avkcpd->acodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                (*avkcpd->acodec_context)->flags |= AV_CODEC_FLAG_TRUNCATED;
            }
            (*avkcpd->acodec_context)->channels       = avkcpd->channels;
            (*avkcpd->acodec_context)->channel_layout = avkcpd->channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
            (*avkcpd->acodec_context)->sample_rate    = avkcpd->samprate;
            (*avkcpd->acodec_context)->sample_fmt     = avkcpd->acodec->id == AV_CODEC_ID_AAC ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
            if (avcodec_open2(*avkcpd->acodec_context, avkcpd->acodec, NULL) < 0) {
                avcodec_close(*avkcpd->acodec_context);
                *avkcpd->acodec_context = NULL; avkcpd->acodec = NULL;
            }
        }
        if (avkcpd->vcodec) {
            *avkcpd->vcodec_context = avcodec_alloc_context3(avkcpd->vcodec);
            if (avkcpd->vcodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
                (*avkcpd->vcodec_context)->flags |= AV_CODEC_FLAG_TRUNCATED;
            }
            (*avkcpd->vcodec_context)->width     = avkcpd->vwidth;
            (*avkcpd->vcodec_context)->height    = avkcpd->vheight;
            (*avkcpd->vcodec_context)->framerate = vrate;
            (*avkcpd->vcodec_context)->pix_fmt   = AV_PIX_FMT_YUV420P;
            if (avcodec_open2(*avkcpd->vcodec_context, avkcpd->vcodec, NULL) < 0) {
                avcodec_close(*avkcpd->vcodec_context);
                *avkcpd->vcodec_context = NULL; avkcpd->vcodec = NULL;
            }
        }
       *avkcpd->render = avkcpd->render_open(
            avkcpd->adevtype, avkcpd->samprate, (*avkcpd->acodec_context)->sample_fmt, (*avkcpd->acodec_context)->channel_layout,
            avkcpd->vdevtype, avkcpd->cmnvars->winmsg, vrate, (*avkcpd->vcodec_context)->pix_fmt, avkcpd->vwidth, avkcpd->vheight, avkcpd->cmnvars);
        avkcpd->cmnvars->init_params->avts_syncmode = AVSYNC_MODE_LIVE_SYNC0;
        avkcpd->cmnvars->init_params->video_vwidth  = avkcpd->cmnvars->init_params->video_owidth  = avkcpd->vwidth ;
        avkcpd->cmnvars->init_params->video_vheight = avkcpd->cmnvars->init_params->video_oheight = avkcpd->vheight;
       *avkcpd->playerstatus = 0;
        avkcpd->inited       = 1;
        avkcpd->player_send_message(avkcpd->cmnvars->winmsg, MSG_OPEN_DONE, (int64_t)avkcpd->player);
        break;
    case 'A': case 'V':
        packet = avkcpd->pktqueue_request_packet(avkcpd->pktqueue);
        if (packet == NULL) return -1;
        av_new_packet(packet, fsize);
        packet->pts = packet->dts = GetTickCount();
        ret = ringbuf_read(rbuf, rbsize, rbhead, packet->data, fsize);
        if (type == 'A') avkcpd->pktqueue_audio_enqueue(avkcpd->pktqueue, packet);
        else             avkcpd->pktqueue_video_enqueue(avkcpd->pktqueue, packet);
        break;
    }
    return ret;
}

void* avkcpdemuxer_init(char *url, void *player, void *pktqueue, AVCodecContext **acodec_context, AVCodecContext **vcodec_context, int *playerstatus,
                        AVRational *astream_timebase, AVRational *vstream_timebase, void **render, int adevtype, int vdevtype, CMNVARS *cmnvars,
                        void *pfnrenderopen, void *pfnpktqrequest, void *pfnpktqaenqueue, void *pfnpktqvenqueue, void *pfnplayermsg)
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
 
    avkcpd->avkcpc = avkcpc_init(ipaddr, port, avkcpc_callback, avkcpd);
    if (!avkcpd->avkcpc) { free(avkcpd); return NULL; }

    astream_timebase->num  = 1;
    astream_timebase->den  = 1000;
    vstream_timebase->num  = 1;
    vstream_timebase->den  = 1000;

    avkcpd->player         = player;
    avkcpd->pktqueue       = pktqueue;
    avkcpd->acodec_context = acodec_context;
    avkcpd->vcodec_context = vcodec_context;
    avkcpd->playerstatus   = playerstatus;
    avkcpd->render         = render;
    avkcpd->adevtype       = adevtype;
    avkcpd->vdevtype       = vdevtype;
    avkcpd->cmnvars        = cmnvars;
    avkcpd->render_open    = pfnrenderopen;

    avkcpd->pktqueue_request_packet = pfnpktqrequest;
    avkcpd->pktqueue_audio_enqueue  = pfnpktqaenqueue;
    avkcpd->pktqueue_video_enqueue  = pfnpktqvenqueue;
    avkcpd->player_send_message     = pfnplayermsg;
    return avkcpd;
}

void avkcpdemuxer_exit(void *ctxt)
{
    AVKCPDEMUXER *avkcpd = (AVKCPDEMUXER*)ctxt;
    if (avkcpd) {
        avkcpc_exit(avkcpd->avkcpc);
        free(avkcpd);
    }
}
