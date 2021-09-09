#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "ffrdp.h"
#include "libavutil/log.h"

#ifdef CONFIG_ENABLE_AES256
#include <openssl/aes.h>
#endif

#ifdef WIN32
#pragma warning(disable:4996) // disable warnings
#include <winsock2.h>
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
#define socklen_t int
#else
#include <time.h>
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

#define FFRDP_MAX_MSS       (1500 - 8) // should align to 4 bytes and <= 1500 - 8
#define FFRDP_MIN_RTO        50
#define FFRDP_MAX_RTO        2000
#define FFRDP_MAX_WAITSND    256
#define FFRDP_QUERY_CYCLE    500
#define FFRDP_FLUSH_TIMEOUT  500
#define FFRDP_DEAD_TIMEOUT   5000
#define FFRDP_MIN_CWND_SIZE  1
#define FFRDP_DEF_CWND_SIZE  32
#define FFRDP_MAX_CWND_SIZE  64
#define FFRDP_RECVBUF_SIZE  (128 * (FFRDP_MAX_MSS + 0))
#define FFRDP_UDPSBUF_SIZE  (64  * (FFRDP_MAX_MSS + 6))
#define FFRDP_UDPRBUF_SIZE  (128 * (FFRDP_MAX_MSS + 6))
#define FFRDP_SELECT_SLEEP   1
#define FFRDP_SELECT_TIMEOUT 10000
#define FFRDP_USLEEP_TIMEOUT 1000

#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define GET_FRAME_SEQ(f)        (*(uint32_t*)(f)->data >> 8)
#define SET_FRAME_SEQ(f, seq)   do { *(uint32_t*)(f)->data = ((f)->data[0]) | (((seq) & 0xFFFFFF) << 8); } while (0)

enum {
    FFRDP_FRAME_TYPE_FULL,       // full  frame
    FFRDP_FRAME_TYPE_SHORT,      // short frame
    FFRDP_FRAME_TYPE_FEC2,       // fec2  frame
    FFRDP_FRAME_TYPE_FEC32 = 32, // fec32 frame
    FFRDP_FRAME_TYPE_ACK   = 33, // ack   frame
    FFRDP_FRAME_TYPE_QUERY = 34, // query frame
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
    uint32_t tick_1sts;    // frame first time send tick
    uint32_t tick_send;    // frame send tick
    uint32_t tick_timeout; // frame ack timeout tick
} FFRDP_FRAME_NODE;

typedef struct {
    uint8_t  recv_buff[FFRDP_RECVBUF_SIZE];
    int32_t  recv_size, recv_head, recv_tail;
    #define FLAG_SERVER    (1 << 0)
    #define FLAG_CONNECTED (1 << 1)
    #define FLAG_FLUSH     (1 << 2)
    #define FLAG_TX_AES256 (1 << 3)
    #define FLAG_RX_AES256 (1 << 4)
    uint32_t flags;
    SOCKET   udp_fd;
    struct   sockaddr_in server_addr;
    pthread_mutex_t lock;

    FFRDP_FRAME_NODE *send_list_head;
    FFRDP_FRAME_NODE *send_list_tail;
    FFRDP_FRAME_NODE *recv_list_head;
    FFRDP_FRAME_NODE *recv_list_tail;
    FFRDP_FRAME_NODE *cur_new_node;
    uint32_t          cur_new_size;
    uint32_t          cur_new_tick;
    uint32_t send_seq; // send seq
    uint32_t recv_seq; // send seq
    uint32_t wait_snd; // data frame number wait to send
    uint32_t rttm, rtts, rttd, rto;
    uint32_t rmss, smss, swnd, cwnd, ssthresh;
    uint32_t tick_recv_ack;
    uint32_t tick_send_query;
    uint32_t tick_ffrdp_dump;

    uint8_t  fec_txbuf[4 + FFRDP_MAX_MSS + 2];
    uint8_t  fec_rxbuf[4 + FFRDP_MAX_MSS + 2];
    uint8_t  fec_txredundancy, fec_rxredundancy;
    uint16_t fec_txseq;
    uint16_t fec_rxseq;
    uint16_t fec_rxcnt;
    uint32_t fec_rxmask;

#ifdef CONFIG_ENABLE_AES256
    AES_KEY  aes_encrypt_key;
    AES_KEY  aes_decrypt_key;
#endif

    #define DEADLINK_SENDERR_THRESHOLD 300
    uint32_t counter_udpsenderr;
    uint32_t counter_send_bytes;
    uint32_t counter_recv_bytes;
    uint32_t counter_send_1sttime;
    uint32_t counter_send_failed;
    uint32_t counter_send_query;
    uint32_t counter_resend_fast;
    uint32_t counter_resend_rto;
    uint32_t counter_reach_maxrto;
    uint32_t counter_txfull , counter_rxfull ;
    uint32_t counter_txshort, counter_rxshort;
    uint32_t counter_fec_tx;
    uint32_t counter_fec_rx;
    uint32_t counter_fec_ok;
    uint32_t counter_fec_failed;
    uint32_t reserved;
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
    FFRDP_FRAME_NODE *node = malloc(sizeof(FFRDP_FRAME_NODE) + 4 + size + (type <= FFRDP_FRAME_TYPE_SHORT ? 0 : 2));
    if (!node) return NULL;
    memset(node, 0, sizeof(FFRDP_FRAME_NODE));
    node->size    = 4 + size + (type <= FFRDP_FRAME_TYPE_SHORT ? 0 : 2);
    node->data    = (uint8_t*)node + sizeof(FFRDP_FRAME_NODE);
    node->data[0] = type;
    return node;
}

