#ifndef __FFPLAYER_VEFFECT_H__
#define __FFPLAYER_VEFFECT_H__

#ifdef __cplusplus
extern "C" {
#endif

// º¯ÊýÉùÃ÷
void* veffect_create (void *surface);
void  veffect_destroy(void *ctxt);
void  veffect_render (void *ctxt, int x, int y, int w, int h, int type, void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
