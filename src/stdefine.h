/* 标准头文件 */
#ifndef __STDEFINE_H__
#define __STDEFINE_H__


#ifdef WIN32
// headers
#include <windows.h>
#include <inttypes.h>

// disable warnings
#pragma warning(disable:4996)

// configs
#define CONFIG_ENABLE_VEFFECT    1
#define CONFIG_ENABLE_SNAPSHOT   1
#define CONFIG_ENABLE_SOUNDTOUCH 1
#endif


#ifdef ANDROID
// headers
#include <limits.h>
#include <inttypes.h>
#include <android/log.h>

#define MAX_PATH  PATH_MAX

typedef struct {
    long left;
    long top;
    long right;
    long bottom;
} RECT;

// configs
#define CONFIG_ENABLE_VEFFECT    0
#define CONFIG_ENABLE_SNAPSHOT   0
#define CONFIG_ENABLE_SOUNDTOUCH 0
#endif


#define DO_USE_VAR(a) do { a = a; } while (0)


#endif


