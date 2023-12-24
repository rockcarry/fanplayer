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
    int w, h, stride, format, cdepth;
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
typedef int (*PFN_PLAYER_CB)(void *cbctx, int msg, void *buf, int len);

void* player_init(char *url, char *params, PFN_PLAYER_CB callback, void *cbctx);
void  player_exit(void *ctx);

enum {
    SEEK_STEP_FORWARD = 1,
    SEEK_STEP_BACKWARD,
};
void player_seek(void *ctx, int64_t ms, int type);

#define PARAM_MEDIA_DURATION ((char*)0)
#define PARAM_MEDIA_POSITION ((char*)1)
#define PARAM_VIDEO_WIDTH    ((char*)2)
#define PARAM_VIDEO_HEIGHT   ((char*)3)
void  player_set(void *ctx, char *key, void *val);
void* player_get(void *ctx, char *key, void *val);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
