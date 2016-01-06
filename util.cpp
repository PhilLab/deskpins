#include "stdafx.h"
#include "util.h"
#include "resource.h"


bool isWndRectEmpty(HWND wnd)
{
    RECT rc;
    return GetWindowRect(wnd, &rc) && IsRectEmpty(&rc);
}


bool isChild(HWND wnd)
{
    return (ef::Win::WndH(wnd).getStyle() & WS_CHILD) != 0;
}


HWND getNonChildParent(HWND wnd)
{
    while (isChild(wnd))
        wnd = GetParent(wnd);

    return wnd;
}


HWND getTopParent(HWND wnd /*, bool mustBeVisible*/)
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
        HWND parent = isChild(wnd)
            ? GetParent(wnd)
            : GetWindow(wnd, GW_OWNER);

        if (!parent || parent == wnd) break;
        //if (mustBeVisible && !IsWindowVisible(parent) || IsWndRectEmpty(parent))
        //  break;
        wnd = parent;
    }

    return wnd;
}


bool isProgManWnd(HWND wnd)
{ 
    return strimatch(ef::Win::WndH(wnd).getClassName().c_str(), L"ProgMan")
        && strimatch(ef::Win::WndH(wnd).getText().c_str(), L"Program Manager");
}


bool isTaskBar(HWND wnd)
{
    return strimatch(ef::Win::WndH(wnd).getClassName().c_str(), L"Shell_TrayWnd");
}


bool isTopMost(HWND wnd)
{
    return (ef::Win::WndH(wnd).getExStyle() & WS_EX_TOPMOST) != 0;
}


void error(HWND wnd, LPCWSTR s)
{
    ResStr caption(IDS_ERRBOXTTITLE, 50, reinterpret_cast<DWORD>(App::APPNAME));
    MessageBox(wnd, s, caption, MB_ICONSTOP | MB_TOPMOST);
}


void warning(HWND wnd, LPCWSTR s)
{
    ResStr caption(IDS_WRNBOXTTITLE, 50, reinterpret_cast<DWORD>(App::APPNAME));
    MessageBox(wnd, s, caption, MB_ICONWARNING | MB_TOPMOST);
}


// TODO: move to eflib?
bool getScrSize(SIZE& sz)
{
    return ((sz.cx = GetSystemMetrics(SM_CXSCREEN)) != 0 &&
        (sz.cy = GetSystemMetrics(SM_CYSCREEN)) != 0);
}


void pinWindow(HWND wnd, HWND hitWnd, int trackRate, bool silent)
{
    int err = 0, wrn = 0;

    if (!hitWnd)
        wrn = IDS_ERR_COULDNOTFINDWND;
    else if (isProgManWnd(hitWnd))
        wrn = IDS_ERR_CANNOTPINDESKTOP;
    // NOTE: after creating the layer wnd, the taskbar becomes non-topmost;
    // use this check to avoid pinning it
    else if (isTaskBar(hitWnd))
        wrn = IDS_ERR_CANNOTPINTASKBAR;
    else if (isTopMost(hitWnd))
        wrn = IDS_ERR_ALREADYTOPMOST;
    // hidden wnds are handled by the proxy mechanism
    //else if (!IsWindowVisible(hitWnd))
    //  Error(wnd, "Cannot pin a hidden window");
    else {
        // create a pin wnd
        HWND pin = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            App::WNDCLS_PIN,
            L"",
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
            error(wnd, ResStr(err));
        else
            warning(wnd, ResStr(wrn));
    }

}


// If the specified window (top parent) is pinned, 
// return the pin wnd's handle; otherwise return 0.
//
static HWND hasPin(HWND wnd)
{
    // enumerate all pin windows
    HWND pin = 0;
    while ((pin = FindWindowEx(0, pin, App::WNDCLS_PIN, 0)) != 0)
        //if (GetParent(pin) == wnd)
        if (HWND(SendMessage(pin, App::WM_PIN_GETPINNEDWND, 0, 0)) == wnd)
            return pin;

    return 0;
}


void togglePin(HWND wnd, HWND target, int trackRate)
{
    target = getTopParent(target);
    HWND pin = hasPin(target);
    if (pin)
        DestroyWindow(pin);
    else
        pinWindow(wnd, target, trackRate);
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


bool isLastErrResNotFound() {
    DWORD e = GetLastError();
    return e == ERROR_RESOURCE_DATA_NOT_FOUND ||
        e == ERROR_RESOURCE_TYPE_NOT_FOUND ||
        e == ERROR_RESOURCE_NAME_NOT_FOUND ||
        e == ERROR_RESOURCE_LANG_NOT_FOUND;
}


int LocalizedDialogBoxParam(LPCTSTR lpTemplate, HWND hParent, DLGPROC lpDialogFunc, LPARAM dwInit) {
    if (app.resMod) {
        int ret = DialogBoxParam(app.resMod, lpTemplate, hParent, lpDialogFunc, dwInit);
        if (ret != -1 || !isLastErrResNotFound())
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
        if (ret || !isLastErrResNotFound())
            return ret;
    }
    return CreateDialog(app.inst, lpTemplate, hParent, lpDialogFunc);
}

HWND CreateLocalizedDialog(WORD id, HWND hParent, DLGPROC lpDialogFunc) {
    return CreateLocalizedDialog(MAKEINTRESOURCE(id), hParent, lpDialogFunc);
}


// TODO: move to eflib?
bool rectContains(const RECT& rc1, const RECT& rc2)
{
    return rc2.left   >= rc1.left
        && rc2.top    >= rc1.top
        && rc2.right  <= rc1.right
        && rc2.bottom <= rc1.bottom;
}


// enable/disable all ctrls that lie inside the specified ctrl 
// (usually a group, or maybe a tab, etc)
// TODO: move to eflib?
void enableGroup(HWND wnd, int id, bool mode)
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
            if (rectContains(rc, rc2))
                EnableWindow(child, mode);
    }

}


// TODO: move to eflib?
std::vector<std::wstring> getFiles(std::wstring mask)
{
    std::vector<std::wstring> ret;
    for (ef::Win::FileFinder fde(mask, ef::Win::FileFinder::files); fde; ++fde)
        ret.push_back(fde.getName());
    return ret;
}


COLORREF light(COLORREF clr)
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


COLORREF dark(COLORREF clr)
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


BOOL moveWindow(HWND wnd, const RECT& rc, BOOL repaint)
{
    return MoveWindow(wnd, rc.left, rc.top, 
        rc.right-rc.left, rc.bottom-rc.top, repaint);
}


BOOL rectangle(HDC dc, const RECT& rc)
{
    return Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
}


bool psChanged(HWND page)
{
    return !!PropSheet_Changed(GetParent(page), page);
}


// removes the first accelerator prefix ('&')
// from a string and returns the result
std::wstring remAccel(std::wstring s)
{
    std::wstring::size_type i = s.find_first_of(L"&");
    if (i != std::wstring::npos) s.erase(i, 1);
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
std::wstring substrAfterLast(const std::wstring& s, const std::wstring& delim)
{
    std::wstring::size_type i = s.find_last_of(delim);
    return i == std::wstring::npos ? L"" : s.substr(i + delim.length());
}
