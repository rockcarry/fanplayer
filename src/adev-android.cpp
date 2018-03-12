// 包含头文件
#include <jni.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "adev.h"

// for jni
JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);

// 内部常量定义
#define DEF_ADEV_BUF_NUM  5
#define DEF_ADEV_BUF_LEN  2048

// 内部类型定义
typedef struct
{
    ADEV_COMMON_MEMBERS

    uint8_t   *pWaveBuf;
    AUDIOBUF  *pWaveHdr;

    //++ for audio render thread
    sem_t      semr;
    sem_t      semw;
    #define ADEV_CLOSE (1 << 0)
    #define ADEV_PAUSE (1 << 1)
    int        status;
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

    while (1) {
        if (c->status & ADEV_PAUSE) {
            usleep(10*1000);
            continue;
        }

        sem_wait(&c->semr);
        if (c->status & ADEV_CLOSE) break;

        if (c->pWaveHdr[c->head].size) {
            if (c->vol_curvol) {
                env->CallIntMethod(c->jobj_at, c->jmid_at_write, c->audio_buffer, c->head * c->buflen, c->pWaveHdr[c->head].size);
            }
            c->pWaveHdr[c->head].size = 0;
        }
        if (c->apts) *c->apts = c->ppts[c->head];
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);
    }

    // close audiotrack
    env->CallVoidMethod(c->jobj_at, c->jmid_at_close);

    // need detach current thread
    get_jni_jvm()->DetachCurrentThread();
    return NULL;
}

// 接口函数实现
void* adev_create(int type, int bufnum, int buflen)
{
    JNIEnv       *env  = get_jni_env();
    ADEV_CONTEXT *ctxt = NULL;
    int           i;

    DO_USE_VAR(type);

    // allocate adev context
    ctxt = (ADEV_CONTEXT*)calloc(1, sizeof(ADEV_CONTEXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate adev context !\n");
        exit(0);
    }

    bufnum         = bufnum ? bufnum : DEF_ADEV_BUF_NUM;
    buflen         = buflen ? buflen : DEF_ADEV_BUF_LEN;
    ctxt->bufnum   = bufnum;
    ctxt->buflen   = buflen;
    ctxt->head     = 0;
    ctxt->tail     = 0;
    ctxt->ppts     = (int64_t *)calloc(bufnum, sizeof(int64_t));
    ctxt->pWaveHdr = (AUDIOBUF*)calloc(bufnum, sizeof(AUDIOBUF));

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
    jobject at_obj = env->NewObject(jcls, ctxt->jmid_at_init,
        STREAM_MUSIC, ADEV_SAMPLE_RATE, CHANNEL_STEREO, ENCODING_PCM_16BIT, ctxt->buflen * 2, MODE_STREAM);
    ctxt->jobj_at  = env->NewGlobalRef(at_obj);
    env->DeleteLocalRef(at_obj);

    // start audiotrack
    env->CallVoidMethod(ctxt->jobj_at, ctxt->jmid_at_play);

    // init software volume scaler
    ctxt->vol_zerodb = swvol_scaler_init(ctxt->vol_scaler, SW_VOLUME_MINDB, SW_VOLUME_MAXDB);
    ctxt->vol_curvol = ctxt->vol_zerodb;

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    // create audio rendering thread
    pthread_create(&ctxt->thread, NULL, audio_render_thread_proc, ctxt);
    return ctxt;
}

void adev_destroy(void *ctxt)
{
    JNIEnv *env = get_jni_env();
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;

    // make audio rendering thread safely exit
    c->status = ADEV_CLOSE;
    sem_post(&c->semr);
    pthread_join(c->thread, NULL);

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

    // free buffers
    free(c->ppts);
    free(c->pWaveHdr);

    // for jni
    env->ReleaseByteArrayElements(c->audio_buffer, (jbyte*)c->pWaveBuf, 0);
    env->DeleteGlobalRef(c->audio_buffer);
    env->DeleteGlobalRef(c->jobj_at     );

    // free adev
    free(c);
}

void adev_lock(void *ctxt, AUDIOBUF **ppab)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    sem_wait(&c->semw);
    *ppab = (AUDIOBUF*)&c->pWaveHdr[c->tail];
    (*ppab)->size = c->buflen;
}

void adev_unlock(void *ctxt, int64_t pts)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    c->ppts[c->tail] = pts;

    //++ software volume scale
    int      multiplier = c->vol_scaler[c->vol_curvol];
    int16_t *buf        = c->pWaveHdr[c->tail].data;
    int      n          = c->pWaveHdr[c->tail].size / sizeof(int16_t);
    swvol_scaler_run(buf, n, multiplier);
    //-- software volume scale

    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

void adev_pause(void *ctxt, int pause)
{
    JNIEnv *env = get_jni_env();
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    if (pause) {
        c->status |=  ADEV_PAUSE;
        env->CallVoidMethod(c->jobj_at, c->jmid_at_pause);
    }
    else {
        c->status &= ~ADEV_PAUSE;
        env->CallVoidMethod(c->jobj_at, c->jmid_at_play );
    }
}

void adev_reset(void *ctxt)
{
    JNIEnv *env = get_jni_env();
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    while (0 == sem_trywait(&c->semr)) {
        sem_post(&c->semw);
    }
    c->head   = 0;
    c->tail   = 0;
    c->status = 0;
}

