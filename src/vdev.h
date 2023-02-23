#ifndef __FANPLAYER_VDEV_H__
#define __FANPLAYER_VDEV_H__

// 包含头文件
#include <pthread.h>
#include "ffplayer.h"
#include "ffrender.h"

#if CONFIG_ENABLE_FFOBJDET
#include "ffobjdet.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
#define VDEV_CLOSE      (1 << 0)
#define VDEV_COMPLETED  (1 << 1)
#define VDEV_CLEAR      (1 << 2)

//++ vdev context common members
#define VDEV_COMMON_MEMBERS        \
    int         bufnum;            \
    int         pixfmt;            \
    int         vw, vh, vm;        \
    RECT        rrect;             \
    RECT        vrect;             \
                                   \
    void       *surface;           \
    int64_t    *ppts;              \
                                   \
    int         head;              \
    int         tail;              \
    int         size;              \
                                   \
    pthread_mutex_t mutex;         \
    pthread_cond_t  cond;          \
                                   \
    CMNVARS    *cmnvars;           \
    int         tickavdiff;        \
    int         tickframe;         \
    int         ticksleep;         \
    int64_t     ticklast;          \
                                   \
    int         speed;             \
    int         status;            \
    pthread_t   thread;            \
                                   \
    int         completed_counter; \
    int64_t     completed_apts;    \
    int64_t     completed_vpts;    \
    void       *bbox_list;         \
    void (*lock    )(void *ctxt, uint8_t *buffer[8], int linesize[8], int64_t pts); \
    void (*unlock  )(void *ctxt);  \
    void (*setrect )(void *ctxt, int x, int y, int w, int h); \
    void (*setparam)(void *ctxt, int id, void *param);        \
    void (*getparam)(void *ctxt, int id, void *param);        \
    void (*destroy )(void *ctxt);
//-- vdev context common members

#define VDEV_WIN32__MEMBERS \
    HPEN        hbboxpen; \
    HDC         hoverlay; \
    HBITMAP     hoverbmp; \
    BYTE       *poverlay; \
    RECTOVERLAY overlay_rects[8];

// 类型定义
typedef struct {
    VDEV_COMMON_MEMBERS
#ifdef WIN32
    VDEV_WIN32__MEMBERS
#endif
} VDEV_COMMON_CTXT;

#ifdef WIN32
void* vdev_gdi_create(void *surface, int bufnum);
void* vdev_d3d_create(void *surface, int bufnum);
void  vdev_win32_render_overlay(void *ctxt, HDC hdc, int erase);
void  vdev_win32_render_bboxes (void *ctxt, HDC hdc, void *boxlist);
#endif

#ifdef ANDROID
void* vdev_android_create(void *surface, int bufnum);
#endif

// 函数声明
void* vdev_create  (int type, void *surface, int bufnum, int w, int h, int ftime, CMNVARS *cmnvars);
void  vdev_destroy (void *ctxt);
void  vdev_lock    (void *ctxt, uint8_t *buffer[8], int linesize[8], int64_t pts);
void  vdev_unlock  (void *ctxt);
void  vdev_setrect (void *ctxt, int x, int y, int w, int h);
void  vdev_setparam(void *ctxt, int id, void *param);
void  vdev_getparam(void *ctxt, int id, void *param);

// internal helper function
void  vdev_avsync_and_complete(void *ctxt);

#ifdef __cplusplus
}
#endif

#endif



