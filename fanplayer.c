#include <stdlib.h>
#include <pthread.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "pktqueue.h"
#include "recorder.h"
#include "ffrender.h"
#include "fanplayer.h"

#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

#define ALIGN(a, b) (((a) + (b) - 1) & ~((b) - 1))

typedef struct {
    // format
    AVFormatContext *avformat_context;

    // audio
    AVCodecContext  *acodec_context;
    int              astream_index;
    AVRational       astream_timebase;

    // video
    AVCodecContext  *vcodec_context;
    int              vstream_index;
    AVRational       vstream_timebase;

    void            *pktqueue; // pktqueue
    void            *recorder; // recorder
    void            *ffrender; // ffrender

    PFN_PLAYER_CB    callback;
    void            *cbctx;

    // thread
    #define PS_A_PAUSE    (1 << 0)  // audio decoding pause
    #define PS_V_PAUSE    (1 << 1)  // video decoding pause
    #define PS_R_PAUSE    (1 << 2)  // rendering pause
    #define PS_F_SEEK     (1 << 3)  // seek flag
    #define PS_A_SEEK     (1 << 4)  // seek audio
    #define PS_V_SEEK     (1 << 5)  // seek video
    #define PS_RECONNECT  (1 << 6)  // reconnect
    #define PS_CLOSE      (1 << 7)  // close flag
    #define PS_COMPLETED  (1 << 8)  // play completed
    #define PS_RECORD     (1 << 9)  // enable recorder
    int              status;
    int64_t          seek_pos ;
    int64_t          seek_dest;
    int              seek_diff;

    pthread_mutex_t  lock;
    pthread_t        avdemux_thread;
    pthread_t        adecode_thread;
    pthread_t        vdecode_thread;

    // player init timeout, and init params
    int64_t          read_timelast;
    int64_t          read_timeout;
    int64_t          start_time;

    int  a_completed_cnt;
    int  v_completed_cnt;

    int  video_vwidth;             // wr video actual width
    int  video_vheight;            // wr video actual height
    int  video_owidth;             // r  video output width  (after rotate)
    int  video_oheight;            // r  video output height (after rotate)
    int  video_frame_rate;         // wr 视频帧率
    int  video_stream_total;       // r  视频流总数
    int  video_stream_cur;         // wr 当前视频流
    int  video_codecid;            // wr 视频解码器的 codecid

    int  audio_channels;           // r  音频通道数
    int  audio_sample_rate;        // r  音频采样率
    int  audio_stream_total;       // r  音频流总数
    int  audio_stream_cur;         // wr 当前音频流

    int  init_timeout;             // w  播放器初始化超时，单位 ms，打开网络流媒体时设置用来防止卡死
    int  open_autoplay;            // w  播放器打开后自动播放
    int  auto_reconnect;           // w  播放流媒体时自动重连的超时时间，毫秒为单位
    int  rtsp_transport;           // w  rtsp 传输模式，0 - 自动，1 - udp，2 - tcp

    int      use_avio;             // w  use avio
    uint8_t *avio_buf;             // avio buffer

    char         hwdec_str[32];
    int          hwdec_type;
    int          hwdec_fmt;
    AVBufferRef *hwdec_devctx;
    void        *hwdec_get_format_sw;

    char  rec[PATH_MAX]; // record file
    char  url[PATH_MAX]; // open url
} PLAYER;

static const AVRational TIMEBASE_MS = { 1, 1000 };

static char* parse_params(char *str, char *key, char *val, int len)
{
    if (!str) return NULL;
    char *p = strstr(str, key);
    int   i;

    if (!p) return NULL;
    p += strlen(key);
    if (*p == '\0') return NULL;

    while (1) {
        if (*p != ' ' && *p != '=' && *p != ':') break;
        p++;
    }

    for (i = 0; i < len; i++) {
        if (*p == ',' || *p == ';' || *p == '\r' || *p == '\n' || *p == '\0') break;
        if (*p == '\\') p++;
        val[i] = *p++;
    }
    val[i < len ? i : len - 1] = '\0';
    return val;
}

static void avlog_callback(void *ptr, int level, const char *fmt, va_list vl) {
    if (level <= av_log_get_level()) vprintf(fmt, vl);
}

