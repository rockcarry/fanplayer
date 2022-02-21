// playerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "stdio.h"
#include "player.h"
#include "playerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TIMER_ID_FIRST_DIALOG       1
#define TIMER_ID_PROGRESS           2
#define TIMER_ID_HIDE_TEXT          3
#define TIMER_ID_DISP_DEFINITIONVAL 4
#define TIMER_ID_DATARATE           5
#define TIMER_ID_LIVEDESK           6

static const int SCREEN_WIDTH  = GetSystemMetrics(SM_CXSCREEN);
static const int SCREEN_HEIGHT = GetSystemMetrics(SM_CYSCREEN);

static void get_app_dir(char *path, int size)
{
    HMODULE handle = GetModuleHandle(NULL);
    GetModuleFileNameA(handle, path, size);
    char  *str = path + strlen(path);
    while (*--str != '\\');
    *str = '\0';
}

static void load_fanplayer_params(PLAYER_INIT_PARAMS *params)
{
    char  file[MAX_PATH];
    FILE *fp = NULL;
    char *buf= NULL;
    int   len= 0;

    // clear params
    memset(params, 0, sizeof(PLAYER_INIT_PARAMS));

    // open params file
    get_app_dir(file, MAX_PATH);
    strcat(file, "\\fanplayer.ini");
    fp = fopen(file, "rb");

    if (fp) {
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        buf = (char*)malloc(len);
        if (buf) {
            fseek(fp, 0, SEEK_SET);
            fread(buf, len, 1, fp);
            player_load_params(params, buf);
            free(buf);
        }
        fclose(fp);
    }
}

static void player_textout(void *player, HFONT hfont, int x, int y, int w, int h, int color, int alpha, TCHAR *text)
{
    RECTOVERLAY overlay[2] = { { 0, 0, w, h, x, y, w, h, OVERLAY_CONST_ALPHA, alpha, 0 } };
    HDC         hdc        = NULL;
    player_getparam(player, PARAM_VDEV_GET_OVERLAY_HDC, &hdc);
    if (hdc && text) {
        CDC cdc; RECT rect = { overlay->srcx, overlay->srcy, overlay->srcx + overlay->srcw, overlay->srcy + overlay->srch };
        cdc.Attach(hdc);
        cdc.FillSolidRect(&rect, RGB(0, 0, 100));
        cdc.SelectObject(CFont::FromHandle(hfont));
        cdc.SetTextColor(RGB(255, 255, 255));
        cdc.SetBkMode(TRANSPARENT);
        cdc.DrawText(text, -1, &rect, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        cdc.Detach();
    }
    player_setparam(player, PARAM_VDEV_SET_OVERLAY_RECT, overlay + !text);
}

static void ffrdp_send_mouse_event(void *player, uint8_t dx, uint8_t dy, uint8_t btns, uint8_t wheel)
{
    uint8_t data[4 + 4];
    struct {
        void  *data;
        DWORD  size;
    } param;
    data[0] = 'M';
    data[1] = 'E';
    data[2] = 'V';
    data[3] = 'T';
    data[4] = dx;
    data[5] = dy;
    data[6] = btns;
    data[7] = wheel;
    param.data = data;
    param.size = sizeof(data);
    player_setparam(player, PARAM_FFRDP_SENDDATA, &param);
}

static void ffrdp_send_keybd_event(void *player, uint8_t key, uint8_t scancode, uint8_t flags1, uint8_t flags2)
{
    uint8_t data[4 + 4];
    struct {
        void  *data;
        DWORD  size;
    } param;
    data[0] = 'K';
    data[1] = 'E';
    data[2] = 'V';
    data[3] = 'T';
    data[4] = key;
    data[5] = scancode;
    data[6] = flags1;
    data[7] = flags2;
    param.data = data;
    param.size = sizeof(data);
    player_setparam(player, PARAM_FFRDP_SENDDATA, &param);
}

// CplayerDlg dialog
CplayerDlg::CplayerDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CplayerDlg::IDD, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_ffPlayer      = NULL;
    m_bLiveStream   = FALSE;
    m_bResetPlayer  = FALSE;
    m_bIsRecording  = FALSE;
    m_bDefinitionEn = FALSE;
    m_bShowDataRate = FALSE;
    m_bLiveDeskMode = FALSE;
    m_bYoloDetect   = FALSE;
    m_bMouseSelFlag = FALSE;
    m_nCurMouseBtns = 0;
}

void CplayerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

void CplayerDlg::PlayerReset(PLAYER_INIT_PARAMS *params)
{
    player_close(m_ffPlayer);
    m_ffPlayer = player_open(m_strUrl, GetSafeHwnd(), params);
}

void CplayerDlg::PlayerOpenFile(TCHAR *file)
{
    CFileDialog dlg(TRUE);
    TCHAR       str[MAX_PATH];

    // kill player progress timer
    KillTimer(TIMER_ID_PROGRESS);

    // open file dialog
    if (!file) {
        if (dlg.DoModal() == IDOK) {
            _tcscpy(str, dlg.GetPathName());
        } else {
            OnOK();
            return;
        }
    } else {
        _tcscpy(str, file);
    }
    WideCharToMultiByte(CP_UTF8, 0, str, -1, m_strUrl, MAX_PATH, NULL, NULL);

    // set window title
    SetWindowText(TEXT("testplayer - loading"));

    // player open file
    char ext[MAX_PATH]; _splitpath(m_strUrl, NULL, NULL, NULL, ext);
    if (  strnicmp(m_strUrl, "http://", 7) == 0 && stricmp(ext, ".m3u8") == 0
       || strnicmp(m_strUrl, "rtmp://", 7) == 0
       || strnicmp(m_strUrl, "rtsp://", 7) == 0
       || strnicmp(m_strUrl, "gdigrab://", 10) == 0
       || strnicmp(m_strUrl, "dshow://", 8) == 0
       || strnicmp(m_strUrl, "vfwcap", 6) == 0
       || strnicmp(m_strUrl, "avkcp://", 8) == 0
       || strnicmp(m_strUrl, "ffrdp://", 8) == 0
       || strnicmp(m_strUrl, "tcp://", 6) == 0
       || stricmp(ext, ".264" ) == 0
       || stricmp(ext, ".265" ) == 0
       || stricmp(ext, ".h264") == 0
       || stricmp(ext, ".h265") == 0
       || stricmp(ext, ".bmp" ) == 0
       || stricmp(ext, ".jpg" ) == 0
       || stricmp(ext, ".jpeg") == 0
       || stricmp(ext, ".png" ) == 0
       || stricmp(ext, ".gif" ) == 0) {
        m_bLiveStream = TRUE;
    } else {
        m_bLiveStream = FALSE;
    }

    KillTimer(TIMER_ID_DISP_DEFINITIONVAL);
    KillTimer(TIMER_ID_LIVEDESK);
    m_bDefinitionEn = FALSE;
    m_bLiveDeskMode = FALSE;

    PLAYER_INIT_PARAMS params;
    load_fanplayer_params(&params); // load fanplayer init params
    PlayerReset(&params); // reset player
}

void CplayerDlg::PlayerShowText(int time)
{
    player_textout(m_ffPlayer, m_hFont, 20, 20, 360, 50, RGB(0, 255, 0), 200, m_strTxt);
    SetTimer(TIMER_ID_HIDE_TEXT, time, NULL);
}