#ifdef CONFIG_ENABLE_AES256
static void frame_node_encrypt(FFRDP_FRAME_NODE *node, AES_KEY *key, int enc)
{
    uint8_t *pdata = node->data + 4, *pend = node->data + node->size - (node->data[0] <= FFRDP_FRAME_TYPE_SHORT ? 0 : 2) - AES_BLOCK_SIZE;
    while (pdata <= pend) {
        AES_ecb_encrypt(pdata, pdata, key, enc);
        pdata += AES_BLOCK_SIZE;
    }
}
#endif

static int frame_payload_size(FFRDP_FRAME_NODE *node) {
    return  node->size - 4 - (node->data[0] <= FFRDP_FRAME_TYPE_SHORT ? 0 : 2);
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
    if (ffrdp->flags & FLAG_FLUSH) { ffrdp->flags &= ~FLAG_FLUSH; return 0; }
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
    switch (frame->size - ffrdp->smss) {
    case 6 : ffrdp->counter_fec_tx ++; *(uint16_t*)(frame->data + 4 + ffrdp->smss) = ffrdp->fec_txseq++; break; // tx fec frame
    case 4 : ffrdp->counter_txfull ++; break; // tx full  frame
    default: ffrdp->counter_txshort++; break; // tx short frame
    }
    if (sendto(ffrdp->udp_fd, frame->data, frame->size, 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)) != frame->size) { ffrdp->counter_udpsenderr++; return -1; }
    else ffrdp->counter_udpsenderr = 0;
    if (frame->size == 4 + ffrdp->smss + 2) { // fec frame
        uint32_t *psrc = (uint32_t*)frame->data, *pdst = (uint32_t*)ffrdp->fec_txbuf, i;
        for (i=0; i<(4+ffrdp->smss)/sizeof(uint32_t); i++) *pdst++ ^= *psrc++; // make xor fec frame
        if (ffrdp->fec_txseq % ffrdp->fec_txredundancy == ffrdp->fec_txredundancy - 1) {
            *(uint16_t*)(ffrdp->fec_txbuf + 4 + ffrdp->smss) = ffrdp->fec_txseq++; ffrdp->fec_txbuf[0] = ffrdp->fec_txredundancy;
            sendto(ffrdp->udp_fd, ffrdp->fec_txbuf, frame->size, 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)); // send fec frame
            memset(ffrdp->fec_txbuf, 0, sizeof(ffrdp->fec_txbuf)); // clear tx_fecbuf
            ffrdp->counter_fec_tx++;
        }
    }
    return 0;
}

