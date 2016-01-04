#pragma once

#include "pinshape.h"
#include "trayicon.h"
#include "help.h"


struct App : boost::noncopyable {
    ef::Win::PrevInstance prevInst;
    HWND      mainWnd, aboutDlg, optionsDlg, layerWnd; //, activeModelessDlg;
    HINSTANCE inst;
    HMODULE   resMod;
    PinShape  pinShape;
    HICON     smIcon, smClrIcon;
    Help      help;
    int       pinsUsed;
    int       optionPage;
    TrayIcon  trayIcon;

    static LPCWSTR APPNAME;
    static LPCWSTR WNDCLS_MAIN;
    static LPCWSTR WNDCLS_PIN;
    static LPCWSTR WNDCLS_PINLAYER;

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

    App() : prevInst(L"EFDeskPinsRunning"),
        mainWnd(0), aboutDlg(0), optionsDlg(0), layerWnd(0), 
        /*activeModelessDlg(0), */ inst(0), resMod(0), 
        smIcon(0), smClrIcon(0), 
        pinsUsed(0), optionPage(0), trayIcon(WM_TRAYICON, 0) {}
    ~App() { freeResMod(); DeleteObject(smIcon); DeleteObject(smClrIcon); }


    bool loadResMod(const std::wstring& file, HWND msgParent);
    void freeResMod();
    bool initComctl();
    bool chkPrevInst();
    bool regWndCls();
    bool createMainWnd();
    void createSmClrIcon(COLORREF clr);
};
