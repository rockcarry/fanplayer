#ifndef __FFRDP_H__
#define __FFRDP_H__

void* ffrdp_init  (char *ip, int port, int server, int fec);
void  ffrdp_free  (void *ctxt);
int   ffrdp_send  (void *ctxt, char *buf, int len);
int   ffrdp_recv  (void *ctxt, char *buf, int len);
int   ffrdp_isdead(void *ctxt);
void  ffrdp_update(void *ctxt);
void  ffrdp_flush (void *ctxt);

#endif