static int interrupt_callback(void *param)
{
    PLAYER *player = (PLAYER*)param;
    if (player->read_timeout == -1) return 0;
    return av_gettime_relative() - player->read_timelast > player->read_timeout ? AVERROR_EOF : 0;
}

static int avio_read_callback(void *opaque, uint8_t *buf, int size)
{
    PLAYER *player = (PLAYER*)opaque;
    return player->callback(player->cbctx, PLAYER_AVIO_READ, buf, size);
}

static int64_t avio_seek_callback(void *opaque, int64_t offset, int whence)
{
    PLAYER *player = (PLAYER*)opaque;
    player->callback(player->cbctx, PLAYER_AVIO_SEEK, &offset, whence);
    return offset;
}

static int player_callback(void *cbctx, int msg, void *buf, int len)
{
    switch (msg) {
    case PLAYER_ADEV_SAMPRATE: return 48000;
    case PLAYER_ADEV_CHANNELS: return 2;
    case PLAYER_VDEV_LOCK: {
            SURFACE *surface = buf;
            surface->w       = 320;
            surface->h       = 240;
            surface->stride  = 320 * 4;
            surface->format  = SURFACE_FMT_RGB32;
        }
        break;
    }
    return 0;
}

static void player_update_status(PLAYER *player, int clear, int set)
{
    pthread_mutex_lock(&player->lock);
    player->status = (player->status & ~clear) | set;
    pthread_mutex_unlock(&player->lock);
}

static enum AVPixelFormat find_fmt_by_hw_type(const enum AVHWDeviceType type)
{
    switch (type) {
    case AV_HWDEVICE_TYPE_VAAPI       : return AV_PIX_FMT_VAAPI;
    case AV_HWDEVICE_TYPE_DXVA2       : return AV_PIX_FMT_DXVA2_VLD;
    case AV_HWDEVICE_TYPE_D3D11VA     : return AV_PIX_FMT_D3D11;
    case AV_HWDEVICE_TYPE_VDPAU       : return AV_PIX_FMT_VDPAU;
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return AV_PIX_FMT_VIDEOTOOLBOX;
    default:                            return AV_PIX_FMT_NONE;
    }
}

static enum AVPixelFormat get_format_hw(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    PLAYER *player = ctx->opaque;
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == player->hwdec_fmt) return *p;
    }
    return AV_PIX_FMT_NONE;
}

static int init_stream(PLAYER *player, enum AVMediaType type, int sel) {
    int idx = -1, cur = -1, ret, i;
    for (i = 0; i < player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codecpar->codec_type == type) {
            idx = i; if (++cur == sel) break;
        }
    }
    if (idx == -1) return -1;
    AVCodec *decoder = avcodec_find_decoder(player->avformat_context->streams[i]->codecpar->codec_id);

    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        player->acodec_context   = avcodec_alloc_context3(decoder);
        player->astream_timebase = player->avformat_context->streams[idx]->time_base;
        avcodec_parameters_to_context(player->acodec_context, player->avformat_context->streams[idx]->codecpar);
        if (decoder && avcodec_open2(player->acodec_context, decoder, NULL) == 0) {
            player->astream_index = idx;
        } else {
            av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for audio !\n");
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        player->vcodec_context   = avcodec_alloc_context3(decoder);
        player->vstream_timebase = player->avformat_context->streams[idx]->time_base;
        avcodec_parameters_to_context(player->vcodec_context, player->avformat_context->streams[idx]->codecpar);

        if (player->hwdec_fmt != -1) { // for hwdec
            player->hwdec_get_format_sw        = player->vcodec_context->get_format;
            player->vcodec_context->opaque     = player;
            player->vcodec_context->get_format = get_format_hw;
            ret = av_hwdevice_ctx_create(&player->hwdec_devctx, player->hwdec_type, NULL, NULL, 0);
            if (ret >= 0) player->vcodec_context->hw_device_ctx = av_buffer_ref(player->hwdec_devctx);
        }

        if (decoder && avcodec_open2(player->vcodec_context, decoder, NULL) == 0) {
            player->vstream_index = idx;
        } else {
            av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for video !\n");
        }
        break;
    default:
        return -1;
    }
    return 0;
}

