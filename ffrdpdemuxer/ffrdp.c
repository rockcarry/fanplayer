#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "ffrdp.h"

#ifdef WIN32
#include <winsock2.h>
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
typedef   signed char    int8_t;
typedef unsigned char   uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef   signed int    int32_t;
#pragma warning(disable:4996) // disable warnings
#else
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define SOCKET int
#define closesocket close
#define stricmp strcasecmp
#define strtok_s strtok_r
static uint32_t get_tick_count()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

#define FFRDP_RECVBUF_SIZE  (64 * 1024 - 4) // should < 64KB
#define FFRDP_MTU_SIZE       1024 // should align to 4 bytes
#define FFRDP_MIN_RTO        100
#define FFRDP_MAX_RTO        2000
#define FFRDP_MAX_WAITSND    256
#define FFRDP_POLL_CYCLE     100
#define FFRDP_DEAD_TIMEOUT   5000
#define FFRDP_DATFRM_FLOWCTL 32
#define FFRDP_UDPRBUF_SIZE  (128 * FFRDP_MTU_SIZE)
#define FFRDP_SELECT_SLEEP   1
#define FFRDP_SELECT_TIMEOUT 10000
#define FFRDP_USLEEP_TIMEOUT 1000

#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define GET_FRAME_SEQ(f)        (*(uint32_t*)(f)->data >> 8)
#define SET_FRAME_SEQ(f, seq)   do { *(uint32_t*)(f)->data = ((f)->data[0]) | (((seq) & 0xFFFFFF) << 8); } while (0)

enum {
    FFRDP_FRAME_TYPE_FEC0,
    FFRDP_FRAME_TYPE_ACK ,
    FFRDP_FRAME_TYPE_POLL,
    FFRDP_FRAME_TYPE_FEC3,
    FFRDP_FRAME_TYPE_FEC63 = 63,
};

typedef struct tagFFRDP_FRAME_NODE {
    struct tagFFRDP_FRAME_NODE *next;
    struct tagFFRDP_FRAME_NODE *prev;
    uint16_t size; // frame size
    uint8_t *data; // frame data
    #define FLAG_FIRST_SEND     (1 << 0) // after frame first send, this flag will be set
    #define FLAG_TIMEOUT_RESEND (1 << 1) // data frame wait ack timeout and be resend
    #define FLAG_FAST_RESEND    (1 << 2) // data frame need fast resend when next update
    uint32_t flags;        // frame flags
    uint32_t tick_send;    // frame send tick
    uint32_t tick_timeout; // frame ack timeout
} FFRDP_FRAME_NODE;

typedef struct {
    uint8_t  recv_buff[FFRDP_RECVBUF_SIZE];
    int32_t  recv_size, recv_head, recv_tail;
    #define FLAG_SERVER    (1 << 0)
    #define FLAG_CONNECTED (1 << 1)
    uint32_t flags;
    SOCKET   udp_fd;
    struct   sockaddr_in server_addr;
    struct   sockaddr_in client_addr;

    FFRDP_FRAME_NODE *send_list_head;
    FFRDP_FRAME_NODE *send_list_tail;
    FFRDP_FRAME_NODE *recv_list_head;
    FFRDP_FRAME_NODE *recv_list_tail;
    uint32_t send_seq; // send seq
    uint32_t recv_seq; // send seq
    uint32_t recv_win; // remote receive window
    uint32_t wait_snd; // data frame number wait to send
    uint32_t rttm, rtts, rttd, rto;
    uint32_t tick_query_rwin;
    uint32_t counter_send_1sttime;
    uint32_t counter_send_failed;
    uint32_t counter_send_poll;
    uint32_t counter_resend_fast;
    uint32_t counter_resend_rto;
    uint32_t counter_reach_maxrto;

    uint8_t  fec_txbuf[4 + FFRDP_MTU_SIZE + 2];
    uint8_t  fec_rxbuf[4 + FFRDP_MTU_SIZE + 2];
    uint32_t fec_redundancy;
    uint32_t fec_txseq;
    uint32_t fec_rxseq;
    uint32_t fec_rxcnt;
    uint32_t fec_rxmask;
    uint32_t counter_fec_tx_short;
    uint32_t counter_fec_tx_full;
    uint32_t counter_fec_rx_short;
    uint32_t counter_fec_rx_full;
    uint32_t counter_fec_ok;
    uint32_t counter_fec_failed;
} FFRDPCONTEXT;

