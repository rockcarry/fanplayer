#include <stdlib.h>
#include <pthread.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "pktqueue.h"
#include "ffrender.h"
#include "fanplayer.h"

#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

typedef struct {
    // format
    AVFormatContext *avformat_context;

    // audio
    AVCodecContext  *acodec_context;
    int              astream_index;
    AVRational       astream_timebase;
    AVFrame          aframe;

    // video
    AVCodecContext  *vcodec_context;
    int              vstream_index;
    AVRational       vstream_timebase;
    AVFrame          vframe;

    void            *pktqueue; // pktqueue
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
    int  video_thread_count;       // wr 视频解码线程数
    int  video_codecid;            // wr 视频解码器的 codecid

    int  audio_channels;           // r  音频通道数
    int  audio_sample_rate;        // r  音频采样率
    int  audio_stream_total;       // r  音频流总数
    int  audio_stream_cur;         // wr 当前音频流

    int  init_timeout;             // w  播放器初始化超时，单位 ms，打开网络流媒体时设置用来防止卡死
    int  open_autoplay;            // w  播放器打开后自动播放
    int  auto_reconnect;           // w  播放流媒体时自动重连的超时时间，毫秒为单位
    int  rtsp_transport;           // w  rtsp 传输模式，0 - 自动，1 - udp，2 - tcp

    // save url
    char  url[PATH_MAX];
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

static int init_stream(PLAYER *player, enum AVMediaType type, int sel) {
    AVCodec *decoder = NULL;
    int idx = -1, cur = -1, i;

    if (sel == -1) return -1;
    for (i=0; i<(int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codec->codec_type == type) {
            idx = i; if (++cur == sel) break;
        }
    }
    if (idx == -1) return -1;

    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        // get new acodec_context & astream_timebase
        player->acodec_context   = player->avformat_context->streams[idx]->codec;
        player->astream_timebase = player->avformat_context->streams[idx]->time_base;

        // reopen codec
        decoder = avcodec_find_decoder(player->acodec_context->codec_id);
        if (decoder && avcodec_open2(player->acodec_context, decoder, NULL) == 0) {
            player->astream_index = idx;
        } else {
            av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for audio !\n");
        }
        break;

    case AVMEDIA_TYPE_VIDEO:
        // get new vcodec_context & vstream_timebase
        player->vcodec_context   = player->avformat_context->streams[idx]->codec;
        player->vstream_timebase = player->avformat_context->streams[idx]->time_base;

        //++ open codec
        if (!decoder) {
            //+ try to set video decoding thread count
            if (player->video_thread_count > 0) {
                player->vcodec_context->thread_count = player->video_thread_count;
            }
            //- try to set video decoding thread count
            decoder = avcodec_find_decoder(player->vcodec_context->codec_id);
            if (decoder && avcodec_open2(player->vcodec_context, decoder, NULL) == 0) {
                player->vstream_index = idx;
            } else {
                av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for video !\n");
            }
            // get the actual video decoding thread count
            player->video_thread_count = player->vcodec_context->thread_count;
        }
        //-- open codec
        break;

    default:
        return -1;
    }

    return 0;
}

static int get_stream_total(PLAYER *player, enum AVMediaType type) {
    int total, i;
    for (total = 0, i = 0; i < (int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codec->codec_type == type) total++;
    }
    return total;
}

static void player_send_message(PLAYER *player, int msg, void *buf, int len) {
    player->callback(player->cbctx, msg, buf, len);
}