static int get_stream_total(PLAYER *player, enum AVMediaType type) {
    int  total, i;
    for (total = 0, i = 0; i < (int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codecpar->codec_type == type) total++;
    }
    return total;
}

static void player_send_message(PLAYER *player, int msg, void *buf, int len) {
    player->callback(player->cbctx, msg, buf, len);
}

static void player_play(void *ctx, int play)
{
    if (!ctx) return;
    PLAYER *player = ctx;
    if (play) render_set(player->ffrender, "reset", (void*)-1);
    pthread_mutex_lock(&player->lock);
    if (play) player->status &= PS_CLOSE;
    else      player->status |= PS_R_PAUSE;
    pthread_mutex_unlock(&player->lock);
}

static int player_prepare_or_free(PLAYER *player, int prepare)
{
    AVDictionary *opts = NULL;
    int           ret  = -1;
    char vsize[64], frate[64];

    if (player->acodec_context  ) avcodec_free_context(&player->acodec_context  );
    if (player->vcodec_context  ) avcodec_free_context(&player->vcodec_context  );
    if (player->avformat_context) avformat_close_input(&player->avformat_context);
    if (player->hwdec_devctx    ) av_buffer_unref     (&player->hwdec_devctx    );
    if (!prepare) return 0;

    // open input file
    if (  strstr(player->url, "rtsp://" ) == player->url
       || strstr(player->url, "rtmp://" ) == player->url
       || strstr(player->url, "dshow://") == player->url) {
        if (player->rtsp_transport) {
            av_dict_set(&opts, "rtsp_transport", player->rtsp_transport == 1 ? "udp" : "tcp", 0);
        }
        av_dict_set(&opts, "buffer_size"    , "1048576", 0);
        av_dict_set(&opts, "fpsprobesize"   , "2"      , 0);
        av_dict_set(&opts, "analyzeduration", "5000000", 0);
    }
    if (player->video_vwidth != 0 && player->video_vheight != 0) {
        sprintf(vsize, "%dx%d", player->video_vwidth, player->video_vheight);
        av_dict_set(&opts, "video_size", vsize, 0);
    }
    if (player->video_frame_rate != 0) {
        sprintf(frate, "%d", player->video_frame_rate);
        av_dict_set(&opts, "framerate" , frate, 0);
    }

    while (1) {
        // allocate avformat_context
        if (!(player->avformat_context = avformat_alloc_context())) goto done;

        // setup interrupt_callback
        player->avformat_context->interrupt_callback.callback = interrupt_callback;
        player->avformat_context->interrupt_callback.opaque = player;
        player->avformat_context->video_codec_id = player->video_codecid;

        // set init_timetick & init_timeout
        player->read_timelast = av_gettime_relative();
        player->read_timeout  = player->init_timeout ? player->init_timeout * 1000 : -1;

        if (player->use_avio) {
            player->avformat_context->pb = avio_alloc_context(player->avio_buf, player->use_avio, 0, NULL, avio_read_callback, NULL, avio_seek_callback);
            player->avformat_context->pb->opaque = player;
        }
        if (avformat_open_input(&player->avformat_context, player->url, NULL, &opts) != 0) { // if failed, no need call avformat_free_context
            av_usleep(100 * 1000);
            if (player->auto_reconnect > 0 && !(player->status & PS_CLOSE)) {
                av_log(NULL, AV_LOG_INFO, "retry to open url: %s ...\n", player->url);
            } else {
                av_log(NULL, AV_LOG_ERROR, "failed to open url: %s !\n", player->url); goto done;
            }
        } else { av_log(NULL, AV_LOG_INFO, "successed to open url: %s !\n", player->url); break; }
    }

    // find stream info
    if (avformat_find_stream_info(player->avformat_context, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to find stream info !\n"); goto done;
    }

    // set current audio & video stream
    player->astream_index = -1; init_stream(player, AVMEDIA_TYPE_AUDIO, player->audio_stream_cur);
    player->vstream_index = -1; init_stream(player, AVMEDIA_TYPE_VIDEO, player->video_stream_cur);

    // for audio
    if (player->astream_index != -1) {
        //++ fix audio channel layout issue
        if (player->acodec_context->channel_layout == 0) {
            player->acodec_context->channel_layout = av_get_default_channel_layout(player->acodec_context->channels);
        }
        //-- fix audio channel layout issue
    }

    // for video
    AVRational vfrate = { .num = 1, .den = 1 };
    if (player->vstream_index != -1) {
        vfrate = player->avformat_context->streams[player->vstream_index]->avg_frame_rate;
        player->video_vwidth  = player->video_owidth  = player->vcodec_context->width;
        player->video_vheight = player->video_oheight = player->vcodec_context->height;
        if (vfrate.den == 0) vfrate.den = 1;
    }

    // calculate start_time
    player->start_time = player->avformat_context->start_time * 1000 / AV_TIME_BASE;

    // for player init params
    player->video_frame_rate   = vfrate.num / vfrate.den;
    player->video_stream_total = get_stream_total(player, AVMEDIA_TYPE_VIDEO);
    player->audio_channels     = player->acodec_context ? av_get_channel_layout_nb_channels(player->acodec_context->channel_layout) : 0;
    player->audio_sample_rate  = player->acodec_context ? player->acodec_context->sample_rate : 0;
    player->audio_stream_total = get_stream_total(player, AVMEDIA_TYPE_AUDIO);
    player->video_codecid      = player->avformat_context->video_codec_id;
    ret = 0; // prepare ok

done:
    // send player init message
    player_send_message(player, ret == 0 ? PLAYER_OPEN_SUCCESS : PLAYER_OPEN_FAILED, NULL, 0);
    if (ret == 0 && player->open_autoplay) player_play(player, 1);
    return ret;
}

static int handle_fseek_or_reconnect(PLAYER *player)
{
    int PAUSE_REQ = 0, PAUSE_ACK = 0, ret = 0;

    if (player->avformat_context && (player->status & (PS_F_SEEK|PS_RECONNECT)) == 0) return 0;
    if (player->astream_index != -1) { PAUSE_REQ |= PS_A_PAUSE; PAUSE_ACK |= PS_A_PAUSE << 16; }
    if (player->vstream_index != -1) { PAUSE_REQ |= PS_V_PAUSE; PAUSE_ACK |= PS_V_PAUSE << 16; }

    // set audio & video decoding pause flags
    player_update_status(player, PAUSE_ACK, PAUSE_REQ);

    // wait for pause done
    while ((player->status & PAUSE_ACK) != PAUSE_ACK) {
        if (player->status & PS_CLOSE) return 0;
        av_usleep(20 * 1000);
    }

    if (!player->avformat_context || (player->status & PS_RECONNECT)) { // do reconnect
        if (ret == 0) player_send_message(player, PLAYER_STREAM_DISCONNECT, NULL, 0);
        ret = player_prepare_or_free(player, 1);
        if (ret == 0) player_send_message(player, PLAYER_STREAM_CONNECTED , NULL, 0);
    } else { // do seek
        av_seek_frame(player->avformat_context, -1, player->seek_pos, AVSEEK_FLAG_BACKWARD);
        if (player->astream_index != -1) avcodec_flush_buffers(player->acodec_context);
        if (player->vstream_index != -1) avcodec_flush_buffers(player->vcodec_context);
    }

    pktqueue_reset(player->pktqueue);                 // reset pktqueue
    render_set(player->ffrender, "reset", (void*)-1); // reset render

    // make audio & video decoding thread resume
    if (ret == 0) player_update_status(player, PS_F_SEEK|PS_RECONNECT|PAUSE_REQ|PAUSE_ACK, (player->status & PS_F_SEEK) ? (PS_A_SEEK|PS_V_SEEK) : 0);
    return ret;
}

static void check_play_completed(PLAYER *player, int av)
{
    #define MAX_COMPLETED_COUNT 20
    if (!(player->status & PS_COMPLETED) && player->avformat_context->duration != (1ull << 63)) {
        if (av) {
            if (player->v_completed_cnt < MAX_COMPLETED_COUNT) player->v_completed_cnt++;
            if (player->a_completed_cnt == MAX_COMPLETED_COUNT && player->v_completed_cnt == MAX_COMPLETED_COUNT) {
                player_update_status(player, PS_RECORD, PS_COMPLETED);
                recorder_free(player->recorder); player->recorder = NULL;
                player_send_message(player, PLAYER_PLAY_COMPLETED, NULL, 0);
            }
        } else {
            if (player->a_completed_cnt < MAX_COMPLETED_COUNT) player->a_completed_cnt++;
        }
    }
}

static void* audio_decode_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    AVFrame   frame  = {};
    int       npkt;
    while (!(player->status & PS_CLOSE)) {
        // handle audio decode pause
        if (player->status & PS_A_PAUSE) {
            player_update_status(player, 0, (PS_A_PAUSE << 16));
            av_usleep(20 * 1000); continue;
        }

        // dequeue audio packet
        if (!(packet = pktqueue_audio_dequeue(player->pktqueue, &npkt))) { check_play_completed(player, 0); av_usleep(20 * 1000); continue; }

        // decode audio packet
        int ret = avcodec_send_packet(player->acodec_context, packet);
        while (ret >= 0 && !(player->status & (PS_A_PAUSE|PS_CLOSE))) {
            if ((ret = avcodec_receive_frame(player->acodec_context, &frame)) == 0) {
                frame.pts = av_rescale_q(frame.best_effort_timestamp, player->astream_timebase, TIMEBASE_MS);
                if ( (player->status & PS_A_SEEK) && player->seek_dest - frame.pts < player->seek_diff) player_update_status(player, PS_A_SEEK, 0);
                if (!(player->status & PS_A_SEEK)) render_audio(player->ffrender, &frame, npkt);
                while ((player->status & PS_R_PAUSE) && !(player->status & (PS_CLOSE|PS_A_PAUSE|PS_A_SEEK))) av_usleep(20 * 1000);
            }
        }

        // release packet
        pktqueue_release_packet(player->pktqueue, packet);
    }
    av_frame_unref(&frame);
    return NULL;
}

