// 包含头文件
#include <tchar.h>
#include <d3d9.h>
#include "vdev.h"
#include "libavformat/avformat.h"

// 预编译开关
#define ENABLE_WAIT_D3D_VSYNC    FALSE
#define ENABLE_D3DMULTISAMPLE_X4 FALSE

// 内部常量定义
#define DEF_VDEV_BUF_NUM       3
#define VDEV_D3D_SET_RECT     (1 << 16)
#define VDEV_D3D_SET_ROTATE   (1 << 17)

// 内部类型定义
typedef LPDIRECT3D9 (WINAPI *PFNDirect3DCreate9)(UINT);

typedef struct {
    // common members
    VDEV_COMMON_MEMBERS

    HMODULE                 hDll ;
    LPDIRECT3D9             pD3D9;
    LPDIRECT3DDEVICE9       pD3DDev;
    LPDIRECT3DSURFACE9     *surfs; // offset screen surfaces
    LPDIRECT3DSURFACE9      surfw; // surface keeps same size as window
    LPDIRECT3DSURFACE9      bkbuf; // back buffer surface
    D3DPRESENT_PARAMETERS   d3dpp;
    D3DFORMAT               d3dfmt;

    LPDIRECT3DTEXTURE9      texture; // texture for rotate
    LPDIRECT3DVERTEXBUFFER9 vertexes;// vertex buffer for rotate
    LPDIRECT3DSURFACE9      surft;   // surface of texture
    LPDIRECT3DSURFACE9      surfr;   // surface for rotate
    int                     rotate;  // rotate angle

    HFONT                   hfont;
} VDEVD3DCTXT;

typedef struct {
    float    x, y, z;
    float    rhw;
    float    tu, tv;
} CUSTOMVERTEX;
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX1)

// 内部函数实现
static void rotate_point(float w, float h, float xi, float yi, float cx, float cy, float radian, float *xo, float *yo)
{
    xi += cx - w / 2;
    yi += cy - h / 2;
    *xo = (xi - cx) * (float)cos(radian) + (yi - cy) * (float)sin(radian) + cx;
    *yo =-(xi - cx) * (float)sin(radian) + (yi - cy) * (float)cos(radian) + cy;
}

static void d3d_reinit_for_rotate(VDEVD3DCTXT *c, int w, int h, int angle, int *ow, int *oh)
{
    float         radian= (float)(-angle * M_PI / 180);
    float         fow   = (float)(fabs(w * cos(radian)) + fabs(h * sin(radian)));
    float         foh   = (float)(fabs(w * sin(radian)) + fabs(h * cos(radian)));
    CUSTOMVERTEX *pv    = NULL;

    if (ow) *ow = (int)fow;
    if (oh) *oh = (int)foh;

    if (c->surfr) IDirect3DSurface9_Release(c->surfr);
    IDirect3DDevice9_CreateRenderTarget(c->pD3DDev,
        (int)fow, (int)foh, c->d3dpp.BackBufferFormat, c->d3dpp.MultiSampleType,
        c->d3dpp.MultiSampleQuality, FALSE, &c->surfr, NULL);

    if (!c->texture) {
        IDirect3DDevice9_CreateTexture(c->pD3DDev, w, h, 1, D3DUSAGE_RENDERTARGET, c->d3dpp.BackBufferFormat, D3DPOOL_DEFAULT, &c->texture , NULL);
        IDirect3DTexture9_GetSurfaceLevel(c->texture, 0, &c->surft);
    }
    if (!c->vertexes) {
        IDirect3DDevice9_CreateVertexBuffer(c->pD3DDev, 4 * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &c->vertexes, NULL);
    }

    if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(c->vertexes, 0, 4 * sizeof(CUSTOMVERTEX), (void**)&pv, 0))) {
        pv[0].rhw = pv[1].rhw = pv[2].rhw = pv[3].rhw = 1.0f;
        pv[0].tu  = 0.0f; pv[0].tv  = 0.0f;
        pv[1].tu  = 1.0f; pv[1].tv  = 0.0f;
        pv[2].tu  = 1.0f; pv[2].tv  = 1.0f;
        pv[3].tu  = 0.0f; pv[3].tv  = 1.0f;
        pv[0].z = pv[1].z = pv[2].z = pv[3].z = 0.0f;
        rotate_point((float)w, (float)h, (float)0, (float)0, fow / 2, foh / 2, radian, &(pv[0].x), &(pv[0].y));
        rotate_point((float)w, (float)h, (float)w, (float)0, fow / 2, foh / 2, radian, &(pv[1].x), &(pv[1].y));
        rotate_point((float)w, (float)h, (float)w, (float)h, fow / 2, foh / 2, radian, &(pv[2].x), &(pv[2].y));
        rotate_point((float)w, (float)h, (float)0, (float)h, fow / 2, foh / 2, radian, &(pv[3].x), &(pv[3].y));
        IDirect3DVertexBuffer9_Unlock(c->vertexes);
    }
}

