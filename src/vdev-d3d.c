// 包含头文件
#include <tchar.h>
#include <d3d9.h>
#include "vdev.h"
#include "libavformat/avformat.h"

// 预编译开关
#define ENABLE_WAIT_D3D_VSYNC  TRUE

// 内部常量定义
#define DEF_VDEV_BUF_NUM       3
#define VDEV_D3D_SET_RECT     (1 << 16)
#define VDEV_D3D_SET_ROTATE   (1 << 17)
#define VDEV_D3D_DEVICE_LOST  (1 << 18)

// 内部类型定义
typedef LPDIRECT3D9 (WINAPI *PFNDirect3DCreate9)(UINT);

typedef struct {
    // common members
    VDEV_COMMON_MEMBERS
    VDEV_WIN32__MEMBERS

    HMODULE                 hDll ;
    LPDIRECT3D9             pD3D9;
    LPDIRECT3DDEVICE9       pD3DDev;
    LPDIRECT3DSURFACE9      surfb;     // black color surface
    LPDIRECT3DSURFACE9     *surfs;     // offset screen surfaces
    LPDIRECT3DSURFACE9      surfw;     // surface keeps same size as render window
    LPDIRECT3DSURFACE9      surfh1;    // surface1 used for dxva2 hardware accel
    LPDIRECT3DSURFACE9      surfh2;    // surface2 used for dxva2 hardware accel
    LPDIRECT3DSURFACE9      bkbuf;     // back buffer surface
    D3DPRESENT_PARAMETERS   d3dpp;
    D3DFORMAT               d3dfmt;

    LPDIRECT3DTEXTURE9      texture;   // texture for rotate
    LPDIRECT3DVERTEXBUFFER9 vertexes;  // vertex buffer for rotate
    LPDIRECT3DSURFACE9      surft;     // surface of texture
    LPDIRECT3DSURFACE9      surfr;     // surface for rotate
    int                     rotate;    // rotate angle
    int                     rotw, roth;
    RECT                    rotrect;
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

static void d3d_reinit_for_rotate(VDEVD3DCTXT *c, int w, int h, int angle)
{
    float         radian= (float)(-angle * M_PI / 180);
    float         frw   = (float)(fabs(w * cos(radian)) + fabs(h * sin(radian)));
    float         frh   = (float)(fabs(w * sin(radian)) + fabs(h * cos(radian)));
    CUSTOMVERTEX *pv    = NULL;

    float bw, bh, rw, rh;
    bw = (float)c->d3dpp.BackBufferWidth;
    bh = (float)c->d3dpp.BackBufferHeight;
    if (bw * frh < bh * frw) { rw = bw; rh = rw * frh / frw; }
    else { rh = bh; rw = rh * frw / frh; }
    w = (int)(w * rw / frw);
    h = (int)(h * rh / frh);

    if (c->surfr) IDirect3DSurface9_Release(c->surfr);
    IDirect3DDevice9_CreateRenderTarget(c->pD3DDev, (int)rw, (int)rh, c->d3dpp.BackBufferFormat,
        c->d3dpp.MultiSampleType, c->d3dpp.MultiSampleQuality, FALSE, &c->surfr, NULL);

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
        rotate_point((float)w, (float)h, (float)0, (float)0, rw / 2, rh / 2, radian, &(pv[0].x), &(pv[0].y));
        rotate_point((float)w, (float)h, (float)w, (float)0, rw / 2, rh / 2, radian, &(pv[1].x), &(pv[1].y));
        rotate_point((float)w, (float)h, (float)w, (float)h, rw / 2, rh / 2, radian, &(pv[2].x), &(pv[2].y));
        rotate_point((float)w, (float)h, (float)0, (float)h, rw / 2, rh / 2, radian, &(pv[3].x), &(pv[3].y));
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

static void d3d_release_and_create(VDEVD3DCTXT *c, int release, int create)
{
    if (release) {
        int i;
        d3d_release_for_rotate(c);

        for (i=0; i<c->bufnum; i++) {
            if (c->surfs[i]) {
                IDirect3DSurface9_Release(c->surfs[i]);
                c->surfs[i] = NULL;
            }
        }

        if (c->surfb  ) { IDirect3DSurface9_Release(c->surfb  ); c->surfb  = NULL; }
        if (c->surfw  ) { IDirect3DSurface9_Release(c->surfw  ); c->surfw  = NULL; }
        if (c->surfh1 ) { IDirect3DSurface9_Release(c->surfh1 ); c->surfh1 = NULL; }
        if (c->surfh2 ) { IDirect3DSurface9_Release(c->surfh2 ); c->surfh2 = NULL; }
        if (c->bkbuf  ) { IDirect3DSurface9_Release(c->bkbuf  ); c->bkbuf  = NULL; }
        if (c->pD3DDev) { IDirect3DDevice9_Release (c->pD3DDev); c->pD3DDev= NULL; }
    }

    if (create) {
        if (!c->pD3DDev) { // try create d3d device
            if ((int)c->d3dpp.BackBufferWidth  < GetSystemMetrics(SM_CXSCREEN)) c->d3dpp.BackBufferWidth  = GetSystemMetrics(SM_CXSCREEN);
            if ((int)c->d3dpp.BackBufferHeight < GetSystemMetrics(SM_CYSCREEN)) c->d3dpp.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
            IDirect3D9_CreateDevice(c->pD3D9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)c->surface, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &c->d3dpp, &c->pD3DDev);
            if (!c->pD3DDev) return;
        }
        if (!c->bkbuf) IDirect3DDevice9_GetBackBuffer(c->pD3DDev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &c->bkbuf); // try get back buffer

        if (!c->d3dfmt) { // try pixel format
            if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, 2, 2, D3DFMT_YUY2, D3DPOOL_DEFAULT, &c->surfb, NULL))) {
                c->d3dfmt = D3DFMT_YUY2;
                c->pixfmt = AV_PIX_FMT_YUYV422;
            } else if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, 2, 2, D3DFMT_UYVY, D3DPOOL_DEFAULT, &c->surfb, NULL))) {
                c->d3dfmt = D3DFMT_UYVY;
                c->pixfmt = AV_PIX_FMT_UYVY422;
            } else {
                c->d3dfmt = D3DFMT_X8R8G8B8;
                c->pixfmt = AV_PIX_FMT_RGB32;
            }
            if (c->surfb) { IDirect3DSurface9_Release(c->surfb); c->surfb = NULL; }
        }

        if (!c->surfb) { // try create black color surface
            IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, 2, 2, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &c->surfb, NULL);
            if (c->surfb) {
                D3DLOCKED_RECT rect;
                IDirect3DSurface9_LockRect(c->surfb, &rect, NULL, D3DLOCK_DISCARD);
                memset(rect.pBits, 0, 2 * rect.Pitch);
                IDirect3DSurface9_UnlockRect(c->surfb);
                c->status &= ~VDEV_D3D_DEVICE_LOST;
            }
        }
    }
}