static uint32_t ringbuf_write(uint8_t *rbuf, uint32_t maxsize, uint32_t tail, uint8_t *src, uint32_t len)
{
    uint8_t *buf1 = rbuf + tail;
    int      len1 = MIN(maxsize-tail, len);
    uint8_t *buf2 = rbuf;
    int      len2 = len  - len1;
    memcpy(buf1, src + 0   , len1);
    memcpy(buf2, src + len1, len2);
    return len2 ? len2 : tail + len1;
}

static uint32_t ringbuf_read(uint8_t *rbuf, uint32_t maxsize, uint32_t head, uint8_t *dst, uint32_t len)
{
    uint8_t *buf1 = rbuf + head;
    int      len1 = MIN(maxsize-head, len);
    uint8_t *buf2 = rbuf;
    int      len2 = len  - len1;
    if (dst) memcpy(dst + 0   , buf1, len1);
    if (dst) memcpy(dst + len1, buf2, len2);
    return len2 ? len2 : head + len1;
}

static int seq_distance(uint32_t seq1, uint32_t seq2) // calculate seq distance
{
    int c = seq1 - seq2;
    if      (c >=  0x7FFFFF) return c - 0x1000000;
    else if (c <= -0x7FFFFF) return c + 0x1000000;
    else return c;
}

static FFRDP_FRAME_NODE* frame_node_new(int type, int size) // create a new frame node
{
    FFRDP_FRAME_NODE *node = malloc(sizeof(FFRDP_FRAME_NODE) + 4 + size + (type ? 2 : 0));
    if (!node) return NULL;
    memset(node, 0, sizeof(FFRDP_FRAME_NODE));
    node->size    = 4 + size + (type ? 2 : 0);
    node->data    = (uint8_t*)node + sizeof(FFRDP_FRAME_NODE);
    node->data[0] = type;
    return node;
}

static int frame_payload_size(FFRDP_FRAME_NODE *node) {
    return  node->size - 4 - (node->data[0] ? 2 : 0);
}

static void list_enqueue(FFRDP_FRAME_NODE **head, FFRDP_FRAME_NODE **tail, FFRDP_FRAME_NODE *node)
{
    FFRDP_FRAME_NODE *p;
    uint32_t seqnew, seqcur;
    int      dist;
    if (*head == NULL) {
        *head = node;
        *tail = node;
    } else {
        seqnew = GET_FRAME_SEQ(node);
        for (p=*tail; p; p=p->prev) {
            seqcur = GET_FRAME_SEQ(p);
            dist   = seq_distance(seqnew, seqcur);
            if (dist == 0) return;
            if (dist >  0) {
                if (p->next) p->next->prev = node;
                else *tail = node;
                node->next = p->next;
                node->prev = p;
                p->next    = node;
                return;
            }
        }
        node->next = *head;
        node->next->prev = node;
        *head = node;
    }
}

static void list_remove(FFRDP_FRAME_NODE **head, FFRDP_FRAME_NODE **tail, FFRDP_FRAME_NODE *node)
{
    if (node->next) node->next->prev = node->prev;
    else *tail = node->prev;
    if (node->prev) node->prev->next = node->next;
    else *head = node->next;
    free(node);
}

static void list_free(FFRDP_FRAME_NODE **head, FFRDP_FRAME_NODE **tail)
{
    while (*head) list_remove(head, tail, *head);
}

static int ffrdp_sleep(FFRDPCONTEXT *ffrdp, int flag)
{
    if (flag) {
        struct timeval tv;
        fd_set  rs;
        FD_ZERO(&rs);
        FD_SET(ffrdp->udp_fd, &rs);
        tv.tv_sec  = 0;
        tv.tv_usec = FFRDP_SELECT_TIMEOUT;
        if (select((int)ffrdp->udp_fd + 1, &rs, NULL, NULL, &tv) <= 0) return -1;
    } else usleep(FFRDP_USLEEP_TIMEOUT);
    return 0;
}