void CplayerDlg::SetWindowClientSize(int w, int h)
{
    RECT rect1, rect2;
    GetWindowRect(&rect1);
    GetClientRect(&rect2);
    w+= (rect1.right  - rect1.left) - rect2.right;
    h+= (rect1.bottom - rect1.top ) - rect2.bottom + 2; // at bottom of testplayer 2 pixels used for progress bar
    int x = (SCREEN_WIDTH  - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    x = x > 0 ? x : 0;
    y = y > 0 ? y : 0;
    MoveWindow(x, y, w, h, TRUE);
}

BEGIN_MESSAGE_MAP(CplayerDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_ACTIVATE()
    ON_COMMAND(ID_OPEN_FILE       , &CplayerDlg::OnOpenFile       )
    ON_COMMAND(ID_VIDEO_MODE      , &CplayerDlg::OnVideoMode      )
    ON_COMMAND(ID_EFFECT_MODE     , &CplayerDlg::OnEffectMode     )
    ON_COMMAND(ID_VRENDER_TYPE    , &CplayerDlg::OnVRenderType    )
    ON_COMMAND(ID_AUDIO_STREAM    , &CplayerDlg::OnAudioStream    )
    ON_COMMAND(ID_VIDEO_STREAM    , &CplayerDlg::OnVideoStream    )
    ON_COMMAND(ID_TAKE_SNAPSHOT   , &CplayerDlg::OnTakeSnapshot   )
    ON_COMMAND(ID_STEP_FORWARD    , &CplayerDlg::OnStepForward    )
    ON_COMMAND(ID_STEP_BACKWARD   , &CplayerDlg::OnStepBackward   )
    ON_COMMAND(ID_PLAY_SPEED_DEC  , &CplayerDlg::OnPlaySpeedDec   )
    ON_COMMAND(ID_PLAY_SPEED_INC  , &CplayerDlg::OnPlaySpeedInc   )
    ON_COMMAND(ID_PLAY_SPEED_TYPE , &CplayerDlg::OnPlaySpeedType  )
    ON_COMMAND(ID_VDEVD3D_ROTATE  , &CplayerDlg::OnVdevD3dRotate  )
    ON_COMMAND(ID_RECORD_VIDEO    , &CplayerDlg::OnRecordVideo    )
    ON_COMMAND(ID_DEFINITION_EVAL , &CplayerDlg::OnDefinitionEval )
    ON_COMMAND(ID_SHOW_DATARATE   , &CplayerDlg::OnShowDatarate   )
    ON_COMMAND(ID_WINFIT_VIDEOSIZE, &CplayerDlg::OnWinfitVideosize)
    ON_COMMAND(ID_ZOOM_RESTORE    , &CplayerDlg::OnZoomRestore    )
    ON_COMMAND(ID_LIVEDESK_MODE   , &CplayerDlg::OnLivedeskMode   )
    ON_COMMAND(ID_YOLO_DETECT     , &CplayerDlg::OnYoloDetect     )
END_MESSAGE_MAP()


// CplayerDlg message handlers

BOOL CplayerDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE );  // Set big icon
    SetIcon(m_hIcon, FALSE);  // Set small icon

    // load accelerators
    m_hAcc = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR1)); 

    LOGFONT logfont = {0};
    _tcscpy_s(logfont.lfFaceName, _countof(logfont.lfFaceName), TEXT("ËÎÌå"));
    logfont.lfHeight = 32;
    m_hFont = CreateFontIndirect(&logfont);

    // disable CS_DBLCLKS
    LONG lStyle = GetClassLong(GetSafeHwnd(), GCL_STYLE);
    lStyle &= ~CS_DBLCLKS;
    SetClassLong(GetSafeHwnd(), GCL_STYLE, lStyle);

    // setup window size
    MoveWindow(0, 0, 800, 480);

    // setup init timer
    SetTimer(TIMER_ID_FIRST_DIALOG, 100, NULL);
    return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CplayerDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width () - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    } else {
        LONGLONG total = 1, pos = 0;
        player_getparam(m_ffPlayer, PARAM_MEDIA_DURATION, &total);
        player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &pos  );
        if (!m_bLiveStream) {
            if (total > 0 && pos > 0) {
                CPaintDC dc(this);
                RECT fill  = m_rtClient;
                fill.right = (LONG)(fill.right * pos / total);
                fill.top   = fill.bottom - 2;
                dc.FillSolidRect(&fill, RGB(250, 150, 0));
                fill.left  = fill.right;
                fill.right = m_rtClient.right;
                dc.FillSolidRect(&fill, RGB(0, 0, 0));
            }
        } else {
            SetWindowText(pos == -1 ? TEXT("testplayer - buffering") : TEXT("testplayer"));
        }
        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CplayerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CplayerDlg::OnDestroy()
{
    CDialog::OnDestroy();

    // close player
    player_close(m_ffPlayer);
    m_ffPlayer = NULL;

    // delete font
    DeleteObject(m_hFont);
    m_hFont = NULL;
}

