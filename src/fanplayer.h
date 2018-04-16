#ifndef __FANPLAYER_FANPLAYER_H__
#define __FANPLAYER_FANPLAYER_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
// message
#define MSG_FFPLAYER        (WM_APP + 1)
#define MSG_OPEN_DONE       (('O' << 24) | ('P' << 16) | ('E' << 8) | ('N' << 0))
#define MSG_OPEN_FAILED     (('F' << 24) | ('A' << 16) | ('I' << 8) | ('L' << 0))
#define MSG_PLAY_COMPLETED  (('E' << 24) | ('N' << 16) | ('D' << 8) | (' ' << 0))
#define MSG_TAKE_SNAPSHOT   (('S' << 24) | ('N' << 16) | ('A' << 8) | ('P' << 0))

// adev render type
enum {
    ADEV_RENDER_TYPE_WAVEOUT,
    ADEV_RENDER_TYPE_MAX_NUM,
};

// vdev render type
enum {
    VDEV_RENDER_TYPE_GDI,
    VDEV_RENDER_TYPE_D3D,
    VDEV_RENDER_TYPE_MAX_NUM,
};

// render mode
enum {
    VIDEO_MODE_LETTERBOX,
    VIDEO_MODE_STRETCHED,
    VIDEO_MODE_MAX_NUM,
};

// visual effect
enum {
    VISUAL_EFFECT_DISABLE,
    VISUAL_EFFECT_WAVEFORM,
    VISUAL_EFFECT_SPECTRUM,
    VISUAL_EFFECT_MAX_NUM,
};

// seek flags
enum {
    SEEK_FAST,
    SEEK_PRECISELY,
    SEEK_STEP,
};

// param
enum {
    //++ public
    // duration & position
    PARAM_MEDIA_DURATION = 0x1000,
    PARAM_MEDIA_POSITION,

    // media detail info
    PARAM_VIDEO_WIDTH,
    PARAM_VIDEO_HEIGHT,

    // video display mode
    PARAM_VIDEO_MODE,

    // audio volume control
    PARAM_AUDIO_VOLUME,

    // playback speed control
    PARAM_PLAY_SPEED,

    // visual effect mode
    PARAM_VISUAL_EFFECT,

    // audio/video sync diff
    PARAM_AVSYNC_TIME_DIFF,
    //-- public

    //++ for adev
    PARAM_ADEV_GET_CONTEXT = 0x2000,
    //-- for adev

    //++ for vdev
    PARAM_VDEV_GET_CONTEXT = 0x3000,
    PARAM_VDEV_FRAME_RATE,
    PARAM_VDEV_POST_SURFACE,
    PARAM_VDEV_GET_D3DDEV,
    //-- for vdev

    //++ for render
    PARAM_RENDER_GET_CONTEXT = 0x4000,
    PARAM_RENDER_SEEK_STEP,
    //-- for render
};

// 初始化参数说明
// PLAYER_INIT_PARAMS 为播放器初始化参数，在 player_open 时传入，并可获得视频文件打开后的一些参数信息
// r 表示参数只读，w 表示参数只写，wr 表示参数可配置，但配置是否成功可在 player_open 后读取判断
typedef struct {
    int video_vwidth;             // r  video actual width
    int video_vheight;            // r  video actual height
    int video_owidth;             // r  video output width  (after rotate)
    int video_oheight;            // r  video output height (after rotate)
    int video_frame_rate;         // r  视频帧率
    int video_stream_total;       // r  视频流总数
    int video_stream_cur;         // wr 当前视频流
    int video_thread_count;       // wr 视频解码线程数
    int video_hwaccel;            // wr 视频硬解码使能
    int video_deinterlace;        // wr 视频反交错使能
    int video_rotate;             // wr 视频旋转角度

    int audio_channels;           // r  音频通道数
    int audio_sample_rate;        // r  音频采样率
    int audio_stream_total;       // r  音频流总数
    int audio_stream_cur;         // wr 当前音频流

    int subtitle_stream_total;    // r  字幕流总数
    int subtitle_stream_cur;      // wr 当前字幕流

    int vdev_render_type;         // w  vdev 类型
    int adev_render_type;         // w  adev 类型

    int init_timeout;             // w  播放器初始化超时，单位 ms，打开网络流媒体时设置用来防止卡死
    int open_syncmode;            // w  播放器以同步方式打开，调用 player_open 将等待播放器初始化成功
} PLAYER_INIT_PARAMS;