static int ffrdp_send_data_frame(FFRDPCONTEXT *ffrdp, FFRDP_FRAME_NODE *frame, struct sockaddr_in *dstaddr)
{
    if (frame->size != 4 + FFRDP_MTU_SIZE + 2) { // short frame
        ffrdp->counter_fec_tx_short++;
        return sendto(ffrdp->udp_fd, frame->data, frame->size, 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)) == frame->size ? 0 : -1;
    } else { // full frame
        uint32_t *psrc = (uint32_t*)frame->data, *pdst = (uint32_t*)ffrdp->fec_txbuf, i;
        *(uint16_t*)(frame->data + 4 + FFRDP_MTU_SIZE) = (uint16_t)ffrdp->fec_txseq;
        if (sendto(ffrdp->udp_fd, frame->data, frame->size, 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)) != frame->size) return -1;
        else ffrdp->fec_txseq++;
        for (i=0; i<(4+FFRDP_MTU_SIZE)/sizeof(uint32_t); i++) *pdst++ ^= *psrc++; // make xor fec frame
        if (ffrdp->fec_txseq % ffrdp->fec_redundancy == ffrdp->fec_redundancy - 1) {
            *(uint16_t*)(ffrdp->fec_txbuf + 4 + FFRDP_MTU_SIZE) = ffrdp->fec_txseq++;
            ffrdp->fec_txbuf[0] = ffrdp->fec_redundancy;
            sendto(ffrdp->udp_fd, ffrdp->fec_txbuf, sizeof(ffrdp->fec_txbuf), 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)); // send fec frame
            memset(ffrdp->fec_txbuf, 0, sizeof(ffrdp->fec_txbuf)); // clear tx_fecbuf
        }
        ffrdp->counter_fec_tx_full++;
        return 0;
    }
}

static int ffrdp_recv_data_frame(FFRDPCONTEXT *ffrdp, FFRDP_FRAME_NODE *frame)
{
    uint32_t fecseq, fecrdc, *psrc, *pdst, type, i;
    if (frame->size != 4 + FFRDP_MTU_SIZE + 2) { // short frame
        ffrdp->counter_fec_rx_short++; return 0;
    } else {
        fecseq = *(uint16_t*)(frame->data + 4 + FFRDP_MTU_SIZE); // full frame
        fecrdc = frame->data[0];
    }
    if (fecseq / fecrdc != ffrdp->fec_rxseq / fecrdc) { //group changed
        memcpy(ffrdp->fec_rxbuf, frame->data, sizeof(ffrdp->fec_rxbuf));
        ffrdp->fec_rxseq = fecseq; ffrdp->fec_rxmask = 1 << (fecseq % fecrdc); ffrdp->fec_rxcnt = 1;
        return fecseq % fecrdc != fecrdc - 1 ? 0 : -1;
    } else ffrdp->fec_rxseq = fecseq; // group not changed
    if (fecseq % fecrdc == fecrdc - 1) { // it's redundance frame
        if (ffrdp->fec_rxcnt == fecrdc - 1) { return -1; }
        if (ffrdp->fec_rxcnt != fecrdc - 2) { ffrdp->counter_fec_failed++; return -1; }
        type = frame->data[0];
        psrc = (uint32_t*)ffrdp->fec_rxbuf; pdst = (uint32_t*)frame->data;
        for (i=0; i<(4+FFRDP_MTU_SIZE)/sizeof(uint32_t); i++) *pdst++ ^= *psrc++;
        frame->data[0] = type;
        ffrdp->counter_fec_ok++;
    } else if (!(ffrdp->fec_rxmask & (1 << (fecseq % fecrdc)))) { // update fec_rxbuf
        psrc = (uint32_t*)frame->data; pdst = (uint32_t*)ffrdp->fec_rxbuf;
        for (i=0; i<(4+FFRDP_MTU_SIZE)/sizeof(uint32_t); i++) *pdst++ ^= *psrc++;
        ffrdp->fec_rxmask |= 1 << (fecseq % fecrdc); ffrdp->fec_rxcnt++;
    }
    ffrdp->counter_fec_rx_full++;
    return 0;
}

void* ffrdp_init(char *ip, int port, int server, int fec)
{
#ifdef WIN32
    WSADATA wsaData;
#endif
    unsigned long opt;
    FFRDPCONTEXT *ffrdp = calloc(1, sizeof(FFRDPCONTEXT));
    if (!ffrdp) return NULL;

#ifdef WIN32
    timeBeginPeriod(1);
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed !\n");
        return NULL;
    }
#endif

    ffrdp->recv_win       = FFRDP_RECVBUF_SIZE;
    ffrdp->rtts           = (uint32_t) -1;
    ffrdp->rto            = FFRDP_MIN_RTO;
    ffrdp->fec_redundancy = MAX(0, MIN(fec, 63));

    ffrdp->server_addr.sin_family      = AF_INET;
    ffrdp->server_addr.sin_port        = htons(port);
    ffrdp->server_addr.sin_addr.s_addr = inet_addr(ip);
    ffrdp->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ffrdp->udp_fd < 0) {
        printf("failed to open socket !\n");
        goto failed;
    }

