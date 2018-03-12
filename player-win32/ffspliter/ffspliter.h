#ifndef __FFSPLITER_H__
#define __FFSPLITER_H__

#ifdef __cplusplus
extern "C" {
#endif

// 类型定义
typedef void (*PFN_SPC)(__int64 cur, __int64 total); // split progress callback

// 函数声明
int split_media_file(char *dst, char *src, __int64 start, __int64 end, PFN_SPC spc);

#ifdef __cplusplus
}
#endif

#endif

