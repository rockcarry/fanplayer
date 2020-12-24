#ifndef __FFRDP_H__
#define __FFRDP_H__

void* ffrdp_init  (char *ip, int port, char *txkey, char *rxkey, int server, int smss, int sfec);
void  ffrdp_free  (void *ctxt);
int   ffrdp_send  (void *ctxt, char *buf, int len);
int   ffrdp_recv  (void *ctxt, char *buf, int len);
int   ffrdp_isdead(void *ctxt);
void  ffrdp_update(void *ctxt);
void  ffrdp_flush (void *ctxt);
void  ffrdp_dump  (void *ctxt, int clearhistory);

#endif

