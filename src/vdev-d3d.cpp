// 包含头文件
#include <d3d9.h>
#include <d3dx9.h>
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 预编译开关
#define ENABLE_WAIT_D3D_VSYNC    FALSE
#define ENABLE_D3DMULTISAMPLE_X4 FALSE

// 内部常量定义
#define DEF_VDEV_BUF_NUM       3
#define VDEV_D3D_SET_RECT     (1 << 16)
#define VDEV_D3D_SET_ROTATE   (1 << 17)

// 内部类型定义
typedef struct {
    // common members
    VDEV_COMMON_MEMBERS

    LPDIRECT3D9             pD3D9;
    LPDIRECT3DDEVICE9       pD3DDev;
    LPDIRECT3DSURFACE9     *surfs; // offset screen surfaces
    LPDIRECT3DSURFACE9      surfw; // surface keeps same size as window
    LPDIRECT3DSURFACE9      bkbuf; // back buffer surface
    D3DPRESENT_PARAMETERS   d3dpp;
    D3DFORMAT               d3dfmt;
    LPD3DXFONT              d3dfont;

    LPDIRECT3DTEXTURE9      texture; // texture for rotate
    LPDIRECT3DVERTEXBUFFER9 vertexes;// vertex buffer for rotate
    LPDIRECT3DSURFACE9      surft;   // surface of texture
    LPDIRECT3DSURFACE9      surfr;   // surface for rotate
    int                     rotate;  // rotate angle
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
    *xo = (xi - cx) * cos(radian) + (yi - cy) * sin(radian) + cx;
    *yo =-(xi - cx) * sin(radian) + (yi - cy) * cos(radian) + cy;
}

static void d3d_reinit_for_rotate(VDEVD3DCTXT *c, int w, int h, int angle, int *ow, int *oh)
{
    float radian = (float)(-angle * M_PI / 180);
    float fow = abs(float(w * cos(radian)))
              + abs(float(h * sin(radian)));
    float foh = abs(float(w * sin(radian)))
              + abs(float(h * cos(radian)));
    if (ow) *ow = (int)fow;
    if (oh) *oh = (int)foh;

    if (c->surfr) c->surfr->Release();
    c->pD3DDev->CreateRenderTarget((int)fow, (int)foh, c->d3dpp.BackBufferFormat, c->d3dpp.MultiSampleType,
                                   c->d3dpp.MultiSampleQuality, FALSE, &c->surfr, NULL);

    if (!c->texture) {
        c->pD3DDev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, c->d3dpp.BackBufferFormat, D3DPOOL_DEFAULT, &c->texture , NULL);
        c->texture->GetSurfaceLevel(0, &c->surft);
    }
    if (!c->vertexes) {
        c->pD3DDev->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &c->vertexes, NULL);
    }

    CUSTOMVERTEX *pv = NULL;
    if (SUCCEEDED(c->vertexes->Lock(0, 4 * sizeof(CUSTOMVERTEX), (void**)&pv, 0))) {
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
        c->vertexes->Unlock();
    }
}

static void d3d_release_for_rotate(VDEVD3DCTXT *c)
{
    if (c->surft) {
        c->surft->Release();
        c->surft = NULL;
    }
    if (c->surfr) {
        c->surfr->Release();
        c->surfr = NULL;
    }
    if (c->texture) {
        c->texture->Release();
        c->texture = NULL;
    }
    if (c->vertexes) {
        c->vertexes->Release();
        c->vertexes = NULL;
    }
}

