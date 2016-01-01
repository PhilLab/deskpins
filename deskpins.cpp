#include "stdafx.h"
#include "pinshape.h"
#include "util.h"
#include "apputils.h"
#include "help.h"
#include "options.h"
#include "optpins.h"
#include "optautopin.h"
#include "opthotkeys.h"
#include "optlang.h"
#include "app.h"
#include "resource.h"

//#define TEST_OPTIONS_PAGE  1

// enable visual styles
#pragma comment(linker, "/manifestdependency:\""                               \
    "type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'" \
    "\"")

#include "ef/std/addlib.hpp"
#include "ef/Win/addlib.hpp"

typedef ef::Win::CustomControls::LinkCtrl UrlLink;


// global app object
App app;

// options object visible only in this file
static Options opt;


//------------------------------------------
enum {
    CMDLINE_NEWPIN     = 1<<0,  // n
    CMDLINE_REMOVEALL  = 1<<1,  // r
    CMDLINE_OPTIONS    = 1<<2,  // o
    CMDLINE_QUIT       = 1<<3,  // q
    CMDLINE_HIDE       = 1<<4,  // h
    CMDLINE_SHOW       = 1<<5,  // s
    CMDLINE_AUTOEXIT   = 1<<6,  // x
    CMDLINE_NOAUTOEXIT = 1<<7,  // -x
};


typedef void (&CmdLineCallback)(int flag);


// Parse cmdline (from global VC vars), passing flags to callback.
// Invalid switches and parameters are ignored.
// Return true if any switch is processed,
// so that the process can terminate if needed.
static bool parseCmdLine(CmdLineCallback cb) {
    bool msgSent = false;
    typedef const char** Ptr;
    Ptr end = (Ptr)__argv + __argc;
    for (Ptr s = (Ptr)__argv + 1; s < end; ++s) {
        char c = *((*s)++);
        if (c == '/' || c == '-') {
            if      (_stricmp(*s, "n"))  { cb(CMDLINE_NEWPIN);     msgSent = true; }
            else if (_stricmp(*s, "r"))  { cb(CMDLINE_REMOVEALL);  msgSent = true; }
            else if (_stricmp(*s, "o"))  { cb(CMDLINE_OPTIONS);    msgSent = true; }
            else if (_stricmp(*s, "p"))  { cb(CMDLINE_QUIT);       msgSent = true; }
            else if (_stricmp(*s, "h"))  { cb(CMDLINE_HIDE);       msgSent = true; }
            else if (_stricmp(*s, "s"))  { cb(CMDLINE_SHOW);       msgSent = true; }
            else if (_stricmp(*s, "x"))  { cb(CMDLINE_AUTOEXIT);   msgSent = true; }
            else if (_stricmp(*s, "-x")) { cb(CMDLINE_NOAUTOEXIT); msgSent = true; }
        }
    }
    return msgSent;
}


struct DispatcherParams
{
    DispatcherParams(HANDLE doneEvent) :
        doneEvent(doneEvent) {}
    HANDLE doneEvent;  // 0 if not needed
};


HWND findAppWindow()
{
    for (int i = 0; i < 20; ++i) {
        HWND wnd = FindWindow(_T("EFDeskPins"), _T("DeskPins"));
        if (wnd) return wnd;
    }
    return 0;
}


void callback(int flag);


unsigned __stdcall dispatcher(void* args)
{
    const DispatcherParams& dp = *reinterpret_cast<const DispatcherParams*>(args);

    HWND wnd = findAppWindow();
    if (!wnd)
        throw _T("error: could not find app window");

    if (dp.doneEvent)
        SetEvent(dp.doneEvent);
    return 0;
}


//------------------------------------------


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // FIXME: sending cmdline options to already running instance; must use a different method
    /*
    if (argc__ > 1) {
        HANDLE doneEvent = app.prevInst.isRunning() ? CreateEvent(0, false, false, 0) : 0;
        const DispatcherParams dp(doneEvent);

        unsigned tid;
        HANDLE thread = (HANDLE)_beginthreadex(0, 0, dispatcher, (void*)args, 0, &tid);
        if (!thread)
            throw _T("error: could not create thread");
        CloseHandle(thread);

        if (dp.doneEvent) {
            WaitForSingleObject(dp.doneEvent, INFINITE);
            CloseHandle(dp.doneEvent);
            dp.doneEvent = 0;
            return 0;
        }
    }*/

    //COMInitializer comInit;
    app.hInst = hInstance;

    // load settings as soon as possible
    opt.load();

    // setup UI dll & help early to get correct language msgs
    if (!app.loadResMod(opt.uiFile.c_str(), 0)) opt.uiFile = _T("");
    app.help.init(app.hInst, opt.helpFile);

    if (!app.chkPrevInst() || !app.initComctl())
        return 0;

    if (!app.regWndCls() || !app.createMainWnd()) {
        Error(0, ResStr(IDS_ERR_WNDCLSREG));
        return 0;
    }

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        //if (!app.hActiveModelessDlg || !IsDialogMessage(app.hActiveModelessDlg, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        //}
    }

    opt.save();

    return msg.wParam;
}


