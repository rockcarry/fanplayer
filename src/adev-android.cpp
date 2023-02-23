// 包含头文件
#include <jni.h>
#include <unistd.h>
#include <pthread.h>
#include "adev.h"

// 类型定义
typedef struct {
    int16_t *data;
    int32_t  size;
} AUDIOBUF;

// for jni
JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);

// 内部常量定义
#define DEF_ADEV_BUF_NUM  3
#define DEF_ADEV_BUF_LEN  2048

// 内部类型定义
typedef struct {
    ADEV_COMMON_MEMBERS

    uint8_t   *pWaveBuf;
    AUDIOBUF  *pWaveHdr;
    int        curnum;

    //++ for audio render thread
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    pthread_t  thread;
    //-- for audio render thread

    // for jni
    jobject    jobj_at;
    jmethodID  jmid_at_init ;
    jmethodID  jmid_at_close;
    jmethodID  jmid_at_play ;
    jmethodID  jmid_at_pause;
    jmethodID  jmid_at_write;
    jbyteArray audio_buffer;
} ADEV_CONTEXT;

static void* audio_render_thread_proc(void *param)
{
    JNIEnv     *env = get_jni_env();
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)param;

    // start audiotrack
    env->CallVoidMethod(c->jobj_at, c->jmid_at_play);

    while (!(c->status & ADEV_CLOSE)) {
        pthread_mutex_lock(&c->lock);
        while (c->curnum == 0 && !(c->status & ADEV_CLOSE))  pthread_cond_wait(&c->cond, &c->lock);
        if (!(c->status & ADEV_CLOSE)) {
            env->CallIntMethod(c->jobj_at, c->jmid_at_write, c->audio_buffer, c->head * c->buflen, c->pWaveHdr[c->head].size);
            c->curnum--; c->bufcur = c->pWaveHdr[c->head].data;
            c->cmnvars->apts = c->ppts[c->head];
            if (++c->head == c->bufnum) c->head = 0;
            pthread_cond_signal(&c->cond);
        }
        pthread_mutex_unlock(&c->lock);
    }

    // close audiotrack
    env->CallVoidMethod(c->jobj_at, c->jmid_at_close);

    // need detach current thread
    get_jni_jvm()->DetachCurrentThread();
    return NULL;
}

// 接口函数实现
void* adev_create(int type, int bufnum, int buflen, CMNVARS *cmnvars)
{
    JNIEnv       *env  = get_jni_env();
    ADEV_CONTEXT *ctxt = NULL;
    int           i;

    DO_USE_VAR(type);
    bufnum = bufnum ? bufnum : DEF_ADEV_BUF_NUM;
    buflen = buflen ? buflen : DEF_ADEV_BUF_LEN;

    // allocate adev context
    ctxt = (ADEV_CONTEXT*)calloc(1, sizeof(ADEV_CONTEXT) + bufnum * sizeof(int64_t) + bufnum * sizeof(AUDIOBUF));
    if (!ctxt) return NULL;
    ctxt->bufnum   = bufnum;
    ctxt->buflen   = buflen;
    ctxt->ppts     = (int64_t *)((uint8_t*)ctxt + sizeof(ADEV_CONTEXT));
    ctxt->pWaveHdr = (AUDIOBUF*)((uint8_t*)ctxt->ppts + bufnum * sizeof(int64_t));
    ctxt->cmnvars  = cmnvars;

    // new buffer
    jbyteArray local_audio_buffer = env->NewByteArray(bufnum * buflen);
    ctxt->audio_buffer = (jbyteArray)env->NewGlobalRef(local_audio_buffer);
    ctxt->pWaveBuf     = (uint8_t  *)env->GetByteArrayElements(ctxt->audio_buffer, 0);
    env->DeleteLocalRef(local_audio_buffer);

    // init wavebuf
    for (i=0; i<bufnum; i++) {
        ctxt->pWaveHdr[i].data = (int16_t*)(ctxt->pWaveBuf + i * buflen);
        ctxt->pWaveHdr[i].size = buflen;
    }

    jclass jcls         = env->FindClass("android/media/AudioTrack");
    ctxt->jmid_at_init  = env->GetMethodID(jcls, "<init>" , "(IIIIII)V");
    ctxt->jmid_at_close = env->GetMethodID(jcls, "release", "()V");
    ctxt->jmid_at_play  = env->GetMethodID(jcls, "play"   , "()V");
    ctxt->jmid_at_pause = env->GetMethodID(jcls, "pause"  , "()V");
    ctxt->jmid_at_write = env->GetMethodID(jcls, "write"  , "([BII)I");

    // new AudioRecord
    #define STREAM_MUSIC        3
    #define ENCODING_PCM_16BIT  2
    #define CHANNEL_STEREO      3
    #define MODE_STREAM         1
    jobject at_obj = env->NewObject(jcls, ctxt->jmid_at_init, STREAM_MUSIC, ADEV_SAMPLE_RATE, CHANNEL_STEREO, ENCODING_PCM_16BIT, ctxt->buflen * 2, MODE_STREAM);
    ctxt->jobj_at  = env->NewGlobalRef(at_obj);
    env->DeleteLocalRef(at_obj);

    // create mutex & cond
    pthread_mutex_init(&ctxt->lock, NULL);
    pthread_cond_init (&ctxt->cond, NULL);

    // create audio rendering thread
    pthread_create(&ctxt->thread, NULL, audio_render_thread_proc, ctxt);
    return ctxt;
}

void adev_destroy(void *ctxt)
{
    if (!ctxt) return;
    JNIEnv *env = get_jni_env();
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;

    // make audio rendering thread safely exit
    pthread_mutex_lock(&c->lock);
    c->status |= ADEV_CLOSE;
    pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->lock);
    pthread_join(c->thread, NULL);

    // close mutex & cond
    pthread_mutex_destroy(&c->lock);
    pthread_cond_destroy (&c->cond);

    // for jni
    env->ReleaseByteArrayElements(c->audio_buffer, (jbyte*)c->pWaveBuf, 0);
    env->DeleteGlobalRef(c->audio_buffer);
    env->DeleteGlobalRef(c->jobj_at     );

    // free adev
    free(c);
}

void adev_write(void *ctxt, uint8_t *buf, int len, int64_t pts)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    pthread_mutex_lock(&c->lock);
    while (c->curnum == c->bufnum && (c->status & ADEV_CLOSE) == 0)  pthread_cond_wait(&c->cond, &c->lock);
    if (c->curnum < c->bufnum) {
        memcpy(c->pWaveHdr[c->tail].data, buf, MIN(c->pWaveHdr[c->tail].size, len));
        c->curnum++; c->ppts[c->tail] = pts; if (++c->tail == c->bufnum) c->tail = 0;
        pthread_cond_signal(&c->cond);
    }
    pthread_mutex_unlock(&c->lock);
}

void adev_setparam(void *ctxt, int id, void *param) {}
void adev_getparam(void *ctxt, int id, void *param) {}
