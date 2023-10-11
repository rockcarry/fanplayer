#include <pthread.h>
#include "recorder.h"

typedef struct {
    #define FLAG_GOT_KEYFRAME (1 << 0)
    int            flags;
    AVFormatContext *ifc;
    AVFormatContext *ofc;
    int  *stream_mapping;
    pthread_mutex_t lock;
} RECORDER;

void* recorder_init(char *filename, AVFormatContext *ifc)
{
    RECORDER *recorder     = NULL;
    int       stream_index = 0;
    AVStream *is, *os;
    int       ret, i;

    // check params invalid
    if (!filename || !ifc) return NULL;

    // allocate context for recorder
    recorder = (RECORDER*)calloc(1, sizeof(RECORDER));
    if (!recorder) return NULL;

    // save input avformat context
    recorder->ifc = ifc;

    /* allocate the output media context */
    avformat_alloc_output_context2(&recorder->ofc, NULL, NULL, filename);
    if (!recorder->ofc) {
        printf("could not deduce output format from file extension !\n");
        goto failed;
    }

    recorder->stream_mapping = av_mallocz_array(ifc->nb_streams, sizeof(int));
    if (!recorder->stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto failed;
    }

    for (i = 0; i < (int)ifc->nb_streams; i++) {
        is = ifc->streams[i];
        if (is->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            is->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            is->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            recorder->stream_mapping[i] = -1;
            continue;
        }
        recorder->stream_mapping[i] = stream_index++;

        os = avformat_new_stream(recorder->ofc, NULL);
        if (!os) {
            printf("failed allocating output stream !\n");
            goto failed;
        }

        ret = avcodec_parameters_copy(os->codecpar, is->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            goto failed;
        }
        os->codecpar->codec_tag = 0;
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
    RECORDER *recorder = (RECORDER*)ctxt;
    if (!ctxt) return;

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

    // free stream_mapping
    av_freep(&recorder->stream_mapping);

    // free the stream
    avformat_free_context(recorder->ofc);

    // unlock
    pthread_mutex_unlock(&recorder->lock);

    // destroy
    pthread_mutex_destroy(&recorder->lock);

    // free recorder context
    free(recorder);
}

int recorder_packet(void *ctxt, AVPacket *pkt)
{
    RECORDER *recorder = (RECORDER*)ctxt;
    AVPacket  packet   = {0};
    AVStream *is, *os;

    if (!ctxt || !pkt || pkt->stream_index >= (int)recorder->ifc->nb_streams || recorder->stream_mapping[pkt->stream_index] < 0) return -1;
    if (!(recorder->flags & FLAG_GOT_KEYFRAME) && (pkt->flags & AV_PKT_FLAG_KEY)
       && recorder->ifc->streams[pkt->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        recorder->flags |= FLAG_GOT_KEYFRAME;
    }
    if (!(recorder->flags & FLAG_GOT_KEYFRAME)) return -1;

    pthread_mutex_lock(&recorder->lock);
    av_packet_ref(&packet, pkt);
    packet.stream_index = recorder->stream_mapping[pkt->stream_index];
    is = recorder->ifc->streams[  pkt->stream_index];
    os = recorder->ofc->streams[packet.stream_index];
    packet.pts      = av_rescale_q_rnd(packet.pts, is->time_base, os->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    packet.dts      = av_rescale_q_rnd(packet.dts, is->time_base, os->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    packet.duration = av_rescale_q(packet.duration, is->time_base, os->time_base);
    packet.pos      = -1;
    av_interleaved_write_frame(recorder->ofc, &packet);
    av_packet_unref(&packet);
    pthread_mutex_unlock(&recorder->lock);
    return 0;
}