#ifdef WIN32
    opt = 1; ioctlsocket(ffrdp->udp_fd, FIONBIO, &opt); // setup non-block io mode
#else
    fcntl(ffrdp->udp_fd, F_SETFL, fcntl(ffrdp->udp_fd, F_GETFL, 0) | O_NONBLOCK);  // setup non-block io mode
#endif
    opt = FFRDP_UDPRBUF_SIZE; setsockopt(ffrdp->udp_fd, SOL_SOCKET, SO_RCVBUF   , (char*)&opt, sizeof(int)); // setup udp recv buffer size
    opt = 1;                  setsockopt(ffrdp->udp_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int)); // setup reuse addr

    if (server) {
        ffrdp->flags |= FLAG_SERVER;
        if (bind(ffrdp->udp_fd, (struct sockaddr*)&ffrdp->server_addr, sizeof(ffrdp->server_addr)) == -1) {
            printf("failed to bind !\n");
            goto failed;
        }
    }
    return ffrdp;

failed:
    if (ffrdp->udp_fd > 0) closesocket(ffrdp->udp_fd);
    free(ffrdp);
    return NULL;
}

void ffrdp_free(void *ctxt)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    if (!ctxt) return;
    if (ffrdp->udp_fd > 0) closesocket(ffrdp->udp_fd);
    list_free(&ffrdp->send_list_head, &ffrdp->send_list_tail);
    list_free(&ffrdp->recv_list_head, &ffrdp->recv_list_tail);
    free(ffrdp);
#ifdef WIN32
    WSACleanup();
    timeEndPeriod(1);
#endif
}

int ffrdp_send(void *ctxt, char *buf, int len)
{
    FFRDPCONTEXT     *ffrdp = (FFRDPCONTEXT*)ctxt;
    FFRDP_FRAME_NODE *node  = NULL;
    int               n = len, type, size;
    if (  !ffrdp || ((ffrdp->flags & FLAG_SERVER) && (ffrdp->flags & FLAG_CONNECTED) == 0)
       || ((len + FFRDP_MTU_SIZE - 1) / FFRDP_MTU_SIZE + ffrdp->wait_snd > FFRDP_MAX_WAITSND)) {
        if (ffrdp) ffrdp->counter_send_failed++;
        return -1;
    }
    while (n > 0) {
        size = MIN(FFRDP_MTU_SIZE, n);
        type = ffrdp->fec_redundancy == 0 ? 0 : size != FFRDP_MTU_SIZE ? 0 : ffrdp->fec_redundancy;
        if (!(node = frame_node_new(type, size))) break;
        SET_FRAME_SEQ(node, ffrdp->send_seq);
        memcpy(node->data + 4, buf, size);
        list_enqueue(&ffrdp->send_list_head, &ffrdp->send_list_tail, node);
        ffrdp->send_seq++; ffrdp->wait_snd++;
        buf += size; n -= size;
    }
    return len - n;
}

int ffrdp_recv(void *ctxt, char *buf, int len)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    int           ret;
    if (!ctxt) return -1;
    ret = MIN(len, ffrdp->recv_size);
    if (ret > 0) {
        ffrdp->recv_head = ringbuf_read(ffrdp->recv_buff, sizeof(ffrdp->recv_buff), ffrdp->recv_head, (uint8_t*)buf, ret);
        ffrdp->recv_size-= ret;
    }
    return ret;
}

int ffrdp_isdead(void *ctxt)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    if (!ctxt) return -1;
    return ffrdp->send_list_head && (ffrdp->send_list_head->flags & FLAG_FIRST_SEND) && (int32_t)get_tick_count() - (int32_t)ffrdp->send_list_head->tick_send > FFRDP_DEAD_TIMEOUT;
}