static void d3d_draw_surf(VDEVD3DCTXT *c, LPDIRECT3DSURFACE9 surf)
{
    D3DSURFACE_DESC desc = {0};
    D3DLOCKED_RECT  rect = {0};
    HDC             hdc  = NULL;

    if (c->rotate && (c->status & VDEV_D3D_SET_ROTATE)) {
        double radian = c->rotate * M_PI / 180;
        c->rotw = abs((int)(c->vw  * cos(radian))) + abs((int)(c->vh * sin(radian)));
        c->roth = abs((int)(c->vw  * sin(radian))) + abs((int)(c->vh * cos(radian)));

        d3d_reinit_for_rotate(c, c->vrect.right - c->vrect.left, c->vrect.bottom - c->vrect.top, c->rotate);
        if (c->surft && c->surfr) c->status &= ~VDEV_D3D_SET_ROTATE;
    }

    if (c->surfw) IDirect3DSurface9_GetDesc(c->surfw, &desc);
    if (desc.Width != c->rrect.right - c->rrect.left || desc.Height != c->rrect.bottom - c->rrect.top) {
        if (c->surfw) { IDirect3DSurface9_Release(c->surfw); c->surfw = NULL; }
        IDirect3DDevice9_CreateRenderTarget(c->pD3DDev, c->rrect.right - c->rrect.left, c->rrect.bottom - c->rrect.top,
            c->d3dpp.BackBufferFormat, c->d3dpp.MultiSampleType, c->d3dpp.MultiSampleQuality, TRUE, &c->surfw, NULL);
        if (!c->surfw) return;
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

        if (c->vm == VIDEO_MODE_LETTERBOX) {
            int rw = c->rrect.right - c->rrect.left, rh = c->rrect.bottom - c->rrect.top, vw, vh;
            if (rw * c->roth < rh * c->rotw) {
                vw = rw; vh = vw * c->roth / c->rotw;
            } else {
                vh = rh; vw = vh * c->rotw / c->roth;
            }
            c->rotrect.left  = (rw - vw) / 2;
            c->rotrect.top   = (rh - vh) / 2;
            c->rotrect.right = c->rotrect.left + vw;
            c->rotrect.bottom= c->rotrect.top  + vh;
        } else c->rotrect = c->rrect;
    }

    IDirect3DDevice9_StretchRect(c->pD3DDev, c->surfb, NULL, c->surfw, NULL, D3DTEXF_POINT);
    IDirect3DDevice9_StretchRect(c->pD3DDev, surf, NULL, c->surfw, c->rotate ? &c->rotrect : &c->vrect, D3DTEXF_LINEAR);
    IDirect3DSurface9_GetDC     (c->surfw, &hdc);
    vdev_win32_render_bboxes    (c, hdc, c->bbox_list);
    vdev_win32_render_overlay   (c, hdc, 0           );
    IDirect3DSurface9_ReleaseDC (c->surfw,  hdc);
    IDirect3DDevice9_StretchRect(c->pD3DDev, c->surfw, NULL, c->bkbuf, &c->rrect, D3DTEXF_LINEAR);
    if (D3DERR_DEVICELOST == IDirect3DDevice9_Present(c->pD3DDev, &c->rrect, &c->rrect, NULL, NULL)) {
        av_log(NULL, AV_LOG_WARNING, "D3DERR_DEVICELOST\n");
        if (c->cmnvars->init_params->video_hwaccel) { // for hwaccel decoding case can't reset d3d, so send msg to reopen player
            player_send_message(c->cmnvars->winmsg, MSG_D3D_DEVICE_LOST, 0);
        } else {
            c->status |= VDEV_D3D_DEVICE_LOST; // for sw decoding case set VDEV_D3D_DEVICE_LOST flag to reset d3d
        }
    }
}