static bool isWin8orGreater()
{
    return ef::Win::OsVer().majMin() >= ef::Win::packVer(6, 2);
}


static void CmNewPin(HWND hWnd)
{
    // avoid re-entrancy
    if (app.hLayerWnd) return;

    // NOTE: fix for Win8+ (the top-left corner doesn't work)
    const POINT layerWndPos = isWin8orGreater() ? ef::Win::Point(100, 100) : ef::Win::Point(0, 0);
    const SIZE layerWndSize = { 1, 1 };

    app.hLayerWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        App::WNDCLS_PINLAYER,
        _T("DeskPin"),
        WS_POPUP | WS_VISIBLE,
        layerWndPos.x, layerWndPos.y, layerWndSize.cx, layerWndSize.cy,
        hWnd, 0, app.hInst, 0);

    if (!app.hLayerWnd) return;

    ShowWindow(app.hLayerWnd, SW_SHOW);

    // synthesize a WM_LBUTTONDOWN on hLayerWnd, so that it captures the mouse
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(layerWndPos.x, layerWndPos.y);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    SetCursorPos(pt.x, pt.y);
}


static void EvPinReq(HWND hWnd, int x, int y)
{
    POINT pt = {x,y};
    HWND hHitWnd = GetTopParent(WindowFromPoint(pt));
    PinWindow(hWnd, hHitWnd, opt.trackRate.value);
}


static void CmRemovePins(HWND /*hWnd*/)
{
    HWND hPin;
    while ((hPin = FindWindow(App::WNDCLS_PIN, 0)) != 0)
        DestroyWindow(hPin);
}


static void FixOptPSPos(HWND hWnd)
{
    // - find taskbar ("Shell_TrayWnd")
    // - find notification area ("TrayNotifyWnd") (child of taskbar)
    // - get center of notification area
    // - determine quadrant of screen which includes the center point
    // - position prop sheet at proper corner of workarea
    RECT rc, rcWA, rcNW;
    HWND hTrayWnd, hNotityWnd;
    if (GetWindowRect(hWnd, &rc)
        && SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWA, 0)
        && (hTrayWnd = FindWindow(_T("Shell_TrayWnd"), _T("")))
        && (hNotityWnd = FindWindowEx(hTrayWnd, 0, _T("TrayNotifyWnd"), _T("")))
        && GetWindowRect(hNotityWnd, &rcNW))
    {
        // '/2' simplified from the following two inequalities
        bool isLeft = (rcNW.left + rcNW.right) < GetSystemMetrics(SM_CXSCREEN);
        bool isTop  = (rcNW.top + rcNW.bottom) < GetSystemMetrics(SM_CYSCREEN);
        int x = isLeft ? rcWA.left : rcWA.right - (rc.right - rc.left);
        int y = isTop ? rcWA.top : rcWA.bottom - (rc.bottom - rc.top);
        OffsetRect(&rc, x-rc.left, y-rc.top);
        MoveWindow(hWnd, rc);
    }

}


static tstring 
trayIconTip()
{
    TCHAR s[100];
    wsprintf(s, _T("%s - %s: %d"), App::APPNAME, ResStr(IDS_TRAYTIP, 50), app.pinsUsed);
    return s;
}


static WNDPROC orgOptPSProc;


LRESULT CALLBACK OptPSSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_SHOWWINDOW) {
        FixOptPSPos(hWnd);

        // also set the big icon (for Alt-Tab)
        SendMessage(hWnd, WM_SETICON, ICON_BIG, 
            LPARAM(LoadIcon(app.hInst, MAKEINTRESOURCE(IDI_APP))));

        LRESULT ret = CallWindowProc(orgOptPSProc, hWnd, uMsg, wParam, lParam);
        SetWindowLong(hWnd, GWL_WNDPROC, LONG(orgOptPSProc));
        orgOptPSProc = 0;
        return ret;
    }

    return CallWindowProc(orgOptPSProc, hWnd, uMsg, wParam, lParam);
}


