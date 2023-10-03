#ifndef __FANPLAYER_H__
#define __FANPLAYER_H__

#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

enum {
    SURFACE_FMT_BGR32  = 28,
    SURFACE_FMT_RGB32  = 30,
    SURFACE_FMT_RGB565 = 44,
    SURFACE_FMT_BGR565 = 48,
};

typedef struct {
    int w, h, stride, format, cdepth;
    void *data;
} SURFACE;

enum {
    PLAYER_ADEV_SAMPRATE,
    PLAYER_ADEV_CHANNELS,
    PLAYER_ADEV_BUFFER,
    PLAYER_VDEV_LOCK,
    PLAYER_VDEV_UNLOCK,

    PLAYER_OPEN_SUCCESS = 0x10000,
    PLAYER_OPEN_FAILED,
    PLAYER_PLAY_COMPLETED,
    PLAYER_STREAM_CONNECTED,
    PLAYER_STREAM_DISCONNECT,
};

typedef int (*PFN_PLAYER_CB)(void *cbctx, int msg, void *buf, int len);

void* player_init(char *url, char *params, PFN_PLAYER_CB callback, void *cbctx);
void  player_exit(void *ctx);
void  player_play(void *ctx, int play);
void  player_seek(void *ctx, int64_t ms);
void  player_set (void *ctx, char *key, void *val);
long  player_get (void *ctx, char *key, void *val);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
