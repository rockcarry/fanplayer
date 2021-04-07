#ifndef __FANPLAYER_DATARATE_H__
#define __FANPLAYER_DATARATE_H__

#include "libavformat/avformat.h"

#ifdef __cplusplus
extern "C" {
#endif

// º¯ÊýÉùÃ÷
void* datarate_create (void);
void  datarate_destroy(void *ctxt);
void  datarate_reset  (void *ctxt);
void  datarate_result (void *ctxt, int *arate, int *vrate, int *drate);
void  datarate_audio_packet(void *ctxt, AVPacket *pkt);
void  datarate_video_packet(void *ctxt, AVPacket *pkt);

#ifdef __cplusplus
}
#endif

#endif
