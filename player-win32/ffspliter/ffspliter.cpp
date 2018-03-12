// 包含头文件
#include <windows.h>
#include <conio.h>
#include "ffspliter.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 内部常量定义
static int g_exit_remux = 0;

// 内部函数实现
static int split_media_file(char *dst, char *src, __int64 start, __int64 end, PFN_SPC spc)
{
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVStream        *istream  = NULL;
    AVStream        *ostream  = NULL;
    AVRational       tbms     = { 1, 1000 };
    AVRational       tbvs     = { 1, 1    };
    int64_t          startpts = -1;
    int64_t          duration = -1;
    int64_t          total    = -1;
    int64_t          current  = -1;
    int              streamidx=  0;
    int              ret      = -1;

    av_register_all();
    avformat_network_init();

    if ((ret = avformat_open_input(&ifmt_ctx, src, 0, 0)) < 0) {
        printf("could not open input file '%s' !", src);
        goto done;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("failed to retrieve input stream information ! \n");
        goto done;
    }

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, dst);
    if (!ofmt_ctx) {
        printf("could not create output context !\n");
        goto done;
    }

    for (unsigned i=0; i<ifmt_ctx->nb_streams; i++) {
        istream = ifmt_ctx->streams[i];
        if (istream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            streamidx = i;
            tbvs      = ifmt_ctx->streams[i]->time_base;
        }

        ostream = avformat_new_stream(ofmt_ctx, istream->codec->codec);
        if (!ostream) {
            printf("failed allocating output stream !\n");
            goto done;
        }

        ret = avcodec_copy_context(ostream->codec, istream->codec);
        if (ret < 0) {
            printf("failed to copy context from input to output stream codec context !\n");
            goto done;
        }

        ostream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            ostream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, dst, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("could not open output file '%s' !", dst);
            goto done;
        }
    }

    // calulate pts
    if (start >= 0) {
        startpts = ifmt_ctx->start_time * 1000 / AV_TIME_BASE;
        duration = ifmt_ctx->duration   * 1000 / AV_TIME_BASE;
        total    = duration - start; if (total < 0) total = 1;
        current  = 0;
        start   += startpts;
        end     += startpts;
        start    = av_rescale_q_rnd(start, tbms, tbvs, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        end      = av_rescale_q_rnd(end  , tbms, tbvs, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

        // seek to start position
        av_seek_frame(ifmt_ctx, streamidx, start, AVSEEK_FLAG_BACKWARD);
    } else {
        startpts = ifmt_ctx->start_time * 1000 / AV_TIME_BASE;
        duration = end;
        total    = end;
        current  = 0;
        start    = startpts;
        end     += startpts;
        start    = av_rescale_q_rnd(start, tbms, tbvs, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        end      = av_rescale_q_rnd(end  , tbms, tbvs, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    }

    // write header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        printf("error occurred when writing output file header !\n");
        goto done;
    }

    while (!g_exit_remux) {
        AVPacket pkt;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
//          fprintf(stderr, "failed to read frame !\n");
            break;
        }

        // get start pts
        if (pkt.stream_index == streamidx) {
            if (pkt.pts > end) {
                g_exit_remux = 1;
                goto next;
            }
            if (spc) {
                current = av_rescale_q_rnd(pkt.pts - start, tbvs, tbms, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                if (current < 0    ) current = 0;
                if (current > total) current = total;
                spc(current, total);
            }
        }

        istream = ifmt_ctx->streams[pkt.stream_index];
        ostream = ofmt_ctx->streams[pkt.stream_index];
        pkt.pts = av_rescale_q_rnd(pkt.pts, istream->time_base, ostream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, istream->time_base, ostream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, istream->time_base, ostream->time_base);
        pkt.pos = -1;

        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            printf("error muxing packet !\n");
            g_exit_remux = 1;
            goto next;
        }

next:
        av_packet_unref(&pkt);
    }

    // write trailer
    av_write_trailer(ofmt_ctx);

done:
    // close input
    avformat_close_input(&ifmt_ctx);

    // close output
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }

    avformat_free_context(ofmt_ctx);
    avformat_network_deinit();

    // done
    printf("\n");
    spc(total, total);
    printf("\ndone.\n");
    return ret;
}

static void split_progress_callback(__int64 cur, __int64 total)
{
    printf("\rsplit progress: %3d%", 100 * cur / total);
}

static BOOL console_ctrl_handler(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
        g_exit_remux = 1;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char *argv[])
{
    char   *input, *output;
    int64_t start,  end;

#if 1
    if (argc < 5) {
        printf(
            "ffspliter: tools for spliter media file\n"
            "usage: ffspliter input start end output\n"
            "input support any file format a        \n"
            "output support only flv and mp4 format \n"
            "start and end time is in ms unit       \n"
        );
        return -1;
    }

    input  = argv[1];
    output = argv[4];
    start  = _atoi64(argv[2]);
    end    = _atoi64(argv[3]);
#else
    input  = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
    output = "c:\\record.mp4";
    start  = -1;
    end    = 60000;
#endif

    // set console ctrl handler
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_ctrl_handler, TRUE);

    // start split media file
    split_media_file(output, input, start, end, split_progress_callback);

    _getch();
    return 0;
}


