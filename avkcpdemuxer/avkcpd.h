#ifndef __AVKCPD_H__
#define __AVKCPD_H__

#include "ffplayer.h"
#include "ffrender.h"
#include "libavutil/rational.h"
#include "libavcodec/avcodec.h"

void* avkcpdemuxer_init(char *url, void *player, void *pktqueue, AVCodecContext **acodec_context, AVCodecContext **vcodec_context, int *playerstatus,
                        AVRational *astream_timebase, AVRational *vstream_timebase, void **render, int adevtype, int vdevtype, CMNVARS *cmnvars,
                        void *pfnrenderopen, void *pfnrendergetparam, void *pfnpktqrequest, void *pfnpktqaenqueue, void *pfnpktqvenqueue, void *pfnplayermsg, void *pfndxva2hwinit);
void  avkcpdemuxer_exit(void *ctxt);

#endif
