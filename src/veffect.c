// 包含头文件
#include <math.h>
#include "adev.h"
#include "veffect.h"
#include "ffplayer.h"

//++ fft ++//
// 内部常量定义
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 内部类型定义
typedef struct {
    int    N;
    float *W;
    float *T;
    int   *order;
} FFT_CONTEXT;

// 内部函数实现
// r = c1 + c2
static _inline void complex_add(float *r, float *c1, float *c2)
{
    r[0] = c1[0] + c2[0];
    r[1] = c1[1] + c2[1];
}

// r = c1 - c2
static _inline void complex_sub(float *r, float *c1, float *c2)
{
    r[0] = c1[0] - c2[0];
    r[1] = c1[1] - c2[1];
}

// r = c1 * c2
static _inline void complex_mul(float *r, float *c1, float *c2)
{
    r[0] = c1[0] * c2[0] - c1[1] * c2[1];
    r[1] = c1[1] * c2[0] + c1[0] * c2[1];
}

static int reverse_bits(int n)
{
    n = ((n & 0xAAAAAAAA) >> 1 ) | ((n & 0x55555555) << 1 );
    n = ((n & 0xCCCCCCCC) >> 2 ) | ((n & 0x33333333) << 2 );
    n = ((n & 0xF0F0F0F0) >> 4 ) | ((n & 0x0F0F0F0F) << 4 );
    n = ((n & 0xFF00FF00) >> 8 ) | ((n & 0x00FF00FF) << 8 );
    n = ((n & 0xFFFF0000) >> 16) | ((n & 0x0000FFFF) << 16);
    return n;
}

static void fft_execute_internal(FFT_CONTEXT *ctxt, float *data, int n, int w)
{
    int i;
    for (i=0; i<n/2; i++) {
        // C = (A + B)
        // D = (A - B) * W
        float  A[2] = { data[(0   + i) * 2 + 0], data[(0   + i) * 2 + 1] };
        float  B[2] = { data[(n/2 + i) * 2 + 0], data[(n/2 + i) * 2 + 1] };
        float *C    = &(data[(0   + i) * 2]);
        float *D    = &(data[(n/2 + i) * 2]);
        float *W    = &(ctxt->W[i*w*2]);
        float  T[2];
        complex_add(C, A, B);
        complex_sub(T, A, B);
        complex_mul(D, T, W);
    }

    n /= 2;
    w *= 2;
    if (n > 1) {
        fft_execute_internal(ctxt, data + 0    , n, w);
        fft_execute_internal(ctxt, data + n * 2, n, w);
    }
}

// 函数实现
static void fft_free(void *c)
{
    FFT_CONTEXT *ctxt = (FFT_CONTEXT*)c;
    if (!ctxt) return;
    if (ctxt->W    ) free(ctxt->W    );
    if (ctxt->T    ) free(ctxt->T    );
    if (ctxt->order) free(ctxt->order);
    free(ctxt);
}

static void *fft_init(int n)
{
    int shift;
    int i;

    FFT_CONTEXT *ctxt = (FFT_CONTEXT*)calloc(1, sizeof(FFT_CONTEXT));
    if (!ctxt) return NULL;

    ctxt->N     = n;
    ctxt->W     = (float*)calloc(n, sizeof(float) * 1);
    ctxt->T     = (float*)calloc(n, sizeof(float) * 2);
    ctxt->order = (int  *)calloc(n, sizeof(int  ) * 1);
    if (!ctxt->W || !ctxt->T || !ctxt->order) {
        fft_free(ctxt);
        return NULL;
    }

    for (i=0; i<ctxt->N/2; i++) {
        ctxt->W[i * 2 + 0] =(float) cos(2 * M_PI * i / ctxt->N);
        ctxt->W[i * 2 + 1] =(float)-sin(2 * M_PI * i / ctxt->N);
    }

    shift = 32 - (int)ceil(log(n)/log(2));
    for (i=0; i<ctxt->N; i++) {
        ctxt->order[i] = (unsigned)reverse_bits(i) >> shift;
    }
    return ctxt;
}

