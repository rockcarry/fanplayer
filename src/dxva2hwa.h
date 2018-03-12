#ifndef __FFPLAYER_DXVA2HWA_H__
#define __FFPLAYER_DXVA2HWA_H__

#ifdef __cplusplus
extern "C" {
#endif

// 包含头文件
#include "libavcodec/avcodec.h"

// 函数声明
int  dxva2hwa_init(AVCodecContext *ctxt, void *d3ddev);
void dxva2hwa_free(AVCodecContext *ctxt);

#ifdef __cplusplus
}
#endif

#endif


