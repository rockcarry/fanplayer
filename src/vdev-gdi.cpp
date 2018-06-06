// ����ͷ�ļ�
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// �ڲ���������
#define DEF_VDEV_BUF_NUM  3

// �ڲ����Ͷ���
typedef struct {
    // common members
    VDEV_COMMON_MEMBERS

    HDC      hdcsrc;
    HDC      hdcdst;
    HBITMAP *hbitmaps;
    BYTE   **pbmpbufs;
    HFONT    hfont;
} VDEVGDICTXT;

// �ڲ�����ʵ��
static void* video_render_thread_proc(void *param)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)param;

    while (1) {
        sem_wait(&c->semr);
        if (c->status & VDEV_CLOSE) break;

        if (c->status & VDEV_REFRESHBG) {
            c->status &= ~VDEV_REFRESHBG;
            vdev_refresh_background(c);
        }

        if (c->ppts[c->head] != -1) {
            SelectObject(c->hdcsrc, c->hbitmaps[c->head]);
            if (c->textt) {
                SetTextColor(c->hdcsrc, c->textc & 0xffffff);
                TextOutW(c->hdcsrc, c->textx, c->texty, c->textt, (int)wcslen(c->textt));
            }
            BitBlt(c->hdcdst, c->x, c->y, c->w, c->h, c->hdcsrc, 0, 0, SRCCOPY);
            if (c->ppts[c->head] != AV_NOPTS_VALUE) c->vpts = c->ppts[c->head];
        }

        av_log(NULL, AV_LOG_DEBUG, "vpts: %lld\n", c->vpts);
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);

        // handle av-sync & frame rate & complete
        vdev_avsync_and_complete(c);
    }

    return NULL;
}

static void vdev_gdi_lock(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;

    sem_wait(&c->semw);

    BITMAP bitmap;
    int bmpw = 0;
    int bmph = 0;
    if (c->hbitmaps[c->tail]) {
        GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
        bmpw = bitmap.bmWidth ;
        bmph = bitmap.bmHeight;
    }

    if (bmpw != c->w || bmph != c->h) {
        c->sw = c->w; c->sh = c->h;
        if (c->hbitmaps[c->tail]) {
            DeleteObject(c->hbitmaps[c->tail]);
        }

        BITMAPINFO bmpinfo = {0};
        bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
        bmpinfo.bmiHeader.biWidth       =  c->w;
        bmpinfo.bmiHeader.biHeight      = -c->h;
        bmpinfo.bmiHeader.biPlanes      =  1;
        bmpinfo.bmiHeader.biBitCount    =  32;
        bmpinfo.bmiHeader.biCompression =  BI_RGB;
        c->hbitmaps[c->tail] = CreateDIBSection(c->hdcsrc, &bmpinfo, DIB_RGB_COLORS,
                                        (void**)&c->pbmpbufs[c->tail], NULL, 0);
        GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
    }

    if (buffer  ) buffer[0]   = c->pbmpbufs[c->tail];
    if (linesize) linesize[0] = bitmap.bmWidthBytes ;
}

static void vdev_gdi_unlock(void *ctxt, int64_t pts)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

static void vdev_gdi_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->sw = w; c->sh = h;
}

static void vdev_gdi_destroy(void *ctxt)
{
    int i;
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;

    // make visual effect & rendering thread safely exit
    c->status = VDEV_CLOSE;
    sem_post(&c->semr);
    pthread_join(c->thread, NULL);

    //++ for video
    DeleteDC (c->hdcsrc);
    ReleaseDC((HWND)c->surface, c->hdcdst);
    for (i=0; i<c->bufnum; i++) {
        if (c->hbitmaps[i]) {
            DeleteObject(c->hbitmaps[i]);
        }
    }
    //-- for video

    // delete font
    DeleteObject(c->hfont);

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

    // free memory
    free(c->ppts    );
    free(c->hbitmaps);
    free(c->pbmpbufs);
    free(c);
}

// �ӿں���ʵ��
void* vdev_gdi_create(void *surface, int bufnum, int w, int h, int frate)
{
    VDEVGDICTXT *ctxt = (VDEVGDICTXT*)calloc(1, sizeof(VDEVGDICTXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate gdi vdev context !\n");
        exit(0);
    }

    // init vdev context
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->surface   = surface;
    ctxt->bufnum    = bufnum;
    ctxt->pixfmt    = AV_PIX_FMT_RGB32;
    ctxt->w         = w;
    ctxt->h         = h;
    ctxt->sw        = w;
    ctxt->sh        = h;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->apts      = -1;
    ctxt->vpts      = -1;
    ctxt->lock      = vdev_gdi_lock;
    ctxt->unlock    = vdev_gdi_unlock;
    ctxt->setrect   = vdev_gdi_setrect;
    ctxt->destroy   = vdev_gdi_destroy;

    // alloc buffer & semaphore
    ctxt->ppts     = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->hbitmaps = (HBITMAP*)calloc(bufnum, sizeof(HBITMAP));
    ctxt->pbmpbufs = (BYTE**  )calloc(bufnum, sizeof(BYTE*  ));

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    ctxt->hdcdst = GetDC((HWND)ctxt->surface);
    ctxt->hdcsrc = CreateCompatibleDC(ctxt->hdcdst);
    if (!ctxt->ppts || !ctxt->hbitmaps || !ctxt->pbmpbufs || !ctxt->semr || !ctxt->semw || !ctxt->hdcdst || !ctxt->hdcsrc) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for vdev-gdi !\n");
        exit(0);
    }

    LOGFONT logfont;
    memset(&logfont, 0, sizeof(logfont));
    wcscpy(logfont.lfFaceName, TEXT(DEF_FONT_NAME));
    logfont.lfHeight = DEF_FONT_SIZE;
    ctxt->hfont = CreateFontIndirect(&logfont);
    SelectObject(ctxt->hdcsrc, ctxt->hfont);
    SetBkMode(ctxt->hdcsrc, TRANSPARENT);

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}
