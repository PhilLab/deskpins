#include "stdafx.h"
#include "util.h"
#include "resource.h"


bool IsWndRectEmpty(HWND wnd)
{
    RECT rc;
    return GetWindowRect(wnd, &rc) && IsRectEmpty(&rc);
}


bool IsChild(HWND wnd)
{
    return (ef::Win::WndH(wnd).getStyle() & WS_CHILD) != 0;
}


HWND GetNonChildParent(HWND wnd)
{
    while (IsChild(wnd))
        wnd = GetParent(wnd);

    return wnd;
}


HWND GetTopParent(HWND wnd /*, bool mustBeVisible*/)
{
    // ------------------------------------------------------
    // NOTE: 'mustBeVisible' is not used currently
    // ------------------------------------------------------
    // By setting the 'mustBeVisible' flag,
    // more constraints are applied to the ultimate parent searching:
    //
    // #1: Stop when parent is invisible (e.g. the shell's Display Properties
    //     which has a hidden parent from RunDll)
    // #2: Stop when parent rect is null. This is the case with VCL apps
    //     where a parent window (TApplication class) is created as a proxy
    //     for app functionality (it has WS_VISIBLE, but the width/height are 0)
    //
    // The pinning engine handles invisible wnds (via the proxy mechanism).
    // 'mustBeVisible' is used mostly for user interaction
    // (e.g. peeking a window's attributes)
    // ------------------------------------------------------

    // go up the window chain to find the ultimate top-level wnd
    // for WS_CHILD wnds use GetParent()
    // for the rest use GetWindow(GW_OWNER)
    //
    for (;;) {
        HWND parent = IsChild(wnd)
            ? GetParent(wnd)
            : GetWindow(wnd, GW_OWNER);

        if (!parent || parent == wnd) break;
        //if (mustBeVisible && !IsWindowVisible(parent) || IsWndRectEmpty(parent))
        //  break;
        wnd = parent;
    }

    return wnd;
}


bool IsProgManWnd(HWND wnd)
{ 
    return strimatch(ef::Win::WndH(wnd).getClassName().c_str(), _T("ProgMan"))
        && strimatch(ef::Win::WndH(wnd).getText().c_str(), _T("Program Manager"));
}


bool IsTaskBar(HWND wnd)
{
    return strimatch(ef::Win::WndH(wnd).getClassName().c_str(), _T("Shell_TrayWnd"));
}


bool IsTopMost(HWND wnd)
{
    return (ef::Win::WndH(wnd).getExStyle() & WS_EX_TOPMOST) != 0;
}


void Error(HWND wnd, const tchar* s)
{
    ResStr caption(IDS_ERRBOXTTITLE, 50, reinterpret_cast<DWORD>(App::APPNAME));
    MessageBox(wnd, s, caption, MB_ICONSTOP | MB_TOPMOST);
}


void Warning(HWND wnd, const tchar* s)
{
    ResStr caption(IDS_WRNBOXTTITLE, 50, reinterpret_cast<DWORD>(App::APPNAME));
    MessageBox(wnd, s, caption, MB_ICONWARNING | MB_TOPMOST);
}


// TODO: move to eflib?
bool GetScrSize(SIZE& sz)
{
    return ((sz.cx = GetSystemMetrics(SM_CXSCREEN)) != 0 &&
        (sz.cy = GetSystemMetrics(SM_CYSCREEN)) != 0);
}


void PinWindow(HWND wnd, HWND hitWnd, int trackRate, bool silent)
{
    int err = 0, wrn = 0;

    if (!hitWnd)
        wrn = IDS_ERR_COULDNOTFINDWND;
    else if (IsProgManWnd(hitWnd))
        wrn = IDS_ERR_CANNOTPINDESKTOP;
    // NOTE: after creating the layer wnd, the taskbar becomes non-topmost;
    // use this check to avoid pinning it
    else if (IsTaskBar(hitWnd))
        wrn = IDS_ERR_CANNOTPINTASKBAR;
    else if (IsTopMost(hitWnd))
        wrn = IDS_ERR_ALREADYTOPMOST;
    // hidden wnds are handled by the proxy mechanism
    //else if (!IsWindowVisible(hitWnd))
    //  Error(wnd, "Cannot pin a hidden window");
    else {
        // create a pin wnd
        HWND pin = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            App::WNDCLS_PIN,
            _T(""),
            WS_POPUP | WS_VISIBLE,
            0, 0, 0, 0,   // real pos/size set on wnd assignment
            0, 0, app.inst, 0);

        if (!pin)
            err = IDS_ERR_PINCREATE;
        else if (!SendMessage(pin, App::WM_PIN_ASSIGNWND, WPARAM(hitWnd), trackRate)) {
            err = IDS_ERR_PINWND;
            DestroyWindow(pin);
        }
    }

    if (!silent && (err || wrn)) {
        if (err)
            Error(wnd, ResStr(err));
        else
            Warning(wnd, ResStr(wrn));
    }

}


