#ifndef __FANPLAYER_VDEV_H__
#define __FANPLAYER_VDEV_H__

// 包含头文件
#include <pthread.h>
#include <semaphore.h>
#include "ffplayer.h"
#include "ffrender.h"

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
#define VDEV_CLOSE       (1 << 0)
#define VDEV_PAUSE       (1 << 1)
#define VDEV_COMPLETED   (1 << 2)
#define VDEV_ERASE_BG0   (1 << 3)
#define VDEV_ERASE_BG1   (1 << 4)
#define VDEV_CONFIG_FONT (1 << 5)

//++ vdev context common members
#define VDEV_COMMON_MEMBERS \
    int         bufnum; \
    int         pixfmt; \
    int         x;   /* video display rect x */ \
    int         y;   /* video display rect y */ \
    int         w;   /* video display rect w */ \
    int         h;   /* video display rect h */ \
    int         sw;  /* surface width        */ \
    int         sh;  /* surface height       */ \
                                                \
    void       *surface;                        \
    int64_t    *ppts;                           \
                                                \
    int         head;                           \
    int         tail;                           \
    sem_t       semr;                           \
    sem_t       semw;                           \
                                                \
    CMNVARS    *cmnvars;                        \
    int         tickavdiff;                     \
    int         tickframe;                      \
    int         ticksleep;                      \
    int64_t     ticklast;                       \
                                                \
    int         speed;                          \
    int         status;                         \
    pthread_t   thread;                         \
                                                \
    int         completed_counter;              \
    int64_t     completed_apts;                 \
    int64_t     completed_vpts;                 \
                                                \
    int         textx;                          \
    int         texty;                          \
    int         textc;                          \
    TCHAR      *textt;                          \
    TCHAR       font_name[32];                  \
    int         font_size;                      \
    void (*lock    )(void *ctxt, uint8_t *buffer[8], int linesize[8]); \
    void (*unlock  )(void *ctxt, int64_t pts);                         \
    void (*setrect )(void *ctxt, int x, int y, int w, int h);          \
    void (*setparam)(void *ctxt, int id, void *param);                 \
    void (*getparam)(void *ctxt, int id, void *param);                 \
    void (*destroy )(void *ctxt);
//-- vdev context common members

// 类型定义
typedef struct {
    VDEV_COMMON_MEMBERS
} VDEV_COMMON_CTXT;

#ifdef WIN32
void* vdev_gdi_create(void *surface, int bufnum, int w, int h);
void* vdev_d3d_create(void *surface, int bufnum, int w, int h);
#endif

#ifdef ANDROID
void* vdev_android_create(void *surface, int bufnum, int w, int h);
#endif

// 函数声明
void* vdev_create  (int type, void *surface, int bufnum, int w, int h, int ftime, CMNVARS *cmnvars);
void  vdev_destroy (void *ctxt);
void  vdev_lock    (void *ctxt, uint8_t *buffer[8], int linesize[8]);
void  vdev_unlock  (void *ctxt, int64_t pts);
void  vdev_setrect (void *ctxt, int x, int y, int w, int h);
void  vdev_pause   (void *ctxt, int pause);
void  vdev_reset   (void *ctxt);
void  vdev_setparam(void *ctxt, int id, void *param);
void  vdev_getparam(void *ctxt, int id, void *param);

#ifdef WIN32
#define DEF_FONT_SIZE   32
#define DEF_FONT_NAME   TEXT("Arial")
void  vdev_textout (void *ctxt, int x, int y, int color, TCHAR *text);
void  vdev_textcfg (void *ctxt, TCHAR *fontname, int fontsize);
#endif

// internal helper function
int   vdev_refresh_background (void *ctxt);
void  vdev_avsync_and_complete(void *ctxt);

#ifdef __cplusplus
}
#endif

#endif