// remove WS_EX_CONTEXTHELP and subclass to fix pos
static int CALLBACK OptPSCallback(HWND hWnd, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED) {
        // remove caption help button
        ef::Win::WndH(hWnd).modifyExStyle(WS_EX_CONTEXTHELP, 0);
        orgOptPSProc = WNDPROC(SetWindowLong(hWnd, GWL_WNDPROC, LONG(OptPSSubclass)));
    }

    return 0;
}


static void BuildOptPropSheet(PROPSHEETHEADER& psh, PROPSHEETPAGE psp[], 
    int dlgIds[], DLGPROC dlgProcs[], int pageCnt, HWND hParentWnd, 
    OptionsPropSheetData& data, ResStr& capStr)
{
    for (int n = 0; n < pageCnt; ++n) {
        psp[n].dwSize      = sizeof(psp[n]);
        psp[n].dwFlags     = PSP_HASHELP;
        psp[n].hInstance   = app.hResMod ? app.hResMod : app.hInst;
        psp[n].pszTemplate = MAKEINTRESOURCE(dlgIds[n]);
        psp[n].hIcon       = 0;
        psp[n].pszTitle    = 0;
        psp[n].pfnDlgProc  = dlgProcs[n];
        psp[n].lParam      = 0;
        psp[n].pfnCallback = 0;
        psp[n].pcRefParent = 0;
        psp[n].lParam      = reinterpret_cast<LPARAM>(&data);
    }

    psh.dwSize      = sizeof(psh);
    psh.dwFlags     = PSH_HASHELP | PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_USEHICON;
    psh.hwndParent  = hParentWnd;
    psh.hInstance   = app.hResMod ? app.hResMod : app.hInst;
    psh.hIcon       = app.hSmIcon;
    psh.pszCaption  = capStr;
    psh.nPages      = pageCnt;
    psh.nStartPage  = (app.optionPage >= 0 && app.optionPage < pageCnt) 
        ? app.optionPage : 0;
    psh.ppsp        = psp;
    psh.pfnCallback = OptPSCallback;

}


static void CmOptions(HWND hWnd, WindowCreationMonitor& winCreMon)
{
    // sentry to avoid multiple dialogs
    static bool isOptDlgOn;
    if (isOptDlgOn) return;
    isOptDlgOn = true;

    const int pageCnt = 4;
    PROPSHEETPAGE psp[pageCnt];
    PROPSHEETHEADER psh;
    int dlgIds[pageCnt] = {
        IDD_OPT_PINS, IDD_OPT_AUTOPIN, IDD_OPT_HOTKEYS, IDD_OPT_LANG
    };
    DLGPROC dlgProcs[pageCnt] = { 
        OptPinsProc, OptAutoPinProc, OptHotkeysProc, OptLangProc
    };

    // This must remain in scope until PropertySheet() call,
    // because we use a char* from it in BuildOptPropSheet().
    ResStr capStr(IDS_OPTIONSTITLE);

    OptionsPropSheetData data = { opt, winCreMon };
    BuildOptPropSheet(psh, psp, dlgIds, dlgProcs, pageCnt, 0, data, capStr);

    // HACK: ensure tab pages creation, even if lang change is applied
    //
    // If there's a loaded res DLL, we reload it to increase its ref count
    // and then free it again when PropertySheet() returns.
    // This is in case the user changes the lang and hits Apply.
    // That causes the current res mod to change, but the struct passed
    // to PropertySheet() describes the prop pages using the initial
    // res mod. Unless we do this ref-count trick, any pages that were
    // not loaded before the lang change apply would fail to be created.
    //
    HINSTANCE hCurResMod = 0;
    if (!opt.uiFile.empty()) {
        tchar buf[MAX_PATH];
        GetModuleFileName(app.hResMod, buf, sizeof(buf));
        hCurResMod = LoadLibrary(buf);
    }

    //#ifdef TEST_OPTIONS_PAGE
    //  psh.nStartPage = TEST_OPTIONS_PAGE;
    //#endif

    if (PropertySheet(&psh) == -1) {
        tstring msg = ResStr(IDS_ERR_DLGCREATE);
        msg += _T("\r\n");
        msg += ef::Win::getLastErrorStr();
        Error(hWnd, msg.c_str());
    }

    if (hCurResMod) {
        FreeLibrary(hCurResMod);
        hCurResMod = 0;
    }

    // reset tray tip, in case lang was changed
    app.trayIcon.setTip(trayIconTip().c_str());

    isOptDlgOn = false;

    //#ifdef TEST_OPTIONS_PAGE
    //  // PostMessage() stalls the debugger a bit...
    //  //PostMessage(hWnd, WM_COMMAND, CM_CLOSE, 0);
    //  DestroyWindow(hWnd);
    //#endif
}


