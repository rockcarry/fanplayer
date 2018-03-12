// player.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CplayerApp:
// See player.cpp for the implementation of this class
//

class CplayerApp : public CWinApp
{
public:
	CplayerApp();

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation
    DECLARE_MESSAGE_MAP()
    afx_msg void OnVideoMode();
};

extern CplayerApp theApp;