static int ffrdp_recv_data_frame(FFRDPCONTEXT *ffrdp, FFRDP_FRAME_NODE *frame)
{
    uint32_t fecseq, fecrdc, *psrc, *pdst, type, i;
    switch (frame->data[0]) {
    case FFRDP_FRAME_TYPE_SHORT: ffrdp->counter_rxshort++; return 0; // short frame
    case FFRDP_FRAME_TYPE_FULL : ffrdp->counter_rxfull ++; ffrdp->rmss = frame->size - 4; return 0; // full frame
    default:                     ffrdp->counter_fec_rx ++; ffrdp->rmss = frame->size - 6; break;    // fec  frame
    }
    fecseq = *(uint16_t*)(frame->data + frame->size - 2);
    fecrdc = frame->data[0];
    if (fecseq / fecrdc != ffrdp->fec_rxseq / fecrdc || ffrdp->fec_rxredundancy != fecrdc) { // group changed or fec_rxredundancy changed
        memcpy(ffrdp->fec_rxbuf, frame->data, frame->size);
        ffrdp->fec_rxseq = fecseq; ffrdp->fec_rxmask = 1 << (fecseq % fecrdc); ffrdp->fec_rxcnt = 1; ffrdp->fec_rxredundancy = fecrdc;
        return fecseq % fecrdc != fecrdc - 1 ? 0 : -1;
    } else ffrdp->fec_rxseq = fecseq; // group not changed
    if (fecseq % fecrdc == fecrdc - 1) { // it's redundance frame
        if (ffrdp->fec_rxcnt == fecrdc - 1) return -1;
        if (ffrdp->fec_rxcnt != fecrdc - 2) { ffrdp->counter_fec_failed++; return -1; }
        type = frame->data[0];
        psrc = (uint32_t*)ffrdp->fec_rxbuf; pdst = (uint32_t*)frame->data;
        for (i=0; i<(frame->size-2)/sizeof(uint32_t); i++) *pdst++ ^= *psrc++;
        frame->data[0] = type;
        ffrdp->counter_fec_ok++;
    } else if (!(ffrdp->fec_rxmask & (1 << (fecseq % fecrdc)))) { // update fec_rxbuf
        psrc = (uint32_t*)frame->data; pdst = (uint32_t*)ffrdp->fec_rxbuf;
        for (i=0; i<(frame->size-2)/sizeof(uint32_t); i++) *pdst++ ^= *psrc++;
        ffrdp->fec_rxmask |= 1 << (fecseq % fecrdc); ffrdp->fec_rxcnt++;
    }
    return 0;
}

void* ffrdp_init(char *ip, int port, char *txkey, char *rxkey, int server, int smss, int sfec)
{
    FFRDPCONTEXT *ffrdp = NULL;
    unsigned long opt;
#ifdef WIN32
    WSADATA wsaData;
    timeBeginPeriod(1);
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed !\n");
        return NULL;
    }
#endif

    if (!(ffrdp = calloc(1, sizeof(FFRDPCONTEXT)))) return NULL;
    ffrdp->swnd     = FFRDP_DEF_CWND_SIZE;
    ffrdp->cwnd     = FFRDP_DEF_CWND_SIZE;
    ffrdp->ssthresh = FFRDP_DEF_CWND_SIZE;
    ffrdp->rtts     = (uint32_t) -1;
    ffrdp->rto      = FFRDP_MIN_RTO;
    ffrdp->rmss     = FFRDP_MAX_MSS;
    ffrdp->smss     = MAX(1, MIN(smss, FFRDP_MAX_MSS));
    ffrdp->fec_txredundancy = MAX(0, MIN(sfec, FFRDP_FRAME_TYPE_FEC32));
    ffrdp->tick_ffrdp_dump  = get_tick_count();

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
    opt = FFRDP_UDPSBUF_SIZE; setsockopt(ffrdp->udp_fd, SOL_SOCKET, SO_SNDBUF   , (char*)&opt, sizeof(int)); // setup udp send buffer size
    opt = FFRDP_UDPRBUF_SIZE; setsockopt(ffrdp->udp_fd, SOL_SOCKET, SO_RCVBUF   , (char*)&opt, sizeof(int)); // setup udp recv buffer size
    opt = 1;                  setsockopt(ffrdp->udp_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int)); // setup reuse addr

    if (server) {
        ffrdp->flags |= FLAG_SERVER;
        if (bind(ffrdp->udp_fd, (struct sockaddr*)&ffrdp->server_addr, sizeof(ffrdp->server_addr)) == -1) {
            printf("failed to bind !\n");
            goto failed;
        }
    }

    if (txkey) {
#ifdef CONFIG_ENABLE_AES256
        ffrdp->flags |= FLAG_TX_AES256;
        AES_set_encrypt_key((uint8_t*)txkey, 256, &ffrdp->aes_encrypt_key);
#endif
    }
    if (rxkey) {
#ifdef CONFIG_ENABLE_AES256
        ffrdp->flags |= FLAG_RX_AES256;
        AES_set_decrypt_key((uint8_t*)rxkey, 256, &ffrdp->aes_decrypt_key);
#endif
    }
    pthread_mutex_init(&ffrdp->lock, NULL);
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
    if (ffrdp->cur_new_node) free(ffrdp->cur_new_node);
    list_free(&ffrdp->send_list_head, &ffrdp->send_list_tail);
    list_free(&ffrdp->recv_list_head, &ffrdp->recv_list_tail);
    pthread_mutex_destroy(&ffrdp->lock);
    free(ffrdp);
