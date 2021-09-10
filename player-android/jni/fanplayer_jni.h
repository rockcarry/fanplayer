#include <stdint.h>
#include <jni.h>

#ifndef _included_fanplayer_jni_
#define _included_fanplayer_jni_

JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*);

#ifdef __cplusplus
extern "C" {
#endif

void  JniAttachCurrentThread(void);
void  JniDetachCurrentThread(void);
void *JniRequestWinObj(void *data);
void  JniReleaseWinObj(void *data);
void  JniPostMessage  (void *extra, int32_t msg, void *param);

#ifdef __cplusplus
}
#endif

#endif