static void d3d_release_for_rotate(VDEVD3DCTXT *c)
{
    if (c->surft) {
        IDirect3DSurface9_Release(c->surft);
        c->surft = NULL;
    }
    if (c->surfr) {
        IDirect3DSurface9_Release(c->surfr);
        c->surfr = NULL;
    }
    if (c->texture) {
        IDirect3DTexture9_Release(c->texture);
        c->texture = NULL;
    }
    if (c->vertexes) {
        IDirect3DVertexBuffer9_Release(c->vertexes);
        c->vertexes = NULL;
    }
}

static void d3d_draw_surf(VDEVD3DCTXT *c, LPDIRECT3DSURFACE9 surf)
{
    RECT    rect    = { c->x, c->y, c->x + c->w, c->y + c->h };
    LOGFONT logfont = {0};
    HDC     hdc;

    if (c->rotate && (c->status & VDEV_D3D_SET_ROTATE)) {
        d3d_reinit_for_rotate(c, c->sw, c->sh, c->rotate, NULL, NULL);
        if (c->surft && c->surfr) c->status &= ~VDEV_D3D_SET_ROTATE;
    }

    if (c->textt && (c->status & VDEV_D3D_SET_RECT)) {
        if (c->surfw) IDirect3DSurface9_Release(c->surfw);
        IDirect3DDevice9_CreateRenderTarget(c->pD3DDev,
            c->w, c->h, c->d3dpp.BackBufferFormat, c->d3dpp.MultiSampleType,
            c->d3dpp.MultiSampleQuality, TRUE, &c->surfw, NULL);
        if (c->surfw) c->status &= ~VDEV_D3D_SET_RECT;
    }

    if (c->rotate && c->surft && c->surfr) {
        IDirect3DDevice9_StretchRect(c->pD3DDev, surf, NULL, c->surft, NULL, D3DTEXF_LINEAR);
        if (SUCCEEDED(IDirect3DDevice9_BeginScene(c->pD3DDev))) {
            IDirect3DDevice9_SetRenderTarget(c->pD3DDev, 0, c->surfr);
            IDirect3DDevice9_Clear(c->pD3DDev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            IDirect3DDevice9_SetTexture(c->pD3DDev, 0, (IDirect3DBaseTexture9*)c->texture);
            IDirect3DDevice9_SetStreamSource(c->pD3DDev, 0, c->vertexes, 0, sizeof(CUSTOMVERTEX));
            IDirect3DDevice9_SetFVF(c->pD3DDev, D3DFVF_CUSTOMVERTEX);
            IDirect3DDevice9_DrawPrimitive(c->pD3DDev, D3DPT_TRIANGLEFAN, 0, 2);
            IDirect3DDevice9_EndScene(c->pD3DDev);
            surf = c->surfr;
        }
    }

    if (c->textt && c->surfw) {
        IDirect3DDevice9_StretchRect(c->pD3DDev, surf, NULL, c->surfw, NULL, D3DTEXF_LINEAR);

        if (c->status & VDEV_CONFIG_FONT) {
            c->status &= ~VDEV_CONFIG_FONT;
            logfont.lfHeight = c->font_size;
            _tcscpy_s(logfont.lfFaceName, _countof(logfont.lfFaceName), c->font_name);
            if (c->hfont) DeleteObject(c->hfont);
            c->hfont = CreateFontIndirect(&logfont);
        }

        IDirect3DSurface9_GetDC(c->surfw, &hdc);
        SelectObject(hdc, c->hfont);
        SetTextColor(hdc, c->textc);
        SetBkMode   (hdc, TRANSPARENT);
        TextOut(hdc, c->textx, c->texty, c->textt, (int)_tcslen(c->textt));
        IDirect3DSurface9_ReleaseDC(c->surfw, hdc);

        surf = c->surfw;
    }

    IDirect3DDevice9_StretchRect(c->pD3DDev, surf, NULL, c->bkbuf, NULL, D3DTEXF_LINEAR);
    IDirect3DDevice9_Present(c->pD3DDev, NULL, &rect, NULL, NULL);
}

static void* video_render_thread_proc(void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)param;

    while (1) {
        sem_wait(&c->semr);
        if (c->status & VDEV_CLOSE) break;

        if (vdev_refresh_background(c) && c->ppts[c->head] != -1) {
            d3d_draw_surf(c, c->surfs[c->head]);
            c->cmnvars->vpts = c->ppts[c->head];
        }

        av_log(NULL, AV_LOG_DEBUG, "vpts: %lld\n", c->cmnvars->vpts);
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);

        // handle av-sync & frame rate & complete
        vdev_avsync_and_complete(c);
    }

    return NULL;
}