#ifdef WIN32
    WSACleanup();
    timeEndPeriod(1);
#endif
}

int ffrdp_send(void *ctxt, char *buf, int len)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    int           n = len, size;
    if (  !ffrdp || ((ffrdp->flags & FLAG_SERVER) && (ffrdp->flags & FLAG_CONNECTED) == 0)
        || ((len + ffrdp->smss - 1) / ffrdp->smss + ffrdp->wait_snd > FFRDP_MAX_WAITSND)) {
        if (ffrdp) ffrdp->counter_send_failed++;
        return -1;
    }
    pthread_mutex_lock(&ffrdp->lock);
    while (n > 0) {
        if (!ffrdp->cur_new_node) ffrdp->cur_new_node = frame_node_new(ffrdp->fec_txredundancy, ffrdp->smss);
        if (!ffrdp->cur_new_node) break;
        else SET_FRAME_SEQ(ffrdp->cur_new_node, ffrdp->send_seq);
        size = MIN(n, (int)(ffrdp->smss - ffrdp->cur_new_size));
        memcpy(ffrdp->cur_new_node->data + 4 + ffrdp->cur_new_size, buf, size);
        ffrdp->cur_new_size += size; buf += size; n -= size;
        if (ffrdp->cur_new_size == ffrdp->smss) {
#ifdef CONFIG_ENABLE_AES256
            if ((ffrdp->flags & FLAG_TX_AES256)) frame_node_encrypt(ffrdp->cur_new_node, &ffrdp->aes_encrypt_key, AES_ENCRYPT);
#endif
            list_enqueue(&ffrdp->send_list_head, &ffrdp->send_list_tail, ffrdp->cur_new_node);
            ffrdp->send_seq++; ffrdp->wait_snd++;
            ffrdp->cur_new_node = NULL;
            ffrdp->cur_new_size = 0;
        } else ffrdp->cur_new_tick = get_tick_count();
    }
    pthread_mutex_unlock(&ffrdp->lock);
    return len - n;
}

int ffrdp_recv(void *ctxt, char *buf, int len)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    int           ret;
    if (!ctxt) return -1;
    pthread_mutex_lock(&ffrdp->lock);
    ret = MIN(len, ffrdp->recv_size);
    if (ret > 0) {
        ffrdp->recv_head = ringbuf_read(ffrdp->recv_buff, sizeof(ffrdp->recv_buff), ffrdp->recv_head, (uint8_t*)buf, ret);
        ffrdp->recv_size-= ret; ffrdp->counter_recv_bytes += ret;
    }
    pthread_mutex_unlock(&ffrdp->lock);
    return ret;
}

int ffrdp_isdead(void *ctxt)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    if (!ctxt) return -1;
    if (!ffrdp->send_list_head) return 0;
    if (ffrdp->send_list_head->flags & FLAG_FIRST_SEND) {
        return (int32_t)get_tick_count() - (int32_t)ffrdp->send_list_head->tick_1sts > FFRDP_DEAD_TIMEOUT;
    } else {
        return (int32_t)ffrdp->tick_send_query - (int32_t)ffrdp->tick_recv_ack > FFRDP_DEAD_TIMEOUT || ffrdp->counter_udpsenderr > DEADLINK_SENDERR_THRESHOLD;
    }
}

