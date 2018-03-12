#ifndef __FFPLAYER_PKTQUEUE_H__
#define __FFPLAYER_PKTQUEUE_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// avformat.h
#include "libavformat/avformat.h"

// 函数声明
void* pktqueue_create (int   size); // important!! size must be power of 2
void  pktqueue_destroy(void *ctxt);
void  pktqueue_reset  (void *ctxt);

void      pktqueue_free_enqueue(void *ctxt, AVPacket *pkt); // enqueue a packet to free-queue
AVPacket* pktqueue_free_dequeue(void *ctxt); // dequeue a packet from free-queue
void      pktqueue_free_cancel (void *ctxt, AVPacket *pkt); // cancel packet dequeuing from free-queue

void      pktqueue_audio_enqueue(void *ctxt, AVPacket *pkt); // enqueue a packet from audio-queue
AVPacket* pktqueue_audio_dequeue(void *ctxt); // dequeue a audio packet to audio-queue

void      pktqueue_video_enqueue(void *ctxt, AVPacket *pkt);  // enqueue a packet from video-queue
AVPacket* pktqueue_video_dequeue(void *ctxt); // dequeue a audio packet to video-queue

#ifdef __cplusplus
}
#endif

#endif





