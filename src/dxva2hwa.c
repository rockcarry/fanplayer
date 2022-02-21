// 包含头文件
#include <windows.h>
#include <initguid.h>
#include <dxva2api.h>
#include "dxva2hwa.h"

#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_dxva2.h"
#include "libavcodec/dxva2.h"

// 内部类型定义
DEFINE_GUID(IID_IDirectXVideoDecoderService, 0xfc51a551,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
DEFINE_GUID(DXVA2_ModeMPEG2_VLD,             0xee27417f,0x5e28,0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID(DXVA2_ModeMPEG2and1_VLD,         0x86695f12,0x340e,0x4f04,0x9f,0xd3,0x92,0x53,0xdd,0x32,0x74,0x60);
DEFINE_GUID(DXVA2_ModeH264_E,                0x1b81be68,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_F,                0x1b81be69,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVADDI_Intel_ModeH264_E,        0x604F8E68,0x4951,0x4C54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(DXVA2_ModeVC1_D,                 0x1b81beA3,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D2010,             0x1b81beA4,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main,         0x5b11d51b,0x2f4c,0x4452,0xbc,0xc3,0x09,0xf2,0xa1,0x16,0x0c,0xc0);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main10,       0x107af0e0,0xef1a,0x4d19,0xab,0xa8,0x67,0xa1,0x63,0x07,0x3d,0x13);
DEFINE_GUID(DXVA2_ModeVP9_VLD_Profile0,      0x463707f8,0xa1d0,0x4585,0x87,0x6d,0x83,0xaa,0x6d,0x60,0xb8,0x9e);
DEFINE_GUID(DXVA2_NoEncrypt,                 0x1b81beD0,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(GUID_NULL,                       0x00000000,0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

typedef struct dxva2_mode {
    REFGUID        guid;
    enum AVCodecID codec;
} dxva2_mode;

static const dxva2_mode dxva2_modes[] = {
    /* MPEG-2 */
    { &DXVA2_ModeMPEG2_VLD,        AV_CODEC_ID_MPEG2VIDEO },
    { &DXVA2_ModeMPEG2and1_VLD,    AV_CODEC_ID_MPEG2VIDEO },

    /* H.264 */
    { &DXVA2_ModeH264_F,           AV_CODEC_ID_H264 },
    { &DXVA2_ModeH264_E,           AV_CODEC_ID_H264 },

    /* Intel specific H.264 mode */
    { &DXVADDI_Intel_ModeH264_E,   AV_CODEC_ID_H264 },

    /* VC-1 / WMV3 */
    { &DXVA2_ModeVC1_D2010,        AV_CODEC_ID_VC1  },
    { &DXVA2_ModeVC1_D2010,        AV_CODEC_ID_WMV3 },
    { &DXVA2_ModeVC1_D,            AV_CODEC_ID_VC1  },
    { &DXVA2_ModeVC1_D,            AV_CODEC_ID_WMV3 },

    /* HEVC/H.265 */
    { &DXVA2_ModeHEVC_VLD_Main,    AV_CODEC_ID_HEVC },
    { &DXVA2_ModeHEVC_VLD_Main10,  AV_CODEC_ID_HEVC },

    /* VP8/9 */
    { &DXVA2_ModeVP9_VLD_Profile0, AV_CODEC_ID_VP9  },

    { &GUID_NULL }
};

typedef struct DXVA2Context {
    HMODULE                      hDll;
    LPDIRECT3D9                  pD3D9;
    LPDIRECT3DDEVICE9            pD3DDev;

    IDirectXVideoDecoder        *decoder;

    GUID                         decoder_guid;
    DXVA2_ConfigPictureDecode    decoder_config;
    IDirectXVideoDecoderService *decoder_service;

    AVBufferRef                 *hw_device_ctx;
    AVBufferRef                 *hw_frames_ctx;
} DXVA2Context;

typedef struct {
    void  *hwaccel_d3ddev;
    void  *hwaccel_ctx;
    int  (*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
    enum AVPixelFormat (*hwaccel_get_format)(AVCodecContext *s, const enum AVPixelFormat *fmts);
} HWACCEL;

typedef LPDIRECT3D9(WINAPI *PFNDirect3DCreate9)(UINT);

// 内部函数实现
static int dxva2_get_buffer(AVCodecContext *s, AVFrame *frame, int flags)
{
    HWACCEL      *hwa = (HWACCEL     *)s->opaque;
    DXVA2Context *ctx = (DXVA2Context*)hwa->hwaccel_ctx;

    return av_hwframe_get_buffer(ctx->hw_frames_ctx, frame, 0);
}

static enum AVPixelFormat dxva2_get_format(AVCodecContext *s, const enum AVPixelFormat *fmts)
{
    return AV_PIX_FMT_DXVA2_VLD;
}

static int dxva2_alloc(AVCodecContext *s)
{
    HWACCEL *hwa = (HWACCEL*)s->opaque;
    DXVA2Context *ctx;
    HANDLE  device_handle;
    HRESULT hr;

    AVHWDeviceContext    *device_ctx;
    AVDXVA2DeviceContext *device_hwctx;
    int ret;

    ctx = (DXVA2Context*)av_mallocz(sizeof(*ctx));
    if (!ctx) return AVERROR(ENOMEM);

    hwa->hwaccel_ctx = ctx;
    hwa->hwaccel_get_buffer = dxva2_get_buffer;
    hwa->hwaccel_get_format = dxva2_get_format;
    ret = av_hwdevice_ctx_create(&ctx->hw_device_ctx, AV_HWDEVICE_TYPE_DXVA2,
                                 (char*)hwa->hwaccel_d3ddev, NULL, 0);
    if (ret < 0) goto fail;
    device_ctx   = (AVHWDeviceContext*)ctx->hw_device_ctx->data;
    device_hwctx = (AVDXVA2DeviceContext*)device_ctx->hwctx;

    hr = IDirect3DDeviceManager9_OpenDeviceHandle(device_hwctx->devmgr,
                                                  &device_handle);
    if (FAILED(hr)) {
        av_log(NULL, AV_LOG_INFO, "Failed to open a device handle\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(device_hwctx->devmgr, device_handle,
                                                 &IID_IDirectXVideoDecoderService,
                                                 (void **)&ctx->decoder_service);
    IDirect3DDeviceManager9_CloseDeviceHandle(device_hwctx->devmgr, device_handle);
    if (FAILED(hr)) {
        av_log(NULL, AV_LOG_INFO, "Failed to create IDirectXVideoDecoderService\n");
        goto fail;
    }

    s->hwaccel_context = av_mallocz(sizeof(struct dxva_context));
    if (!s->hwaccel_context) goto fail;
    return 0;

fail:
    dxva2hwa_free(s);
    return AVERROR(EINVAL);
}

static int dxva2_get_decoder_configuration(AVCodecContext *s, const GUID *device_guid,
                                           const DXVA2_VideoDesc *desc,
                                           DXVA2_ConfigPictureDecode *config)
{
    HWACCEL      *hwa = (HWACCEL*)s->opaque;
    DXVA2Context *ctx = (DXVA2Context*)hwa->hwaccel_ctx;
    unsigned cfg_count  = 0;
    unsigned best_score = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    DXVA2_ConfigPictureDecode  best_cfg = {{0}};
    HRESULT hr;
    unsigned i;

    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(ctx->decoder_service, device_guid, desc, NULL, &cfg_count, &cfg_list);
    if (FAILED(hr)) {
        av_log(NULL, AV_LOG_INFO, "Unable to retrieve decoder configurations\n");
        return AVERROR(EINVAL);
    }

    for (i = 0; i < cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];
        unsigned score;
        if (cfg->ConfigBitstreamRaw == 1) {
            score = 1;
        } else if (s->codec_id == AV_CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2) {
            score = 2;
        } else {
            continue;
        }
        if (IsEqualGUID(&cfg->guidConfigBitstreamEncryption, &DXVA2_NoEncrypt))
            score += 16;
        if (score > best_score) {
            best_score = score;
            best_cfg   = *cfg;
        }
    }
    CoTaskMemFree(cfg_list);

    if (!best_score) {
        av_log(NULL, AV_LOG_INFO, "No valid decoder configuration available\n");
        return AVERROR(EINVAL);
    }

    *config = best_cfg;
    return 0;
}

static int dxva2_create_decoder(AVCodecContext *s)
{
    HWACCEL             *hwa = (HWACCEL*)s->opaque;
    DXVA2Context        *ctx = (DXVA2Context*)hwa->hwaccel_ctx;
    struct dxva_context *dxva_ctx = (struct dxva_context*)s->hwaccel_context;
    GUID    *guid_list  = NULL;
    unsigned guid_count = 0, i, j;
    GUID    device_guid = GUID_NULL;
    const D3DFORMAT surface_format = (D3DFORMAT)((s->sw_pix_fmt == AV_PIX_FMT_YUV420P10) ? MKTAG('P','0','1','0') : MKTAG('N','V','1','2'));
    D3DFORMAT        target_format = (D3DFORMAT)0;
    DXVA2_VideoDesc           desc = {0};
    DXVA2_ConfigPictureDecode config;
    int surface_alignment;
    int num_surfaces;
    int    ret;
    HRESULT hr;

    AVDXVA2FramesContext *frames_hwctx;
    AVHWFramesContext *frames_ctx;

    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(ctx->decoder_service, &guid_count, &guid_list);
    if (FAILED(hr)) {
        av_log(NULL, AV_LOG_INFO, "Failed to retrieve decoder device GUIDs\n");
        goto fail;
    }

    for (i=0; !IsEqualGUID(dxva2_modes[i].guid, &GUID_NULL); i++) {
        D3DFORMAT *target_list = NULL;
        unsigned  target_count = 0;
        const dxva2_mode *mode = &dxva2_modes[i];
        if (mode->codec != s->codec_id) continue;

        for (j=0; j<guid_count; j++) {
            if (IsEqualGUID(mode->guid, &guid_list[j]))
                break;
        }
        if (j == guid_count) continue;

        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(ctx->decoder_service, mode->guid, &target_count, &target_list);
        if (FAILED(hr)) continue;

        for (j=0; j<target_count; j++) {
            const D3DFORMAT format = target_list[j];
            if (format == surface_format) {
                target_format = format;
                break;
            }
        }
        CoTaskMemFree(target_list);
        if (target_format) {
            device_guid = *mode->guid;
            break;
        }
    }
    CoTaskMemFree(guid_list);

    if (IsEqualGUID(&device_guid, &GUID_NULL)) {
        av_log(NULL, AV_LOG_INFO, "No decoder device for codec found\n");
        goto fail;
    }

    desc.SampleWidth  = s->coded_width;
    desc.SampleHeight = s->coded_height;
    desc.Format       = target_format;

    ret = dxva2_get_decoder_configuration(s, &device_guid, &desc, &config);
    if (ret < 0) goto fail;

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
       but it causes issues for H.264 on certain AMD GPUs..... */
    if (s->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        surface_alignment = 32;
    }
    /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
       all coding features have enough room to work with */
    else if (s->codec_id == AV_CODEC_ID_HEVC) {
        surface_alignment = 128;
    } else {
        surface_alignment = 16;
    }

    /* 4 base work surfaces */
    num_surfaces = 4;

    /* add surfaces based on number of possible refs */
    if (s->codec_id == AV_CODEC_ID_H264 || s->codec_id == AV_CODEC_ID_HEVC) {
        num_surfaces += 16;
    } else if (s->codec_id == AV_CODEC_ID_VP9) {
        num_surfaces += 8;
    } else {
        num_surfaces += 2;
    }

    /* add extra surfaces for frame threading */
    if (s->active_thread_type & FF_THREAD_FRAME) {
        num_surfaces += s->thread_count;
    }

    ctx->hw_frames_ctx = av_hwframe_ctx_alloc(ctx->hw_device_ctx);
    if (!ctx->hw_frames_ctx) {
        goto fail;
    }
    frames_ctx   = (AVHWFramesContext*)ctx->hw_frames_ctx->data;
    frames_hwctx = (AVDXVA2FramesContext*)frames_ctx->hwctx;

    frames_ctx->format            = AV_PIX_FMT_DXVA2_VLD;
    frames_ctx->sw_format         = (target_format == MKTAG('P','0','1','0') ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12);
    frames_ctx->width             = FFALIGN(s->coded_width, surface_alignment);
    frames_ctx->height            = FFALIGN(s->coded_height, surface_alignment);
    frames_ctx->initial_pool_size = num_surfaces;

    frames_hwctx->surface_type = DXVA2_VideoDecoderRenderTarget;

    ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_INFO, "Failed to initialize the HW frames context\n");
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateVideoDecoder(ctx->decoder_service, &device_guid,
                                                        &desc, &config, frames_hwctx->surfaces,
                                                        frames_hwctx->nb_surfaces, &frames_hwctx->decoder_to_release);
    if (FAILED(hr)) {
        av_log(NULL, AV_LOG_INFO, "Failed to create DXVA2 video decoder\n");
        goto fail;
    }

    ctx->decoder_guid   = device_guid;
    ctx->decoder_config = config;

    dxva_ctx->cfg           = &ctx->decoder_config;
    dxva_ctx->decoder       = frames_hwctx->decoder_to_release;
    dxva_ctx->surface       = frames_hwctx->surfaces;
    dxva_ctx->surface_count = frames_hwctx->nb_surfaces;

    if (IsEqualGUID(&ctx->decoder_guid, &DXVADDI_Intel_ModeH264_E)) {
        dxva_ctx->workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
    }

    return 0;

fail:
    av_buffer_unref(&ctx->hw_frames_ctx);
    return AVERROR(EINVAL);
}

int dxva2hwa_init(AVCodecContext *s, void *d3ddev, void *hwnd)
{
    HMODULE            hdll  = NULL;
    PFNDirect3DCreate9 create= NULL;
    LPDIRECT3D9        pd3d9 = NULL;
    HWACCEL           *hwa   = NULL;
    DXVA2Context      *ctx   = NULL;
    int                ret;

    if (!d3ddev) {
        hdll   = LoadLibrary(TEXT("d3d9.dll"));
        create = (PFNDirect3DCreate9)GetProcAddress(hdll, "Direct3DCreate9");
        if (create) pd3d9 = create(D3D_SDK_VERSION);
        if (pd3d9) {
            D3DPRESENT_PARAMETERS d3dpp = {0};
            d3dpp.SwapEffect            = D3DSWAPEFFECT_DISCARD;
            d3dpp.hDeviceWindow         = (HWND)hwnd;
            d3dpp.Windowed              = TRUE;
            IDirect3D9_CreateDevice(pd3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, (LPDIRECT3DDEVICE9*)&d3ddev);
        }
        if (!hdll || !create || !pd3d9 || !d3ddev) {
            if (d3ddev) IDirect3DDevice9_Release((LPDIRECT3DDEVICE9)d3ddev);
            if (pd3d9 ) IDirect3D9_Release(pd3d9);
            if (hdll  ) FreeLibrary(hdll);
            av_log(NULL, AV_LOG_ERROR, "dxva2hwa_init:: d3ddev is NULL !\n");
            return AVERROR(ENODEV);
        }
    }

    hwa = (HWACCEL*)av_mallocz(sizeof(HWACCEL));
    if (!hwa) {
        av_log(NULL, AV_LOG_ERROR, "dxva2hwa_init:: failed to allocate memory for HWACCEL !\n");
        return AVERROR(ENOMEM);
    }
    hwa->hwaccel_d3ddev = d3ddev;
    s->opaque = hwa;

    if (!hwa->hwaccel_ctx) {
        ret = dxva2_alloc(s);
        if (ret < 0) return ret;
    }
    ctx = (DXVA2Context*)hwa->hwaccel_ctx;
    ctx->hDll    = hdll;
    ctx->pD3D9   = pd3d9;
    ctx->pD3DDev = d3ddev;

    ret = dxva2_create_decoder(s);
    if (ret < 0) {
        av_log(NULL, AV_LOG_INFO, "Error creating the DXVA2 decoder\n");
        dxva2hwa_free(s);
        return ret;
    }

    s->thread_safe_callbacks = 1;
    s->get_buffer2           = hwa->hwaccel_get_buffer;
    s->get_format            = hwa->hwaccel_get_format;
    return 0;
}

void dxva2hwa_free(AVCodecContext *s)
{
    HWACCEL      *hwa = (HWACCEL*)s->opaque;
    DXVA2Context *ctx = hwa ? (DXVA2Context*)hwa->hwaccel_ctx : NULL;
    if (!hwa) return;

    hwa->hwaccel_get_buffer = NULL;
    hwa->hwaccel_get_format = NULL;

    if (ctx->decoder_service) IDirectXVideoDecoderService_Release(ctx->decoder_service);
    if (ctx->hDll) {
        if (ctx->pD3DDev) IDirect3DDevice9_Release(ctx->pD3DDev);
        if (ctx->pD3D9  ) IDirect3D9_Release(ctx->pD3D9);
        FreeLibrary(ctx->hDll);
    }

    av_buffer_unref(&ctx->hw_frames_ctx);
    av_buffer_unref(&ctx->hw_device_ctx);

    av_freep(&hwa->hwaccel_ctx);
    av_freep(&s->hwaccel_context);
    av_freep(&s->opaque);
}

void dxva2hwa_lock_frame(AVFrame *dxva2frame, AVFrame *lockedframe)
{
    LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)dxva2frame->data[3];
    D3DSURFACE_DESC    desc;
    D3DLOCKED_RECT     rect;
    if (surface) {
        IDirect3DSurface9_GetDesc (surface, &desc);
        IDirect3DSurface9_LockRect(surface, &rect, NULL, D3DLOCK_READONLY);
        switch (desc.Format) {
        case MAKEFOURCC('N', 'V', '1', '2'):
            if (lockedframe) {
                lockedframe->width       = desc.Width;
                lockedframe->height      = desc.Height;
                lockedframe->format      = AV_PIX_FMT_NV12;
                lockedframe->pts         = dxva2frame->pts;
                lockedframe->data[0]     = (uint8_t*)rect.pBits;
                lockedframe->data[1]     = (uint8_t*)rect.pBits + desc.Height * rect.Pitch;
                lockedframe->linesize[0] = rect.Pitch;
                lockedframe->linesize[1] = rect.Pitch;
            }
            break;
        }
    }
}

void dxva2hwa_unlock_frame(AVFrame *dxva2frame)
{
    LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)dxva2frame->data[3];
    if (surface) IDirect3DSurface9_UnlockRect(surface);
}