static void ffrdp_recvdata_and_sendack(FFRDPCONTEXT *ffrdp, struct sockaddr_in *dstaddr)
{
    FFRDP_FRAME_NODE *p;
    int32_t dist, recv_mack, recv_wnd, size, i;
    uint8_t data[8];
    pthread_mutex_lock(&ffrdp->lock);
    while (ffrdp->recv_list_head) {
        dist = seq_distance(GET_FRAME_SEQ(ffrdp->recv_list_head), ffrdp->recv_seq);
        if (dist == 0 && (size = frame_payload_size(ffrdp->recv_list_head)) <= (int)(sizeof(ffrdp->recv_buff) - ffrdp->recv_size)) {
#ifdef CONFIG_ENABLE_AES256
            if ((ffrdp->flags & FLAG_RX_AES256)) frame_node_encrypt(ffrdp->recv_list_head, &ffrdp->aes_decrypt_key, AES_DECRYPT);
#endif
            ffrdp->recv_tail = ringbuf_write(ffrdp->recv_buff, sizeof(ffrdp->recv_buff), ffrdp->recv_tail, ffrdp->recv_list_head->data + 4, size);
            ffrdp->recv_size+= size;
            ffrdp->recv_seq++; ffrdp->recv_seq &= 0xFFFFFF;
            list_remove(&ffrdp->recv_list_head, &ffrdp->recv_list_tail, ffrdp->recv_list_head);
        } else break;
    }
    for (recv_mack=0,i=0,p=ffrdp->recv_list_head; i<=24&&p; i++,p=p->next) {
        dist = seq_distance(GET_FRAME_SEQ(p), ffrdp->recv_seq);
        if (dist <= 24) recv_mack |= 1 << (dist - 1); // dist is obviously > 0
    }
    recv_wnd = (sizeof(ffrdp->recv_buff) - ffrdp->recv_size) / ffrdp->rmss;
    recv_wnd = MIN(recv_wnd, 255);
    *(uint32_t*)(data + 0) = (FFRDP_FRAME_TYPE_ACK << 0) | (ffrdp->recv_seq << 8);
    *(uint32_t*)(data + 4) = (recv_mack <<  0);
    *(uint32_t*)(data + 4)|= (recv_wnd  << 24);
    pthread_mutex_unlock(&ffrdp->lock);
    sendto(ffrdp->udp_fd, data, sizeof(data), 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in)); // send ack frame
}

enum { CEVENT_ACK_OK, CEVENT_ACK_TIMEOUT, CEVENT_FAST_RESEND, CEVENT_SEND_FAILED };
static void ffrdp_congestion_control(FFRDPCONTEXT *ffrdp, int event)
{
    switch (event) {
    case CEVENT_ACK_OK:
        if (ffrdp->cwnd < ffrdp->ssthresh) ffrdp->cwnd *= 2;
        else ffrdp->cwnd++;
        ffrdp->cwnd = MIN(ffrdp->cwnd, FFRDP_MAX_CWND_SIZE);
        ffrdp->cwnd = MAX(ffrdp->cwnd, FFRDP_MIN_CWND_SIZE);
        break;
    case CEVENT_ACK_TIMEOUT:
    case CEVENT_SEND_FAILED:
        ffrdp->ssthresh = MAX(ffrdp->cwnd / 2, FFRDP_MIN_CWND_SIZE);
        ffrdp->cwnd     = FFRDP_MIN_CWND_SIZE;
        break;
    case CEVENT_FAST_RESEND:
        ffrdp->ssthresh = MAX(ffrdp->cwnd / 2, FFRDP_MIN_CWND_SIZE);
        ffrdp->cwnd     = ffrdp->ssthresh;
        break;
    }
}

