// 包含头文件
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// for jni
JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);

// 内部常量定义
#define DEF_WIN_PIX_FMT  WINDOW_FORMAT_RGBX_8888
#define VDEV_ANDROID_UPDATE_WIN  (1 << 31)

// 内部类型定义
typedef struct {
    // common members
    VDEV_COMMON_MEMBERS
    ANativeWindow *win;
} VDEVCTXT;

// 内部函数实现
inline int android_pixfmt_to_ffmpeg_pixfmt(int fmt)
{
    switch (fmt) {
    case WINDOW_FORMAT_RGB_565:   return AV_PIX_FMT_RGB565;
    case WINDOW_FORMAT_RGBX_8888: return AV_PIX_FMT_BGR32 ;
    default:                      return 0;
    }
}

static void vdev_android_lock(void *ctxt, uint8_t *buffer[8], int linesize[8], int64_t pts)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    if (c->status & VDEV_ANDROID_UPDATE_WIN) {
        if (c->win    ) { ANativeWindow_release(c->win); c->win = NULL; }
        if (c->surface) c->win = ANativeWindow_fromSurface(get_jni_env(), (jobject)c->surface);
        if (c->win    ) ANativeWindow_setBuffersGeometry(c->win, c->vw, c->vh, DEF_WIN_PIX_FMT);
        c->status &= ~VDEV_ANDROID_UPDATE_WIN;
    }
    if (c->win) {
        ANativeWindow_Buffer winbuf;
        if (0 == ANativeWindow_lock(c->win, &winbuf, NULL)) {
            buffer  [0] = (uint8_t*)winbuf.bits;
            linesize[0] = winbuf.stride * 4;
            linesize[6] = c->vw;
            linesize[7] = c->vh;
        }
    }
    c->cmnvars->vpts = pts;
}

static void vdev_android_unlock(void *ctxt)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    if (c->win) ANativeWindow_unlockAndPost(c->win);
    vdev_avsync_and_complete(c);
}

static void vdev_android_setparam(void *ctxt, int id, void *param)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    switch (id) {
    case PARAM_RENDER_VDEV_WIN:
        c->surface = param;
        c->status |= VDEV_ANDROID_UPDATE_WIN;
        break;
    }
}

static void vdev_android_destroy(void *ctxt)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    if (c->win) ANativeWindow_release(c->win);
    free(ctxt);
}

// 接口函数实现
void* vdev_android_create(void *surface, int bufnum)
{
    VDEVCTXT *ctxt = (VDEVCTXT*)calloc(1, sizeof(VDEVCTXT));
    if (!ctxt) return NULL;
    // init vdev context
    ctxt->pixfmt  = android_pixfmt_to_ffmpeg_pixfmt(DEF_WIN_PIX_FMT);
    ctxt->lock    = vdev_android_lock;
    ctxt->unlock  = vdev_android_unlock;
    ctxt->setparam= vdev_android_setparam;
    ctxt->destroy = vdev_android_destroy;
    ctxt->status |= VDEV_ANDROID_UPDATE_WIN;
    return ctxt;
}