static void* video_render_thread_proc(void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)param;
    while (!(c->status & VDEV_CLOSE)) {
        pthread_mutex_lock(&c->mutex);
        while (c->size <= 0 && (c->status & VDEV_CLOSE) == 0) pthread_cond_wait(&c->cond, &c->mutex);
        if (c->size > 0) {
            c->size--;
            if (c->ppts[c->head] != -1) {
                if (c->status & VDEV_D3D_DEVICE_LOST) d3d_release_and_create(c, 1, 1);
                else d3d_draw_surf(c, c->surfs[c->head]);
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
    d3d_release_and_create(c, 1, 0);
    return NULL;
}

static void vdev_d3d_lock(void *ctxt, uint8_t *buffer[8], int linesize[8], int64_t pts)
{
    VDEVD3DCTXT    *c    = (VDEVD3DCTXT*)ctxt;
    D3DSURFACE_DESC desc = {0};
    D3DLOCKED_RECT  rect;

    pthread_mutex_lock(&c->mutex);
    while (c->size >= c->bufnum && (c->status & VDEV_CLOSE) == 0) pthread_cond_wait(&c->cond, &c->mutex);
    if (c->size < c->bufnum && c->pD3DDev) {
        c->ppts[c->tail] = pts;
        if (c->surfs[c->tail]) IDirect3DSurface9_GetDesc(c->surfs[c->tail], &desc);
        if (desc.Width != c->vrect.right - c->vrect.left || desc.Height != c->vrect.bottom - c->vrect.top) {
            if (c->surfs[c->tail]) { IDirect3DSurface9_Release(c->surfs[c->tail]); c->surfs[c->tail] = NULL; }
            IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, c->vrect.right - c->vrect.left, c->vrect.bottom - c->vrect.top,
                c->d3dfmt, D3DPOOL_DEFAULT, &c->surfs[c->tail], NULL);
            if (c->surfs[c->tail] == NULL) return;
        }

        // lock texture rect
        IDirect3DSurface9_LockRect(c->surfs[c->tail], &rect, NULL, D3DLOCK_DISCARD);
        if (buffer  ) buffer[0]   = (uint8_t*)rect.pBits;
        if (linesize) linesize[0] = rect.Pitch;
        if (linesize) linesize[6] = c->vrect.right  - c->vrect.left;
        if (linesize) linesize[7] = c->vrect.bottom - c->vrect.top ;
    }
}

static void vdev_d3d_unlock(void *ctxt)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    if (c->surfs[c->tail]) IDirect3DSurface9_UnlockRect(c->surfs[c->tail]);
    if (++c->tail == c->bufnum) c->tail = 0;
    c->size++; pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->mutex);
}