void ffrdp_update(void *ctxt)
{
    FFRDPCONTEXT       *ffrdp   = (FFRDPCONTEXT*)ctxt;
    FFRDP_FRAME_NODE   *node    = NULL, *p = NULL, *t = NULL;
    struct sockaddr_in *dstaddr = NULL, srcaddr;
    socklen_t addrlen = sizeof(srcaddr);
    int32_t   una, mack, ret, got_data = 0, got_query = 0, send_una, send_mack = 0, recv_una, dist, maxack, i;
    uint8_t   data[8];

    if (!ctxt) return;
    if (!(ffrdp->flags & FLAG_SERVER) && !(ffrdp->flags & FLAG_CONNECTED)) dstaddr = &ffrdp->server_addr;
    send_una = ffrdp->send_list_head ? GET_FRAME_SEQ(ffrdp->send_list_head) : 0;
    recv_una = ffrdp->recv_seq;

    pthread_mutex_lock(&ffrdp->lock);
    if (ffrdp->cur_new_node && ((int32_t)get_tick_count() - (int32_t)ffrdp->cur_new_tick > FFRDP_FLUSH_TIMEOUT || ffrdp->flags & FLAG_FLUSH)) {
        ffrdp->cur_new_node->data[0] = FFRDP_FRAME_TYPE_SHORT;
        ffrdp->cur_new_node->size    = 4 + ffrdp->cur_new_size;
        list_enqueue(&ffrdp->send_list_head, &ffrdp->send_list_tail, ffrdp->cur_new_node);
        ffrdp->send_seq++; ffrdp->wait_snd++;
        ffrdp->cur_new_node = NULL;
        ffrdp->cur_new_size = 0;
    }
    pthread_mutex_unlock(&ffrdp->lock);

    for (i=0,p=ffrdp->send_list_head; i<(int32_t)ffrdp->cwnd&&p; i++,p=p->next) {
        if (!(p->flags & FLAG_FIRST_SEND)) { // first send
            if (ffrdp->swnd > 0) {
                if (ffrdp_send_data_frame(ffrdp, p, dstaddr) != 0) { ffrdp_congestion_control(ffrdp, CEVENT_SEND_FAILED); break; }
                p->tick_1sts = p->tick_send = get_tick_count();
                p->tick_timeout = p->tick_send + ffrdp->rto;
                p->flags       |= FLAG_FIRST_SEND;
                ffrdp->swnd--; ffrdp->counter_send_1sttime++;
            } else if (ffrdp->tick_send_query == 0 || (int32_t)get_tick_count() - (int32_t)ffrdp->tick_send_query > FFRDP_QUERY_CYCLE) { // query remote receive window size
                data[0] = FFRDP_FRAME_TYPE_QUERY; sendto(ffrdp->udp_fd, data, 1, 0, (struct sockaddr*)dstaddr, sizeof(struct sockaddr_in));
                ffrdp->tick_send_query = get_tick_count(); ffrdp->counter_send_query++;
                break;
            }
        } else if ((p->flags & FLAG_FIRST_SEND) && ((int32_t)get_tick_count() - (int32_t)p->tick_timeout > 0 || (p->flags & FLAG_FAST_RESEND))) { // resend
            ffrdp_congestion_control(ffrdp, CEVENT_ACK_TIMEOUT);
            if (ffrdp_send_data_frame(ffrdp, p, dstaddr) != 0) break;
            if (!(p->flags & FLAG_FAST_RESEND)) {
                if (ffrdp->rto == FFRDP_MAX_RTO) {
                    p->tick_send = get_tick_count();
                    p->flags    &=~FLAG_TIMEOUT_RESEND;
                    ffrdp->counter_reach_maxrto++;
                } else p->flags |= FLAG_TIMEOUT_RESEND;
                ffrdp->rto += ffrdp->rto / 2;
                ffrdp->rto  = MIN(ffrdp->rto, FFRDP_MAX_RTO);
                ffrdp->counter_resend_rto++;
            } else {
                p->flags &= ~(FLAG_FAST_RESEND|FLAG_TIMEOUT_RESEND);
                ffrdp->counter_resend_fast++;
            }
            p->tick_timeout+= ffrdp->rto;
        }
    }

    if (ffrdp_sleep(ffrdp, FFRDP_SELECT_SLEEP) != 0) return;
    for (node=NULL;;) { // receive data
        if (!node && !(node = frame_node_new(FFRDP_FRAME_TYPE_FEC2, FFRDP_MAX_MSS))) break;;
        if ((ffrdp->flags & FLAG_CONNECTED) == 0) {
            if ((ret = recvfrom(ffrdp->udp_fd, node->data, node->size, 0, (struct sockaddr*)&srcaddr, &addrlen)) <= 0) break;
            connect(ffrdp->udp_fd, (struct sockaddr*)&srcaddr, addrlen); ffrdp->flags |= FLAG_CONNECTED;
        } else if ((ret = recv(ffrdp->udp_fd, node->data, node->size, 0)) <= 0) break;

        if (node->data[0] <= FFRDP_FRAME_TYPE_FEC32) { // data frame
            node->size = ret; // frame size is the return size of recv
            if (ffrdp_recv_data_frame(ffrdp, node) == 0) {
                dist = seq_distance(GET_FRAME_SEQ(node), recv_una);
                if (dist == 0) { recv_una++; }
                if (dist >= 0) { list_enqueue(&ffrdp->recv_list_head, &ffrdp->recv_list_tail, node); node = NULL; }
                got_data = 1;
            }
        } else if (node->data[0] == FFRDP_FRAME_TYPE_ACK ) {
            una  = *(uint32_t*)(node->data + 0) >> 8;
            mack = *(uint32_t*)(node->data + 4) & 0xFFFFFF;
            dist = seq_distance(una, send_una);
            if (dist >= 0) {
                send_una    = una;
                send_mack   = (send_mack >> dist) | mack;
                ffrdp->swnd = node->data[7]; ffrdp->tick_recv_ack = get_tick_count();
            }
        } else if (node->data[0] == FFRDP_FRAME_TYPE_QUERY) got_query = 1;
    }
    if (node) free(node);

    if (got_data || got_query) ffrdp_recvdata_and_sendack(ffrdp, dstaddr); // send ack frame
    if (ffrdp->send_list_head && seq_distance(send_una, GET_FRAME_SEQ(ffrdp->send_list_head)) > 0) { // got ack frame
        for (p=ffrdp->send_list_head; p;) {
            dist = seq_distance(GET_FRAME_SEQ(p), send_una);
            for (i=23; i>=0 && !(send_mack&(1<<i)); i--);
            if (i < 0) maxack = (send_una - 1) & 0xFFFFFF;
            else maxack = (send_una + i + 1) & 0xFFFFFF;

            if (dist > 24 || !(p->flags & FLAG_FIRST_SEND)) break;
            else if (dist < 0 || (dist > 0 && (send_mack & (1 << (dist-1))))) { // this frame got ack
                ffrdp->counter_send_bytes += frame_payload_size(p); ffrdp->wait_snd--;
                ffrdp_congestion_control(ffrdp, CEVENT_ACK_OK);
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
                pthread_mutex_lock(&ffrdp->lock);
                t = p; p = p->next; list_remove(&ffrdp->send_list_head, &ffrdp->send_list_tail, t);
                pthread_mutex_unlock(&ffrdp->lock);
                continue;
            } else if (seq_distance(maxack, GET_FRAME_SEQ(p)) > 0) {
                ffrdp_congestion_control(ffrdp, CEVENT_FAST_RESEND);
                p->flags |= FLAG_FAST_RESEND;
            }
            p = p->next;
        }
    }
}

