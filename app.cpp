#include "stdafx.h"
#include "util.h"
#include "pinwnd.h"
#include "pinlayerwnd.h"
#include "app.h"
#include "resource.h"


LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);


const tchar* App::APPNAME         = _T("DeskPins");
const tchar* App::WNDCLS_MAIN     = _T("EFDeskPins");
const tchar* App::WNDCLS_PIN      = _T("EFPinWnd");
const tchar* App::WNDCLS_PINLAYER = _T("EFPinLayerWnd");


// Load a resource dll and store it in 'hResMod'.
// The previous dll, if any, is released.
// On error (or if file is nul), 'hResMod' is set to 0 
// to use the built-in EXE resources.
// Returns success.
bool App::loadResMod(const tstring& file, HWND hMsgParent)
{
    // unload current
    freeResMod();

    // no res module -> use .exe resources
    if (file.empty())
        return true;

    // try to load module if it's a DLL
    tstring s = ef::dirSpec(ef::Win::getModulePath(hInst));
    if (!s.empty()) {
#ifdef _DEBUG
        s += _T("..\\Localization\\");
#endif
        s += file;
        hResMod = LoadLibrary(s.c_str());
    }

    // display warning if failed
    if (!hResMod) {
        tchar buf[MAX_PATH + 100];
        const tchar* msg = _T("Could not load language file: %s\r\n")
            _T("Reverting to English interface.");
        wsprintf(buf, msg, file.c_str());
        Error(hMsgParent, buf);
    }

    return hResMod != 0;
}


void App::freeResMod()
{
    if (hResMod) {
        FreeLibrary(hResMod);
        hResMod = 0;
    }
}


// TODO: move to eflib?
bool App::initComctl()
{
    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof(iccx);
    //iccx.dwICC = ICC_LISTVIEW_CLASSES | ICC_HOTKEY_CLASS | 
    //             ICC_TAB_CLASSES | ICC_UPDOWN_CLASS;
    iccx.dwICC = ICC_WIN95_CLASSES;
    bool ret = InitCommonControlsEx(&iccx);
    if (!ret)
        Error(0, ResStr(IDS_ERR_CCINIT));
    return ret;
}


// Return true to continue loading or false to abort.
// If automatic detection fails, ask the user.
bool App::chkPrevInst()
{
    if (prevInst.isRunning()) {
        MessageBox(0, ResStr(IDS_ERR_ALREADYRUNNING), APPNAME, MB_ICONINFORMATION);
        return false;
    }
    else if (!prevInst.isRunning())
        return true;
    else
        return MessageBox(0, ResStr(IDS_ERR_MUTEXFAILCONFIRM), APPNAME, MB_ICONEXCLAMATION | MB_YESNO) != IDNO;
}


bool App::regWndCls()
{
    WNDCLASS wc;

    wc.style         = 0;
    wc.lpfnWndProc   = MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = app.hInst;
    wc.hIcon         = 0;
    wc.hCursor       = 0;
    wc.hbrBackground = 0;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WNDCLS_MAIN;
    if (!RegisterClass(&wc))
        return false;

    wc.style         = 0;
    wc.lpfnWndProc   = PinWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(void*);  // data object ptr
    wc.hInstance     = app.hInst;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor(app.hInst, MAKEINTRESOURCE(IDC_REMOVEPIN));
    wc.hbrBackground = 0;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WNDCLS_PIN;
    if (!RegisterClass(&wc))
        return false;

    wc.style         = 0;
    wc.lpfnWndProc   = PinLayerWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = app.hInst;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor(app.hInst, MAKEINTRESOURCE(IDC_PLACEPIN));
    wc.hbrBackground = 0;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WNDCLS_PINLAYER;
    if (!RegisterClass(&wc))
        return false;

    return true;
}


bool App::createMainWnd()
{
    // app.hMainWnd set in WM_CREATE
    CreateWindow(App::WNDCLS_MAIN, App::APPNAME, 
        WS_POPUP, 0,0,0,0, 0, 0, app.hInst, 0);

    return app.hMainWnd;
}


// Make a new small icon, by painting the original.
// On failure, we get the original icon.
void App::createSmClrIcon(COLORREF clr)
{
    if (hSmClrIcon) DeleteObject(hSmClrIcon);
    hSmClrIcon = HICON(LoadImage(app.hInst, MAKEINTRESOURCE(IDI_APP), 
        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    ICONINFO ii;
    if (GetIconInfo(hSmClrIcon, &ii)) {
        // (assuming non-monochrome)
        COLORREF clrMap[][2] = {
            { CLR_LRED, clr       }, 
            { CLR_DRED, Dark(clr) }
        };
        remapBmpColors(ii.hbmColor, clrMap, ARRSIZE(clrMap));
        HICON hNewIcon = CreateIconIndirect(&ii);
        if (hNewIcon) {
            DestroyIcon(hSmClrIcon);
            hSmClrIcon = hNewIcon;
        }
        // (assuming non-monochrome)
        // destroy bmps returned from GetIconInfo()
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
    }

    tstring ss;


}

