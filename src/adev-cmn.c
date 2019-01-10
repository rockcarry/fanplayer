// 包含头文件
#include "adev.h"

// 函数实现
int swvol_scaler_init(int *scaler, int mindb, int maxdb)
{
    double tabdb[256];
    double tabf [256];
    int    z, i;

    for (i=0; i<256; i++) {
        tabdb[i]  = mindb + (maxdb - mindb) * i / 256.0;
        tabf [i]  = pow(10.0, tabdb[i] / 20.0);
        scaler[i] = (int)((1 << 14) * tabf[i]); // Q14 fix point
    }

    z = -mindb * 256 / (maxdb - mindb);
    z = MAX(z, 0  );
    z = MIN(z, 255);
    scaler[0] = 0;        // mute
    scaler[z] = (1 << 14);// 0db
    return z;
}

void swvol_scaler_run(int16_t *buf, int n, int multiplier)
{
    if (multiplier > (1 << 14)) {
        int64_t v;
        while (n--) {
            v = ((int32_t)*buf * multiplier) >> 14;
            v = MAX(v,-0x7fff);
            v = MIN(v, 0x7fff);
            *buf++ = (int16_t)v;
        }
    } else if (multiplier < (1 << 14)) {
        while (n--) {
            *buf = ((int32_t)*buf * multiplier) >> 14; buf++;
        }
    }
}

void adev_bufcur(void *ctxt, void **buf, int *len)
{
    ADEV_COMMON_CTXT *c = (ADEV_COMMON_CTXT*)ctxt;
    if (!ctxt) return;
    if (buf) *buf = c->bufcur;
    if (len) *len = c->buflen;
}

void adev_setparam(void *ctxt, int id, void *param)
{
    ADEV_COMMON_CTXT *c = (ADEV_COMMON_CTXT*)ctxt;
    if (!ctxt || !param) return;

    switch (id) {
    case PARAM_AUDIO_VOLUME:
        {
            int vol = *(int*)param;
            vol += c->vol_zerodb;
            vol  = MAX(vol, 0  );
            vol  = MIN(vol, 255);;
            c->vol_curvol = vol;
        }
        break;
    }
}

void adev_getparam(void *ctxt, int id, void *param)
{
    ADEV_COMMON_CTXT *c = (ADEV_COMMON_CTXT*)ctxt;
    if (!ctxt || !param) return;

    switch (id) {
    case PARAM_AUDIO_VOLUME:
        *(int*)param = c->vol_curvol - c->vol_zerodb;
        break;
    }
}