void ffrdp_flush(void *ctxt)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt;
    if (ffrdp) ffrdp->flags |= FLAG_FLUSH;
}

void ffrdp_dump(void *ctxt, int clearhistory)
{
    FFRDPCONTEXT *ffrdp = (FFRDPCONTEXT*)ctxt; int secs;
    if (!ctxt) return;
    secs = ((int32_t)get_tick_count() - (int32_t)ffrdp->tick_ffrdp_dump) / 1000;
    secs = secs ? secs : 1;
    av_log(NULL, AV_LOG_WARNING, "rttm: %u, rtts: %u, rttd: %u, rto: %u\n", ffrdp->rttm, ffrdp->rtts, ffrdp->rttd, ffrdp->rto);
    av_log(NULL, AV_LOG_WARNING, "total_send, total_recv: %.2fMB, %.2fMB\n"    , ffrdp->counter_send_bytes / (1024.0 * 1024), ffrdp->counter_recv_bytes / (1024.0 * 1024));
    av_log(NULL, AV_LOG_WARNING, "averg_send, averg_recv: %.2fKB/s, %.2fKB/s\n", ffrdp->counter_send_bytes / (1024.0 * secs), ffrdp->counter_recv_bytes / (1024.0 * secs));
    av_log(NULL, AV_LOG_WARNING, "recv_size           : %d\n"  , ffrdp->recv_size           );
    av_log(NULL, AV_LOG_WARNING, "flags               : %x\n"  , ffrdp->flags               );
    av_log(NULL, AV_LOG_WARNING, "send_seq            : %u\n"  , ffrdp->send_seq            );
    av_log(NULL, AV_LOG_WARNING, "recv_seq            : %u\n"  , ffrdp->recv_seq            );
    av_log(NULL, AV_LOG_WARNING, "wait_snd            : %u\n"  , ffrdp->wait_snd            );
    av_log(NULL, AV_LOG_WARNING, "rmss, smss          : %u, %u\n"    , ffrdp->rmss, ffrdp->smss);
    av_log(NULL, AV_LOG_WARNING, "swnd, cwnd, ssthresh: %u, %u, %u\n", ffrdp->swnd, ffrdp->cwnd, ffrdp->ssthresh);
    av_log(NULL, AV_LOG_WARNING, "fec_txredundancy    : %d\n"  , ffrdp->fec_txredundancy    );
    av_log(NULL, AV_LOG_WARNING, "fec_rxredundancy    : %d\n"  , ffrdp->fec_rxredundancy    );
    av_log(NULL, AV_LOG_WARNING, "fec_txseq           : %d\n"  , ffrdp->fec_txseq           );
    av_log(NULL, AV_LOG_WARNING, "fec_rxseq           : %d\n"  , ffrdp->fec_rxseq           );
    av_log(NULL, AV_LOG_WARNING, "fec_rxmask          : %08x\n", ffrdp->fec_rxmask          );
    av_log(NULL, AV_LOG_WARNING, "counter_send_1sttime: %u\n"  , ffrdp->counter_send_1sttime);
    av_log(NULL, AV_LOG_WARNING, "counter_send_failed : %u\n"  , ffrdp->counter_send_failed );
    av_log(NULL, AV_LOG_WARNING, "counter_send_query  : %u\n"  , ffrdp->counter_send_query  );
    av_log(NULL, AV_LOG_WARNING, "counter_resend_rto  : %u\n"  , ffrdp->counter_resend_rto  );
    av_log(NULL, AV_LOG_WARNING, "counter_resend_fast : %u\n"  , ffrdp->counter_resend_fast );
    av_log(NULL, AV_LOG_WARNING, "counter_resend_ratio: %.2f%%\n", 100.0 * (ffrdp->counter_resend_rto + ffrdp->counter_resend_fast) / MAX(ffrdp->counter_send_1sttime, 1));
    av_log(NULL, AV_LOG_WARNING, "counter_reach_maxrto: %u\n"  , ffrdp->counter_reach_maxrto);
    av_log(NULL, AV_LOG_WARNING, "counter_txfull      : %u\n"  , ffrdp->counter_txfull      );
    av_log(NULL, AV_LOG_WARNING, "counter_txshort     : %u\n"  , ffrdp->counter_txshort     );
    av_log(NULL, AV_LOG_WARNING, "counter_rxfull      : %u\n"  , ffrdp->counter_rxfull      );
    av_log(NULL, AV_LOG_WARNING, "counter_rxshort     : %u\n"  , ffrdp->counter_rxshort     );
    av_log(NULL, AV_LOG_WARNING, "counter_fec_tx      : %u\n"  , ffrdp->counter_fec_tx      );
    av_log(NULL, AV_LOG_WARNING, "counter_fec_rx      : %u\n"  , ffrdp->counter_fec_rx      );
    av_log(NULL, AV_LOG_WARNING, "counter_fec_ok      : %u\n"  , ffrdp->counter_fec_ok      );
    av_log(NULL, AV_LOG_WARNING, "counter_fec_failed  : %u\n\n", ffrdp->counter_fec_failed  );
    if (secs > 1 && clearhistory) {
        ffrdp->tick_ffrdp_dump = get_tick_count();
        memset(&ffrdp->counter_send_bytes, 0, (uint8_t*)&ffrdp->reserved - (uint8_t*)&ffrdp->counter_send_bytes);
    }
}