static void* video_decode_thread_proc(void *param)
{
    PLAYER   *player  = (PLAYER*)param;
    AVPacket *packet  = NULL;
    AVFrame   frame[2]= {};
    int       npkt, idx = 0;
    while (!(player->status & PS_CLOSE)) {
        // handle video decode pause
        if (player->status & PS_V_PAUSE) {
            player_update_status(player, 0, (PS_V_PAUSE << 16));
            av_usleep(20 * 1000); continue;
        }

        // dequeue video packet
        if (!(packet = pktqueue_video_dequeue(player->pktqueue, &npkt))) { check_play_completed(player, 1); render_video(player->ffrender, &frame[idx], npkt); continue; }

        // decode video packet
decode: int ret = avcodec_send_packet(player->vcodec_context, packet);
        if (ret == -1 && player->vcodec_context->hw_device_ctx) { // if hardware decode failed fallback to software decode
            player->vcodec_context->get_format    = player->hwdec_get_format_sw;
            player->vcodec_context->hw_device_ctx = NULL;
            goto decode;
        }
        while (ret >= 0 && !(player->status & (PS_V_PAUSE|PS_CLOSE))) {
            if ((ret = avcodec_receive_frame(player->vcodec_context, &frame[idx])) == 0) {
                frame[idx].pts = av_rescale_q(frame[idx].best_effort_timestamp, player->vstream_timebase, TIMEBASE_MS);
                if ((player->status & PS_V_SEEK) && player->seek_dest - frame[idx].pts <= player->seek_diff) player_update_status(player, PS_V_SEEK, 0);
                else if (frame[idx].format == player->hwdec_fmt) {
                    if (av_hwframe_transfer_data(&(frame[(idx + 1) % 2]), &(frame[idx]), 0) >= 0) {
                        frame[(idx + 1) % 2].pts = frame[idx].pts;
                        idx = (idx + 1) % 2;
                    }
                }
                do {
                    if (!(player->status & PS_V_SEEK)) render_video(player->ffrender, &frame[idx], npkt);
                } while ((player->status & PS_R_PAUSE) && !(player->status & (PS_CLOSE|PS_V_PAUSE|PS_V_SEEK)));
            }
            idx = (idx + 1) % 2;
        }

        // release packet
        pktqueue_release_packet(player->pktqueue, packet);
    }
    av_frame_unref(&(frame[0]));
    av_frame_unref(&(frame[1]));
    return NULL;
}

