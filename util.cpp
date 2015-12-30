#include "stdafx.h"
#include "util.h"
#include "resource.h"


bool IsWndRectEmpty(HWND hWnd)
{
    RECT rc;
    return GetWindowRect(hWnd, &rc) && IsRectEmpty(&rc);
}


/*
// I thought that GetWindowLong(GWW_HWNDPARENT) returned the real parent
// and GetParent() returned the owner (as stated in MSDN).
// However, Spy++ shows the opposite...
HWND GetOwner(HWND hWnd)
{
return reinterpret_cast<HWND>(::GetWindowLong(hWnd, GWL_HWNDPARENT));
}
*/


bool IsChild(HWND hWnd)
{
    return ef::Win::WndH(hWnd).getStyle() & WS_CHILD;
}


HWND GetNonChildParent(HWND hWnd)
{
    while (IsChild(hWnd))
        hWnd = GetParent(hWnd);

    return hWnd;
}


HWND GetTopParent(HWND hWnd /*, bool mustBeVisible = false*/)
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
        HWND hParent = IsChild(hWnd)
            ? GetParent(hWnd)
            : GetWindow(hWnd, GW_OWNER);

        if (!hParent || hParent == hWnd) break;
        //if (mustBeVisible && !IsWindowVisible(hParent) || IsWndRectEmpty(hParent))
        //  break;
        hWnd = hParent;
    }

    return hWnd;
}


bool IsProgManWnd(HWND hWnd)
{ 
    return strimatch(ef::Win::WndH(hWnd).getClassName().c_str(), _T("ProgMan"))
        && strimatch(ef::Win::WndH(hWnd).getText().c_str(), _T("Program Manager"));
}


bool IsTaskBar(HWND hWnd)
{
    return strimatch(ef::Win::WndH(hWnd).getClassName().c_str(), _T("Shell_TrayWnd"));
}


bool IsTopMost(HWND hWnd)
{
    return ef::Win::WndH(hWnd).getExStyle() & WS_EX_TOPMOST;
}


void Error(HWND hWnd, const tchar* s)
{
    ResStr caption(IDS_ERRBOXTTITLE, 50, reinterpret_cast<DWORD>(App::APPNAME));
    MessageBox(hWnd, s, caption, MB_ICONSTOP | MB_TOPMOST);
}


void Warning(HWND hWnd, const tchar* s)
{
    ResStr caption(IDS_WRNBOXTTITLE, 50, reinterpret_cast<DWORD>(App::APPNAME));
    MessageBox(hWnd, s, caption, MB_ICONWARNING | MB_TOPMOST);
}


// TODO: move to eflib?
bool GetScrSize(SIZE& sz)
{
    return ((sz.cx = GetSystemMetrics(SM_CXSCREEN)) != 0 &&
        (sz.cy = GetSystemMetrics(SM_CYSCREEN)) != 0);
}


void PinWindow(HWND hWnd, HWND hHitWnd, int trackRate, bool silent /*= false*/)
{
    int err = 0, wrn = 0;

    if (!hHitWnd)
        wrn = IDS_ERR_COULDNOTFINDWND;
    else if (IsProgManWnd(hHitWnd))
        wrn = IDS_ERR_CANNOTPINDESKTOP;
    // NOTE: after creating the layer wnd, the taskbar becomes non-topmost;
    // use this check to avoid pinning it
    else if (IsTaskBar(hHitWnd))
        wrn = IDS_ERR_CANNOTPINTASKBAR;
    else if (IsTopMost(hHitWnd))
        wrn = IDS_ERR_ALREADYTOPMOST;
    // hidden wnds are handled by the proxy mechanism
    //else if (!IsWindowVisible(hHitWnd))
    //  Error(hWnd, "Cannot pin a hidden window");
    else {
        // create a pin wnd
        HWND hPin = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            App::WNDCLS_PIN,
            _T(""),
            WS_POPUP | WS_VISIBLE,
            0, 0, 0, 0,   // real pos/size set on wnd assignment
            0, 0, app.hInst, 0);

        if (!hPin)
            err = IDS_ERR_PINCREATE;
        else if (!SendMessage(hPin, App::WM_PIN_ASSIGNWND, WPARAM(hHitWnd), trackRate)) {
            err = IDS_ERR_PINWND;
            DestroyWindow(hPin);
        }
    }

    if (!silent && (err || wrn)) {
        if (err)
            Error(hWnd, ResStr(err));
        else
            Warning(hWnd, ResStr(wrn));
    }

}


// If the specified window (top parent) is pinned, 
// return the pin wnd's handle; otherwise return 0.
//
static HWND HasPin(HWND hWnd)
{
    // enum all pin windows
    HWND hPin = 0;
    while ((hPin = FindWindowEx(0, hPin, App::WNDCLS_PIN, 0)) != 0)
        //if (GetParent(hPin) == hWnd)
        if (HWND(SendMessage(hPin, App::WM_PIN_GETPINNEDWND, 0, 0)) == hWnd)
            return hPin;

    return 0;
}