static int player_prepare_or_free(PLAYER *player, int prepare)
{
    AVDictionary *opts = NULL;
    int           ret  = -1;
    char vsize[64], frate[64];

    if (player->acodec_context  ) { avcodec_close(player->acodec_context); player->acodec_context = NULL; }
    if (player->vcodec_context  ) { avcodec_close(player->vcodec_context); player->vcodec_context = NULL; }
    if (player->avformat_context) { avformat_close_input(&player->avformat_context); }
    av_frame_unref(&player->aframe); player->aframe.pts = -1;
    av_frame_unref(&player->vframe); player->vframe.pts = -1;
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
    AVRational vfrate = { .num = 0, .den = 1 };
    if (player->vstream_index != -1) {
        vfrate = player->avformat_context->streams[player->vstream_index]->avg_frame_rate;
        player->video_vwidth  = player->video_owidth  = player->vcodec_context->width;
        player->video_vheight = player->video_oheight = player->vcodec_context->height;
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
                player->status |= PS_COMPLETED;
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
        while (packet->size > 0 && !(player->status & (PS_A_PAUSE|PS_CLOSE))) {
            int gotaudio = 0, consumed = avcodec_decode_audio4(player->acodec_context, &player->aframe, &gotaudio, packet);
            if (consumed < 0) { av_log(NULL, AV_LOG_WARNING, "an error occurred during decoding audio.\n"); break; }
            if (gotaudio) {
                player->aframe.pts = av_rescale_q(av_frame_get_best_effort_timestamp(&player->aframe), player->astream_timebase, TIMEBASE_MS);
                if ( (player->status & PS_A_SEEK) && player->seek_dest - player->aframe.pts < player->seek_diff) player_update_status(player, PS_A_SEEK, 0);
                if (!(player->status & PS_A_SEEK)) render_audio(player->ffrender, &player->aframe, npkt);
                while ((player->status & PS_R_PAUSE) && !(player->status & (PS_CLOSE|PS_A_PAUSE|PS_A_SEEK))) av_usleep(20 * 1000);
            }
            packet->data += consumed;
            packet->size -= consumed;
        }

        // release packet
        pktqueue_release_packet(player->pktqueue, packet);
    }
    return NULL;
}

static void* video_decode_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    int       npkt;
    while (!(player->status & PS_CLOSE)) {
        // handle video decode pause
        if (player->status & PS_V_PAUSE) {
            player_update_status(player, 0, (PS_V_PAUSE << 16));
            av_usleep(20 * 1000); continue;
        }

        // dequeue video packet
        if (!(packet = pktqueue_video_dequeue(player->pktqueue, &npkt))) { check_play_completed(player, 1); render_video(player->ffrender, &player->vframe, npkt); continue; }

        // decode video packet
        while (packet->size > 0 && !(player->status & (PS_V_PAUSE|PS_CLOSE))) {
            int gotvideo = 0, consumed = avcodec_decode_video2(player->vcodec_context, &player->vframe, &gotvideo, packet);;
            if (consumed < 0) { av_log(NULL, AV_LOG_WARNING, "an error occurred during decoding video.\n"); break; }
            if (gotvideo) {
                player->vframe.pts = av_rescale_q(av_frame_get_best_effort_timestamp(&player->vframe), player->vstream_timebase, TIMEBASE_MS);
                if ((player->status & PS_V_SEEK) && player->seek_dest - player->vframe.pts <= player->seek_diff) player_update_status(player, PS_V_SEEK, 0);
                do {
                    if (!(player->status & PS_V_SEEK)) render_video(player->ffrender, &player->vframe, npkt);
                } while ((player->status & PS_R_PAUSE) && !(player->status & (PS_CLOSE|PS_V_PAUSE|PS_V_SEEK)));
            }
            packet->data += packet->size;
            packet->size -= packet->size;
        }

        // release packet
        pktqueue_release_packet(player->pktqueue, packet);
    }
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

        if (av_read_frame(player->avformat_context, packet) < 0) {
            pktqueue_release_packet(player->pktqueue, packet);
            if (player->auto_reconnect > 0 && av_gettime_relative() - player->read_timelast > player->auto_reconnect * 1000) {
                player_update_status(player, 0, PS_RECONNECT);
            }
        } else {
            player->read_timelast = av_gettime_relative();
            if      (packet->stream_index == player->astream_index) pktqueue_audio_enqueue (player->pktqueue, packet); // audio
            else if (packet->stream_index == player->vstream_index) pktqueue_video_enqueue (player->pktqueue, packet); // video
            else                                                    pktqueue_release_packet(player->pktqueue, packet); // other
        }
    }
    return NULL;
}