static void* av_demux_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    while (!(player->status & PS_CLOSE)) {
        if (handle_fseek_or_reconnect(player) != 0) {
            if (!player->auto_reconnect) break;
            continue;
        }
        if ((packet = pktqueue_request_packet(player->pktqueue)) == NULL) continue;

        if (player->recorder == NULL && (player->status & PS_RECORD)) {
            player->recorder = recorder_init(player->rec, player->avformat_context);
        } else if (player->recorder != NULL && !(player->status & PS_RECORD)) {
            recorder_free(player->recorder); player->recorder = NULL;
        }

        if (av_read_frame(player->avformat_context, packet) < 0) {
            pktqueue_release_packet(player->pktqueue, packet);
            if (player->auto_reconnect > 0 && av_gettime_relative() - player->read_timelast > player->auto_reconnect * 1000) {
                player_update_status(player, 0, PS_RECONNECT);
            }
            av_usleep(20 * 1000);
        } else {
            player->read_timelast = av_gettime_relative();
            recorder_packet(player->recorder, packet);
            if      (packet->stream_index == player->astream_index) pktqueue_audio_enqueue (player->pktqueue, packet); // audio
            else if (packet->stream_index == player->vstream_index) pktqueue_video_enqueue (player->pktqueue, packet); // video
            else                                                    pktqueue_release_packet(player->pktqueue, packet); // other
        }
    }
    return NULL;
}

