#pragma once

#include "pinshape.h"
#include "trayicon.h"
#include "help.h"


struct App : boost::noncopyable {
    ef::Win::PrevInstance prevInst;
    HWND      hMainWnd, hAboutDlg, hOptionsDlg, hLayerWnd; //, hActiveModelessDlg;
    HINSTANCE hInst;
    HMODULE   hResMod;
    PinShape  pinShape;
    HICON     hSmIcon, hSmClrIcon;
    Help      help;
    int       pinsUsed;
    int       optionPage;
    TrayIcon  trayIcon;

    static const ef::tchar* APPNAME;
    static const ef::tchar* WNDCLS_MAIN;
    static const ef::tchar* WNDCLS_PIN;
    static const ef::tchar* WNDCLS_PINLAYER;

    enum {
        // app messages
        WM_TRAYICON = WM_APP,
        WM_PINSTATUS,
        WM_PINREQ,
        WM_QUEUEWINDOW,
        WM_CMDLINE_OPTION,  // for posting CMDLINE_* options in wparam

        // pin wnd messages
        WM_PIN_ASSIGNWND = WM_USER,
        WM_PIN_RESETTIMER,
        WM_PIN_GETPINNEDWND,

        // system hotkey IDs
        HOTID_ENTERPINMODE = 0,
        HOTID_TOGGLEPIN    = 1,

        // app timers
        TIMERID_AUTOPIN = 1,
    };

    App() : prevInst(_T("EFDeskPinsRunning")),
        hMainWnd(0), hAboutDlg(0), hOptionsDlg(0), hLayerWnd(0), 
        /*hActiveModelessDlg(0), */ hInst(0), hResMod(0), 
        hSmIcon(0), hSmClrIcon(0), 
        pinsUsed(0), optionPage(0), trayIcon(WM_TRAYICON, 0) {}
    ~App() { freeResMod(); DeleteObject(hSmIcon); DeleteObject(hSmClrIcon); }


    bool loadResMod(const ef::tstring& file, HWND hMsgParent);
    void freeResMod();
    bool initComctl();
    bool chkPrevInst();
    bool regWndCls();
    bool createMainWnd();
    void createSmClrIcon(COLORREF clr);
};