static void vdev_d3d_lock(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVD3DCTXT    *c = (VDEVD3DCTXT*)ctxt;
    D3DLOCKED_RECT  rect;

    sem_wait(&c->semw);

    if (!c->surfs[c->tail]) {
        // create surface
        if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev,
                   c->sw, c->sh, c->d3dfmt, D3DPOOL_DEFAULT, &c->surfs[c->tail], NULL))) {
            av_log(NULL, AV_LOG_ERROR, "failed to create d3d off screen plain surface !\n");
            return;
        }
    }

    // lock texture rect
    IDirect3DSurface9_LockRect(c->surfs[c->tail], &rect, NULL, D3DLOCK_DISCARD);
    if (buffer  ) buffer[0]   = (uint8_t*)rect.pBits;
    if (linesize) linesize[0] = rect.Pitch;
}

static void vdev_d3d_unlock(void *ctxt, int64_t pts)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    if (c->surfs[c->tail]) IDirect3DSurface9_UnlockRect(c->surfs[c->tail]);
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

static void vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEVD3DCTXT    *c    = (VDEVD3DCTXT*)ctxt;
    D3DSURFACE_DESC desc = {0};
    if (!c->surfw || SUCCEEDED(IDirect3DSurface9_GetDesc(c->surfw, &desc))) {
        if (desc.Width != w || desc.Height != h) {
            c->status |= VDEV_D3D_SET_RECT;
        }
    }
}

void vdev_d3d_setparam(void *ctxt, int id, void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    if (!ctxt || !param) return;
    switch (id) {
    case PARAM_VDEV_POST_SURFACE:
        d3d_draw_surf(c, (LPDIRECT3DSURFACE9)((AVFrame*)param)->data[3]);
        c->cmnvars->vpts = ((AVFrame*)param)->pts;
        vdev_avsync_and_complete(c);
        break;
    case PARAM_VDEV_D3D_ROTATE:
        if (c->rotate != *(int*)param) {
            c->rotate  = *(int*)param;
            c->status |= VDEV_D3D_SET_ROTATE;
        }
        break;
    }
}

void vdev_d3d_getparam(void *ctxt, int id, void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    if (!ctxt || !param) return;
    switch (id) {
    case PARAM_VDEV_GET_D3DDEV:
        *(LPDIRECT3DDEVICE9*)param = c->pD3DDev;
        break;
    case PARAM_VDEV_D3D_ROTATE:
        *(int*)param = c->rotate;
        break;
    }
}

