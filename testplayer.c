#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "fanplayer.h"

#ifdef WITH_LIBAVDEV
#include "libavdev/adev.h"
#include "libavdev/vdev.h"
#include "libavdev/idev.h"
#define ADEV_SAMPRATE    48000
#define ADEV_CHANNELS    2
#define ADEV_FRAME_SIZE (ADEV_SAMPRATE / 20)
#define ADEV_FRAME_NUM   8
#endif

#ifdef WIN32
#include <windows.h>
static int open_file_dialog(HWND hwnd, char *name, int len)
{
    wchar_t file[256] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = hwnd;
    ofn.lpstrFilter     = L"AVI Files (*.avi)\0*.avi\0FLV Files (*.flv)\0*.flv\0MP3 Files (*.mp3)\0*.mp3\0MP4 Files (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex    = 4;
    ofn.lpstrFile       = file;
    ofn.nMaxFile        = sizeof(file) / sizeof(file[0]);
    ofn.lpstrFileTitle  = NULL;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle      = L"Open File";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt     = NULL;
    if (!GetOpenFileNameW(&ofn)) return -1;
    WideCharToMultiByte(CP_UTF8, 0, file, -1, name, len, NULL, NULL);
    return 0;
}
#else
static int open_file_dialog(HWND hwnd, char *name, int len) { return -1; }
#endif

typedef struct {
    void *adev;
    void *vdev;
    void *idev;
    void *player;
    int   playing;
    FILE *fp;
} MYAPP;

#ifdef WITH_LIBAVDEV
static char* gen_file_name(char *name, int len, char *ext)
{
    time_t tt = time(NULL);
    struct tm tm;
    localtime_s(&tm, &tt);
    snprintf(name, len, "rec-%d-%02d-%02d-%02d%02d%02d.%s", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ext);
    return name;
}

#ifdef WITH_LIBAVDEV
static long my_idev_cb(void *cbctx, int type, void *buf, int len)
{
    MYAPP *app  = cbctx;
    IDEV  *idev = buf;
    char   file[256];
    int    playing;
    switch (type) {
    case IDEV_CALLBACK_KEY_EVENT:
        if (buf) {
            switch (len) {
            case ' ':
                playing = (intptr_t)player_get(app->player, PLAYER_KEY_STATE, NULL) == 1;
                player_set(app->player, PLAYER_KEY_STATE, (void*)(playing ? 2 : 1));
                break;
            case 'S': player_set(app->player, PLAYER_KEY_STRETCH, (void*)(intptr_t)(!player_get(app->player, PLAYER_KEY_STRETCH, NULL))); break;
            case 189: player_set(app->player, PLAYER_KEY_SPEED  , (void*)(intptr_t)( player_get(app->player, PLAYER_KEY_SPEED  , NULL) - 10)); break;
            case 187: player_set(app->player, PLAYER_KEY_SPEED  , (void*)(intptr_t)( player_get(app->player, PLAYER_KEY_SPEED  , NULL) + 10)); break;
            case 'R':
                if (player_get(app->player, PLAYER_KEY_RECORDING, NULL)) {
                    player_set(app->player, PLAYER_KEY_RECFILE  , NULL);
                } else {
                    player_set(app->player, PLAYER_KEY_RECFILE  , gen_file_name(file, sizeof(file), "avi"));
                }
                break;
            case 'P':
                player_set(app->player, PLAYER_KEY_SNAPSHOT, gen_file_name(file, sizeof(file), "png"));
                break;
            case 'O':
                if (0 == open_file_dialog((HWND)vdev_get(app->vdev, VDEV_KEY_HWND, NULL), file, sizeof(file))) {
                    player_set(app->player, PLAYER_KEY_URL, file);
                    player_set(app->player, PLAYER_KEY_STATE, (void*)4);
                }
                break;
            }
        }
        break;
    case IDEV_CALLBACK_MOUSE_MOVE:
    case IDEV_CALLBACK_MOUSE_LBTNDOWN:
        if (idev && idev->curr_mouse_y > vdev_get(app->vdev, VDEV_KEY_HEIGHT, NULL) - 16 && (idev->curr_mouse_btns & 1)) {
            uint32_t duration = (intptr_t)player_get(app->player, PLAYER_KEY_MEDIA_DURATION, NULL);
            player_set(app->player, PLAYER_KEY_MEDIA_POSITION, (void*)(duration * idev->curr_mouse_x / (intptr_t)vdev_get(app->vdev, VDEV_KEY_WIDTH, NULL)));
        }
        break;
    }
    return 0;
}
#endif

static void bar(BMP *bmp, int x, int y, int w, int h, int c)
{
    uint32_t *p = (uint32_t*)bmp->pdata + y * bmp->width + x;
    int  i, j;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) *p++ = c;
        p += bmp->width - w;
    }
}
#endif

