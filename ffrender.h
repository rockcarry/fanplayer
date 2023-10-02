#ifndef __FANPLAYER_FFRENDER_H__
#define __FANPLAYER_FFRENDER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "fanplayer.h"
#include "libavcodec/avcodec.h"

void*render_init (char *type, PFN_PLAYER_CB callback, void *cbctx);
void render_exit (void *ctx);
void render_audio(void *ctx, struct AVFrame *audio);
void render_video(void *ctx, struct AVFrame *video);
void render_set  (void *ctx, char *key, void *val);
long render_get  (void *ctx, char *key, void *val);

#ifdef __cplusplus
}
#endif

#endif