static void d3d_draw_surf(VDEVD3DCTXT *c, LPDIRECT3DSURFACE9 surf)
{
    RECT rect = { c->x, c->y, c->x + c->w, c->y + c->h };

    if (c->rotate && (c->status & VDEV_D3D_SET_ROTATE)) {
        d3d_reinit_for_rotate(c, c->sw, c->sh, c->rotate, NULL, NULL);
        if (c->surft && c->surfr) c->status &= ~VDEV_D3D_SET_ROTATE;
    }

    if (c->textt && (c->status & VDEV_D3D_SET_RECT)) {
        if (c->surfw) c->surfw->Release();
        c->pD3DDev->CreateRenderTarget(c->w, c->h, c->d3dpp.BackBufferFormat, c->d3dpp.MultiSampleType,
                                       c->d3dpp.MultiSampleQuality, FALSE, &c->surfw, NULL);
        if (c->surfw) c->status &= ~VDEV_D3D_SET_RECT;
    }

    if (c->rotate && c->surft && c->surfr) {
        c->pD3DDev->StretchRect(surf, NULL, c->surft, NULL, D3DTEXF_LINEAR);
        if (SUCCEEDED(c->pD3DDev->BeginScene())) {
            c->pD3DDev->SetRenderTarget(0, c->surfr);
            c->pD3DDev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            c->pD3DDev->SetTexture(0, c->texture);
            c->pD3DDev->SetStreamSource(0, c->vertexes, 0, sizeof(CUSTOMVERTEX));
            c->pD3DDev->SetFVF(D3DFVF_CUSTOMVERTEX);
            c->pD3DDev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
            c->pD3DDev->EndScene();
            surf = c->surfr;
        }
    }

    if (c->textt && c->surfw) {
        c->pD3DDev->StretchRect(surf, NULL, c->surfw, NULL, D3DTEXF_LINEAR);
        if (SUCCEEDED(c->pD3DDev->BeginScene())) {
            RECT r = { c->textx, c->texty, rect.right, rect.bottom };
            c->pD3DDev->SetRenderTarget(0, c->surfw);
            if (!(c->textc >> 24)) c->textc |= (0xff << 24);
            c->d3dfont->DrawTextA(c->textt, -1, &r, 0, c->textc);
            c->pD3DDev->EndScene();
            surf = c->surfw;
        }
    }

    c->pD3DDev->StretchRect(surf, NULL, c->bkbuf, NULL, D3DTEXF_LINEAR);
    c->pD3DDev->Present(NULL, &rect, NULL, NULL);
}

static void* video_render_thread_proc(void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)param;

    while (1) {
        sem_wait(&c->semr);
        if (c->status & VDEV_CLOSE) break;

        if (c->status & VDEV_REFRESHBG) {
            c->status &= ~VDEV_REFRESHBG;
            vdev_refresh_background(c);
        }

        if (c->ppts[c->head] != -1) {
            d3d_draw_surf(c, c->surfs[c->head]);
            c->vpts = c->ppts[c->head];
        }

        av_log(NULL, AV_LOG_DEBUG, "vpts: %lld\n", c->vpts);
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);

        // handle av-sync & frame rate & complete
        vdev_avsync_and_complete(c);
    }

    return NULL;
}

static void vdev_d3d_lock(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;

    sem_wait(&c->semw);

    if (!c->surfs[c->tail]) {
        // create surface
        if (FAILED(c->pD3DDev->CreateOffscreenPlainSurface(c->sw, c->sh,
                   c->d3dfmt, D3DPOOL_DEFAULT, &c->surfs[c->tail], NULL))) {
            av_log(NULL, AV_LOG_ERROR, "failed to create d3d off screen plain surface !\n");
            return;
        }
    }

    // lock texture rect
    D3DLOCKED_RECT d3d_rect;
    c->surfs[c->tail]->LockRect(&d3d_rect, NULL, D3DLOCK_DISCARD);

    if (buffer  ) buffer[0]   = (uint8_t*)d3d_rect.pBits;
    if (linesize) linesize[0] = d3d_rect.Pitch;
}

static void vdev_d3d_unlock(void *ctxt, int64_t pts)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    if (c->surfs[c->tail]) c->surfs[c->tail]->UnlockRect();
    c->ppts [c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

static void vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEVD3DCTXT    *c    = (VDEVD3DCTXT*)ctxt;
    D3DSURFACE_DESC desc = {};
    if (!c->surfw || SUCCEEDED(c->surfw->GetDesc(&desc))) {
        if (desc.Width != w || desc.Height != h) {
            c->status |= VDEV_D3D_SET_RECT;
        }
    }
}

