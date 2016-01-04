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


int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int)
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

    app.inst = inst;

    // load settings as soon as possible
    opt.load();

    // setup UI dll & help early to get correct language msgs
    if (!app.loadResMod(opt.uiFile.c_str(), 0)) opt.uiFile = _T("");
    app.help.init(app.inst, opt.helpFile);

    if (!app.chkPrevInst() || !app.initComctl())
        return 0;

    if (!app.regWndCls() || !app.createMainWnd()) {
        Error(0, ResStr(IDS_ERR_WNDCLSREG));
        return 0;
    }

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        //if (app.activeModelessDlg && IsDialogMessage(app.activeModelessDlg, &msg))
        //    continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    opt.save();

    return msg.wParam;
}


static bool isWin8orGreater()
{
    return ef::Win::OsVer().majMin() >= ef::Win::packVer(6, 2);
}


static void CmNewPin(HWND wnd)
{
    // avoid re-entrancy
    if (app.layerWnd) return;

    // NOTE: fix for Win8+ (the top-left corner doesn't work)
    const POINT layerWndPos = isWin8orGreater() ? ef::Win::Point(100, 100) : ef::Win::Point(0, 0);
    const SIZE layerWndSize = { 1, 1 };

    app.layerWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        App::WNDCLS_PINLAYER,
        _T("DeskPin"),
        WS_POPUP | WS_VISIBLE,
        layerWndPos.x, layerWndPos.y, layerWndSize.cx, layerWndSize.cy,
        wnd, 0, app.inst, 0);

    if (!app.layerWnd) return;

    ShowWindow(app.layerWnd, SW_SHOW);

    // synthesize a WM_LBUTTONDOWN on layerWnd, so that it captures the mouse
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(layerWndPos.x, layerWndPos.y);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    SetCursorPos(pt.x, pt.y);
}


static void EvPinReq(HWND wnd, int x, int y)
{
    POINT pt = {x,y};
    HWND hitWnd = GetTopParent(WindowFromPoint(pt));
    PinWindow(wnd, hitWnd, opt.trackRate.value);
}


static void CmRemovePins(HWND wnd)
{
    HWND pin;
    while ((pin = FindWindow(App::WNDCLS_PIN, 0)) != 0)
        DestroyWindow(pin);
}


static void FixOptPSPos(HWND wnd)
{
    // - find taskbar ("Shell_TrayWnd")
    // - find notification area ("TrayNotifyWnd") (child of taskbar)
    // - get center of notification area
    // - determine quadrant of screen which includes the center point
    // - position prop sheet at proper corner of workarea
    RECT rc, rcWA, rcNW;
    HWND trayWnd, notityWnd;
    if (GetWindowRect(wnd, &rc)
        && SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWA, 0)
        && (trayWnd = FindWindow(_T("Shell_TrayWnd"), _T("")))
        && (notityWnd = FindWindowEx(trayWnd, 0, _T("TrayNotifyWnd"), _T("")))
        && GetWindowRect(notityWnd, &rcNW))
    {
        // '/2' simplified from the following two inequalities
        bool isLeft = (rcNW.left + rcNW.right) < GetSystemMetrics(SM_CXSCREEN);
        bool isTop  = (rcNW.top + rcNW.bottom) < GetSystemMetrics(SM_CYSCREEN);
        int x = isLeft ? rcWA.left : rcWA.right - (rc.right - rc.left);
        int y = isTop ? rcWA.top : rcWA.bottom - (rc.bottom - rc.top);
        OffsetRect(&rc, x-rc.left, y-rc.top);
        MoveWindow(wnd, rc);
    }

}


static tstring trayIconTip()
{
    TCHAR s[100];
    wsprintf(s, _T("%s - %s: %d"), App::APPNAME, ResStr(IDS_TRAYTIP, 50), app.pinsUsed);
    return s;
}


static WNDPROC orgOptPSProc;


