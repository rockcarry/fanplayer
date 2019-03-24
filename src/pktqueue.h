#ifndef __FANPLAYER_PKTQUEUE_H__
#define __FANPLAYER_PKTQUEUE_H__

// 包含头文件
#include "ffplayer.h"

#ifdef __cplusplus
extern "C" {
#endif

// avformat.h
#include "libavformat/avformat.h"

// 函数声明
void* pktqueue_create (int size, CMNVARS *cmnvars); // important!! size must be power of 2
void  pktqueue_destroy(void *ctxt);
void  pktqueue_reset  (void *ctxt);

AVPacket* pktqueue_request_packet(void *ctxt); // request a packet
void      pktqueue_release_packet(void *ctxt, AVPacket *pkt); // release a packet

void      pktqueue_audio_enqueue(void *ctxt, AVPacket *pkt); // enqueue a packet to audio-queue
AVPacket* pktqueue_audio_dequeue(void *ctxt); // dequeue a audio packet from audio-queue

void      pktqueue_video_enqueue(void *ctxt, AVPacket *pkt);  // enqueue a packet to video-queue
AVPacket* pktqueue_video_dequeue(void *ctxt); // dequeue a audio packet from video-queue

#ifdef __cplusplus
}
#endif

#endif





