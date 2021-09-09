#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "ringbuf.h"
#include "ffrdpc.h"
#include "ffrdp.h"

#ifdef WIN32
#include <windows.h>
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
#pragma warning(disable:4996) // disable warnings
#else
#include <unistd.h>
static uint32_t get_tick_count()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif
#ifdef ANDROID
#include "fanplayer_jni.h"
#endif

typedef struct {
    #define TS_EXIT      (1 << 0)
    #define TS_START     (1 << 1)
    #define TS_CONNECTED (1 << 2)
    uint32_t  status;
    pthread_t pthread;

    int            port;
    void          *ffrdp;
    void          *cbctxt;
    PFN_FFRDPC_CB  callback;

    int       head;
    int       tail;
    int       size;
    uint8_t   buff[2 * 1024 * 1024];
    char      ipaddr[32];
    char      txkey [32];
    char      rxkey [32];
} FFRDPC;

static int is_null_key(char key[32])
{
    int  i;
    for (i=0; i<32; i++) {
        if (key[i]) return 0;
    }
    return 1;
}

static void* ffrdpc_thread_proc(void *argv)
{
    FFRDPC  *ffrdpc = (FFRDPC*)argv;
    uint32_t tickgetframe = 0, ticktryconnect = 0, tickdump = 0;
    uint8_t  buffer[1500];
    int      ret;

#ifdef ANDROID
    JniAttachCurrentThread();
#endif

    while (!(ffrdpc->status & TS_EXIT)) {
        if (!(ffrdpc->status & TS_START)) { usleep(100*1000); continue; }

        if (!ffrdpc->ffrdp) {
            ffrdpc->ffrdp = ffrdp_init(ffrdpc->ipaddr, ffrdpc->port,
                is_null_key(ffrdpc->txkey) ? NULL : ffrdpc->txkey,
                is_null_key(ffrdpc->rxkey) ? NULL : ffrdpc->rxkey,
                0, 1024, 0);
            if (!ffrdpc->ffrdp) { usleep(100 * 1000); continue; }
        }

        if (!(ffrdpc->status & TS_CONNECTED)) {
            if ((int32_t)get_tick_count() - (int32_t)ticktryconnect > 1000) {
                ffrdp_send(ffrdpc->ffrdp, "1", 1);
                ticktryconnect = get_tick_count();
            }
        } else {
            if ((int32_t)get_tick_count() - (int32_t)tickdump > 1000) {
//              ffrdp_dump(ffrdpc->ffrdp, 0);
                tickdump = get_tick_count();
            }
        }

        while (1) {
            int n = sizeof(ffrdpc->buff) - ffrdpc->size < sizeof(buffer) ? sizeof(ffrdpc->buff) - ffrdpc->size : sizeof(buffer);
            if (n == 0 || (ret = ffrdp_recv(ffrdpc->ffrdp, (char*)buffer, n)) <= 0) break;
            ffrdpc->tail = ringbuf_write(ffrdpc->buff, sizeof(ffrdpc->buff), ffrdpc->tail, buffer, ret);
            ffrdpc->size+= ret;
            ffrdpc->status |= TS_CONNECTED;
        }

        while (ffrdpc->size > sizeof(uint32_t)) {
            uint32_t typelen, head;
            head = ringbuf_read(ffrdpc->buff, sizeof(ffrdpc->buff), ffrdpc->head, (uint8_t*)&typelen, sizeof(typelen));
            if ((char)typelen != 'T' && (int)((typelen >> 8) + sizeof(typelen)) > ffrdpc->size) break;
            ret = ffrdpc->callback(ffrdpc->cbctxt, typelen, (char*)ffrdpc->buff, sizeof(ffrdpc->buff), head, (typelen >> 8));
            if (ret >= 0) {
                ffrdpc->head = ret;
                ffrdpc->size-=(char)typelen == 'T' ? sizeof(typelen) : sizeof(typelen) + (typelen >> 8);
                tickgetframe = get_tick_count();
            } else break;
        }

        ffrdp_update(ffrdpc->ffrdp);
        if (((ffrdpc->status & TS_CONNECTED) && (int32_t)get_tick_count() - (int32_t)tickgetframe > 5000) || ffrdp_isdead(ffrdpc->ffrdp)) {
            printf("server lost !\n");
            ffrdp_free(ffrdpc->ffrdp); ffrdpc->ffrdp = NULL;
            ffrdpc->head = ffrdpc->tail = ffrdpc->size = 0;
            ffrdpc->status &= ~TS_CONNECTED;
        }
    }

    ffrdp_free(ffrdpc->ffrdp);
#ifdef ANDROID
    JniDetachCurrentThread();
#endif
    return NULL;
}

void* ffrdpc_init(char *ip, int port, char *txkey, char *rxkey, PFN_FFRDPC_CB callback, void *cbctxt)
{
    FFRDPC *ffrdpc = calloc(1, sizeof(FFRDPC));
    if (!ffrdpc) {
        printf("failed to allocate memory for ffrdpc !\n");
        return NULL;
    }

    ffrdpc->port     = port;
    ffrdpc->callback = callback;
    ffrdpc->cbctxt   = cbctxt;
    strncpy(ffrdpc->ipaddr, ip, sizeof(ffrdpc->ipaddr));
    if (txkey) strncpy(ffrdpc->txkey, txkey, sizeof(ffrdpc->txkey));
    if (rxkey) strncpy(ffrdpc->rxkey, rxkey, sizeof(ffrdpc->rxkey));

    // create client thread
    pthread_create(&ffrdpc->pthread, NULL, ffrdpc_thread_proc, ffrdpc);
    ffrdpc_start(ffrdpc, 1);
    return ffrdpc;
}

void ffrdpc_exit(void *ctxt)
{
    FFRDPC *ffrdpc = ctxt;
    if (!ctxt) return;
    ffrdpc->status |= TS_EXIT;
    pthread_join(ffrdpc->pthread, NULL);
    free(ctxt);
}

void ffrdpc_start(void *ctxt, int start)
{
    FFRDPC *ffrdpc = ctxt;
    if (!ctxt) return;
    if (start) {
        ffrdpc->status |= TS_START;
    } else {
        ffrdpc->status &=~TS_START;
    }
}

void ffrdpc_send(void *ctxt, char *data, int size)
{
    FFRDPC *ffrdpc = ctxt;
    if (!ctxt) return;
    ffrdp_send (ffrdpc->ffrdp, data, size);
    ffrdp_flush(ffrdpc->ffrdp);
}
