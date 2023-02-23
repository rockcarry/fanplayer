#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "ringbuf.h"
#include "ikcp.h"
#include "avkcpc.h"
#ifdef WIN32
#include <winsock2.h>
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
#define socklen_t int
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define SOCKET int
#define closesocket close
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

#define AVKCP_CONV (('A' << 0) | ('V' << 8) | ('K' << 16) | ('C' << 24))

typedef struct {
    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    uint32_t  status;
    pthread_t pthread;

    ikcpcb   *ikcp;
    uint32_t  tick_kcp_update;
    struct    sockaddr_in server_addr;
    struct    sockaddr_in client_addr;
    SOCKET    client_fd;
    uint8_t   buff[2 * 1024 * 1024];
    int       head;
    int       tail;
    int       size;
    PFN_AVKCPC_CB callback;
    void         *cbctxt;
} AVKCPC;

static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    AVKCPC *avkcpc = (AVKCPC*)user;
    return sendto(avkcpc->client_fd, buf, len, 0, (struct sockaddr*)&avkcpc->server_addr, sizeof(avkcpc->server_addr));
}

static void avkcpc_ikcp_update(AVKCPC *avkcpc)
{
    uint32_t tickcur = get_tick_count();
    if ((int32_t)tickcur - (int32_t)avkcpc->tick_kcp_update >= 0) {
        ikcp_update(avkcpc->ikcp, tickcur);
        avkcpc->tick_kcp_update = ikcp_check(avkcpc->ikcp, get_tick_count());
    }
}

static void* avkcpc_thread_proc(void *argv)
{
    AVKCPC   *avkcpc = (AVKCPC*)argv;
    struct    sockaddr_in fromaddr;
    socklen_t addrlen = sizeof(fromaddr);
    uint32_t  tickheartbeat = 0, tickgetframe = 0;
    uint8_t   buffer[1500];
    int       ret;
    unsigned long opt;

#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed !\n");
        return NULL;
    }
#endif
#ifdef ANDROID
    JniAttachCurrentThread();
#endif

    while (!(avkcpc->status & TS_EXIT)) {
        if (!(avkcpc->status & TS_START)) { usleep(100*1000); continue; }

        if (avkcpc->client_fd <= 0) {
            avkcpc->client_fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (avkcpc->client_fd > 0) {
                opt = 256*1024; setsockopt(avkcpc->client_fd, SOL_SOCKET, SO_RCVBUF, (char*)&opt, sizeof(int));
#ifdef WIN32
                opt = 1; ioctlsocket(avkcpc->client_fd, FIONBIO, &opt); // setup non-block io mode
#else
                fcntl(avkcpc->client_fd, F_SETFL, fcntl(avkcpc->client_fd, F_GETFL, 0) | O_NONBLOCK);  // setup non-block io mode
#endif
            } else {
                printf("failed to open socket !\n");
                usleep(100*1000); continue;
            }
        }

        if (avkcpc->ikcp == NULL) {
            avkcpc->ikcp = ikcp_create(AVKCP_CONV, avkcpc);
            if (avkcpc->ikcp) {
                ikcp_setoutput(avkcpc->ikcp, udp_output);
                ikcp_nodelay(avkcpc->ikcp, 1, 10, 2, 1);
                ikcp_wndsize(avkcpc->ikcp, 256, 1024);
                ikcp_setmtu(avkcpc->ikcp, 512);
                avkcpc->ikcp->interval = 10;
                avkcpc->ikcp->rx_minrto = 50;
                avkcpc->ikcp->fastresend = 1;
                avkcpc->ikcp->stream = 1;
            } else { usleep(100*1000); continue; }
        }

        if ((int32_t)get_tick_count() - (int32_t)tickheartbeat >= 1000) {
            tickheartbeat = get_tick_count();
            ikcp_send(avkcpc->ikcp, "hb", 3);
        }

        while (1) {
            if ((ret = recvfrom(avkcpc->client_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&fromaddr, &addrlen)) <= 0) break;
            ikcp_input(avkcpc->ikcp, (char*)buffer, ret);
        }

        while (1) {
            int n = sizeof(avkcpc->buff) - avkcpc->size < sizeof(buffer) ? sizeof(avkcpc->buff) - avkcpc->size : sizeof(buffer);
            if (n == 0 || (ret = ikcp_recv(avkcpc->ikcp, (char*)buffer, n)) <= 0) break;
            avkcpc->tail = ringbuf_write(avkcpc->buff, sizeof(avkcpc->buff), avkcpc->tail, buffer, ret);
            avkcpc->size+= ret;
        }

        while (avkcpc->size > sizeof(uint32_t)) {
            uint32_t typelen, head;
            head = ringbuf_read(avkcpc->buff, sizeof(avkcpc->buff), avkcpc->head, (uint8_t*)&typelen, sizeof(typelen));
            if ((char)typelen != 'T' && (int)((typelen >> 8) + sizeof(typelen)) > avkcpc->size) break;
            ret = avkcpc->callback(avkcpc->cbctxt, typelen, (char*)avkcpc->buff, sizeof(avkcpc->buff), head, (typelen >> 8));
            if (ret >= 0) {
                avkcpc->head = ret;
                avkcpc->size-=(char)typelen == 'T' ? sizeof(typelen) : sizeof(typelen) + (typelen >> 8);
                tickgetframe = get_tick_count();
            }
        }

        if (tickgetframe && (int32_t)get_tick_count() - (int32_t)tickgetframe > 3000) {
//          printf("===ck=== avkcpc disconnect !\n");
            ikcp_release(avkcpc->ikcp); avkcpc->ikcp = NULL;
            closesocket(avkcpc->client_fd); avkcpc->client_fd = 0;
            avkcpc->head = avkcpc->tail = avkcpc->size = 0;
            tickgetframe = 0; tickheartbeat= 0;
        }

        if (avkcpc->ikcp) avkcpc_ikcp_update(avkcpc);
        usleep(1*1000);
    }

    if (avkcpc->ikcp) ikcp_release(avkcpc->ikcp);
    if (avkcpc->client_fd > 0) closesocket(avkcpc->client_fd);
#ifdef WIN32
    WSACleanup();
#endif
#ifdef ANDROID
    JniDetachCurrentThread();
#endif
    return NULL;
}

void* avkcpc_init(char *ip, int port, PFN_AVKCPC_CB callback, void *cbctxt)
{
    AVKCPC *avkcpc = calloc(1, sizeof(AVKCPC));
    if (!avkcpc) {
        printf("failed to allocate memory for avkcpc !\n");
        return NULL;
    }

    avkcpc->server_addr.sin_family      = AF_INET;
    avkcpc->server_addr.sin_port        = htons(port);
    avkcpc->server_addr.sin_addr.s_addr = inet_addr(ip);

    avkcpc->callback = callback;
    avkcpc->cbctxt   = cbctxt;

    // create client thread
    pthread_create(&avkcpc->pthread, NULL, avkcpc_thread_proc, avkcpc);
    avkcpc_start(avkcpc, 1);
    return avkcpc;
}

void avkcpc_exit(void *ctxt)
{
    AVKCPC *avkcpc = ctxt;
    if (!ctxt) return;
    avkcpc->status |= TS_EXIT;
    pthread_join(avkcpc->pthread, NULL);
    free(ctxt);
}

void avkcpc_start(void *ctxt, int start)
{
    AVKCPC *avkcpc = ctxt;
    if (!ctxt) return;
    if (start) {
        avkcpc->status |= TS_START;
    } else {
        avkcpc->status &=~TS_START;
    }
}