// set and manage tray menu images
class TrayMenuDecorations {
public:
    TrayMenuDecorations(HMENU hMenu)
    {
        int w = GetSystemMetrics(SM_CXMENUCHECK);
        int h = GetSystemMetrics(SM_CYMENUCHECK);

        HDC    hMemDC  = CreateCompatibleDC(0);
        HFONT  hFnt    = ef::Win::FontH::create(_T("Marlett"), h, SYMBOL_CHARSET, ef::Win::FontH::noStyle);
        HBRUSH bkBrush = HBRUSH(GetStockObject(WHITE_BRUSH));

        HGDIOBJ orgFnt = GetCurrentObject(hMemDC, OBJ_FONT);
        HGDIOBJ orgBmp = GetCurrentObject(hMemDC, OBJ_BITMAP);

        hBmpClose = makeBmp(hMemDC, w, h, bkBrush, hMenu, CM_CLOSE, hFnt, _T("r"));
        hBmpAbout = makeBmp(hMemDC, w, h, bkBrush, hMenu, CM_ABOUT, hFnt, _T("s"));

        SelectObject(hMemDC, orgBmp);
        SelectObject(hMemDC, orgFnt);

        DeleteObject(hFnt);
        DeleteDC(hMemDC);
    }

    ~TrayMenuDecorations()
    {
        if (hBmpClose) DeleteObject(hBmpClose);
        if (hBmpAbout) DeleteObject(hBmpAbout);
    }

protected:
    TrayMenuDecorations(const TrayMenuDecorations&);
    TrayMenuDecorations& operator=(const TrayMenuDecorations&);

    HBITMAP hBmpClose, hBmpAbout;

    HBITMAP makeBmp(HDC dc, int w, int h, HBRUSH bkBrush, HMENU hMenu, int idCmd, HFONT fnt, const TCHAR* c)
    {
        HBITMAP hRetBmp = CreateBitmap(w, h, 1, 1, 0);
        if (hRetBmp) {
            RECT rc = {0, 0, w, h};
            SelectObject(dc, fnt);
            SelectObject(dc, hRetBmp);
            FillRect(dc, &rc, bkBrush);
            DrawText(dc, c, 1, &rc, DT_CENTER | DT_NOPREFIX);
            SetMenuItemBitmaps(hMenu, idCmd, MF_BYCOMMAND, hRetBmp, hRetBmp);
        }
        return hRetBmp;
    }

};


static void EvTrayIcon(HWND hWnd, WPARAM id, LPARAM msg)
{
    static bool gotLButtonDblClk = false;

    if (id != 0) return;

    switch (msg) {
        case WM_LBUTTONUP: {
            if (!opt.dblClkTray || gotLButtonDblClk) {
                SendMessage(hWnd, WM_COMMAND, CM_NEWPIN, 0);
                gotLButtonDblClk = false;
            }
            break;
        }
        case WM_LBUTTONDBLCLK: {
            gotLButtonDblClk = true;
            break;
        }
        case WM_RBUTTONDOWN: {
            SetForegroundWindow(hWnd);
            HMENU hDummy = LoadLocalizedMenu(IDM_TRAY);
            HMENU hMenu = GetSubMenu(hDummy,0);
            SetMenuDefaultItem(hMenu, CM_NEWPIN, false);

            TrayMenuDecorations tmd(hMenu);

            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, 0);
            DestroyMenu(hDummy);
            SendMessage(hWnd, WM_NULL, 0, 0);

            break;
        }
    }

}


static void EvHotkey(HWND hWnd, int idHotKey)
{
    // ignore if there's an active pin layer
    if (app.hLayerWnd) return;

    switch (idHotKey) {
    case App::HOTID_ENTERPINMODE:
        PostMessage(hWnd, WM_COMMAND, CM_NEWPIN, 0);
        break;
    case App::HOTID_TOGGLEPIN:
        TogglePin(hWnd, GetForegroundWindow(), opt.trackRate.value);
        break;
    }

}


// =======================================================================
// =======================================================================


