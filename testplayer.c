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
#define ADEV_FRAME_SIZE (48000 / 20)
#define ADEV_FRAME_NUM   8
#endif

#ifdef WIN32
#include <windows.h>
static int open_file_dialog(HWND hwnd, char *name, int len)
{
    OPENFILENAME ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = hwnd;
    ofn.lpstrFilter     = "AVI Files (*.avi)\0*.avi\0FLV Files (*.flv)\0*.flv\0MP3 Files (*.mp3)\0*.mp3\0MP4 Files (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex    = 4;
    ofn.lpstrFile       = name;
    ofn.nMaxFile        = len;
    ofn.lpstrFileTitle  = NULL;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle      = "Open File";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt     = NULL;
    return GetOpenFileName(&ofn) ? 0 : -1;
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

static int my_videv_cb(void *cbctx, int msg, uint32_t param1, uint32_t param2, uint32_t param3)
{
    MYAPP *app = cbctx;
    char   file[256];
    switch (msg) {
    case DEV_MSG_KEY_EVENT:
        if (param1) {
            switch (param2) {
            case ' ': player_set(app->player, "play"   , (void*)(!player_get(app->player, "play"   , NULL))); break;
            case 'S': player_set(app->player, "stretch", (void*)(!player_get(app->player, "stretch", NULL))); break;
            case 189: player_set(app->player, "speed"  , (void*)( player_get(app->player, "speed"  , NULL) - 10)); break;
            case 187: player_set(app->player, "speed"  , (void*)( player_get(app->player, "speed"  , NULL) + 10)); break;
            case 'R':
                if (player_get(app->player, "record", NULL)) {
                    player_set(app->player, "record", NULL);
                } else {
                    player_set(app->player, "record", gen_file_name(file, sizeof(file), "avi"));
                }
                break;
            case 'P':
                player_set(app->player, "snapshot", gen_file_name(file, sizeof(file), "png"));
                break;
            }
        }
        break;
    case DEV_MSG_MOUSE_MOVE:
    case DEV_MSG_MOUSE_LBUTTON_D:
        if (param3 & 1) { // mouse lbutton down or drag
            uint32_t duration = player_get(app->player, PARAM_MEDIA_DURATION, NULL);
            player_seek(app->player, duration * param1 / vdev_get(app->vdev, "width", NULL));
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

static int my_player_cb(void *cbctx, int msg, void *buf, int len)
{
    MYAPP *app = cbctx;
    switch (msg) {
    case PLAYER_OPEN_SUCCESS:
        player_set(app->player, "play", (void*)1);
        break;
    case PLAYER_PLAY_COMPLETED:
        printf("play completed !\n");
        break;
    case PLAYER_ADEV_SAMPRATE:
        return 48000;
    case PLAYER_ADEV_CHANNELS:
        return 2;
#ifdef WITH_LIBAVDEV
    case PLAYER_ADEV_FRAMENUM:
        return ADEV_FRAME_NUM / 2;
    case PLAYER_ADEV_WRITE:
        adev_play(app->adev, buf, len, 100);
        break;
    case PLAYER_VDEV_LOCK: {
            BMP     *bmp  = vdev_lock(app->vdev);
            SURFACE *surf = buf;
            surf->w       = bmp ? bmp->width : 0;
            surf->h       = bmp ? bmp->height: 0;
            surf->data    = bmp ? bmp->pdata : NULL;
            surf->stride  = bmp ? bmp->stride: 0;
            surf->format  = bmp ? bmp->pixfmt: SURFACE_FMT_RGB32;
            surf->cdepth  = bmp ? bmp->cdepth: 32;
            if (surf->h > 3) {
                surf->h -= 3;
                uint32_t duration = player_get(app->player, PARAM_MEDIA_DURATION, NULL);
                uint32_t position = player_get(app->player, PARAM_MEDIA_POSITION, NULL);
                uint32_t w = surf->w * position / duration;
                w = w < surf->w ? w : surf->w;
                bar(bmp, 0, surf->h, w, 3, 0xFF8800);
                bar(bmp, w, surf->h, surf->w - w, 3, 0);
            }
        }
        break;
    case PLAYER_VDEV_UNLOCK:
        vdev_unlock(app->vdev);
        break;
#endif
    }
    return 0;
}

int main(int argc, char *argv[])
{
    MYAPP myapp     = {};
    char  url[256]  = "test.mp4";
    char *initparams= NULL;
    int   i;

    if (argc < 2) {
        if (open_file_dialog(NULL, url, sizeof(url)) != 0) return 0;
    } else {
        for (i = 1; i < argc; i++) {
            if (strstr(argv[i], "--init_params=") == argv[i]) initparams = argv[i] + sizeof("--init_params=") - 1;
            else strncpy(url, argv[i], sizeof(url) - 1);
        }
    }

    printf("url   : %s\n", url       );
    printf("params: %s\n", initparams);

#ifdef WITH_LIBAVDEV
    myapp.adev   = adev_init(48000, 2, ADEV_FRAME_SIZE, ADEV_FRAME_NUM);
    myapp.vdev   = vdev_init(1024, 600, "resizable", my_videv_cb, &myapp);
    myapp.idev   = (void*)vdev_get(myapp.vdev, "idev", NULL);
    vdev_set(myapp.vdev, "title", "fanplayer");
    idev_set(myapp.idev, "cbctx"   , &myapp);
    idev_set(myapp.idev, "callback", my_videv_cb);
#endif

    myapp.player = player_init(url, initparams, my_player_cb, &myapp);

#ifdef WITH_LIBAVDEV
    while (strcmp((char*)vdev_get(myapp.vdev, "state", NULL), "running") == 0) { usleep(100 * 1000); }
#endif

    player_exit(myapp.player);

#ifdef WITH_LIBAVDEV
    vdev_exit(myapp.vdev, 0);
    adev_exit(myapp.adev);
#endif
    return 0;
}