LRESULT CALLBACK OptPSSubclass(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_SHOWWINDOW) {
        FixOptPSPos(wnd);

        // also set the big icon (for Alt-Tab)
        SendMessage(wnd, WM_SETICON, ICON_BIG, 
            LPARAM(LoadIcon(app.inst, MAKEINTRESOURCE(IDI_APP))));

        LRESULT ret = CallWindowProc(orgOptPSProc, wnd, msg, wparam, lparam);
        SetWindowLong(wnd, GWL_WNDPROC, LONG(orgOptPSProc));
        orgOptPSProc = 0;
        return ret;
    }

    return CallWindowProc(orgOptPSProc, wnd, msg, wparam, lparam);
}


// remove WS_EX_CONTEXTHELP and subclass to fix pos
static int CALLBACK OptPSCallback(HWND wnd, UINT msg, LPARAM param)
{
    if (msg == PSCB_INITIALIZED) {
        // remove caption help button
        ef::Win::WndH(wnd).modifyExStyle(WS_EX_CONTEXTHELP, 0);
        orgOptPSProc = WNDPROC(SetWindowLong(wnd, GWL_WNDPROC, LONG(OptPSSubclass)));
    }

    return 0;
}


static void BuildOptPropSheet(PROPSHEETHEADER& psh, PROPSHEETPAGE psp[], 
    int dlgIds[], DLGPROC dlgProcs[], int pageCnt, HWND parentWnd, 
    OptionsPropSheetData& data, ResStr& capStr)
{
    for (int n = 0; n < pageCnt; ++n) {
        psp[n].dwSize      = sizeof(psp[n]);
        psp[n].dwFlags     = PSP_HASHELP;
        psp[n].hInstance   = app.resMod ? app.resMod : app.inst;
        psp[n].pszTemplate = MAKEINTRESOURCE(dlgIds[n]);
        psp[n].hIcon       = 0;
        psp[n].pszTitle    = 0;
        psp[n].pfnDlgProc  = dlgProcs[n];
        psp[n].lParam      = reinterpret_cast<LPARAM>(&data);
        psp[n].pfnCallback = 0;
        psp[n].pcRefParent = 0;
    }

    psh.dwSize      = sizeof(psh);
    psh.dwFlags     = PSH_HASHELP | PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_USEHICON;
    psh.hwndParent  = parentWnd;
    psh.hInstance   = app.resMod ? app.resMod : app.inst;
    psh.hIcon       = app.smIcon;
    psh.pszCaption  = capStr;
    psh.nPages      = pageCnt;
    psh.nStartPage  = (app.optionPage >= 0 && app.optionPage < pageCnt) 
        ? app.optionPage : 0;
    psh.ppsp        = psp;
    psh.pfnCallback = OptPSCallback;

}


static void CmOptions(HWND wnd, WindowCreationMonitor& winCreMon)
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
    HINSTANCE curResMod = 0;
    if (!opt.uiFile.empty()) {
        tchar buf[MAX_PATH];
        GetModuleFileName(app.resMod, buf, sizeof(buf));
        curResMod = LoadLibrary(buf);
    }

    //#ifdef TEST_OPTIONS_PAGE
    //  psh.nStartPage = TEST_OPTIONS_PAGE;
    //#endif

    if (PropertySheet(&psh) == -1) {
        tstring msg = ResStr(IDS_ERR_DLGCREATE);
        msg += _T("\r\n");
        msg += ef::Win::getLastErrorStr();
        Error(wnd, msg.c_str());
    }

    if (curResMod) {
        FreeLibrary(curResMod);
        curResMod = 0;
    }

    // reset tray tip, in case lang was changed
    app.trayIcon.setTip(trayIconTip().c_str());

    isOptDlgOn = false;

    //#ifdef TEST_OPTIONS_PAGE
    //  // PostMessage() stalls the debugger a bit...
    //  //PostMessage(wnd, WM_COMMAND, CM_CLOSE, 0);
    //  DestroyWindow(wnd);
    //#endif
}


