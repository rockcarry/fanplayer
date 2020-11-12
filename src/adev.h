#ifndef __FANPLAYER_ADEV_H__
#define __FANPLAYER_ADEV_H__

// 包含头文件
#include "ffplayer.h"
#include "ffrender.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADEV_SAMPLE_RATE  48000

//++ adev context common members
#define ADEV_COMMON_MEMBERS \
    int64_t    *ppts;       \
    int16_t    *bufcur;     \
    int         bufnum;     \
    int         buflen;     \
    int         head;       \
    int         tail;       \
                            \
    /* common vars */       \
    CMNVARS    *cmnvars;
//-- adev context common members

// 类型定义
typedef struct {
    ADEV_COMMON_MEMBERS
} ADEV_COMMON_CTXT;

// 函数声明
void* adev_create (int type, int bufnum, int buflen, CMNVARS *cmnvars);
void  adev_destroy(void *ctxt);
void  adev_write  (void *ctxt, uint8_t *buf, int len, int64_t pts);
void  adev_pause  (void *ctxt, int pause);
void  adev_reset  (void *ctxt);

#ifdef __cplusplus
}
#endif

#endif