static void fft_execute(void *c, float *in, float *out)
{
    int i;
    FFT_CONTEXT *ctxt = (FFT_CONTEXT*)c;
    memcpy(ctxt->T, in, sizeof(float) * 2 * ctxt->N);
    fft_execute_internal(ctxt, ctxt->T, ctxt->N, 1);
    for (i=0; i<ctxt->N; i++) {
        out[ctxt->order[i] * 2 + 0] = ctxt->T[i * 2 + 0];
        out[ctxt->order[i] * 2 + 1] = ctxt->T[i * 2 + 1];
    }
}
//++ fft ++//


// 内部常量定义
#define MAX_GRID_COLS  64
#define MAX_GRID_ROWS  16

// 内部类型定义
typedef struct {
    HWND     hwnd;
    int      w;
    int      h;
    HDC      hdcdst;
    HDC      hdcsrc;
    HPEN     hpen0;
    HPEN     hpen1;
    HBITMAP  hbmp;
    HBITMAP  hfill;
    BYTE    *pbmp;
    int      stride;
    int      data_len;
    float   *data_buf;
    int      peak_y[MAX_GRID_COLS];
    int      peak_v[MAX_GRID_COLS];
    void    *fft;
} VEFFECT;

// 内部函数实现
static void resize_veffect_ifneeded(VEFFECT *ve, int w, int h)
{
    if (!ve->hbmp || ve->w != w || ve->h != h) {
        //++ re-create bitmap for draw buffer
        BITMAPINFO    bmpinfo = {0};
        BITMAP        bitmap  = {0};
        HBITMAP       hbmp;
        HANDLE        hobj;
        HDC           hdc ;
        TRIVERTEX     vert[2];
        GRADIENT_RECT grect;

        bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
        bmpinfo.bmiHeader.biWidth       =  w;
        bmpinfo.bmiHeader.biHeight      = -h;
        bmpinfo.bmiHeader.biPlanes      =  1;
        bmpinfo.bmiHeader.biBitCount    =  32;
        bmpinfo.bmiHeader.biCompression =  BI_RGB;
        hbmp = CreateDIBSection(ve->hdcsrc, &bmpinfo, DIB_RGB_COLORS, (void**)&ve->pbmp, NULL, 0);
        hobj = SelectObject(ve->hdcsrc, hbmp);
        if (hobj) DeleteObject(hobj);

        GetObject(hbmp, sizeof(BITMAP), &bitmap);
        ve->hbmp   = hbmp;
        ve->w      = w;
        ve->h      = h;
        ve->stride = bitmap.bmWidthBytes;
        //-- re-create bitmap for draw buffer

        //++ re-create bitmap for gradient fill
        if (ve->hfill) DeleteObject(ve->hfill);
        bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
        bmpinfo.bmiHeader.biWidth       =  w / MAX_GRID_COLS;
        bmpinfo.bmiHeader.biHeight      = -h / MAX_GRID_ROWS * MAX_GRID_ROWS;
        bmpinfo.bmiHeader.biPlanes      =  1;
        bmpinfo.bmiHeader.biBitCount    =  32;
        bmpinfo.bmiHeader.biCompression =  BI_RGB;
        ve->hfill = CreateDIBSection(ve->hdcsrc, &bmpinfo, DIB_RGB_COLORS, NULL, NULL, 0);
        hdc       = CreateCompatibleDC(ve->hdcsrc);
        SelectObject(hdc, ve->hfill);
        vert[0].x        = 0;
        vert[0].y        = 0;
        vert[0].Red      = 0xffff;
        vert[0].Green    = 0xdddd;
        vert[0].Blue     = 0x1111;
        vert[0].Alpha    = 0x0000;
        vert[1].x        = bmpinfo.bmiHeader.biWidth;
        vert[1].y        =-bmpinfo.bmiHeader.biHeight;
        vert[1].Red      = 0x8888;
        vert[1].Green    = 0xffff;
        vert[1].Blue     = 0x1111;
        vert[1].Alpha    = 0x0000;
        grect.UpperLeft  = 0;
        grect.LowerRight = 1;
        GradientFill(hdc, vert, 2, &grect, 1, GRADIENT_FILL_RECT_V);
        DeleteDC(hdc);
        //-- re-create bitmap for gradient fill
    }
}