void CplayerDlg::OnTimer(UINT_PTR nIDEvent)
{
    switch (nIDEvent)
    {
    case TIMER_ID_FIRST_DIALOG:
        // kill timer first
        KillTimer(TIMER_ID_FIRST_DIALOG);
        PlayerOpenFile(__argc > 1 ? __wargv[1] : NULL);
        break;

    case TIMER_ID_PROGRESS:
        RECT rect;
        rect.top    = m_rtClient.bottom - 2;
        rect.left   = m_rtClient.left;
        rect.bottom = m_rtClient.bottom;
        rect.right  = m_rtClient.right;
        InvalidateRect(&rect, FALSE);
        break;

    case TIMER_ID_HIDE_TEXT:
        KillTimer(TIMER_ID_HIDE_TEXT);
        player_textout(m_ffPlayer, m_hFont, 0, 0, 0, 0, 0, 0, NULL);
        m_strTxt[0] = '\0';
        break;

    case TIMER_ID_DISP_DEFINITIONVAL: {
            float val;
            player_getparam(m_ffPlayer, PARAM_DEFINITION_VALUE, &val);
            _stprintf(m_strTxt, TEXT("ÇåÎú¶È %3.1f"), val);
            player_textout(m_ffPlayer, m_hFont, 20, 20, 360, 50, RGB(0, 255, 0), 200, m_strTxt);
        }
        break;
    case TIMER_ID_DATARATE: {
            int val;
            player_getparam(m_ffPlayer, PARAM_DATARATE_VALUE, &val);
            _stprintf(m_strTxt, TEXT("%d KB/s"), val / 1024);
            player_textout(m_ffPlayer, m_hFont, 20, 20, 360, 50, RGB(0, 255, 0), 200, m_strTxt);
        }
        break;
    case TIMER_ID_LIVEDESK: {
            POINT point;
            int   dx, dy;
            GetCursorPos(&point);
            SetCursorPos(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
            dx = (point.x - SCREEN_WIDTH  / 2) * 2 / 3;
            dy = (point.y - SCREEN_HEIGHT / 2) * 2 / 3;
            if      (dx < -127) dx = -127;
            else if (dx >  127) dx =  127;
            if      (dy < -127) dy = -127;
            else if (dy >  127) dy =  127;
            if (dx || dy) ffrdp_send_mouse_event(m_ffPlayer, (char)dx, (char)dy, m_nCurMouseBtns, 0);
        }
        break;
    default:
        CDialog::OnTimer(nIDEvent);
        break;
    }
}

HBRUSH CplayerDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    // TODO: Change any attributes of the DC here
    // TODO: Return a different brush if the default is not desired
    if (pWnd == this) return (HBRUSH)GetStockObject(BLACK_BRUSH);
    else return hbr;
}

void CplayerDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    if (nType != SIZE_MINIMIZED) {
        GetClientRect(&m_rtClient);
        cx = cx < SCREEN_WIDTH  ? cx : SCREEN_WIDTH;
        cy = cy < SCREEN_HEIGHT ? cy : SCREEN_HEIGHT;
        player_setrect(m_ffPlayer, 0, 0, 0, cx, cy - 2);
        player_setrect(m_ffPlayer, 1, 0, 0, cx, cy - 2);
    }
}

