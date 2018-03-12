#ifndef __FFPLAYER_ADEV_H__
#define __FFPLAYER_ADEV_H__

// 包含头文件
#include "ffrender.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
// 类型定义
typedef struct {
    BYTE  *lpdata;
    DWORD  buflen;
} AUDIOBUF;
#endif

#define ADEV_SAMPLE_RATE  44100

//++ adev context common members
#define ADEV_COMMON_MEMBERS         \
    int64_t *ppts;                  \
    int      bufnum;                \
    int      buflen;                \
    int      head;                  \
    int      tail;                  \
    int64_t *apts;                  \
                                    \
    /* store current audio data */  \
    int16_t *curdata;               \
                                    \
    /* software volume */           \
    int      vol_scaler[256];       \
    int      vol_zerodb;            \
    int      vol_curvol;
//-- adev context common members

// 类型定义
typedef struct {
    ADEV_COMMON_MEMBERS
} ADEV_COMMON_CTXT;

// 函数声明
void* adev_create  (int type, int bufnum, int buflen);
void  adev_destroy (void *ctxt);
void  adev_lock    (void *ctxt, AUDIOBUF **ppab);
void  adev_unlock  (void *ctxt, int64_t pts);
void  adev_pause   (void *ctxt, int pause);
void  adev_reset   (void *ctxt);
void  adev_syncapts(void *ctxt, int64_t *apts);
void  adev_curdata (void *ctxt, void **buf, int *len );
void  adev_setparam(void *ctxt, int id, void *param);
void  adev_getparam(void *ctxt, int id, void *param);

#define SW_VOLUME_MINDB  -30
#define SW_VOLUME_MAXDB  +12
int   swvol_scaler_init(int *scaler, int mindb, int maxdb);
void  swvol_scaler_run (int16_t *buf, int n, int multiplier);

#ifdef __cplusplus
}
#endif

#endif