// If the specified window (top parent) is pinned, 
// return the pin wnd's handle; otherwise return 0.
//
static HWND HasPin(HWND wnd)
{
    // enumerate all pin windows
    HWND pin = 0;
    while ((pin = FindWindowEx(0, pin, App::WNDCLS_PIN, 0)) != 0)
        //if (GetParent(pin) == wnd)
        if (HWND(SendMessage(pin, App::WM_PIN_GETPINNEDWND, 0, 0)) == wnd)
            return pin;

    return 0;
}


void TogglePin(HWND wnd, HWND target, int trackRate)
{
    target = GetTopParent(target);
    HWND pin = HasPin(target);
    if (pin)
        DestroyWindow(pin);
    else
        PinWindow(wnd, target, trackRate);
}


HMENU LoadLocalizedMenu(LPCTSTR lpMenuName) {
    if (app.resMod) {
        HMENU ret = LoadMenu(app.resMod, lpMenuName);
        if (ret)
            return ret;
    }
    return LoadMenu(app.inst, lpMenuName);
}


HMENU LoadLocalizedMenu(WORD id) {
    return LoadLocalizedMenu(MAKEINTRESOURCE(id));
}


bool IsLastErrResNotFound() {
    DWORD e = GetLastError();
    return e == ERROR_RESOURCE_DATA_NOT_FOUND ||
        e == ERROR_RESOURCE_TYPE_NOT_FOUND ||
        e == ERROR_RESOURCE_NAME_NOT_FOUND ||
        e == ERROR_RESOURCE_LANG_NOT_FOUND;
}


int LocalizedDialogBoxParam(LPCTSTR lpTemplate, HWND hParent, DLGPROC lpDialogFunc, LPARAM dwInit) {
    if (app.resMod) {
        int ret = DialogBoxParam(app.resMod, lpTemplate, hParent, lpDialogFunc, dwInit);
        if (ret != -1 || !IsLastErrResNotFound())
            return ret;
    }
    return DialogBoxParam(app.inst, lpTemplate, hParent, lpDialogFunc, dwInit);
}

int LocalizedDialogBoxParam(WORD id, HWND hParent, DLGPROC lpDialogFunc, LPARAM dwInit) {
    return LocalizedDialogBoxParam(MAKEINTRESOURCE(id), hParent, lpDialogFunc, dwInit);
}


HWND CreateLocalizedDialog(LPCTSTR lpTemplate, HWND hParent, DLGPROC lpDialogFunc) {
    if (app.resMod) {
        HWND ret = CreateDialog(app.resMod, lpTemplate, hParent, lpDialogFunc);
        if (ret || !IsLastErrResNotFound())
            return ret;
    }
    return CreateDialog(app.inst, lpTemplate, hParent, lpDialogFunc);
}

HWND CreateLocalizedDialog(WORD id, HWND hParent, DLGPROC lpDialogFunc) {
    return CreateLocalizedDialog(MAKEINTRESOURCE(id), hParent, lpDialogFunc);
}


// TODO: move to eflib?
bool RectContains(const RECT& rc1, const RECT& rc2)
{
    return rc2.left   >= rc1.left
        && rc2.top    >= rc1.top
        && rc2.right  <= rc1.right
        && rc2.bottom <= rc1.bottom;
}


// enable/disable all ctrls that lie inside the specified ctrl 
// (usually a group, or maybe a tab, etc)
// TODO: move to eflib?
void EnableGroup(HWND wnd, int id, bool mode)
{
    HWND container = GetDlgItem(wnd, id);
    RECT rc;
    GetWindowRect(container, &rc);

    // deflate a bit (4 DLUs) to be on the safe side (do we need this?)
    RECT rc2 = {0, 0, 4, 4};
    MapDialogRect(wnd, &rc2);
    InflateRect(&rc, rc2.left-rc2.right, rc2.top-rc2.bottom);

    for (HWND child = GetWindow(wnd, GW_CHILD); 
        child; child = GetWindow(child, GW_HWNDNEXT)) {
            if (child == container)
                continue;
            GetWindowRect(child, &rc2);
            if (RectContains(rc, rc2))
                EnableWindow(child, mode);
    }

}


