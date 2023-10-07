#ifndef __FANPLAYER_SNAPSHOT_H__
#define __FANPLAYER_SNAPSHOT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/frame.h"

int take_snapshot(char *file, int w, int h, AVFrame *video);

#ifdef __cplusplus
}
#endif

#endif