static void draw_waveform(VEFFECT *ve, int x, int y, int w, int h, float divisor, float *sample, int n)
{
    int delta, px, py, i;

    // resize veffect if needed
    resize_veffect_ifneeded(ve, w, h);

    //++ draw visual effect
    delta  = n / w > 1 ? n / w : 1;

    // clear bitmap
    memset(ve->pbmp, 0, ve->stride * h);

    px = 0;
    py = y + h - (int)(h * sample[0] / divisor);
    SelectObject(ve->hdcsrc, ve->hpen0);
    MoveToEx(ve->hdcsrc, px, py, NULL );
    for (i=delta; i<n; i+=delta) {
        px = x + w * i / n;
        py = y + h - (int)(h * sample[i] / divisor);
        LineTo(ve->hdcsrc, px, py);
    }
    BitBlt(ve->hdcdst, x, y, w, h, ve->hdcsrc, 0, 0, SRCCOPY);
    //-- draw visual effect
}

static void draw_spectrum(VEFFECT *ve, int x, int y, int w, int h, float *sample, int n)
{
    int    amplitude;
    int    gridw, gridh;
    int    d = n / MAX_GRID_COLS;
    float *fsrc = sample;
    int    i, j, tx, ty;
    int    sw, sh, sx, sy;
    HDC    hdc;

    // resize veffect if needed
    resize_veffect_ifneeded(ve, w, h);

    // clear bitmap
    memset(ve->pbmp, 0, ve->stride * h);

    // calucate for grid
    gridw = w / MAX_GRID_COLS; if (gridw == 0) gridw = 1;
    gridh = h / MAX_GRID_ROWS; if (gridh == 0) gridh = 1;
    sw = gridw * MAX_GRID_COLS; sx = x + (w - sw) / 2;
    sh = gridh * MAX_GRID_ROWS; sy = y + (h - sh) / 2;

    hdc = CreateCompatibleDC(ve->hdcsrc);
    SelectObject(hdc       , ve->hfill);
    SelectObject(ve->hdcsrc, ve->hpen1);

    for (ty=sy; ty<=sy+sh; ty+=gridh) {
        MoveToEx(ve->hdcsrc, sx, ty, NULL);
        LineTo  (ve->hdcsrc, sx + sw, ty );
    }

    // calculate amplitude
    SelectObject(ve->hdcsrc, ve->hpen0);
    for (i=0; i<MAX_GRID_COLS; i++) {
        amplitude = 0;
        for (j=0; j<d; j++) {
            amplitude += (int)(*fsrc++ * sh/0x100000);
        }
        amplitude /= d;
        if (amplitude > sh) amplitude = sh;
        tx = sx + (i + 0) * gridw;
        ty = sy + sh - amplitude;
        BitBlt(ve->hdcsrc, tx, ty, gridw, amplitude,
               hdc, 0, sh - amplitude, SRCCOPY);

        if (ve->peak_y[i] >= ty) {
            ve->peak_y[i] = ty;
            ve->peak_v[i] = 0 ;
        } else {
            ve->peak_v[i] += 1;
            ve->peak_y[i] += ve->peak_v[i];
            if (ve->peak_y[i] > sy + sh) ve->peak_y[i] = sy + sh;
        }
        MoveToEx(ve->hdcsrc, tx, ve->peak_y[i], NULL);
        LineTo  (ve->hdcsrc, tx+gridw, ve->peak_y[i]);
    }
    SelectObject(ve->hdcsrc, ve->hpen1);

    for (tx=sx; tx<=sx+sw; tx+=gridw) {
        MoveToEx(ve->hdcsrc, tx, sy, NULL);
        LineTo  (ve->hdcsrc, tx, sy + sh );
    }

    BitBlt(ve->hdcdst, x, y, w, h, ve->hdcsrc, 0, 0, SRCCOPY);
    DeleteDC(hdc);
}

