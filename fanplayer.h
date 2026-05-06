#ifndef __FANPLAYER_H__
#define __FANPLAYER_H__

#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// note: if ffmepg version changed, this constant may change
enum { // constant from ffmpeg 4.3.6
    SURFACE_FMT_BGR32  = 26, // AV_PIX_FMT_BGR32
    SURFACE_FMT_RGB32  = 28, // AV_PIX_FMT_RGB32
    SURFACE_FMT_RGB565 = 37, // AV_PIX_FMT_RGB565LE
    SURFACE_FMT_BGR565 = 41, // AV_PIX_FMT_BGB565LE
};

typedef struct {
    int   w, h, stride, format, cdepth;
    void *data;
} SURFACE;

enum {
    PLAYER_ADEV_SAMPRATE,
    PLAYER_ADEV_CHANNELS,
    PLAYER_ADEV_WRITE,
    PLAYER_VDEV_LOCK,
    PLAYER_VDEV_UNLOCK,
    PLAYER_AVSYNC_DELTA,

    PLAYER_AVIO_READ,
    PLAYER_AVIO_SEEK,

    PLAYER_OPEN_SUCCESS = 0x10000,
    PLAYER_OPEN_FAILED,
    PLAYER_PLAY_COMPLETED,
    PLAYER_STREAM_CONNECTED,
    PLAYER_STREAM_DISCONNECT,
};
typedef long (*PFN_PLAYER_CB)(void *cbctx, int msg, void *buf, int len);

enum {
    PLAYER_AVSYNC_MODE_AUTO,  // auto
    PLAYER_AVSYNC_MODE_FILE,  // file mode
    PLAYER_AVSYNC_MODE_LIVE_SYNC0, // live mode, without avts sync
    PLAYER_AVSYNC_MODE_LIVE_SYNC1, // live mode, with avts sync
};

// init params: if NULL return NULL.
// "i_use_avio        : ?\n"   using avio and indicate the avio buffer size. and you must implement PLAYER_AVIO_READ & PLAYER_AVIO_SEEK callback
// "i_video_vwidth    : ?\n"   setup video width
// "i_video_vheight   : ?\n"   setup video height
// "i_video_frame_rate: ?\n"   setup video frame rate, if open camera or other video devices, we can set the resolution and frame rate
// "i_video_stream_cur: ?\n"   setup video stream number, some video contain multi-stream audio and video, we can select which to playback
// "i_audio_stream_cur: ?\n"   setup audio stream number
// "i_init_timeout    : ?\n"   when playing live stream, to avoid blocked when init url, we can set this, in ms unit
// "i_open_autoplay   : ?\n"   if you want auto play after set s_url, set this to 1
// "i_auto_reconnect  : ?\n"   if playing live stream suggest to set this to 1, to enable auto reconnect
// "i_rtsp_transport  : ?\n"   0 - auto, 1 - udp, 2 - tcp
// "s_hwdec_name      : ???\n" if you want to use hw decoding, please set this hwdec name, example: dxva2
// "s_url             : ???\n" the url to play, note: url should be using utf-8 encoding
void* player_init(void *params, PFN_PLAYER_CB callback, void *cbctx);
void  player_exit(void *ctx);

long  player_set (void *ctx, char *key, void *val);
long  player_get (void *ctx, char *key, void *val);
void  player_dump(void *ctx, char *str, int len, int page);

#define PLAYER_KEY_MEDIA_DURATION ((char*)1)   // get only, get the media duration
#define PLAYER_KEY_MEDIA_POSITION ((char*)2)   // set/get, get current play position, set: 1 - step forward, -1 - step backward, other - seek position in ms unit
#define PLAYER_KEY_VIDEO_WIDTH    ((char*)3)   // get only, get video width
#define PLAYER_KEY_VIDEO_HEIGHT   ((char*)4)   // get only, get video height
#define PLAYER_KEY_URL            "s_url"      // set/get, url to play, note: url should be using utf-8 encoding
#define PLAYER_KEY_STATE          "i_state"    // set/get, set: 0 - stop, 1 - start, 2 - pause, 4 - restart, get: 0 - stopped, 1 - running, 2 - paused
#define PLAYER_KEY_RECFILE        "s_recfile"  // set/get, recording file name, set NULL to stop
#define PLAYER_KEY_RECORDING      "i_recording"// get only, get is recording or not
#define PLAYER_KEY_SPEED          "i_speed"    // set/get, playback speed [10, 300] %
#define PLAYER_KEY_STRETCH        "i_stretch"  // set/get, video display stretch or letterbox
#define PLAYER_KEY_SNAPSHOT       "s_snapshot" // set only, to take a snapshot, .jpg & .png file supported
#define PLAYER_KEY_AVSYNC_MODE    "i_avts_sync_mode" // set/get, audio & video sync mode, 0 - auto, 1 - file mode, 2 - live mode without avts sync, 3 - live mode with avts sync
#define PLAYER_KEY_AUDIO_NPKT     "i_audio_buf_npkt" // set/get, audio packet buffer size
#define PLAYER_KEY_VIDEO_NPKT     "i_video_buf_npkt" // set/get, video packet buffer size

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
