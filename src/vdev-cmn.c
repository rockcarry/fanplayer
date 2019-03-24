// 包含头文件
#include "vdev.h"

#ifdef WIN32
#include <tchar.h>
#endif

#include "libavutil/log.h"
#include "libavutil/time.h"

// 内部常量定义
#define COMPLETED_COUNTER  10

// 函数实现
void* vdev_create(int type, void *surface, int bufnum, int w, int h, int ftime, CMNVARS *cmnvars)
{
    VDEV_COMMON_CTXT *c = NULL;
#ifdef WIN32
    switch (type) {
    case VDEV_RENDER_TYPE_GDI: c = (VDEV_COMMON_CTXT*)vdev_gdi_create(surface, bufnum, w, h); break;
    case VDEV_RENDER_TYPE_D3D: c = (VDEV_COMMON_CTXT*)vdev_d3d_create(surface, bufnum, w, h); break;
    }
    if (!c) return NULL;
    _tcscpy(c->font_name, DEF_FONT_NAME);
    c->font_size = DEF_FONT_SIZE;
    c->status   |= VDEV_CONFIG_FONT;
#endif
#ifdef ANDROID
    c = (VDEV_COMMON_CTXT*)vdev_android_create(surface, bufnum, w, h);
    if (!c) return NULL;
    c->tickavdiff=-ftime * 2; // 2 should equals to (DEF_ADEV_BUF_NUM - 1)
#endif
    c->surface   = surface;
    c->w         = MAX(w, 1);
    c->h         = MAX(h, 1);
    c->sw        = MAX(w, 1);
    c->sh        = MAX(h, 1);
    c->tickframe = ftime;
    c->ticksleep = ftime;
    c->cmnvars   = cmnvars;
    return c;
}

void vdev_destroy(void *ctxt)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (c->destroy) c->destroy(c);
}

void vdev_lock(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (c->lock) c->lock(c, buffer, linesize);
}

void vdev_unlock(void *ctxt, int64_t pts)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (c->unlock) c->unlock(c, pts);
}

void vdev_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->x  = x; c->y  = y;
    c->w  = w; c->h  = h;
    c->status |= VDEV_ERASE_BG0;
    if (c->setrect) c->setrect(c, x, y, w, h);
}

void vdev_pause(void *ctxt, int pause)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (!ctxt) return;
    if (pause) {
        c->status |=  VDEV_PAUSE;
    } else {
        c->status &= ~VDEV_PAUSE;
    }
}

void vdev_reset(void *ctxt)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (!ctxt) return;
    c->status &= VDEV_CONFIG_FONT;
}

void vdev_setparam(void *ctxt, int id, void *param)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (!ctxt) return;
    switch (id) {
    case PARAM_PLAY_SPEED_VALUE:
        if (param) c->speed = *(int*)param;
        break;
    case PARAM_AVSYNC_TIME_DIFF:
        if (param) c->tickavdiff = *(int*)param;
        break;
    }
    if (c->setparam) c->setparam(c, id, param);
}

void vdev_getparam(void *ctxt, int id, void *param)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (!ctxt || !param) return;

    switch (id) {
    case PARAM_PLAY_SPEED_VALUE:
        *(int*)param = c->speed;
        break;
    case PARAM_AVSYNC_TIME_DIFF:
        *(int*)param = c->tickavdiff;
        break;
    }
    if (c->getparam) c->getparam(c, id, param);
}

#ifdef WIN32
void vdev_textout(void *ctxt, int x, int y, int color, TCHAR *text)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->textx = x;
    c->texty = y;
    c->textc = color;
    c->textt = text;
}

void vdev_textcfg(void *ctxt, TCHAR *fontname, int fontsize)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->font_size = fontsize;
    _tcscpy_s(c->font_name, _countof(c->font_name), fontname);
    c->status |= VDEV_CONFIG_FONT;
}
#endif