void vdev_d3d_setparam(void *ctxt, int id, void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    if (!ctxt || !param) return;
    switch (id) {
    case PARAM_VDEV_POST_SURFACE:
        pthread_mutex_lock(&c->mutex);
        if (!(c->status & VDEV_D3D_DEVICE_LOST)) {
            D3DSURFACE_DESC desc1 = {0};
            D3DSURFACE_DESC desc2 = {0};
            AVFrame *frame   = ((void**)param)[0];
            RECT    *rectdst = ((void**)param)[1];
            RECT     rectsrc = { 0, 0, frame->width, frame->height };
            if (c->surfh1) IDirect3DSurface9_GetDesc(c->surfh1, &desc1);
            if (desc1.Width != frame->width || desc1.Height != frame->height) {
                if (c->surfh1) { IDirect3DSurface9_Release(c->surfh1); c->surfh1 = NULL; }
                IDirect3DDevice9_CreateRenderTarget(c->pD3DDev, frame->width, frame->height, c->d3dpp.BackBufferFormat,
                    c->d3dpp.MultiSampleType, c->d3dpp.MultiSampleQuality, FALSE, &c->surfh1, NULL);
            }

            if (c->surfh2) IDirect3DSurface9_GetDesc(c->surfh2, &desc2);
            if (desc2.Width != c->vw || desc2.Height != c->vh) {
                if (c->surfh2) { IDirect3DSurface9_Release(c->surfh2); c->surfh2 = NULL; }
                IDirect3DDevice9_CreateRenderTarget(c->pD3DDev, c->vw, c->vh, c->d3dpp.BackBufferFormat,
                    c->d3dpp.MultiSampleType, c->d3dpp.MultiSampleQuality, FALSE, &c->surfh2, NULL);
            }
            if (frame->pts != -1 && c->surfh1 && c->surfh2) {
                IDirect3DDevice9_StretchRect(c->pD3DDev, (LPDIRECT3DSURFACE9)frame->data[3], &rectsrc, c->surfh1, NULL, D3DTEXF_POINT);
                IDirect3DDevice9_StretchRect(c->pD3DDev, c->surfh1, rectdst, c->surfh2, NULL, D3DTEXF_LINEAR);
                d3d_draw_surf(c, c->surfh2);
                c->cmnvars->vpts = frame->pts;
            }
        }
        pthread_mutex_unlock(&c->mutex);
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

    if (c->pD3D9) IDirect3D9_Release(c->pD3D9);
    if (c->hDll ) FreeLibrary(c->hDll);

    pthread_mutex_destroy(&c->mutex);
    pthread_cond_destroy (&c->cond );

    free(c->ppts );
    free(c->surfs);
    free(c);
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int bufnum)
{
    VDEVD3DCTXT       *ctxt    = NULL;
    PFNDirect3DCreate9 create  = NULL;
    D3DDISPLAYMODE     d3dmode = {0};

    ctxt = (VDEVD3DCTXT*)calloc(1, sizeof(VDEVD3DCTXT));
    if (!ctxt) return NULL;

    // init mutex & cond
    pthread_mutex_init(&ctxt->mutex, NULL);
    pthread_cond_init (&ctxt->cond , NULL);

    // init vdev context
    bufnum         = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->surface  = surface;
    ctxt->bufnum   = bufnum;
    ctxt->lock     = vdev_d3d_lock;
    ctxt->unlock   = vdev_d3d_unlock;
    ctxt->setparam = vdev_d3d_setparam;
    ctxt->getparam = vdev_d3d_getparam;
    ctxt->destroy  = vdev_d3d_destroy;

    // alloc buffer & semaphore
    ctxt->ppts  = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->surfs = (LPDIRECT3DSURFACE9*)calloc(bufnum, sizeof(LPDIRECT3DSURFACE9));

    // create d3d
    ctxt->hDll  = LoadLibrary(TEXT("d3d9.dll"));
    create      = (PFNDirect3DCreate9)GetProcAddress(ctxt->hDll, "Direct3DCreate9");
    ctxt->pD3D9 = create(D3D_SDK_VERSION);
    if (!ctxt->hDll || !ctxt->ppts || !ctxt->surfs || !ctxt->mutex || !ctxt->cond || !ctxt->pD3D9) {
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
    ctxt->d3dpp.PresentationInterval  = ENABLE_WAIT_D3D_VSYNC == FALSE ? D3DPRESENT_INTERVAL_IMMEDIATE : d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
    d3d_release_and_create(ctxt, 0, 1); // create d3d resources

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}