void* player_init(char *url, char *params, PFN_PLAYER_CB callback, void *cbctx)
{
    PLAYER *player = (PLAYER*)calloc(1, sizeof(PLAYER));
    if (!player) return NULL;

    player->callback = callback ? callback : player_callback;
    player->cbctx    = cbctx;

    char strval[256] = "";
    player->video_vwidth       = atoi(parse_params(params, "video_vwidth"      , strval, sizeof(strval)) ? strval : "0");
    player->video_vheight      = atoi(parse_params(params, "video_vheight"     , strval, sizeof(strval)) ? strval : "0");
    player->video_frame_rate   = atoi(parse_params(params, "video_frame_rate"  , strval, sizeof(strval)) ? strval : "0");
    player->video_stream_cur   = atoi(parse_params(params, "video_stream_cur"  , strval, sizeof(strval)) ? strval : "0");
    player->video_thread_count = atoi(parse_params(params, "video_thread_count", strval, sizeof(strval)) ? strval : "0");
    player->video_codecid      = atoi(parse_params(params, "video_codecid"     , strval, sizeof(strval)) ? strval : "0");
    player->audio_stream_cur   = atoi(parse_params(params, "audio_stream_cur"  , strval, sizeof(strval)) ? strval : "0");
    player->init_timeout       = atoi(parse_params(params, "init_timeout"      , strval, sizeof(strval)) ? strval : "0");
    player->open_autoplay      = atoi(parse_params(params, "open_autoplay"     , strval, sizeof(strval)) ? strval : "0");
    player->auto_reconnect     = atoi(parse_params(params, "auto_reconnect"    , strval, sizeof(strval)) ? strval : "0");
    player->rtsp_transport     = atoi(parse_params(params, "rtsp_transport"    , strval, sizeof(strval)) ? strval : "0");

    // av register all
    av_register_all();
    avformat_network_init();

    // setup log
    av_log_set_level   (AV_LOG_WARNING);
    av_log_set_callback(avlog_callback);

    strncpy(player->url, url, sizeof(player->url) - 1);
    player->status   = PS_A_PAUSE|PS_V_PAUSE|PS_R_PAUSE; // make sure player paused
    player->pktqueue = pktqueue_create(0);
    player->ffrender = render_init(NULL, player->callback, player->cbctx);
    render_set(player->ffrender, "avts_sync_mode", (void*)atoi(parse_params(params, "avts_sync_mode", strval, sizeof(strval)) ? strval : "0"));
    render_set(player->ffrender, "audio_buf_npkt", (void*)atoi(parse_params(params, "audio_buf_npkt", strval, sizeof(strval)) ? strval : "0"));
    render_set(player->ffrender, "video_buf_npkt", (void*)atoi(parse_params(params, "video_buf_npkt", strval, sizeof(strval)) ? strval : "0"));

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

    pktqueue_destroy(player->pktqueue);
    render_exit(player->ffrender);

    avformat_network_deinit(); // deinit network
    free(player);
}

void player_play(void *ctx, int play)
{
    if (!ctx) return;
    PLAYER *player = ctx;
    if (play) render_set(player->ffrender, "reset", (void*)-1);
    pthread_mutex_lock(&player->lock);
    if (play) player->status &= PS_CLOSE;
    else      player->status |= PS_R_PAUSE;
    pthread_mutex_unlock(&player->lock);
}

void player_seek(void *ctx, int64_t ms)
{
    if (!ctx) return;
    PLAYER *player = ctx;
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
    render_set(player->ffrender, key, val);
}

long player_get(void *ctx, char *key, void *val)
{
    if (!ctx) return 0;
    PLAYER *player = ctx;
    switch ((long)key) {
    case (int)PARAM_MEDIA_DURATION:
        return player->avformat_context ? (player->avformat_context->duration * 1000 / AV_TIME_BASE) : 1;
    case (int)PARAM_MEDIA_POSITION:
        return (uint32_t)(render_get(player->ffrender, key, NULL) - player->start_time);
    case (int)PARAM_VIDEO_WIDTH:
        return player->vcodec_context ? player->video_owidth  : 0;
    case (int)PARAM_VIDEO_HEIGHT:
        return player->vcodec_context ? player->video_oheight : 0;
    }
    return render_get(player->ffrender, key, val);
}