// 函数声明
void* player_open    (char *file, void *appdata, PLAYER_INIT_PARAMS *params);
void  player_close   (void *hplayer);
void  player_play    (void *hplayer);
void  player_pause   (void *hplayer);
void  player_seek    (void *hplayer, int64_t ms, int type);
void  player_setrect (void *hplayer, int type, int x, int y, int w, int h); // type: 0 - video rect, 1 - visual effect rect
int   player_snapshot(void *hplayer, char *file, int w, int h, int waitt);
int   player_record  (void *hplayer, char *file);
void  player_textout (void *hplayer, int x, int y, int color, char *text);
void  player_setparam(void *hplayer, int id, void *param);
void  player_getparam(void *hplayer, int id, void *param);

// internal helper function
void  player_send_message(void *extra, int32_t msg, int64_t param);
void  player_load_params (PLAYER_INIT_PARAMS *params, char *str);

// 函数说明
/*
player_open     创建一个 player 对象
    file        - 文件路径（可以是网络流媒体的 URL）
    appdata     - win32 平台传入窗口句柄，android 平台传入 MediaPlayer 类对象
    params      - 播放器初始化参数
    返回值      - void* 指针类型，指向 player 对象

player_close    关闭播放器
    hplayer     - 指向 player_open 返回的 player 对象

player_play     开始播放
    hplayer     - 指向 player_open 返回的 player 对象

player_pause    暂停播放
    hplayer     - 指向 player_open 返回的 player 对象

player_seek     跳转到指定位置
    hplayer     - 指向 player_open 返回的 player 对象
    ms          - 指定位置，以毫秒为单位
    type        - 指定类型，SEEK_FAST / SEEK_PRECISELY

player_setrect  设置显示区域，有两种显示区域，视频显示区和视觉效果显示区
    hplayer     - 指向 player_open 返回的 player 对象
    type        - 指定区域类型  0 - video rect, 1 - visual effect rect
    x,y,w,h     - 指定显示区域

player_snapshot 视频播放截图
    hplayer     - 指向 player_open 返回的 player 对象
    w, h        - 指定图片宽高，如果 <= 0 则默认使用视频宽高
    file        - 图片文件名（目前只支持 jpeg 格式）
    waitt       - 是否带动截图完成 0 - 不等待，>0 等待超时 ms 为单位

player_record   将当前播放器的视频录像保存
    hplayer     - 指向 player_open 返回的 player 对象
    file        - 录像文件名，仅支持 .mp4 格式，传入 NULL 为停止录像

player_textout  在视频显示上叠加文字输出
    hplayer     - 指向 player_open 返回的 player 对象
    x, y        - 文字输出坐标
    color       - 文字颜色
    text        - 文字字符串

player_setparam 设置参数
    hplayer     - 指向 player_open 返回的 player 对象
    id          - 参数 id
    param       - 参数指针

player_getparam 获取参数
    hplayer     - 指向 player_open 返回的 player 对象
    id          - 参数 id
    param       - 参数指针
 */