// set and manage tray menu images
class TrayMenuDecorations {
public:
    TrayMenuDecorations(HMENU menu)
    {
        int w = GetSystemMetrics(SM_CXMENUCHECK);
        int h = GetSystemMetrics(SM_CYMENUCHECK);

        HDC    memDC  = CreateCompatibleDC(0);
        HFONT  fnt    = ef::Win::FontH::create(_T("Marlett"), h, SYMBOL_CHARSET, ef::Win::FontH::noStyle);
        HBRUSH bkBrush = HBRUSH(GetStockObject(WHITE_BRUSH));

        HGDIOBJ orgFnt = GetCurrentObject(memDC, OBJ_FONT);
        HGDIOBJ orgBmp = GetCurrentObject(memDC, OBJ_BITMAP);

        bmpClose = makeBmp(memDC, w, h, bkBrush, menu, CM_CLOSE, fnt, _T("r"));
        bmpAbout = makeBmp(memDC, w, h, bkBrush, menu, CM_ABOUT, fnt, _T("s"));

        SelectObject(memDC, orgBmp);
        SelectObject(memDC, orgFnt);

        DeleteObject(fnt);
        DeleteDC(memDC);
    }

    ~TrayMenuDecorations()
    {
        if (bmpClose) DeleteObject(bmpClose);
        if (bmpAbout) DeleteObject(bmpAbout);
    }

protected:
    TrayMenuDecorations(const TrayMenuDecorations&);
    TrayMenuDecorations& operator=(const TrayMenuDecorations&);

    HBITMAP bmpClose, bmpAbout;

    HBITMAP makeBmp(HDC dc, int w, int h, HBRUSH bkBrush, HMENU menu, int idCmd, HFONT fnt, const TCHAR* c)
    {
        HBITMAP retBmp = CreateBitmap(w, h, 1, 1, 0);
        if (retBmp) {
            RECT rc = {0, 0, w, h};
            SelectObject(dc, fnt);
            SelectObject(dc, retBmp);
            FillRect(dc, &rc, bkBrush);
            DrawText(dc, c, 1, &rc, DT_CENTER | DT_NOPREFIX);
            SetMenuItemBitmaps(menu, idCmd, MF_BYCOMMAND, retBmp, retBmp);
        }
        return retBmp;
    }

};


static void EvTrayIcon(HWND wnd, WPARAM id, LPARAM msg)
{
    static bool gotLButtonDblClk = false;

    if (id != 0) return;

    switch (msg) {
        case WM_LBUTTONUP: {
            if (!opt.dblClkTray || gotLButtonDblClk) {
                SendMessage(wnd, WM_COMMAND, CM_NEWPIN, 0);
                gotLButtonDblClk = false;
            }
            break;
        }
        case WM_LBUTTONDBLCLK: {
            gotLButtonDblClk = true;
            break;
        }
        case WM_RBUTTONDOWN: {
            SetForegroundWindow(wnd);
            HMENU dummy = LoadLocalizedMenu(IDM_TRAY);
            HMENU menu = GetSubMenu(dummy,0);
            SetMenuDefaultItem(menu, CM_NEWPIN, false);

            TrayMenuDecorations tmd(menu);

            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, wnd, 0);
            DestroyMenu(dummy);
            SendMessage(wnd, WM_NULL, 0, 0);

            break;
        }
    }

}


static void EvHotkey(HWND wnd, int idHotKey)
{
    // ignore if there's an active pin layer
    if (app.layerWnd) return;

    switch (idHotKey) {
    case App::HOTID_ENTERPINMODE:
        PostMessage(wnd, WM_COMMAND, CM_NEWPIN, 0);
        break;
    case App::HOTID_TOGGLEPIN:
        TogglePin(wnd, GetForegroundWindow(), opt.trackRate.value);
        break;
    }

}