static void vdev_d3d_destroy(void *ctxt)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    int          i;

    // make visual effect & rendering thread safely exit
    c->status = VDEV_CLOSE;
    sem_post(&c->semr);
    pthread_join(c->thread, NULL);

    // release for rotate
    d3d_release_for_rotate(c);

    for (i=0; i<c->bufnum; i++) {
        if (c->surfs[i]) {
            IDirect3DSurface9_Release(c->surfs[i]);
        }
    }

    if (c->surfw  ) IDirect3DSurface9_Release(c->surfw);
    if (c->bkbuf  ) IDirect3DSurface9_Release(c->bkbuf);
    if (c->pD3DDev) IDirect3DDevice9_Release(c->pD3DDev);
    if (c->pD3D9  ) IDirect3D9_Release(c->pD3D9);
    if (c->hDll   ) FreeLibrary(c->hDll);

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

    // free memory
    free(c->ppts );
    free(c->surfs);
    free(c);
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int bufnum, int w, int h)
{
    VDEVD3DCTXT       *ctxt    = NULL;
    PFNDirect3DCreate9 create  = NULL;
    D3DDISPLAYMODE     d3dmode = {0};

    ctxt = (VDEVD3DCTXT*)calloc(1, sizeof(VDEVD3DCTXT));
    if (!ctxt) return NULL;

    // init vdev context
    bufnum         = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->bufnum   = bufnum;
    ctxt->lock     = vdev_d3d_lock;
    ctxt->unlock   = vdev_d3d_unlock;
    ctxt->setrect  = vdev_d3d_setrect;
    ctxt->setparam = vdev_d3d_setparam;
    ctxt->getparam = vdev_d3d_getparam;
    ctxt->destroy  = vdev_d3d_destroy;
    ctxt->status   = VDEV_D3D_SET_RECT;

    // alloc buffer & semaphore
    ctxt->ppts  = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->surfs = (LPDIRECT3DSURFACE9*)calloc(bufnum, sizeof(LPDIRECT3DSURFACE9));

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    // create d3d
    ctxt->hDll  = LoadLibrary(TEXT("d3d9.dll"));
    create      = (PFNDirect3DCreate9)GetProcAddress(ctxt->hDll, "Direct3DCreate9");
    ctxt->pD3D9 = create(D3D_SDK_VERSION);
    if (!ctxt->hDll || !ctxt->ppts || !ctxt->surfs || !ctxt->semr || !ctxt->semw || !ctxt->pD3D9) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for vdev-d3d !\n");
        exit(0);
    }

    // fill d3dpp struct
    IDirect3D9_GetAdapterDisplayMode(ctxt->pD3D9, D3DADAPTER_DEFAULT, &d3dmode);
    ctxt->d3dpp.BackBufferFormat      = D3DFMT_UNKNOWN;
    ctxt->d3dpp.BackBufferCount       = 1;
    ctxt->d3dpp.BackBufferWidth       = GetSystemMetrics(SM_CXSCREEN);
    ctxt->d3dpp.BackBufferHeight      = GetSystemMetrics(SM_CYSCREEN);
    ctxt->d3dpp.MultiSampleType       = D3DMULTISAMPLE_NONE;
    ctxt->d3dpp.MultiSampleQuality    = 0;
    ctxt->d3dpp.SwapEffect            = D3DSWAPEFFECT_DISCARD;
    ctxt->d3dpp.hDeviceWindow         = (HWND)surface;
    ctxt->d3dpp.Windowed              = TRUE;
    ctxt->d3dpp.EnableAutoDepthStencil= FALSE;
#if ENABLE_WAIT_D3D_VSYNC
    ctxt->d3dpp.PresentationInterval  = d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
#else
    ctxt->d3dpp.PresentationInterval  = D3DPRESENT_INTERVAL_IMMEDIATE;
#endif

#if ENABLE_D3DMULTISAMPLE_X4
    if (SUCCEEDED(IDirect3D9_CheckDeviceMultiSampleType(ctxt->pD3D9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, NULL))) {
        ctxt->d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
    }
#endif

    if (FAILED(IDirect3D9_CreateDevice(ctxt->pD3D9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)surface,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING, &ctxt->d3dpp, &ctxt->pD3DDev)) ) {
        av_log(NULL, AV_LOG_ERROR, "failed to create d3d device !\n");
        exit(0);
    }
    if (FAILED(IDirect3DDevice9_GetBackBuffer(ctxt->pD3DDev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &ctxt->bkbuf))) {
        av_log(NULL, AV_LOG_ERROR, "failed to get d3d back buffer !\n");
        exit(0);
    }

    //++ try pixel format
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(ctxt->pD3DDev,
            1, 1, D3DFMT_YUY2, D3DPOOL_DEFAULT, &ctxt->surfs[0], NULL))) {
        ctxt->d3dfmt = D3DFMT_YUY2;
        ctxt->pixfmt = AV_PIX_FMT_YUYV422;
    } else if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(ctxt->pD3DDev,
            1, 1, D3DFMT_UYVY, D3DPOOL_DEFAULT, &ctxt->surfs[0], NULL))) {
        ctxt->d3dfmt = D3DFMT_UYVY;
        ctxt->pixfmt = AV_PIX_FMT_UYVY422;
    } else {
        ctxt->d3dfmt = D3DFMT_X8R8G8B8;
        ctxt->pixfmt = AV_PIX_FMT_RGB32;
    }
    IDirect3DSurface9_Release(ctxt->surfs[0]);
    ctxt->surfs[0] = NULL;
    //-- try pixel format

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}