static long my_player_cb(void *cbctx, int msg, void *buf, int len)
{
    MYAPP *app = cbctx;
    switch (msg) {
    case PLAYER_OPEN_SUCCESS: {
            int vw  = player_get(app->player, PLAYER_KEY_VIDEO_WIDTH , NULL);
            int vh  = player_get(app->player, PLAYER_KEY_VIDEO_HEIGHT, NULL);
            int max = vw > vh ? vw : vh;
            char str[128]; snprintf(str, sizeof(str), "sw:%f,sh:%f", vw * 20.0 / max, vh * 20.0 / max);
            vdev_set(app->vdev, VDEV_KEY_SURFACE_PARAMS, str);
            player_set(app->player, PLAYER_KEY_STATE, (void*)1);
        }
        break;
    case PLAYER_PLAY_COMPLETED:
        printf("play completed !\n");
        break;
    case PLAYER_AVIO_READ:
        return app->fp ? fread(buf, 1, len, app->fp) : 0;
    case PLAYER_AVIO_SEEK:
        if (!app->fp) break;
        if (len == 0x10000) { // get file size
            size_t cur = ftell(app->fp);
            fseek(app->fp, 0  , SEEK_END);
            *(int64_t*)buf = ftell(app->fp);
            fseek(app->fp, cur, SEEK_SET);
        } else {
            fseek(app->fp, *(int64_t*)buf, len);
        }
        break;
#ifdef WITH_LIBAVDEV
    case PLAYER_ADEV_SAMPRATE:
        return ADEV_SAMPRATE;
    case PLAYER_ADEV_CHANNELS:
        return ADEV_CHANNELS;
    case PLAYER_AVSYNC_DELTA:
        return 1000 * ADEV_FRAME_SIZE * ADEV_FRAME_NUM / ADEV_SAMPRATE;
    case PLAYER_ADEV_WRITE:
        adev_play(app->adev, buf, len, 100);
        break;
    case PLAYER_VDEV_LOCK: {
            BMP     *bmp  = vdev_lock(app->vdev, 0);
            SURFACE *surf = buf;
            surf->w       = bmp ? bmp->width  : 0;
            surf->h       = bmp ? bmp->height : 0;
            surf->data    = bmp ? bmp->pdata  : NULL;
            surf->stride  = bmp ? bmp->stride : 0;
            surf->format  = SURFACE_FMT_RGB32;
            surf->cdepth  = bmp ? bmp->cdepth : 32;
        }
        break;
    case PLAYER_VDEV_UNLOCK:
        vdev_unlock(app->vdev, 0);
        vdev_render(app->vdev);
        break;
#endif
    }
    return 0;
}

int main(int argc, char *argv[])
{
    MYAPP myapp     = {};
    char  url[256]  = "";
    char *initparams= "";
    int   i;

    for (i = 1; i < argc; i++) {
        if (strstr(argv[i], "--init_params=") == argv[i]) initparams = argv[i] + sizeof("--init_params=") - 1;
        else strncpy(url, argv[i], sizeof(url) - 1);
    }
    if (strlen(url) == 0) {
        if (open_file_dialog(NULL, url, sizeof(url)) != 0) return 0;
    }

    printf("url   : %s\n", url       );
    printf("params: %s\n", initparams);

#ifdef WITH_LIBAVDEV
    char str[256];
    snprintf(str, sizeof(str), "samprate:%d,chnnum:%d,frmsize:%d,frmnum:%d", ADEV_SAMPRATE, ADEV_CHANNELS, ADEV_FRAME_SIZE, ADEV_FRAME_NUM);
    myapp.adev = adev_init(str, NULL, NULL);
    snprintf(str, sizeof(str), "title:fanplayer,surfaces:3,resize,show");
    myapp.vdev = vdev_init(str, my_idev_cb, &myapp);
    vdev_set(myapp.vdev, "s_surface_params1", "parent:-1,sw:0.999999,sh:3,sx:center,sy:-0.0000000000001");
    myapp.idev = (void*)vdev_get(myapp.vdev, VDEV_KEY_IDEV, NULL);
#endif

    if (initparams && strstr(initparams, "i_use_avio")) myapp.fp = fopen(url, "rb");
    myapp.player = player_init(initparams, my_player_cb, &myapp);
    player_set(myapp.player, PLAYER_KEY_URL  , url     );
    player_set(myapp.player, PLAYER_KEY_STATE, (void*)1);

#ifdef WITH_LIBAVDEV
    while (vdev_get(myapp.vdev, VDEV_KEY_STATE, NULL) != VDEV_CALLBACK_VDEV_CLOSED) {
        BMP *bmp = vdev_lock(myapp.vdev, 1);
        if (bmp) {
            uint32_t duration = (intptr_t)player_get(myapp.player, PLAYER_KEY_MEDIA_DURATION, NULL);
            uint32_t position = (intptr_t)player_get(myapp.player, PLAYER_KEY_MEDIA_POSITION, NULL);
            uint32_t w = bmp->width * position / (duration ? duration : 1);
            w = w < bmp->width ? w : bmp->width;
            bar(bmp, 0, 0, w, bmp->height, 0xFF8800);
            bar(bmp, w, 0, bmp->width - w, bmp->height, 0);
            vdev_unlock(myapp.vdev, 1);
        }
        usleep(100 * 1000);
    }
#endif

    player_exit(myapp.player);
    if (myapp.fp) fclose(myapp.fp);

#ifdef WITH_LIBAVDEV
    vdev_exit(myapp.vdev);
    adev_exit(myapp.adev);
#endif
    return 0;
}
