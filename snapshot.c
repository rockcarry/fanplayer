#include <string.h>
#include "snapshot.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

int take_snapshot(char *file, int w, int h, AVFrame *video)
{
    FILE              *fp         = NULL;
    char              *fileext    = NULL;
    int                codecid    = AV_CODEC_ID_NONE;
    struct SwsContext *sws_ctx    = NULL;
    int                swsofmt    = AV_PIX_FMT_NONE;
    AVFrame            picture    = {};
    int                ret        = -1;
    AVCodecContext    *codec_ctxt = NULL;
    AVCodec           *codec      = NULL;
    AVPacket           packet     = {};

    fp = fopen(file, "wb");
    if (!fp) {
        av_log(NULL, AV_LOG_ERROR, "failed to open file: %s !\n", file);
        goto done;
    }

    fileext = file + strlen(file) - 3;
    if (strcasecmp(fileext, "png") == 0) {
        codecid = AV_CODEC_ID_PNG;
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
    sws_ctx = sws_getContext(video->width, video->height, video->format, picture.width, picture.height, swsofmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        av_log(NULL, AV_LOG_ERROR, "could not initialize the conversion context jpg\n");
        goto done;
    }
    sws_scale(sws_ctx, (const uint8_t**)video->data, video->linesize, 0, video->height, picture.data, picture.linesize);

    codec = avcodec_find_encoder(codecid);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "failed to find encoder !\n");
        goto done;
    }
    codec_ctxt                = avcodec_alloc_context3(codec);
    codec_ctxt->codec_id      = codecid;
    codec_ctxt->codec_type    = AVMEDIA_TYPE_VIDEO;
    codec_ctxt->pix_fmt       = swsofmt;
    codec_ctxt->width         = picture.width;
    codec_ctxt->height        = picture.height;
    codec_ctxt->time_base.num = 1;
    codec_ctxt->time_base.den = 25;

    if (avcodec_open2(codec_ctxt, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to open encoder !\n");
        goto done;
    }

    ret = avcodec_send_frame(codec_ctxt, &picture);
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctxt, &packet);
        if (ret == 0) {
            fwrite(packet.data, 1, packet.size, fp);
            av_packet_unref(&packet);
        }
    }

    // ok
    ret = 0;

done:
    av_frame_unref(&picture);
    if (codec_ctxt) avcodec_close(codec_ctxt);
    if (codec_ctxt) avcodec_free_context(&codec_ctxt);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (fp) fclose(fp);
    return ret;
}