static void UpdateStatusMessage(HWND wnd)
{
    //ResStr s = app.pinsUsed == 0 ? ResStr(IDS_PINSUSED_0) :
    //           app.pinsUsed == 1 ? ResStr(IDS_PINSUSED_1) :
    //                               ResStr(IDS_PINSUSED_N, 100, app.pinsUsed);
    SetDlgItemInt(wnd, IDC_STATUS, app.pinsUsed, false);
}


static bool EvInitDlg(HWND wnd, HFONT& boldGUIFont, HFONT& underlineGUIFont)
{
    app.aboutDlg = wnd;

    UpdateStatusMessage(wnd);

    boldGUIFont = ef::Win::FontH::create(ef::Win::FontH::getStockDefaultGui(), 0, ef::Win::FontH::bold);
    underlineGUIFont = ef::Win::FontH::create(ef::Win::FontH::getStockDefaultGui(), 0, ef::Win::FontH::underline);

    if (boldGUIFont) {
        // make status and its label bold
        ef::Win::WndH status = GetDlgItem(wnd, IDC_STATUS);
        status.setFont(boldGUIFont);
        status.getWindow(GW_HWNDPREV).setFont(boldGUIFont);
    }

    // set dlg icons
    HICON appIcon = LoadIcon(app.inst, MAKEINTRESOURCE(IDI_APP));
    SendMessage(wnd, WM_SETICON, ICON_BIG,   LPARAM(appIcon));
    SendMessage(wnd, WM_SETICON, ICON_SMALL, LPARAM(app.smIcon));

    // set static icon if dlg was loaded from lang dll
    if (app.resMod)
        SendDlgItemMessage(wnd,IDC_LOGO,STM_SETIMAGE,IMAGE_ICON,LPARAM(appIcon));

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
        SetDlgItemText(wnd, link.id, link.title);
        UrlLink* ul = UrlLink::subclass(wnd, link.id);
        ul->setFonts(font, underlineGUIFont ? underlineGUIFont : font, font);
        ul->setColors(RGB(0,0,255), RGB(255,0,0), RGB(128,0,128));
        ul->setUrl(link.url);
    }

    //#ifdef TEST_OPTIONS_PAGE
    //  SendMessage(wnd, WM_COMMAND, CM_OPTIONS, 0);
    //#endif

    return true;
}