static void UpdateStatusMessage(HWND hWnd)
{
    //ResStr s = app.pinsUsed == 0 ? ResStr(IDS_PINSUSED_0) :
    //           app.pinsUsed == 1 ? ResStr(IDS_PINSUSED_1) :
    //                               ResStr(IDS_PINSUSED_N, 100, app.pinsUsed);
    SetDlgItemInt(hWnd, IDC_STATUS, app.pinsUsed, false);
}


static bool EvInitDlg(HWND hWnd, HFONT& hBoldGUIFont, HFONT& hUnderlineGUIFont)
{
    app.hAboutDlg = hWnd;

    UpdateStatusMessage(hWnd);

    hBoldGUIFont = ef::Win::FontH::create(ef::Win::FontH::getStockDefaultGui(), 0, ef::Win::FontH::bold);
    hUnderlineGUIFont = ef::Win::FontH::create(ef::Win::FontH::getStockDefaultGui(), 0, ef::Win::FontH::underline);

    if (hBoldGUIFont) {
        // make status and its label bold
        ef::Win::WndH status = GetDlgItem(hWnd, IDC_STATUS);
        status.setFont(hBoldGUIFont);
        status.getWindow(GW_HWNDPREV).setFont(hBoldGUIFont);
    }

    // set dlg icons
    HICON hAppIcon = LoadIcon(app.hInst, MAKEINTRESOURCE(IDI_APP));
    SendMessage(hWnd, WM_SETICON, ICON_BIG,   LPARAM(hAppIcon));
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, LPARAM(app.hSmIcon));

    // set static icon if dlg was loaded from lang dll
    if (app.hResMod)
        SendDlgItemMessage(hWnd,IDC_LOGO,STM_SETIMAGE,IMAGE_ICON,LPARAM(hAppIcon));

    struct Link {
        int id;
        const tchar* title;
        const tchar* url;
    };

    Link links[] = {
        { IDC_MAIL, _T("efotinis@gmail.com"), _T("mailto:efotinis@gmail.com") },
        { IDC_WEB, _T("Deskpins webpage"), _T("http://efotinis.neocities.org/deskpins/index.html") }
    };

    ef::Win::FontH font = ef::Win::FontH::getStockDefaultGui();
    BOOST_FOREACH(const Link& link, links) {
        SetDlgItemText(hWnd, link.id, link.title);
        UrlLink* ul = UrlLink::subclass(hWnd, link.id);
        ul->setFonts(font, hUnderlineGUIFont ? hUnderlineGUIFont : font, font);
        ul->setColors(RGB(0,0,255), RGB(255,0,0), RGB(128,0,128));
        ul->setUrl(link.url);
    }

    //#ifdef TEST_OPTIONS_PAGE
    //  SendMessage(hWnd, WM_COMMAND, CM_OPTIONS, 0);
    //#endif

    return true;
}


static void EvTermDlg(HWND hWnd, HFONT& hBoldGUIFont, HFONT& hUnderlineGUIFont)
{
    app.hAboutDlg = 0;

    if (hBoldGUIFont) {
        DeleteObject(hBoldGUIFont);
        hBoldGUIFont = 0;
    }

    if (hUnderlineGUIFont) {
        DeleteObject(hUnderlineGUIFont);
        hUnderlineGUIFont = 0;
    }

}


static void showSpecialInfo(HWND parent)
{
#if defined(_DEBUG)
    const tchar* build = _T("Debug");
#else
    const tchar* build = _T("Release");
#endif

    // TODO: remove build info and add something more useful (e.g. "portable")
    tchar buf[1000];
    wsprintf(buf, _T("Build: %s"), build);
    MessageBox(parent, buf, _T("Info"), MB_ICONINFORMATION);
}


static BOOL CALLBACK AboutDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HFONT   hBoldGUIFont, hUnderlineGUIFont;

    switch (uMsg) {
        case WM_INITDIALOG:
            return EvInitDlg(hWnd, hBoldGUIFont, hUnderlineGUIFont);
        case WM_DESTROY:
            EvTermDlg(hWnd, hBoldGUIFont, hUnderlineGUIFont);
            return true;
        case App::WM_PINSTATUS:
            UpdateStatusMessage(hWnd);
            return true;
        //case WM_ACTIVATE:
        //  app.hActiveModelessDlg = (LOWORD(wParam) == WA_INACTIVE) ? 0 : hWnd;
        //  return false;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            switch (id) {
                case IDOK:
                case IDCANCEL:
                    DestroyWindow(hWnd);
                    return true;
                case IDC_LOGO:
                    if (code == STN_DBLCLK) {
                        showSpecialInfo(hWnd);
                        return true;
                    }
            }
            return false;
        }
    }
    return false;
}