int vdev_refresh_background(void *ctxt)
{
    int ret = 1;
#ifdef WIN32
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    RECT rtwin, rect1, rect2, rect3, rect4;
    int  x = c->x, y = c->y, w = c->w, h = c->h;
    HWND hwnd = (HWND)c->surface;
    if (c->status & VDEV_ERASE_BG0) {
        c->status &= ~VDEV_ERASE_BG0;
        GetClientRect(hwnd, &rtwin);
        rect1.left = 0;   rect1.top = 0;   rect1.right = rtwin.right; rect1.bottom = y;
        rect2.left = 0;   rect2.top = y;   rect2.right = x;           rect2.bottom = y+h;
        rect3.left = x+w; rect3.top = y;   rect3.right = rtwin.right; rect3.bottom = y+h;
        rect4.left = 0;   rect4.top = y+h; rect4.right = rtwin.right; rect4.bottom = rtwin.bottom;
        InvalidateRect(hwnd, &rect1, TRUE);
        InvalidateRect(hwnd, &rect2, TRUE);
        InvalidateRect(hwnd, &rect3, TRUE);
        InvalidateRect(hwnd, &rect4, TRUE);
    }
    if (c->status & VDEV_ERASE_BG1) {
        rect1.left = x; rect1.top = y; rect1.right = x + w; rect1.bottom = y + h;
        InvalidateRect(hwnd, &rect1, TRUE);
        ret = 0;
    }
#endif
    return ret;
}

void vdev_avsync_and_complete(void *ctxt)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    int     tickframe, tickdiff, scdiff, avdiff = -1;
    int64_t tickcur, sysclock;

    if (!(c->status & VDEV_PAUSE)) {
        //++ play completed ++//
        if (c->completed_apts != c->cmnvars->apts || c->completed_vpts != c->cmnvars->vpts) {
            c->completed_apts = c->cmnvars->apts;
            c->completed_vpts = c->cmnvars->vpts;
            c->completed_counter = 0;
            c->status &=~VDEV_COMPLETED;
        } else if (!c->cmnvars->apktn && !c->cmnvars->apktn && ++c->completed_counter == COMPLETED_COUNTER) {
            c->status |= VDEV_COMPLETED;
            player_send_message(c->cmnvars->winmsg, MSG_PLAY_COMPLETED, 0);
        }
        //-- play completed --//

        //++ frame rate & av sync control ++//
        tickframe   = 100 * c->tickframe / c->speed;
        tickcur     = av_gettime_relative() / 1000;
        tickdiff    = (int)(tickcur - c->ticklast);
        c->ticklast = tickcur;

        sysclock= c->cmnvars->start_pts + (tickcur - c->cmnvars->start_tick) * c->speed / 100;
        scdiff  = (int)(sysclock - c->cmnvars->vpts - c->tickavdiff); // diff between system clock and video pts
        avdiff  = (int)(c->cmnvars->apts  - c->cmnvars->vpts - c->tickavdiff); // diff between audio and video pts
        avdiff  = c->cmnvars->apts <= 0 ? scdiff : avdiff; // if apts is invalid, sync video to system clock

        if (tickdiff - tickframe >  5) c->ticksleep--;
        if (tickdiff - tickframe < -5) c->ticksleep++;
        if (c->cmnvars->vpts >= 0) {
            if      (avdiff >  500) c->ticksleep -= 3;
            else if (avdiff >  50 ) c->ticksleep -= 2;
            else if (avdiff >  30 ) c->ticksleep -= 1;
            else if (avdiff < -500) c->ticksleep += 3;
            else if (avdiff < -50 ) c->ticksleep += 2;
            else if (avdiff < -30 ) c->ticksleep += 1;
        }
        if (c->ticksleep < 0) c->ticksleep = 0;
        //-- frame rate & av sync control --//
    } else {
        c->ticksleep = c->tickframe;
    }

    if (c->ticksleep > 0 && c->cmnvars->init_params->avts_syncmode != AVSYNC_MODE_LIVE) av_usleep(c->ticksleep * 1000);
    av_log(NULL, AV_LOG_INFO, "d: %3d, s: %3d\n", avdiff, c->ticksleep);
}
