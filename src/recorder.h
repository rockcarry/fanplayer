#ifndef __RECORDER_H__
#define __RECORDER_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

// 函数声明
void* recorder_init  (char *filename, AVFormatContext *ifc);
void  recorder_free  (void *ctxt);
int   recorder_packet(void *ctxt, AVPacket *pkt);

#ifdef __cplusplus
}
#endif

#endif


