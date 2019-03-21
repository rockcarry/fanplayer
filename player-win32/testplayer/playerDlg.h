// playerDlg.h : header file
//
#pragma once

#include "ffplayer.h"

// CplayerDlg dialog
class CplayerDlg : public CDialog
{
// Construction
public:
	CplayerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_PLAYER_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL PreTranslateMessage(MSG* pMsg);

// Implementation
protected:
    HICON  m_hIcon;
    HACCEL m_hAcc;

    // Generated message map functions
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()

private:
    char  m_strUrl[MAX_PATH];
    TCHAR m_strTxt[MAX_PATH];
    void *m_ffPlayer;

private:
    LONGLONG m_llLastPos;
    BOOL     m_bResetPlayer;
    BOOL     m_bLiveStream;
    BOOL     m_bIsRecording;
    BOOL     m_DefinitionEvalEnable;
    void PlayerReset(PLAYER_INIT_PARAMS *params);
    void PlayerOpenFile(TCHAR *file);
    void PlayerShowText(int time);

private:
    CDC  *m_pDrawDC;
    BOOL  m_bPlayPause;
    RECT  m_rtClient;

public:
    afx_msg void   OnDestroy();
    afx_msg void   OnTimer(UINT_PTR nIDEvent);
    afx_msg void   OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void   OnSize(UINT nType, int cx, int cy);
    afx_msg void   OnOpenFile();
    afx_msg void   OnAudioStream();
    afx_msg void   OnVideoStream();
    afx_msg void   OnVideoMode();
    afx_msg void   OnEffectMode();
    afx_msg void   OnVRenderType();
    afx_msg void   OnTakeSnapshot();
    afx_msg void   OnStepForward();
    afx_msg void   OnStepBackward();
    afx_msg void   OnPlaySpeedDec();
    afx_msg void   OnPlaySpeedInc();
    afx_msg void   OnPlaySpeedType();
    afx_msg void   OnVdevD3dRotate();
    afx_msg void   OnRecordVideo();
    afx_msg void   OnDefinitionEval();
};