static void ffrdp_send_ack(FFRDPCONTEXT *ffrdp, struct sockaddr_in *dstaddr)
{
    FFRDP_FRAME_NODE *p;
    int32_t dist, recv_mack, size, i;
    uint8_t data[8];
    while (ffrdp->recv_list_head) {
        dist = seq_distance(GET_FRAME_SEQ(ffrdp->recv_list_head), ffrdp->recv_seq);
        if (dist == 0 && (size = frame_payload_size(ffrdp->recv_list_head)) <= (int)(sizeof(ffrdp->recv_buff) - ffrdp->recv_size)) {
            ffrdp->recv_tail = ringbuf_write(ffrdp->recv_buff, sizeof(ffrdp->recv_buff), ffrdp->recv_tail, ffrdp->recv_list_head->data + 4, size);
            ffrdp->recv_size+= size;
            ffrdp->recv_seq++; ffrdp->recv_seq &= 0xFFFFFF;
            list_remove(&ffrdp->recv_list_head, &ffrdp->recv_list_tail, ffrdp->recv_list_head);
        } else break;
    }
    for (recv_mack=0,i=0,p=ffrdp->recv_list_head; i<=16&&p; i++,p=p->next) {
        dist = seq_distance(GET_FRAME_SEQ(p), ffrdp->recv_seq);
        if (dist <= 16) recv_mack |= 1 << (dist - 1); // dist is obviously > 0
    }
    *(uint32_t*)(data + 0) = (FFRDP_FRAME_TYPE_ACK << 0) | (ffrdp->recv_seq << 8);
    *(uint32_t*)(data + 4) = (recv_mack << 0);
    if (!ffrdp->recv_list_head || GET_FRAME_SEQ(ffrdp->recv_list_head) != ffrdp->recv_seq) {
        *(uint32_t*)(data + 4) |= (sizeof(ffrdp->recv_buff) - ffrdp->recv_size) << 16;
    }
    sendto(ffrdp->udp_fd, data, sizeof(data), 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)); // send ack frame
}

