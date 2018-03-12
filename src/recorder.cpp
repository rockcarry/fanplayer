// 包含头文件
#include <pthread.h>
#include "recorder.h"

// 内部类型定义
typedef struct
{
    AVFormatContext *ifc;
    AVFormatContext *ofc;
    pthread_mutex_t lock;
} RECORDER;

// 函数实现
void* recorder_init(char *filename, AVFormatContext *ifc)
{
    int    ret;
    unsigned i;

    // check params invalid
    if (!filename || !ifc) return NULL;

    // allocate context for recorder
    RECORDER *recorder = (RECORDER*)calloc(1, sizeof(RECORDER));
    if (!recorder) {
        return NULL;
    }

    // save input avformat context
    recorder->ifc = ifc;

    /* allocate the output media context */
    avformat_alloc_output_context2(&recorder->ofc, NULL, NULL, filename);
    if (!recorder->ofc) {
        printf("could not deduce output format from file extension !\n");
        goto failed;
    }

    for (i=0; i<ifc->nb_streams; i++) {
        AVStream *is = ifc->streams[i];
        AVStream *os = avformat_new_stream(recorder->ofc, is->codec->codec);
        if (!os) {
            printf("failed allocating output stream !\n");
            goto failed;
        }

        ret = avcodec_copy_context(os->codec, is->codec);
        if (ret < 0) {
            printf("failed to copy context from input to output stream codec context !\n");
            goto failed;
        }

        os->codec->codec_tag = 0;
        if (recorder->ofc->oformat->flags & AVFMT_GLOBALHEADER) {
            os->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    /* open the output file, if needed */
    if (!(recorder->ofc->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&recorder->ofc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("could not open '%s' !\n", filename);
            goto failed;
        }
    }

    /* write the stream header, if any. */
    ret = avformat_write_header(recorder->ofc, NULL);
    if (ret < 0) {
        printf("error occurred when opening output file !\n");
        goto failed;
    }

    // init lock
    pthread_mutex_init(&recorder->lock, NULL);

    // successed
    return recorder;

failed:
    recorder_free(recorder);
    return NULL;
}

void recorder_free(void *ctxt)
{
    if (!ctxt) return;
    RECORDER *recorder = (RECORDER*)ctxt;

    // lock
    pthread_mutex_lock(&recorder->lock);

    if (recorder->ofc) {
        // write the trailer, if any.
        av_write_trailer(recorder->ofc);

        // close the output file, if needed
        if (!(recorder->ofc->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&recorder->ofc->pb);
        }
    }

    /* free the stream */
    avformat_free_context(recorder->ofc);

    // free recorder context
    free(recorder);

    // unlock
    pthread_mutex_unlock(&recorder->lock);
}

int recorder_packet(void *ctxt, AVPacket *pkt)
{
    if (!ctxt || !pkt) return -1;
    RECORDER *recorder = (RECORDER*)ctxt;

    // lock
    pthread_mutex_lock(&recorder->lock);

    AVStream *is = recorder->ifc->streams[pkt->stream_index];
    AVStream *os = recorder->ofc->streams[pkt->stream_index];
    AVPacket  packet;

    memset(&packet, 0, sizeof(AVPacket));
    av_packet_ref(&packet, pkt);
    packet.pts = av_rescale_q_rnd(packet.pts, is->time_base, os->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    packet.dts = av_rescale_q_rnd(packet.dts, is->time_base, os->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    packet.duration = av_rescale_q(packet.duration, is->time_base, os->time_base);
    packet.pos = -1;
    av_interleaved_write_frame(recorder->ofc, &packet);
    av_packet_unref(&packet);

    // unlock & destroy
    pthread_mutex_unlock (&recorder->lock);
    pthread_mutex_destroy(&recorder->lock);
    return 0;
}
