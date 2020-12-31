#ifndef __FFRDPC_H__
#define __FFRDPC_H__

typedef int (*PFN_FFRDPC_CB)(void *ctxt, int type, char *rbuf, int rbsize, int rbhead, int fsize);

void* ffrdpc_init (char *ip, int port, char *txkey, char *rxkey, PFN_FFRDPC_CB callback, void *cbctxt);
void  ffrdpc_exit (void *ctxt);
void  ffrdpc_start(void *ctxt, int start);
void  ffrdpc_send (void *ctxt, char *data, int size);

#endif
