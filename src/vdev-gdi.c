// 包含头文件
#include <tchar.h>
#include "vdev.h"
#include "libavformat/avformat.h"

// 内部常量定义
#define DEF_VDEV_BUF_NUM  3

// 内部类型定义
typedef struct {
    // common members
    VDEV_COMMON_MEMBERS
    VDEV_WIN32__MEMBERS
    HDC      hdcsrc;
    HDC      hdcdst;
    HBITMAP *hbitmaps;
    BYTE   **pbmpbufs;
    int      nclear;
} VDEVGDICTXT;

// 内部函数实现
static void* video_render_thread_proc(void *param)
{
    VDEVGDICTXT  *c = (VDEVGDICTXT*)param;

    while (!(c->status & VDEV_CLOSE)) {
        pthread_mutex_lock(&c->mutex);
        while (c->size <= 0 && (c->status & VDEV_CLOSE) == 0) pthread_cond_wait(&c->cond, &c->mutex);
        if (c->size > 0) {
            c->size--;
            if (c->ppts[c->head] != -1) {
                SelectObject(c->hdcsrc, c->hbitmaps[c->head]);
                vdev_win32_render_bboxes (c, c->hdcsrc, c->bbox_list);
                vdev_win32_render_overlay(c, c->hdcsrc, 1);
                BitBlt(c->hdcdst, c->rrect.left, c->rrect.top, c->rrect.right - c->rrect.left, c->rrect.bottom - c->rrect.top, c->hdcsrc, 0, 0, SRCCOPY);
                c->cmnvars->vpts = c->ppts[c->head];
                av_log(NULL, AV_LOG_INFO, "vpts: %lld\n", c->cmnvars->vpts);
            }
            if (++c->head == c->bufnum) c->head = 0;
            pthread_cond_signal(&c->cond);
        }
        pthread_mutex_unlock(&c->mutex);

        // handle av-sync & frame rate & complete
        vdev_avsync_and_complete(c);
    }

    return NULL;
}

static void vdev_gdi_lock(void *ctxt, uint8_t *buffer[8], int linesize[8], int64_t pts)
{
    VDEVGDICTXT *c       = (VDEVGDICTXT*)ctxt;
    int          bmpw    =  0;
    int          bmph    =  0;
    BITMAPINFO   bmpinfo = {0};
    BITMAP       bitmap;

    pthread_mutex_lock(&c->mutex);
    while (c->size >= c->bufnum && (c->status & VDEV_CLOSE) == 0) pthread_cond_wait(&c->cond, &c->mutex);
    if (c->size < c->bufnum) {
        c->ppts[c->tail] = pts;
        if (c->hbitmaps[c->tail]) {
            GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
            bmpw = bitmap.bmWidth ;
            bmph = bitmap.bmHeight;
        }

        if (bmpw != c->rrect.right - c->rrect.left || bmph != c->rrect.bottom - c->rrect.top) {
            if (c->hbitmaps[c->tail]) DeleteObject(c->hbitmaps[c->tail]);
            bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
            bmpinfo.bmiHeader.biWidth       =  (c->rrect.right  - c->rrect.left);
            bmpinfo.bmiHeader.biHeight      = -(c->rrect.bottom - c->rrect.top );
            bmpinfo.bmiHeader.biPlanes      =  1;
            bmpinfo.bmiHeader.biBitCount    =  32;
            bmpinfo.bmiHeader.biCompression =  BI_RGB;
            c->hbitmaps[c->tail] = CreateDIBSection(c->hdcsrc, &bmpinfo, DIB_RGB_COLORS, (void**)&c->pbmpbufs[c->tail], NULL, 0);
            GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
        } else if (c->status & VDEV_CLEAR) {
            if (c->nclear++ != c->bufnum) {
                memset(c->pbmpbufs[c->tail], 0, bitmap.bmWidthBytes * bitmap.bmHeight);
            } else {
                c->nclear  = 0;
                c->status &= ~VDEV_CLEAR;
            }
        }

        if (buffer  ) buffer  [0] = c->pbmpbufs[c->tail] + c->vrect.top * bitmap.bmWidthBytes + c->vrect.left * sizeof(uint32_t);
        if (linesize) linesize[0] = bitmap.bmWidthBytes;
        if (linesize) linesize[6] = c->vrect.right - c->vrect.left;
        if (linesize) linesize[7] = c->vrect.bottom - c->vrect.top;
        if (!(linesize[6] & 1)) linesize[6] -= 1; // fix swscale right side white line issue.
    }
}

static void vdev_gdi_unlock(void *ctxt)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;
    if (++c->tail == c->bufnum) c->tail = 0;
    c->size++; pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->mutex);
}

static void vdev_gdi_destroy(void *ctxt)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;
    int          i;

    DeleteDC (c->hdcsrc);
    ReleaseDC((HWND)c->surface, c->hdcdst);
    for (i=0; i<c->bufnum; i++) {
        if (c->hbitmaps[i]) {
            DeleteObject(c->hbitmaps[i]);
        }
    }

    pthread_mutex_destroy(&c->mutex);
    pthread_cond_destroy (&c->cond );

    free(c->ppts    );
    free(c->hbitmaps);
    free(c->pbmpbufs);
    free(c);
}

// 接口函数实现
void* vdev_gdi_create(void *surface, int bufnum)
{
    VDEVGDICTXT *ctxt = (VDEVGDICTXT*)calloc(1, sizeof(VDEVGDICTXT));
    if (!ctxt) return NULL;

    // init mutex & cond
    pthread_mutex_init(&ctxt->mutex, NULL);
    pthread_cond_init (&ctxt->cond , NULL);

    // init vdev context
    bufnum         = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->surface  = surface;
    ctxt->bufnum   = bufnum;
    ctxt->pixfmt   = AV_PIX_FMT_RGB32;
    ctxt->lock     = vdev_gdi_lock;
    ctxt->unlock   = vdev_gdi_unlock;
    ctxt->destroy  = vdev_gdi_destroy;

    // alloc buffer & semaphore
    ctxt->ppts     = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->hbitmaps = (HBITMAP*)calloc(bufnum, sizeof(HBITMAP));
    ctxt->pbmpbufs = (BYTE**  )calloc(bufnum, sizeof(BYTE*  ));

    ctxt->hdcdst = GetDC((HWND)surface);
    ctxt->hdcsrc = CreateCompatibleDC(ctxt->hdcdst);
    if (!ctxt->ppts || !ctxt->hbitmaps || !ctxt->pbmpbufs || !ctxt->mutex || !ctxt->cond || !ctxt->hdcdst || !ctxt->hdcsrc) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for vdev-gdi !\n");
        exit(0);
    }

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}