// TODO: move to eflib?
std::vector<tstring> GetFiles(tstring mask)
{
    std::vector<tstring> ret;
    for (ef::Win::FileFinder fde(mask, ef::Win::FileFinder::files); fde; ++fde)
        ret.push_back(fde.getName());
    return ret;
}


COLORREF Light(COLORREF clr)
{
    double r = GetRValue(clr) / 255.0;
    double g = GetGValue(clr) / 255.0;
    double b = GetBValue(clr) / 255.0;
    double h, l, s;
    ef::RGBtoHLS(r, g, b, h, l, s);

    l = min(1, l+0.25);

    ef::HLStoRGB(h, l, s, r, g, b);
    return RGB(r*255, g*255, b*255);
}


COLORREF Dark(COLORREF clr)
{
    double r = GetRValue(clr) / 255.0;
    double g = GetGValue(clr) / 255.0;
    double b = GetBValue(clr) / 255.0;
    double h, l, s;
    ef::RGBtoHLS(r, g, b, h, l, s);

    l = max(0, l-0.25);

    ef::HLStoRGB(h, l, s, r, g, b);
    return RGB(r*255, g*255, b*255);
}


BOOL MoveWindow(HWND wnd, const RECT& rc, BOOL repaint)
{
    return MoveWindow(wnd, rc.left, rc.top, 
        rc.right-rc.left, rc.bottom-rc.top, repaint);
}


BOOL Rectangle(HDC dc, const RECT& rc)
{
    return Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
}


bool PSChanged(HWND page)
{
    return !!PropSheet_Changed(GetParent(page), page);
}


// removes the first accelerator prefix ('&')
// from a string and returns the result
tstring RemAccel(tstring s)
{
    tstring::size_type i = s.find_first_of(_T("&"));
    if (i != tstring::npos) s.erase(i, 1);
    return s;
}


// TODO: move to eflib?
bool getBmpSize(HBITMAP bmp, SIZE& sz)
{
    BITMAP bm;
    if (!GetObject(bmp, sizeof(bm), &bm))
        return false;
    sz.cx = bm.bmWidth;
    sz.cy = abs(bm.bmHeight);
    return true;
}


// TODO: move to eflib?
bool getBmpSizeAndBpp(HBITMAP bmp, SIZE& sz, int& bpp)
{
    BITMAP bm;
    if (!GetObject(bmp, sizeof(bm), &bm))
        return false;
    sz.cx = bm.bmWidth;
    sz.cy = abs(bm.bmHeight);
    bpp   = bm.bmBitsPixel;
    return true;
}


// Convert white/lgray/dgray of bmp to shades of 'clr'.
// NOTE: Bmp must *not* be selected in any DC for this to succeed.
//
bool remapBmpColors(HBITMAP bmp, COLORREF clrs[][2], int cnt)
{
    bool ok = false;
    SIZE sz;
    int bpp;
    if (getBmpSizeAndBpp(bmp, sz, bpp)) {
        if (HDC dc = CreateCompatibleDC(0)) {
            if (HBITMAP orgBmp = HBITMAP(SelectObject(dc, bmp))) {
                if (bpp == 16) {
                    // In 16-bpp modes colors get changed,
                    // e.g. light gray (0xC0C0C0) becomes 0xC6C6C6
                    // GetNearestColor() only works for paletized modes,
                    // so we'll have to patch our source colors manually
                    // using Nearest16BppColor.
                    ef::Win::Nearest16BppColor nearClr;
                    for (int n = 0; n < cnt; ++n)
                        clrs[n][0] = nearClr(clrs[n][0]);
                }
                for (int y = 0; y < sz.cy; ++y) {
                    for (int x = 0; x < sz.cx; ++x) {
                        COLORREF clr = GetPixel(dc, x, y);
                        for (int n = 0; n < cnt; ++n)
                            if (clr == clrs[n][0])
                                SetPixelV(dc, x, y, clrs[n][1]);
                    }
                }
                ok = true;
                SelectObject(dc, orgBmp);
            }
            DeleteDC(dc);
        }
    }

    //ostringstream oss;
    //oss << hex << setfill('0');
    //for (set<COLORREF>::const_iterator it = clrSet.begin(); it != clrSet.end(); ++it)
    //  oss << setw(6) << *it << "\r\n";
    //MessageBoxA(app.mainWnd, oss.str().c_str(), "Unique clrs", 0);

    return ok;
}


// Returns the part of a string after the last occurrence of a token.
// Example: substrAfter("foobar", "oo") --> "bar"
// Returns "" on error.
tstring substrAfterLast(const tstring& s, const tstring& delim)
{
    tstring::size_type i = s.find_last_of(delim);
    return i == tstring::npos ? tstring() : s.substr(i + delim.length());
}