void vdev_d3d_setparam(void *ctxt, int id, void *param)
{
    if (!ctxt || !param) return;
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    switch (id) {
    case PARAM_VDEV_POST_SURFACE:
        d3d_draw_surf(c, (LPDIRECT3DSURFACE9)((AVFrame*)param)->data[3]);
        c->vpts = ((AVFrame*)param)->pts;
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
    if (!ctxt || !param) return;
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
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
    int i;
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;

    // make visual effect & rendering thread safely exit
    c->status = VDEV_CLOSE;
    sem_post(&c->semr);
    pthread_join(c->thread, NULL);

    // release for rotate
    d3d_release_for_rotate(c);

    for (i=0; i<c->bufnum; i++) {
        if (c->surfs[i]) {
            c->surfs[i]->Release();
        }
    }

    if (c->surfw  ) c->surfw  ->Release();
    if (c->bkbuf  ) c->bkbuf  ->Release();
    if (c->d3dfont) c->d3dfont->Release();
    if (c->pD3DDev) c->pD3DDev->Release();
    if (c->pD3D9  ) c->pD3D9  ->Release();

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

    // free memory
    free(c->ppts );
    free(c->surfs);
    free(c);
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int bufnum, int w, int h, int frate)
{
    VDEVD3DCTXT *ctxt = (VDEVD3DCTXT*)calloc(1, sizeof(VDEVD3DCTXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate d3d vdev context !\n");
        exit(0);
    }

    // init vdev context
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->surface   = surface;
    ctxt->bufnum    = bufnum;
    ctxt->w         = w > 1 ? w : 1;
    ctxt->h         = h > 1 ? h : 1;
    ctxt->sw        = w > 1 ? w : 1;
    ctxt->sh        = h > 1 ? h : 1;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->apts      = -1;
    ctxt->vpts      = -1;
    ctxt->lock      = vdev_d3d_lock;
    ctxt->unlock    = vdev_d3d_unlock;
    ctxt->setrect   = vdev_d3d_setrect;
    ctxt->setparam  = vdev_d3d_setparam;
    ctxt->getparam  = vdev_d3d_getparam;
    ctxt->destroy   = vdev_d3d_destroy;
    ctxt->status    = VDEV_D3D_SET_RECT;

    // alloc buffer & semaphore
    ctxt->ppts  = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->surfs = (LPDIRECT3DSURFACE9*)calloc(bufnum, sizeof(LPDIRECT3DSURFACE9));

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    // create d3d
    ctxt->pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!ctxt->ppts || !ctxt->surfs || !ctxt->semr || !ctxt->semw || !ctxt->pD3D9) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for vdev-d3d !\n");
        exit(0);
    }

    // fill d3dpp struct
    D3DDISPLAYMODE d3dmode = {0};
    ctxt->pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dmode);
    ctxt->d3dpp.BackBufferFormat      = D3DFMT_UNKNOWN;
    ctxt->d3dpp.BackBufferCount       = 1;
    ctxt->d3dpp.BackBufferWidth       = GetSystemMetrics(SM_CXSCREEN);
    ctxt->d3dpp.BackBufferHeight      = GetSystemMetrics(SM_CYSCREEN);
    ctxt->d3dpp.MultiSampleType       = D3DMULTISAMPLE_NONE;
    ctxt->d3dpp.MultiSampleQuality    = 0;
    ctxt->d3dpp.SwapEffect            = D3DSWAPEFFECT_DISCARD;
    ctxt->d3dpp.hDeviceWindow         = (HWND)ctxt->surface;
    ctxt->d3dpp.Windowed              = TRUE;
    ctxt->d3dpp.EnableAutoDepthStencil= FALSE;
#if ENABLE_WAIT_D3D_VSYNC
    ctxt->d3dpp.PresentationInterval  = d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
#else
    ctxt->d3dpp.PresentationInterval  = D3DPRESENT_INTERVAL_IMMEDIATE;
#endif

#if ENABLE_D3DMULTISAMPLE_X4
    if (SUCCEEDED(ctxt->pD3D9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, NULL))) {
        ctxt->d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
    }
#endif

    if (FAILED(ctxt->pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)ctxt->surface,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING, &ctxt->d3dpp, &ctxt->pD3DDev)) ) {
        av_log(NULL, AV_LOG_ERROR, "failed to create d3d device !\n");
        exit(0);
    }
    if (FAILED(ctxt->pD3DDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &ctxt->bkbuf))) {
        av_log(NULL, AV_LOG_ERROR, "failed to get d3d back buffer !\n");
        exit(0);
    }

    //++ try pixel format
    if (SUCCEEDED(ctxt->pD3DDev->CreateOffscreenPlainSurface(1, 1, D3DFMT_YUY2,
            D3DPOOL_DEFAULT, &ctxt->surfs[0], NULL))) {
        ctxt->d3dfmt = D3DFMT_YUY2;
        ctxt->pixfmt = AV_PIX_FMT_YUYV422;
    } else if (SUCCEEDED(ctxt->pD3DDev->CreateOffscreenPlainSurface(1, 1, D3DFMT_UYVY,
            D3DPOOL_DEFAULT, &ctxt->surfs[0], NULL))) {
        ctxt->d3dfmt = D3DFMT_UYVY;
        ctxt->pixfmt = AV_PIX_FMT_UYVY422;
    } else {
        ctxt->d3dfmt = D3DFMT_X8R8G8B8;
        ctxt->pixfmt = AV_PIX_FMT_RGB32;
    }
    ctxt->surfs[0]->Release();
    ctxt->surfs[0] = NULL;
    //-- try pixel format

    LOGFONT logfont = {0};
    wcscpy(logfont.lfFaceName, TEXT(DEF_FONT_NAME));
    logfont.lfHeight = DEF_FONT_SIZE;
    D3DXCreateFontIndirect(ctxt->pD3DDev, &logfont, &ctxt->d3dfont);

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}