// 函数实现
void* veffect_create(void *surface)
{
    VEFFECT *ve = (VEFFECT*)calloc(1, sizeof(VEFFECT));
    int      i;
    if (!ve) return NULL;

    ve->hwnd   = (HWND) surface;
    ve->hdcdst = GetDC((HWND)surface);
    ve->hdcsrc = CreateCompatibleDC(ve->hdcdst);
    ve->hpen0  = CreatePen(PS_SOLID, 1, RGB(0 , 255, 0 ));
    ve->hpen1  = CreatePen(PS_SOLID, 1, RGB(32, 32 , 64));
    for (i=0; i<MAX_GRID_COLS; i++) {
        ve->peak_y[i] = 0x7fffffff;
    }
    return ve;
}

void veffect_destroy(void *ctxt)
{
    VEFFECT *ve = (VEFFECT*)ctxt;

    // for dc & object
    ReleaseDC(ve->hwnd, ve->hdcdst);
    DeleteDC (ve->hdcsrc);
    DeleteObject(ve->hpen0);
    DeleteObject(ve->hpen1);
    DeleteObject(ve->hfill);
    DeleteObject(ve->hbmp );

    // free fft
    fft_free(ve->fft);

    // free data_buf
    free(ve->data_buf);

    // free
    free(ve);
}

void veffect_render(void *ctxt, int x, int y, int w, int h, int type, void *adev)
{
    VEFFECT *ve  = (VEFFECT*)ctxt;
    void    *buf = ((ADEV_COMMON_CTXT*)adev)->bufcur;
    int      len = ((ADEV_COMMON_CTXT*)adev)->buflen;

    if (!ve->data_buf) {
        ve->data_len = 1 << (int)(log(len/4.0)/log(2.0));
        ve->data_buf = (float*)calloc(ve->data_len, sizeof(float) * 2);
        ve->fft      = fft_init(ve->data_len);
    }

    switch (type) {
    case VISUAL_EFFECT_DISABLE:
        {
            RECT rect = { x, y, x + w, y + h};
            InvalidateRect(ve->hwnd, &rect, TRUE);
        }
        break;
    case VISUAL_EFFECT_WAVEFORM:
        {
            short *ssrc = (short*)buf;
            float *fdst = (float*)ve->data_buf;
            int    snum = len / 4;
            int    i;
            if (!ssrc) break;
            for (i=0; i<snum; i++) {
                *fdst = (float)(((int)ssrc[0] + (int)ssrc[1]) / 2 + 0x7fff);
                fdst += 1;
                ssrc += 2;
            }
            draw_waveform(ve, x, y, w, h, 0x10000, ve->data_buf, snum);
        }
        break;
    case VISUAL_EFFECT_SPECTRUM:
        {
            short *ssrc = (short*)buf;
            float *fsrc = (float*)ve->data_buf;
            float *fdst = (float*)ve->data_buf;
            int    i;
            if (!ssrc) break;
            for (i=0; i<ve->data_len; i++) {
                *fdst++ = (float)((ssrc[0] + ssrc[1]) / 2);
                *fdst++ = 0;
                ssrc   += 2;
            }
            fft_execute(ve->fft, ve->data_buf, ve->data_buf);
            fsrc = fdst = (float*)ve->data_buf;
            for (i=0; i<ve->data_len; i++) {
                *fdst = (float)sqrt(fsrc[0] * fsrc[0] + fsrc[1] * fsrc[1]);
                fdst += 1;
                fsrc += 2;
            }
            draw_spectrum(ve, x, y, w, h, ve->data_buf, (int)(ve->data_len * 16000.0 / ADEV_SAMPLE_RATE));
        }
        break;
    }
}