BOOL CplayerDlg::PreTranslateMessage(MSG *pMsg) 
{
    if (!m_bLiveDeskMode && TranslateAccelerator(GetSafeHwnd(), m_hAcc, pMsg)) return TRUE;

    if (pMsg->message == MSG_FANPLAYER) {
        switch (pMsg->wParam) {
        case MSG_OPEN_DONE:
            SetWindowText(TEXT("testplayer"));
            if (TRUE) { // set player dynamic params
                int param = 0;
                //++ set dynamic player params
//              param = 150; player_setparam(m_ffPlayer, PARAM_PLAY_SPEED_VALUE, &param);
//              param = 1  ; player_setparam(m_ffPlayer, PARAM_PLAY_SPEED_TYPE , &param);

                // software volume scale -30dB to 12dB
                // range for volume is [-182, 73]
                // -255 - mute, +255 - max volume, 0 - 0dB
                param = -0;  player_setparam(m_ffPlayer, PARAM_AUDIO_VOLUME, &param);
            }
            player_setrect(m_ffPlayer, 0, 0, 0, m_rtClient.right, m_rtClient.bottom - 2);
            player_setrect(m_ffPlayer, 1, 0, 0, m_rtClient.right, m_rtClient.bottom - 2);
            if (m_bResetPlayer) {
                if (!m_bPlayPause ) player_play(m_ffPlayer);
                if (!m_bLiveStream) player_seek(m_ffPlayer, m_llLastPos, 0);
                if ( m_strTxt[0]  ) PlayerShowText(2000);
                m_bResetPlayer = FALSE;
            } else {
                player_play(m_ffPlayer);
                m_bPlayPause = FALSE;
            }
            SetTimer(TIMER_ID_PROGRESS, 100, NULL);
            break;
        case MSG_PLAY_COMPLETED:
            if (!m_bLiveStream) {
                PlayerOpenFile(NULL);
            }
            break;
        case MSG_D3D_DEVICE_LOST:
            PLAYER_INIT_PARAMS params;
            player_getparam(m_ffPlayer, PARAM_PLAYER_INIT_PARAMS, &params);
            player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
            m_bResetPlayer = TRUE;
            m_strTxt[0]    = '\0';
            PlayerReset(&params);
            break;
        }
        return TRUE;
    } else if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYDOWN || pMsg->message == WM_SYSKEYUP) {
        if (m_bLiveDeskMode) {
            if (pMsg->wParam == VK_CONTROL) {
                if (pMsg->lParam & (1 << 31)) m_dwExitLiveDesk &= ~(1 << 0);
                else                          m_dwExitLiveDesk |=  (1 << 0);
            }
            if (pMsg->wParam == 'L') {
                if (pMsg->lParam & (1 << 31)) m_dwExitLiveDesk &= ~(1 << 1);
                else                          m_dwExitLiveDesk |=  (1 << 1);
            }
            if (m_dwExitLiveDesk != 0x3) {
                ffrdp_send_keybd_event(m_ffPlayer, (uint8_t)pMsg->wParam, (uint8_t)(pMsg->lParam >> 16), (uint8_t)(pMsg->lParam >> 24), (uint8_t)pMsg->lParam);
            } else {
                ffrdp_send_keybd_event(m_ffPlayer, VK_CONTROL, MapVirtualKey(VK_CONTROL, 0), (1 << 7), 0);
                OnLivedeskMode();
            }
            return TRUE;
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void CplayerDlg::OnOpenFile()
{
    PlayerOpenFile(NULL);
}

void CplayerDlg::OnAudioStream()
{
    PLAYER_INIT_PARAMS params;
    player_getparam(m_ffPlayer, PARAM_PLAYER_INIT_PARAMS, &params);
    player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
    params.audio_stream_cur++; params.audio_stream_cur %= params.audio_stream_total + 1;
    if (params.audio_stream_cur == params.audio_stream_total) params.audio_stream_cur = -1;
    m_bResetPlayer = TRUE;
    _stprintf(m_strTxt, TEXT("audio stream: %d"), params.audio_stream_cur);
    PlayerReset(&params);
}

void CplayerDlg::OnVideoStream()
{
    PLAYER_INIT_PARAMS params;
    player_getparam(m_ffPlayer, PARAM_PLAYER_INIT_PARAMS, &params);
    player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
    params.video_stream_cur++; params.video_stream_cur %= params.video_stream_total + 1;
    if (params.video_stream_cur == params.video_stream_total) params.video_stream_cur = -1;
    m_bResetPlayer = TRUE;
    _stprintf(m_strTxt, TEXT("video stream: %d"), params.video_stream_cur);
    PlayerReset(&params);
}

void CplayerDlg::OnVideoMode()
{
    int mode = 0;
    player_getparam(m_ffPlayer, PARAM_VIDEO_MODE, &mode);
    mode++; mode %= VIDEO_MODE_MAX_NUM;
    player_setparam(m_ffPlayer, PARAM_VIDEO_MODE, &mode);
    _stprintf(m_strTxt, TEXT("video mode: %d"), mode);
    PlayerShowText(2000);
}

void CplayerDlg::OnEffectMode()
{
    int mode = 0;
    player_getparam(m_ffPlayer, PARAM_VISUAL_EFFECT, &mode);
    mode++; mode %= VISUAL_EFFECT_MAX_NUM;
    player_setparam(m_ffPlayer, PARAM_VISUAL_EFFECT, &mode);
}

void CplayerDlg::OnVRenderType()
{
    PLAYER_INIT_PARAMS params;
    player_getparam(m_ffPlayer, PARAM_PLAYER_INIT_PARAMS, &params);
    player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
    params.vdev_render_type++; params.vdev_render_type %= VDEV_RENDER_TYPE_MAX_NUM;
    m_bResetPlayer = TRUE;
    _stprintf(m_strTxt, TEXT("vdev type: %d"), params.vdev_render_type);
    PlayerReset(&params);
}

void CplayerDlg::OnTakeSnapshot()
{
    char path[MAX_PATH];
    get_app_dir(path, sizeof(path));
    strncat(path, "\\snapshot.jpg", sizeof(path));
    player_snapshot(m_ffPlayer, path, 0, 0, 1000);
    _tcscpy(m_strTxt, TEXT("take snapshot"));
    PlayerShowText(2000);
}

void CplayerDlg::OnStepForward()
{
    player_seek(m_ffPlayer, +1, SEEK_STEP_FORWARD);
    m_bPlayPause = TRUE;
    _tcscpy(m_strTxt, TEXT("step forward"));
    PlayerShowText(2000);
}

void CplayerDlg::OnStepBackward()
{
    player_seek(m_ffPlayer, -1, SEEK_STEP_BACKWARD);
    m_bPlayPause = TRUE;
    _tcscpy(m_strTxt, TEXT("step backward"));
    PlayerShowText(2000);
}

void CplayerDlg::OnPlaySpeedDec()
{
    int speed;
    player_getparam(m_ffPlayer, PARAM_PLAY_SPEED_VALUE, &speed);
    speed -= 10; if (speed < 10) speed = 10;
    player_setparam(m_ffPlayer, PARAM_PLAY_SPEED_VALUE, &speed);

    _stprintf(m_strTxt, TEXT("speed value: %d"), speed);
    PlayerShowText(2000);
}

void CplayerDlg::OnPlaySpeedInc()
{
    int speed;
    player_getparam(m_ffPlayer, PARAM_PLAY_SPEED_VALUE, &speed);
    speed += 10; if (speed > 200) speed = 200;
    player_setparam(m_ffPlayer, PARAM_PLAY_SPEED_VALUE, &speed);

    _stprintf(m_strTxt, TEXT("speed value: %d"), speed);
    PlayerShowText(2000);
}

void CplayerDlg::OnPlaySpeedType()
{
    int type;
    player_getparam(m_ffPlayer, PARAM_PLAY_SPEED_TYPE, &type);
    type = !type;
    player_setparam(m_ffPlayer, PARAM_PLAY_SPEED_TYPE, &type);

    _stprintf(m_strTxt, TEXT("speed type: %s"), type ? TEXT("soundtouch") : TEXT("swresample"));
    PlayerShowText(2000);
}

void CplayerDlg::OnVdevD3dRotate()
{
    PLAYER_INIT_PARAMS params;
    player_getparam(m_ffPlayer, PARAM_PLAYER_INIT_PARAMS, &params);
    if (params.vdev_render_type != VDEV_RENDER_TYPE_D3D) return;

    int angle = 0;
    player_getparam(m_ffPlayer, PARAM_VDEV_D3D_ROTATE, &angle);
    angle += 10; angle %= 360;
    player_setparam(m_ffPlayer, PARAM_VDEV_D3D_ROTATE, &angle);

    _stprintf(m_strTxt, TEXT("rotation: %d"), angle);
    PlayerShowText(2000);
}

void CplayerDlg::OnRecordVideo()
{
    char path[MAX_PATH];
    get_app_dir(path, sizeof(path));
    strncat(path, "\\record.mp4", sizeof(path));
    player_record(m_ffPlayer, m_bIsRecording ? NULL : path);
    m_bIsRecording = !m_bIsRecording;
    _stprintf(m_strTxt, TEXT("recording %s"), m_bIsRecording ? TEXT("started") : TEXT("stoped"));
    PlayerShowText(2000);
}

void CplayerDlg::OnDefinitionEval()
{
    m_bDefinitionEn = !m_bDefinitionEn;
    if (m_bDefinitionEn) {
        SetTimer(TIMER_ID_DISP_DEFINITIONVAL, 200, NULL);
    } else {
        KillTimer(TIMER_ID_DISP_DEFINITIONVAL);
        player_textout(m_ffPlayer, m_hFont, 0, 0, 0, 0, 0, 0, NULL);
    }
}

void CplayerDlg::OnShowDatarate()
{
    m_bShowDataRate = !m_bShowDataRate;
    if (m_bShowDataRate) {
        _stprintf(m_strTxt, TEXT("--- KB/s"));
        player_textout(m_ffPlayer, m_hFont, 20, 20, 360, 50, RGB(0, 255, 0), 200, m_strTxt);
        SetTimer(TIMER_ID_DATARATE, 2000, NULL);
    } else {
        KillTimer(TIMER_ID_DATARATE);
        player_textout(m_ffPlayer, m_hFont, 0, 0, 0, 0, 0, 0, NULL);
    }
}

void CplayerDlg::OnWinfitVideosize()
{
    if (m_ffPlayer) {
        PLAYER_INIT_PARAMS params;
        player_getparam(m_ffPlayer, PARAM_PLAYER_INIT_PARAMS, &params);
        SetWindowClientSize(params.video_owidth, params.video_oheight);
    }
}

void CplayerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_bLiveDeskMode) {
        m_nCurMouseBtns |= (1 << 0);
        ffrdp_send_mouse_event(m_ffPlayer, 0, 0, m_nCurMouseBtns, 0);
    } else if (!m_bLiveStream) {
        if (point.y > m_rtClient.bottom - 8) {
            LONGLONG total = 1;
            player_getparam(m_ffPlayer, PARAM_MEDIA_DURATION, &total);
            KillTimer(TIMER_ID_PROGRESS);
            player_seek(m_ffPlayer, total * point.x / m_rtClient.right, 0);
            SetTimer (TIMER_ID_PROGRESS, 100, NULL);
        } else {
            if (!m_bPlayPause) player_pause(m_ffPlayer);
            else player_play(m_ffPlayer);
            m_bPlayPause = !m_bPlayPause;
        }
    }
    CDialog::OnLButtonDown(nFlags, point);
}

void CplayerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bLiveDeskMode) {
        m_nCurMouseBtns &= ~(1 << 0);
        ffrdp_send_mouse_event(m_ffPlayer, 0, 0, m_nCurMouseBtns, 0);
    }
    CDialog::OnLButtonUp(nFlags, point);
}

void CplayerDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
    if (m_bLiveDeskMode) {
        m_nCurMouseBtns |= (1 << 1);
        ffrdp_send_mouse_event(m_ffPlayer, 0, 0, m_nCurMouseBtns, 0);
    } else if (m_bMouseSelFlag == FALSE) {
        m_bMouseSelFlag  = TRUE;
        m_tMouseSelPoint = point;
    }
    CDialog::OnRButtonDown(nFlags, point);
}

void CplayerDlg::OnRButtonUp(UINT nFlags, CPoint point)
{
    if (m_bLiveDeskMode) {
        m_nCurMouseBtns &= ~(1 << 1);
        ffrdp_send_mouse_event(m_ffPlayer, 0, 0, m_nCurMouseBtns, 0);
    } else if (m_bMouseSelFlag == TRUE) {
        RECT source_rect, video_rect;
        int  tx, ty, tw, th;
        player_getparam(m_ffPlayer, PARAM_RENDER_SOURCE_RECT, &source_rect);
        player_getparam(m_ffPlayer, PARAM_VDEV_GET_VRECT    , &video_rect );
        tx   = MIN(m_tMouseSelPoint.x, point.x) - video_rect.left;
        ty   = MIN(m_tMouseSelPoint.y, point.y) - video_rect.top;
        tw   = abs(point.x - m_tMouseSelPoint.x);
        th   = abs(point.y - m_tMouseSelPoint.y);
        tx   = source_rect.left + tx * (source_rect.right  - source_rect.left) / (video_rect.right  - video_rect.left);
        ty   = source_rect.top  + ty * (source_rect.bottom - source_rect.top ) / (video_rect.bottom - video_rect.top );
        tw   = tw * (source_rect.right  - source_rect.left) / (video_rect.right  - video_rect.left);
        th   = th * (source_rect.bottom - source_rect.top ) / (video_rect.bottom - video_rect.top );
        source_rect.left  = tx;
        source_rect.right = tx + tw;
        source_rect.top   = ty;
        source_rect.bottom= ty + th;
        if (tw >= 16 && th >= 16) player_setparam(m_ffPlayer, PARAM_RENDER_SOURCE_RECT, &source_rect);
        player_setparam(m_ffPlayer, PARAM_VDEV_SET_OVERLAY_RECT, NULL);
        m_bMouseSelFlag = FALSE;
    }
    CDialog::OnRButtonUp(nFlags, point);
}

void CplayerDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bLiveDeskMode) {
        if (m_bMouseSelFlag) {
            RECTOVERLAY overlay[2] = {
                { MIN(m_tMouseSelPoint.x, point.x), MIN(m_tMouseSelPoint.y, point.y), abs(point.x - m_tMouseSelPoint.x), abs(point.y - m_tMouseSelPoint.y),
                  MIN(m_tMouseSelPoint.x, point.x), MIN(m_tMouseSelPoint.y, point.y), abs(point.x - m_tMouseSelPoint.x), abs(point.y - m_tMouseSelPoint.y),
                  OVERLAY_CONST_ALPHA, 128, 0 },
            };
            player_setparam(m_ffPlayer, PARAM_VDEV_SET_OVERLAY_RECT, overlay);
        }
    }
    CDialog::OnMouseMove(nFlags, point);
}

BOOL CplayerDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (m_bLiveDeskMode) {
        short wheel = zDelta / 120;
        if      (wheel < -127) wheel = -127;
        else if (wheel >  127) wheel =  127;
        ffrdp_send_mouse_event(m_ffPlayer, 0, 0, m_nCurMouseBtns, (char)wheel);
    }
    return CDialog::OnMouseWheel(nFlags, zDelta, pt);
}

void CplayerDlg::OnZoomRestore()
{
    RECT rect = {0};
    player_setparam(m_ffPlayer, PARAM_RENDER_SOURCE_RECT, &rect);
}