void* player_init(char *url, char *params, PFN_PLAYER_CB callback, void *cbctx)
{
    char strval[256] = "";
    int use_avio = atoi(parse_params(params, "use_avio", strval, sizeof(strval)) ? strval : "0");
    use_avio = ALIGN(use_avio, 256);

    PLAYER *player = (PLAYER*)malloc(sizeof(PLAYER) + use_avio);
    if (!player) return NULL;
    memset(player, 0, sizeof(PLAYER));

    player->callback = callback ? callback : player_callback;
    player->cbctx    = cbctx;
    player->hwdec_fmt= AV_PIX_FMT_NONE;

    player->video_vwidth     = atoi(parse_params(params, "video_vwidth"      , strval, sizeof(strval)) ? strval : "0");
    player->video_vheight    = atoi(parse_params(params, "video_vheight"     , strval, sizeof(strval)) ? strval : "0");
    player->video_frame_rate = atoi(parse_params(params, "video_frame_rate"  , strval, sizeof(strval)) ? strval : "0");
    player->video_stream_cur = atoi(parse_params(params, "video_stream_cur"  , strval, sizeof(strval)) ? strval : "0");
    player->video_codecid    = atoi(parse_params(params, "video_codecid"     , strval, sizeof(strval)) ? strval : "0");
    player->audio_stream_cur = atoi(parse_params(params, "audio_stream_cur"  , strval, sizeof(strval)) ? strval : "0");
    player->init_timeout     = atoi(parse_params(params, "init_timeout"      , strval, sizeof(strval)) ? strval : "0");
    player->open_autoplay    = atoi(parse_params(params, "open_autoplay"     , strval, sizeof(strval)) ? strval : "0");
    player->auto_reconnect   = atoi(parse_params(params, "auto_reconnect"    , strval, sizeof(strval)) ? strval : "0");
    player->rtsp_transport   = atoi(parse_params(params, "rtsp_transport"    , strval, sizeof(strval)) ? strval : "0");
    player->use_avio         = use_avio;
    player->avio_buf         = (uint8_t*)(player + 1);
    parse_params(params, "hwdec_str", player->hwdec_str, sizeof(player->hwdec_str));

    // init network
    avformat_network_init();

    // for hwdec
    if (strlen(player->hwdec_str) > 0) {
        player->hwdec_type = av_hwdevice_find_type_by_name(player->hwdec_str);
        player->hwdec_fmt  = find_fmt_by_hw_type(player->hwdec_type);
    }

    // setup log
    av_log_set_level   (AV_LOG_WARNING);
    av_log_set_callback(avlog_callback);

    if (url) strncpy(player->url, url, sizeof(player->url) - 1);
    player->status   = PS_A_PAUSE|PS_V_PAUSE|PS_R_PAUSE; // make sure player paused
    player->pktqueue = pktqueue_create(0);
    player->ffrender = render_init(NULL, player->callback, player->cbctx);
    render_set(player->ffrender, "avts_sync_mode", (void*)(intptr_t)atoi(parse_params(params, "avts_sync_mode", strval, sizeof(strval)) ? strval : "0"));
    render_set(player->ffrender, "audio_buf_npkt", (void*)(intptr_t)atoi(parse_params(params, "audio_buf_npkt", strval, sizeof(strval)) ? strval : "0"));
    render_set(player->ffrender, "video_buf_npkt", (void*)(intptr_t)atoi(parse_params(params, "video_buf_npkt", strval, sizeof(strval)) ? strval : "0"));

    pthread_mutex_init(&player->lock, NULL); // init lock
    pthread_create(&player->avdemux_thread, NULL, av_demux_thread_proc    , player);
    pthread_create(&player->adecode_thread, NULL, audio_decode_thread_proc, player);
    pthread_create(&player->vdecode_thread, NULL, video_decode_thread_proc, player);
    return player;
}