static void EvTermDlg(HWND wnd, HFONT& boldGUIFont, HFONT& underlineGUIFont)
{
    app.aboutDlg = 0;

    if (boldGUIFont) {
        DeleteObject(boldGUIFont);
        boldGUIFont = 0;
    }

    if (underlineGUIFont) {
        DeleteObject(underlineGUIFont);
        underlineGUIFont = 0;
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


static BOOL CALLBACK AboutDlgProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static HFONT   boldGUIFont, underlineGUIFont;

    switch (msg) {
        case WM_INITDIALOG:
            return EvInitDlg(wnd, boldGUIFont, underlineGUIFont);
        case WM_DESTROY:
            EvTermDlg(wnd, boldGUIFont, underlineGUIFont);
            return true;
        case App::WM_PINSTATUS:
            UpdateStatusMessage(wnd);
            return true;
        //case WM_ACTIVATE:
        //  app.activeModelessDlg = (LOWORD(wparam) == WA_INACTIVE) ? 0 : wnd;
        //  return false;
        case WM_COMMAND: {
            int id = LOWORD(wparam);
            int code = HIWORD(wparam);
            switch (id) {
                case IDOK:
                case IDCANCEL:
                    DestroyWindow(wnd);
                    return true;
                case IDC_LOGO:
                    if (code == STN_DBLCLK) {
                        showSpecialInfo(wnd);
                        return true;
                    }
            }
            return false;
        }
    }
    return false;
}


LRESULT CALLBACK MainWndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static UINT taskbarMsg = RegisterWindowMessage(_T("TaskbarCreated"));
    static PendingWindows pendWnds;
    static WindowCreationMonitor* winCreMon;

    switch (msg) {
        case WM_CREATE: {
            app.mainWnd = wnd;
            //winCreMon = new HookDllWindowCreationMonitor();
            winCreMon = new EventHookWindowCreationMonitor();

            if (opt.autoPinOn && !winCreMon->init(wnd, App::WM_QUEUEWINDOW)) {
                Error(wnd, ResStr(IDS_ERR_HOOKDLL));
                opt.autoPinOn = false;
            }

            app.smIcon = HICON(LoadImage(app.inst, MAKEINTRESOURCE(IDI_APP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
            app.createSmClrIcon(opt.pinClr);

            // first set the wnd (this can only be done once)...
            app.trayIcon.setWnd(wnd);
            // .. and then create the notification icon
            app.trayIcon.create(app.smClrIcon, trayIconTip().c_str());

            // setup hotkeys
            if (opt.hotkeysOn) {
                bool allKeysSet = true;
                allKeysSet &= opt.hotEnterPin.set(wnd);
                allKeysSet &= opt.hotTogglePin.set(wnd);
                if (!allKeysSet)
                    Error(wnd, ResStr(IDS_ERR_HOTKEYSSET));
            }

            if (opt.autoPinOn)
                SetTimer(wnd, App::TIMERID_AUTOPIN, opt.autoPinDelay.value, 0);

            // init pin image/shape/dims
            app.pinShape.initShape();
            app.pinShape.initImage(opt.pinClr);

            break;
        }
        case WM_DESTROY: {
            app.mainWnd = 0;

            winCreMon->term();
            delete winCreMon;
            winCreMon = 0;

            SendMessage(wnd, WM_COMMAND, CM_REMOVEPINS, 0);

            // remove hotkeys
            opt.hotEnterPin.unset(wnd);
            opt.hotTogglePin.unset(wnd);

            PostQuitMessage(0);
            break;
        }
        case App::WM_TRAYICON:
            EvTrayIcon(wnd, wparam, lparam);
            break;
        case App::WM_PINREQ:
            EvPinReq(wnd, int(wparam), int(lparam));
            break;
        case WM_HOTKEY:
            EvHotkey(wnd, wparam);
            break;
        case WM_TIMER:
            if (wparam == App::TIMERID_AUTOPIN) pendWnds.check(wnd, opt);
            break;
        case App::WM_QUEUEWINDOW:
            pendWnds.add(HWND(wparam));
            break;
        case App::WM_PINSTATUS:
            if (lparam) ++app.pinsUsed; else --app.pinsUsed;
            if (app.aboutDlg) SendMessage(app.aboutDlg, App::WM_PINSTATUS, 0, 0);
            app.trayIcon.setTip(trayIconTip().c_str());
            break;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDHELP:
                    app.help.show(wnd);
                    break;
                case CM_ABOUT:
                    if (app.aboutDlg)
                        SetForegroundWindow(app.aboutDlg);
                    else {
                        CreateLocalizedDialog(IDD_ABOUT, 0, AboutDlgProc);
                        ShowWindow(app.aboutDlg, SW_SHOW);
                    }
                    break;
                case CM_NEWPIN: 
                    CmNewPin(wnd);
                    break;
                case CM_REMOVEPINS:
                    CmRemovePins(wnd);
                    break;
                case CM_OPTIONS:
                    CmOptions(wnd, *winCreMon);
                    break;
                case CM_CLOSE:
                    DestroyWindow(wnd);
                    break;
                    //case ID_TEST:
                    //  CmTest(wnd);
                    //  break;
                default:
                    break;
            }
            break;
        case WM_ENDSESSION: {
            if (wparam)
                opt.save();
            return 0;
        }
        //case WM_SETTINGSCHANGE:
        //case WM_DISPLAYCHANGE:
        default:
            if (msg == taskbarMsg) {
                // taskbar recreated; reset the tray icon
                app.trayIcon.create(app.smClrIcon, trayIconTip().c_str());
                break;
            }
            return DefWindowProc(wnd, msg, wparam, lparam);
    }

    return 0;
}
