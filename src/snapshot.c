// 包含头文件
#include <string.h>
#include "stdefine.h"
#include "snapshot.h"

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

// 函数实现
int take_snapshot(char *file, int w, int h, AVFrame *video)
{
    char              *fileext    = NULL;
    int                codecid    = AV_CODEC_ID_NONE;
    struct SwsContext *sws_ctx    = NULL;
    int                swsofmt    = AV_PIX_FMT_NONE;
    AVFrame            picture    = {{0}};
    int                ret        = -1;

    AVFormatContext   *fmt_ctxt   = NULL;
    AVOutputFormat    *out_fmt    = NULL;
    AVStream          *stream     = NULL;
    AVCodecContext    *codec_ctxt = NULL;
    AVCodec           *codec      = NULL;
    AVPacket           packet     = {0};
    int                retry      =  8;
    int                got        =  0;

    // init ffmpeg
    av_register_all();

    fileext = file + strlen(file) - 3;
    if (strcasecmp(fileext, "png") == 0) {
        codecid = AV_CODEC_ID_APNG;
        swsofmt = AV_PIX_FMT_RGB24;
    } else {
        codecid = AV_CODEC_ID_MJPEG;
        swsofmt = AV_PIX_FMT_YUVJ420P;
    }

    // alloc picture
    picture.format = swsofmt;
    picture.width  = w > 0 ? w : video->width;
    picture.height = h > 0 ? h : video->height;
    if (av_frame_get_buffer(&picture, 32) < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate picture !\n");
        goto done;
    }

    // scale picture
    sws_ctx = sws_getContext(video->width, video->height, video->format,
        picture.width, picture.height, swsofmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        av_log(NULL, AV_LOG_ERROR, "could not initialize the conversion context jpg\n");
        goto done;
    }
    sws_scale(sws_ctx, (const uint8_t**)video->data, video->linesize, 0, video->height, picture.data, picture.linesize);

    // do encoding
    fmt_ctxt = avformat_alloc_context();
    out_fmt  = av_guess_format(codecid == AV_CODEC_ID_APNG ? "apng" : "mjpeg", NULL, NULL);
    fmt_ctxt->oformat = out_fmt;
    if (!out_fmt) {
        av_log(NULL, AV_LOG_ERROR, "failed to guess format !\n");
        goto done;
    }

    if (avio_open(&fmt_ctxt->pb, file, AVIO_FLAG_READ_WRITE) < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to open output file: %s !\n", file);
        goto done;
    }

    stream = avformat_new_stream(fmt_ctxt, 0);
    if (!stream) {
        av_log(NULL, AV_LOG_ERROR, "failed to create a new stream !\n");
        goto done;
    }

    codec_ctxt                = stream->codec;
    codec_ctxt->codec_id      = out_fmt->video_codec;
    codec_ctxt->codec_type    = AVMEDIA_TYPE_VIDEO;
    codec_ctxt->pix_fmt       = swsofmt;
    codec_ctxt->width         = picture.width;
    codec_ctxt->height        = picture.height;
    codec_ctxt->time_base.num = 1;
    codec_ctxt->time_base.den = 25;

    codec = avcodec_find_encoder(codec_ctxt->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "failed to find encoder !\n");
        goto done;
    }

    if (avcodec_open2(codec_ctxt, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to open encoder !\n");
        goto done;
    }

    while (retry-- && !got) {
        if (avcodec_encode_video2(codec_ctxt, &packet, &picture, &got) < 0) {
            av_log(NULL, AV_LOG_ERROR, "failed to do picture encoding !\n");
            goto done;
        }

        if (got) {
            ret = avformat_write_header(fmt_ctxt, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "error occurred when opening output file !\n");
                goto done;
            }
            av_write_frame(fmt_ctxt, &packet);
            av_write_trailer(fmt_ctxt);
        }
    }

    // ok
    ret = 0;

done:
    avcodec_close(codec_ctxt);
    avio_close(fmt_ctxt->pb);
    avformat_free_context(fmt_ctxt);
    av_packet_unref(&packet);
    av_frame_unref(&picture);
    sws_freeContext(sws_ctx);

    return ret;
}