void player_exit(void *ctx)
{
    if (!ctx) return;
    PLAYER *player = ctx;

    player->read_timeout = 0; // set read_timeout to 0
    player_update_status(player, 0, PS_CLOSE);
    if (player->adecode_thread) pthread_join(player->adecode_thread, NULL); // wait audio decoding thread exit
    if (player->vdecode_thread) pthread_join(player->vdecode_thread, NULL); // wait video decoding thread exit
    if (player->avdemux_thread) pthread_join(player->avdemux_thread, NULL); // wait avdemux thread exit
    pthread_mutex_destroy(&player->lock);

    recorder_free   (player->recorder);
    pktqueue_destroy(player->pktqueue);
    render_exit(player->ffrender);

    avformat_network_deinit(); // deinit network
    free(player);
}

void player_seek(void *ctx, int64_t ms, int type)
{
    if (!ctx) return;
    PLAYER *player = ctx;
    if (player->status & (PS_F_SEEK|PS_A_SEEK|PS_V_SEEK)) { av_log(NULL, AV_LOG_WARNING, "seek busy !\n"); return; }
    switch (type) {
    case SEEK_STEP_FORWARD:
        break;
    case SEEK_STEP_BACKWARD:
        break;
    }
    player->seek_dest =  player->start_time + ms;
    player->seek_pos  = (player->start_time + ms) * AV_TIME_BASE / 1000;
    player->seek_diff = 50;
    player->a_completed_cnt = 0;
    player->v_completed_cnt = 0;
    player_update_status(player, PS_COMPLETED, PS_F_SEEK);
}

void player_set(void *ctx, char *key, void *val)
{
    if (!ctx) return;
    PLAYER *player = ctx;
    if (strcmp(key, "play") == 0) {
        player_play(player, (intptr_t)val);
    } else if (strcmp(key, "record") == 0) {
        if ((intptr_t)val) {
            strncpy(player->rec, val, sizeof(player->rec) - 1);
            player->status |=  PS_RECORD;
        } else {
            player->status &= ~PS_RECORD;
        }
    } else {
        render_set(player->ffrender, key, val);
    }
}

void* player_get(void *ctx, char *key, void *val)
{
    if (!ctx) return 0;
    PLAYER  *player = ctx;
    uint32_t position;
    switch ((intptr_t)key) {
    case (intptr_t)PARAM_MEDIA_DURATION:
        return (void*)(intptr_t)(player->avformat_context ? (player->avformat_context->duration * 1000 / AV_TIME_BASE) : 1);
    case (intptr_t)PARAM_MEDIA_POSITION:
        position = (intptr_t)render_get(player->ffrender, key, NULL);
        return (void*)(intptr_t)(position > player->start_time ? position - player->start_time : position);
    case (intptr_t)PARAM_VIDEO_WIDTH:
        return (void*)(intptr_t)(player->vcodec_context ? player->video_owidth  : 0);
    case (intptr_t)PARAM_VIDEO_HEIGHT:
        return (void*)(intptr_t)(player->vcodec_context ? player->video_oheight : 0);
    }
    if (strcmp(key, "play"  ) == 0) return (void*)(intptr_t)!(player->status & PS_R_PAUSE);
    if (strcmp(key, "record") == 0) return (void*)(intptr_t)((player->status & PS_RECORD) ? player->rec : NULL);
    return render_get(player->ffrender, key, val);
}
