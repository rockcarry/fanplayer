#ifndef __FFOBJDET_H__
#define __FFOBJDET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/frame.h>

#ifndef DEFINE_BBOX_TYPE
#define DEFINE_BBOX_TYPE
typedef struct {
    float score, x1, y1, x2, y2;
    int   category;
} BBOX;
#endif

void* ffobjdet_init  (void);
void  ffobjdet_data  (void *ctx, struct AVFrame *video);
BBOX* ffobjdet_bbox  (void *ctx);
void  ffobjdet_enable(void *ctx, int precision);
void  ffobjdet_free  (void *ctx);

#ifdef __cplusplus
}
#endif

#endif
