// 包含头文件
#include "adev.h"

#pragma warning(disable:4311)
#pragma warning(disable:4312)

// 内部常量定义
#define DEF_ADEV_BUF_NUM  3
#define DEF_ADEV_BUF_LEN  2048

// 内部类型定义
typedef struct {
    ADEV_COMMON_MEMBERS
    HWAVEOUT hWaveOut;
    WAVEHDR *pWaveHdr;
    HANDLE   bufsem;
} ADEV_CONTEXT;

// 内部函数实现
static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)dwInstance;
    switch (uMsg) {
    case WOM_DONE:
        c->bufcur = (int16_t*)c->pWaveHdr[c->head].lpData;
        c->cmnvars->apts = c->ppts[c->head];
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->bufsem, 1, NULL);
        av_log(NULL, AV_LOG_INFO, "apts: %lld\n", c->cmnvars->apts);
        break;
    }
}

// 接口函数实现
void* adev_create(int type, int bufnum, int buflen, CMNVARS *cmnvars)
{
    ADEV_CONTEXT *ctxt = NULL;
    WAVEFORMATEX  wfx  = {0};
    BYTE         *pwavbuf;
    MMRESULT      result;
    int           i;

    bufnum = bufnum ? bufnum : DEF_ADEV_BUF_NUM;
    buflen = buflen ? buflen : DEF_ADEV_BUF_LEN;

    // allocate adev context
    ctxt = (ADEV_CONTEXT*)calloc(1, sizeof(ADEV_CONTEXT) + bufnum * sizeof(int64_t) + bufnum * (sizeof(WAVEHDR) + buflen));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate adev context !\n");
        exit(0);
    }

    ctxt->bufnum   = bufnum;
    ctxt->buflen   = buflen;
    ctxt->ppts     = (int64_t*)((uint8_t*)ctxt + sizeof(ADEV_CONTEXT));
    ctxt->pWaveHdr = (WAVEHDR*)((uint8_t*)ctxt->ppts + bufnum * sizeof(int64_t));
    ctxt->bufsem   = CreateSemaphore(NULL, bufnum, bufnum, NULL);
    ctxt->cmnvars  = cmnvars;
    if (!ctxt->bufsem) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate waveout buffer and waveout semaphore !\n");
        exit(0);
    }

    // init for audio
    wfx.cbSize          = sizeof(wfx);
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.wBitsPerSample  = 16;    // 16bit
    wfx.nSamplesPerSec  = ADEV_SAMPLE_RATE;
    wfx.nChannels       = 2;     // stereo
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    result = waveOutOpen(&ctxt->hWaveOut, ctxt->cmnvars->init_params->waveout_device_id, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)ctxt, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        CloseHandle(ctxt->bufsem);
        free(ctxt);
        return NULL;
    }

    // init wavebuf
    pwavbuf = (BYTE*)(ctxt->pWaveHdr + bufnum);
    for (i=0; i<bufnum; i++) {
        ctxt->pWaveHdr[i].lpData         = (LPSTR)(pwavbuf + i * buflen);
        ctxt->pWaveHdr[i].dwBufferLength = buflen;
        waveOutPrepareHeader(ctxt->hWaveOut, &ctxt->pWaveHdr[i], sizeof(WAVEHDR));
    }
    return ctxt;
}

void adev_destroy(void *ctxt)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    int           i;
    if (!ctxt) return;

    // close waveout
    if (c->hWaveOut) {
        waveOutReset(c->hWaveOut);
        for (i=0; i<c->bufnum; i++) {
            waveOutUnprepareHeader(c->hWaveOut, &c->pWaveHdr[i], sizeof(WAVEHDR));
        }
        waveOutClose(c->hWaveOut);
    }

    // close semaphore
    CloseHandle(c->bufsem);

    // free memory
    free(c);
}

void adev_write(void *ctxt, uint8_t *buf, int len, int64_t pts)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    if (!ctxt || WAIT_OBJECT_0 != WaitForSingleObject(c->bufsem, -1) || (c->status & ADEV_CLOSE)) return;
    memcpy(c->pWaveHdr[c->tail].lpData, buf, MIN((int)c->pWaveHdr[c->tail].dwBufferLength, len));
    waveOutWrite(c->hWaveOut, &c->pWaveHdr[c->tail], sizeof(WAVEHDR));
    c->ppts[c->tail] = pts; if (++c->tail == c->bufnum) c->tail = 0;
}

void adev_setparam(void *ctxt, int id, void *param) {}
void adev_getparam(void *ctxt, int id, void *param) {}