// 动态参数说明
/*
PARAM_MEDIA_DURATION 和 PARAM_MEDIA_POSITION
用于获取多媒体文件的总长度和当前播放位置（毫秒为单位）
LONGLONG total = 1, pos = 0;
player_getparam(g_hplayer, PARAM_MEDIA_DURATION, &total);
player_getparam(g_hplayer, PARAM_MEDIA_POSITION, &pos  );

PARAM_VIDEO_WIDTH 和 PARAM_VIDEO_HEIGHT
用于获取多媒体文件的视频宽度和高度（像素为单位）
int vw = 0, vh = 0;
player_getparam(g_hplayer, PARAM_VIDEO_WIDTH , &vw);
player_getparam(g_hplayer, PARAM_VIDEO_HEIGHT, &vh);

PARAM_VIDEO_MODE
用于获取和设置视频显示方式，有两种方式可选：
    1. VIDEO_MODE_LETTERBOX - 按比例缩放到显示区域
    2. VIDEO_MODE_STRETCHED - 拉伸到显示区域
（注：视频显示区域由 player_setrect 进行设定）
int mode = 0;
player_getparam(g_hplayer, PARAM_VIDEO_MODE, &mode);
mode = VIDEO_MODE_STRETCHED;
player_setparam(g_hplayer, PARAM_VIDEO_MODE, &mode);

PARAM_AUDIO_VOLUME
用于设置播放音量，不同于系统音量，fanplayer 内部具有一个 -30dB 到 +12dB 的软件音量控制单元
音量范围：[-182, 73]，-182 对应 -30dB，73 对应 +12dB
特殊值  ：0 对应 0dB 增益，-255 对应静音，+255 对应最大增益
int volume = -0;
player_setparam(g_hplayer, PARAM_AUDIO_VOLUME, &volume);

PARAM_PLAY_SPEED
用于设置播放速度，fanplayer 支持变速播放
int speed = 150;
player_setparam(g_hplayer, PARAM_PLAY_SPEED, &speed);
参数 speed 为百分比速度，150 表示以 150% 进行播放
速度没有上限和下限，设置为 0 没有意义，内部会处理为 1%
播放速度的实际上限，由处理器的处理能力决定，超过处理器能力，播放会出现卡顿现象

PARAM_VISUAL_EFFECT
用于指定视觉效果的类型，fanplayer 支持视觉效果，主要是对音频进行视觉效果的呈现
int mode = 0;
player_getparam(g_hplayer, PARAM_VISUAL_EFFECT, &mode);
mode = VISUAL_EFFECT_WAVEFORM;
player_setparam(g_hplayer, PARAM_VISUAL_EFFECT, &mode);
目前总共有三种视觉效果：
    1. VISUAL_EFFECT_DISABLE  - 关闭
    2. VISUAL_EFFECT_WAVEFORM - 波形
    3. VISUAL_EFFECT_SPECTRUM - 频谱
（注：视觉效果区域由 player_setrect 进行设定）

PARAM_AVSYNC_TIME_DIFF
用于设置 audio 和 video 的时间同步差值（毫秒为单位）
int diff = 100;
player_setparam(g_hplayer, PARAM_AVSYNC_TIME_DIFF, &diff);
设置为 100 后，音频将比视频快 100ms，设置为 -100 则慢 100ms

所有的参数，都是可以 get 的，但并不是所有的参数都可以 set，因为有些参数是只读的。


PARAM_DISABLE_AUDIO_DECODE 和 PARAM_DISABLE_VIDEO_DECODE
用于禁止音频和视频的解码
应用场景，播放视频时，如果用不只想听声音，不想看视频，那么可以禁止视频解码，
这样可以大幅减少视频解码对资源的消耗


// 对 avdevice 输入设备的支持
windows 平台上支持 dshow gdigrab vfwcap 三种输入设备
打开方式举例：
player_open("vfwcap", hwnd, 0, 0, NULL); // 将以 vfw 方式打开摄像头进行预览
player_open("gdigrab://desktop", hwnd, 0, 0, NULL); // 将以 gdigrab 方式打开桌面进行预览
player_open("dshow://video=Integrated Camera", hwnd, 0, 0, NULL); // 将以 dshow 方式打 Integrated Camera

 */

#ifdef __cplusplus
}
#endif

#endif