void ffrdp_update(void *ctxt)
{
    FFRDPCONTEXT       *ffrdp   = (FFRDPCONTEXT*)ctxt;
    FFRDP_FRAME_NODE   *node    = NULL, *p = NULL, *t = NULL;
    struct sockaddr_in *dstaddr = NULL, srcaddr;
    uint32_t addrlen = sizeof(srcaddr);
    int32_t  una, mack, ret, got_data = 0, got_poll = 0, send_una, send_mack = 0, recv_una, dist, size, maxack, i;
    uint8_t  data[8];

    if (!ctxt) return;
    dstaddr  = ffrdp->flags & FLAG_SERVER ? &ffrdp->client_addr : &ffrdp->server_addr;
    send_una = ffrdp->send_list_head ? GET_FRAME_SEQ(ffrdp->send_list_head) : 0;
    recv_una = ffrdp->recv_seq;

    for (i=0,p=ffrdp->send_list_head; i<FFRDP_DATFRM_FLOWCTL&&p; i++,p=p->next) {
        if (!(p->flags & FLAG_FIRST_SEND)) { // first send
            if ((size = frame_payload_size(p)) <= (int)ffrdp->recv_win) {
                if (ffrdp_send_data_frame(ffrdp, p, dstaddr) != 0) break;
                ffrdp->recv_win -= size;
                p->tick_send     = get_tick_count();
                p->tick_timeout  = p->tick_send + ffrdp->rto;
                p->flags        |= FLAG_FIRST_SEND;
                ffrdp->counter_send_1sttime++;
            } else if ((int32_t)get_tick_count() - (int32_t)ffrdp->tick_query_rwin > FFRDP_POLL_CYCLE) { // query remote receive window size
                data[0] = FFRDP_FRAME_TYPE_POLL; sendto(ffrdp->udp_fd, data, 1, 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in));
                ffrdp->counter_send_poll++;
                break;
            }
        } else if ((p->flags & FLAG_FIRST_SEND) && ((int32_t)get_tick_count() - (int32_t)p->tick_timeout > 0 || (p->flags & FLAG_FAST_RESEND))) { // resend
            if (ffrdp_send_data_frame(ffrdp, p, dstaddr) != 0) break;
            if (!(p->flags & FLAG_FAST_RESEND)) {
                if (ffrdp->rto == FFRDP_MAX_RTO) {
                    p->flags &= FLAG_TIMEOUT_RESEND;
                    ffrdp->counter_reach_maxrto ++;
                } else p->flags |= FLAG_TIMEOUT_RESEND;
                ffrdp->rto += ffrdp->rto / 2;
                ffrdp->rto  = MIN(ffrdp->rto, FFRDP_MAX_RTO);
                ffrdp->counter_resend_rto++;
            } else {
                p->flags &=~FLAG_FAST_RESEND;
                ffrdp->counter_resend_fast++;
            }
            p->tick_timeout+= ffrdp->rto;
            if (ffrdp->rto == FFRDP_MAX_RTO) break; // if rto reach FFRDP_MAX_RTO, we only try to resend one data frame
        }
    }

    if (ffrdp_sleep(ffrdp, FFRDP_SELECT_SLEEP) != 0) return;
    for (node=NULL;;) { // receive data
        if (!node && !(node = frame_node_new(FFRDP_FRAME_TYPE_FEC3, FFRDP_MTU_SIZE))) break;;
        if ((ret = recvfrom(ffrdp->udp_fd, node->data, node->size, 0, (struct sockaddr*)&srcaddr, &addrlen)) <= 0) break;
        if ((ffrdp->flags & FLAG_SERVER) && (ffrdp->flags & FLAG_CONNECTED) == 0) {
            if (ffrdp->flags & FLAG_CONNECTED) {
                if (memcmp(&srcaddr, &ffrdp->client_addr, sizeof(srcaddr)) != 0) continue;
            } else {
                ffrdp->flags |= FLAG_CONNECTED;
                memcpy(&ffrdp->client_addr, &srcaddr, sizeof(ffrdp->client_addr));
            }
        }

        if (node->data[0] == FFRDP_FRAME_TYPE_FEC0 || (node->data[0] >= FFRDP_FRAME_TYPE_FEC3 && node->data[0] <= FFRDP_FRAME_TYPE_FEC63)) { // data frame
            node->size = ret; // frame size is the return size of recvfrom
            if (ffrdp_recv_data_frame(ffrdp, node) == 0) {
                dist = seq_distance(GET_FRAME_SEQ(node), recv_una);
                if (dist == 0) { recv_una++; }
                if (dist >= 0) { list_enqueue(&ffrdp->recv_list_head, &ffrdp->recv_list_tail, node); node = NULL; }
                got_data = 1;
            }
        } else if (node->data[0] == FFRDP_FRAME_TYPE_ACK ) {
            una  = *(uint32_t*)(node->data + 0) >> 8;
            mack = *(uint32_t*)(node->data + 4) & 0xFFFF;
            dist = seq_distance(una, send_una);
            if (dist == 0) send_mack |= mack;
            else if (dist > 0) {
                send_una  = una;
                send_mack = (send_mack >> dist) | mack;
                ffrdp->recv_win = *(uint32_t*)(node->data + 4) >> 16; ffrdp->tick_query_rwin = get_tick_count();
            }
        } else if (node->data[0] == FFRDP_FRAME_TYPE_POLL) got_poll = 1;
    }
    if (node) free(node);

    if (got_data || got_poll) ffrdp_send_ack(ffrdp, dstaddr); // send ack frame
    if (ffrdp->send_list_head && seq_distance(send_una, GET_FRAME_SEQ(ffrdp->send_list_head)) > 0) { // got ack frame
        for (p=ffrdp->send_list_head; p;) {
            dist = seq_distance(GET_FRAME_SEQ(p), send_una);
            for (i=15; i>=0 && !(send_mack&(1<<i)); i--);
            if (i < 0) maxack = (send_una - 1) & 0xFFFFFF;
            else maxack = (send_una + i + 1) & 0xFFFFFF;

            if (dist > 16 || !(p->flags & FLAG_FIRST_SEND)) break;
            else if (dist < 0 || (dist > 0 && (send_mack & (1 << (dist-1))))) { // this frame got ack
                if (!(p->flags & FLAG_TIMEOUT_RESEND)) {
                    ffrdp->rttm = (int32_t)get_tick_count() - (int32_t)p->tick_send;
                    if (ffrdp->rtts == (uint32_t)-1) {
                        ffrdp->rtts = ffrdp->rttm;
                        ffrdp->rttd = ffrdp->rttm / 2;
                    } else {
                        ffrdp->rtts = (7 * ffrdp->rtts + 1 * ffrdp->rttm) / 8;
                        ffrdp->rttd = (3 * ffrdp->rttd + 1 * abs((int)ffrdp->rttm - (int)ffrdp->rtts)) / 4;
                    }
                    ffrdp->rto = ffrdp->rtts + 4 * ffrdp->rttd;
                    ffrdp->rto = MAX(FFRDP_MIN_RTO, ffrdp->rto);
                    ffrdp->rto = MIN(FFRDP_MAX_RTO, ffrdp->rto);
                }
                t = p; p = p->next; list_remove(&ffrdp->send_list_head, &ffrdp->send_list_tail, t);
                ffrdp->wait_snd--; continue;
            } else if (seq_distance(maxack, GET_FRAME_SEQ(p)) > 0) {
                p->flags |= FLAG_FAST_RESEND;
            }
            p = p->next;
        }
    }
}