void TogglePin(HWND hWnd, HWND hTarget, int trackRate)
{
    hTarget = GetTopParent(hTarget);
    HWND hPin = HasPin(hTarget);
    if (hPin)
        DestroyWindow(hPin);
    else
        PinWindow(hWnd, hTarget, trackRate);
}


HMENU LoadLocalizedMenu(LPCTSTR lpMenuName) {
    if (app.hResMod) {
        HMENU ret = LoadMenu(app.hResMod, lpMenuName);
        if (ret)
            return ret;
    }
    return LoadMenu(app.hInst, lpMenuName);
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


int LocalizedDialogBoxParam(LPCTSTR lpTemplateName, 
                            HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
                                if (app.hResMod) {
                                    int ret = DialogBoxParam(app.hResMod, lpTemplateName, 
                                        hWndParent, lpDialogFunc, dwInitParam);
                                    if (ret != -1 || !IsLastErrResNotFound())
                                        return ret;
                                }
                                return DialogBoxParam(app.hInst, lpTemplateName, 
                                    hWndParent, lpDialogFunc, dwInitParam);
}

int LocalizedDialogBoxParam(WORD id, 
                            HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
                                return LocalizedDialogBoxParam(MAKEINTRESOURCE(id), 
                                    hWndParent, lpDialogFunc, dwInitParam);
}


HWND CreateLocalizedDialog(LPCTSTR lpTemplate,
                           HWND hWndParent, DLGPROC lpDialogFunc) {
                               if (app.hResMod) {
                                   HWND ret = CreateDialog(app.hResMod, lpTemplate, hWndParent, lpDialogFunc);
                                   if (ret || !IsLastErrResNotFound())
                                       return ret;
                               }
                               return CreateDialog(app.hInst, lpTemplate, hWndParent, lpDialogFunc);
}

HWND CreateLocalizedDialog(WORD id,
                           HWND hWndParent, DLGPROC lpDialogFunc) {
                               return CreateLocalizedDialog(MAKEINTRESOURCE(id), hWndParent, lpDialogFunc);
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
void EnableGroup(HWND hWnd, int id, bool mode)
{
    HWND hContainer = GetDlgItem(hWnd, id);
    RECT rc;
    GetWindowRect(hContainer, &rc);

    // deflate a bit (4 DLUs) to be on the safe side (do we need this?)
    RECT rc2 = {0, 0, 4, 4};
    MapDialogRect(hWnd, &rc2);
    InflateRect(&rc, rc2.left-rc2.right, rc2.top-rc2.bottom);

    for (HWND hChild = GetWindow(hWnd, GW_CHILD); 
        hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
            if (hChild == hContainer)
                continue;
            GetWindowRect(hChild, &rc2);
            if (RectContains(rc, rc2))
                EnableWindow(hChild, mode);
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


BOOL MoveWindow(HWND hWnd, const RECT& rc, BOOL repaint)
{
    return MoveWindow(hWnd, rc.left, rc.top, 
        rc.right-rc.left, rc.bottom-rc.top, repaint);
}


BOOL Rectangle(HDC hDC, const RECT& rc)
{
    return Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
}


bool PSChanged(HWND hPage)
{
    return PropSheet_Changed(GetParent(hPage), hPage);
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
bool getBmpSize(HBITMAP hBmp, SIZE& sz)
{
    BITMAP bm;
    if (!GetObject(hBmp, sizeof(bm), &bm))
        return false;
    sz.cx = bm.bmWidth;
    sz.cy = abs(bm.bmHeight);
    return true;
}


// TODO: move to eflib?
bool getBmpSizeAndBpp(HBITMAP hBmp, SIZE& sz, int& bpp)
{
    BITMAP bm;
    if (!GetObject(hBmp, sizeof(bm), &bm))
        return false;
    sz.cx = bm.bmWidth;
    sz.cy = abs(bm.bmHeight);
    bpp   = bm.bmBitsPixel;
    return true;
}


// Convert white/lgray/dgray of bmp to shades of 'clr'.
// NOTE: Bmp must *not* be selected in any DC for this to succeed.
//
bool remapBmpColors(HBITMAP hBmp, COLORREF clrs[][2], int cnt)
{
    bool ok = false;
    SIZE sz;
    int bpp;
    if (getBmpSizeAndBpp(hBmp, sz, bpp)) {
        if (HDC hDC = CreateCompatibleDC(0)) {
            if (HBITMAP hOrgBmp = HBITMAP(SelectObject(hDC, hBmp))) {
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
                        COLORREF clr = GetPixel(hDC, x, y);
                        for (int n = 0; n < cnt; ++n)
                            if (clr == clrs[n][0])
                                SetPixelV(hDC, x, y, clrs[n][1]);
                    }
                }
                ok = true;
                SelectObject(hDC, hOrgBmp);
            }
            DeleteDC(hDC);
        }
    }

    //ostringstream oss;
    //oss << hex << setfill('0');
    //for (set<COLORREF>::const_iterator it = clrSet.begin(); it != clrSet.end(); ++it)
    //  oss << setw(6) << *it << "\r\n";
    //MessageBoxA(app.hMainWnd, oss.str().c_str(), "Unique clrs", 0);

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
