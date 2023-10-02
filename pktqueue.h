#ifndef __FANPLAYER_PKTQUEUE_H__
#define __FANPLAYER_PKTQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"

// º¯ÊýÉùÃ÷
void* pktqueue_create (int size); // important!! size must be power of 2
void  pktqueue_destroy(void *ctx);
void  pktqueue_reset  (void *ctx);

AVPacket* pktqueue_request_packet(void *ctx); // request a packet
void      pktqueue_release_packet(void *ctx, AVPacket *pkt); // release a packet

void      pktqueue_audio_enqueue (void *ctx, AVPacket *pkt); // enqueue a packet to audio-queue
AVPacket* pktqueue_audio_dequeue (void *ctx); // dequeue a audio packet from audio-queue

void      pktqueue_video_enqueue (void *ctx, AVPacket *pkt); // enqueue a packet to video-queue
AVPacket* pktqueue_video_dequeue (void *ctx); // dequeue a audio packet from video-queue

#ifdef __cplusplus
}
#endif

#endif
