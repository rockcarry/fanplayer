#ifndef __FFRDPD_H__
#define __FFRDPD_H__

#include "ffplayer.h"
#include "ffrender.h"
#include "libavutil/rational.h"
#include "libavcodec/avcodec.h"

void* ffrdpdemuxer_init(char *url, void *player, void *pktqueue, AVCodecContext **acodec_context, AVCodecContext **vcodec_context, int *playerstatus,
                        AVRational *astream_timebase, AVRational *vstream_timebase, void **render, int adevtype, int vdevtype, CMNVARS *cmnvars,
                        void *pfnrenderopen, void *pfnrendergetparam, void *pfnpktqrequest, void *pfnpktqaenqueue, void *pfnpktqvenqueue, void *pfnplayermsg, void *pfndxva2hwinit);
void  ffrdpdemuxer_exit(void *ctxt);
void  ffrdpdemuxer_senddata(void *ctxt, char *data, int size);

#endif