// =======================================================================
// =======================================================================

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static UINT taskbarMsg = RegisterWindowMessage(_T("TaskbarCreated"));
    static PendingWindows pendWnds;
    static WindowCreationMonitor* winCreMon;

    switch (uMsg) {
        case WM_CREATE: {
            app.hMainWnd = hWnd;
            //winCreMon = new HookDllWindowCreationMonitor();
            winCreMon = new EventHookWindowCreationMonitor();

            if (opt.autoPinOn && !winCreMon->init(hWnd, App::WM_QUEUEWINDOW)) {
                Error(hWnd, ResStr(IDS_ERR_HOOKDLL));
                opt.autoPinOn = false;
            }

            app.hSmIcon = HICON(LoadImage(app.hInst, MAKEINTRESOURCE(IDI_APP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
            app.createSmClrIcon(opt.pinClr);

            // first set the hWnd (this can only be done once)...
            app.trayIcon.setWnd(hWnd);
            // .. and then create the notification icon
            app.trayIcon.create(app.hSmClrIcon, trayIconTip().c_str());

            // setup hotkeys
            if (opt.hotkeysOn) {
                bool allKeysSet = true;
                allKeysSet &= opt.hotEnterPin.set(hWnd);
                allKeysSet &= opt.hotTogglePin.set(hWnd);
                if (!allKeysSet)
                    Error(hWnd, ResStr(IDS_ERR_HOTKEYSSET));
            }

            if (opt.autoPinOn)
                SetTimer(hWnd, App::TIMERID_AUTOPIN, opt.autoPinDelay.value, 0);

            // init pin image/shape/dims
            app.pinShape.initShape();
            app.pinShape.initImage(opt.pinClr);

            break;
        }
        case WM_DESTROY: {
            app.hMainWnd = 0;

            winCreMon->term();
            delete winCreMon;
            winCreMon = 0;

            SendMessage(hWnd, WM_COMMAND, CM_REMOVEPINS, 0);

            // remove hotkeys
            opt.hotEnterPin.unset(hWnd);
            opt.hotTogglePin.unset(hWnd);

            PostQuitMessage(0);
            break;
        }
        case App::WM_TRAYICON:
            EvTrayIcon(hWnd, wParam, lParam);
            break;
        case App::WM_PINREQ:
            EvPinReq(hWnd, int(wParam), int(lParam));
            break;
        case WM_HOTKEY:
            EvHotkey(hWnd, wParam);
            break;
        case WM_TIMER:
            if (wParam == App::TIMERID_AUTOPIN) pendWnds.check(hWnd, opt);
            break;
        case App::WM_QUEUEWINDOW:
            pendWnds.add(HWND(wParam));
            break;
        case App::WM_PINSTATUS:
            if (lParam) ++app.pinsUsed; else --app.pinsUsed;
            if (app.hAboutDlg) SendMessage(app.hAboutDlg, App::WM_PINSTATUS, 0, 0);
            app.trayIcon.setTip(trayIconTip().c_str());
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDHELP:
                    app.help.show(hWnd);
                    break;
                case CM_ABOUT:
                    if (app.hAboutDlg)
                        SetForegroundWindow(app.hAboutDlg);
                    else {
                        CreateLocalizedDialog(IDD_ABOUT, 0, AboutDlgProc);
                        ShowWindow(app.hAboutDlg, SW_SHOW);
                    }
                    break;
                case CM_NEWPIN: 
                    CmNewPin(hWnd);
                    break;
                case CM_REMOVEPINS:
                    CmRemovePins(hWnd);
                    break;
                case CM_OPTIONS:
                    CmOptions(hWnd, *winCreMon);
                    break;
                case CM_CLOSE:
                    DestroyWindow(hWnd);
                    break;
                    //case ID_TEST:
                    //  CmTest(hWnd);
                    //  break;
                default:
                    break;
            }
            break;
        case WM_ENDSESSION: {
            if (wParam)
                opt.save();
            return 0;
        }
        //case WM_SETTINGSCHANGE:
        //case WM_DISPLAYCHANGE:
        default:
            if (uMsg == taskbarMsg) {
                // taskbar recreated; reset the tray icon
                app.trayIcon.create(app.hSmClrIcon, trayIconTip().c_str());
                break;
            }
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}
