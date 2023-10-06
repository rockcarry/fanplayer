#ifndef __FANPLAYER_RECORDER_H__
#define __FANPLAYER_RECORDER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

// º¯ÊýÉùÃ÷
void* recorder_init  (char *filename, AVFormatContext *ifc);
void  recorder_free  (void *ctxt);
int   recorder_packet(void *ctxt, AVPacket *pkt);

#ifdef __cplusplus
}
#endif

#endif