void CplayerDlg::OnLivedeskMode()
{
    m_bLiveDeskMode = !m_bLiveDeskMode;
    ShowCursor(!m_bLiveDeskMode);
    if (m_bLiveDeskMode) {
        KillTimer(TIMER_ID_DISP_DEFINITIONVAL);
        SetTimer (TIMER_ID_LIVEDESK, 20, NULL);
        SetCursorPos(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        m_bDefinitionEn  = FALSE;
        m_dwExitLiveDesk = 0;
        _stprintf(m_strTxt, TEXT("livedesk"));
        player_textout(m_ffPlayer, m_hFont, 0, 00, 160, 36, RGB(0, 255, 0), 128, m_strTxt);
    } else {
        KillTimer(TIMER_ID_LIVEDESK);
        player_textout(m_ffPlayer, m_hFont, 0, 0, 0, 0, 0, 0, NULL);
    }
}

void CplayerDlg::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
    CDialog::OnActivate(nState, pWndOther, bMinimized);
    if (m_bLiveDeskMode) {
        switch (nState) {
        case WA_INACTIVE:
            KillTimer(TIMER_ID_LIVEDESK);
            ffrdp_send_keybd_event(m_ffPlayer, VK_CONTROL, MapVirtualKey(VK_CONTROL, 0), (1 << 7), 0);
            ffrdp_send_keybd_event(m_ffPlayer, VK_MENU   , MapVirtualKey(VK_MENU   , 0), (1 << 7), 0);
            ffrdp_send_keybd_event(m_ffPlayer, VK_SHIFT  , MapVirtualKey(VK_SHIFT  , 0), (1 << 7), 0);
            break;
        case WA_ACTIVE:
        case WA_CLICKACTIVE:
            SetTimer (TIMER_ID_LIVEDESK, 20, NULL);
            break;
        }
    }
}

void CplayerDlg::OnYoloDetect()
{
    m_bYoloDetect =!m_bYoloDetect;
    int precision = m_bYoloDetect ? 256 : 0; player_setparam(m_ffPlayer, PARAM_OBJECT_DETECT, &precision);
}
