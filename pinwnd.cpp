#include "stdafx.h"
#include "util.h"
#include "pinshape.h"
#include "pinwnd.h"
#include "resource.h"


class PinData;

static LRESULT EvCreate(HWND wnd, PinData& pd);
static void EvDestroy(HWND wnd, PinData& pd);
static void EvTimer(HWND wnd, PinData& pd, UINT id);
static void EvPaint(HWND wnd, PinData& pd);
static void EvLClick(HWND wnd, PinData& pd);
static bool EvPinAssignWnd(HWND wnd, PinData& pd, HWND target, int pollRate);
static HWND EvGetPinnedWnd(HWND wnd, PinData& pd);
static void EvPinResetTimer(HWND wnd, PinData& pd, int pollRate);

static bool IsVCLAppWnd(HWND wnd);
static bool SelectProxy(HWND wnd, const PinData& pd);
static BOOL CALLBACK EnumThreadWndProc(HWND wnd, LPARAM param);
static void FixTopStyle(HWND /*wnd*/, const PinData& pd);
static void PlaceOnCaption(HWND wnd, const PinData& pd);
static bool FixVisible(HWND wnd, const PinData& pd);


// Data object assigned to each pin wnd.
//
class PinData {
public:
    // Attach to pin wnd and return object or 0 on error.
    static PinData* create(HWND pin, HWND callback) {
        PinData* pd = new PinData(callback);
        SetLastError(0);
        if (!SetWindowLong(pin, 0, LONG(pd)) && GetLastError()) {
            delete pd;
            pd = 0;
        }
        return pd;
    }
    // Get object or 0 on error.
    static PinData* get(HWND pin) {
        return reinterpret_cast<PinData*>(GetWindowLong(pin, 0));
    }
    // Detach from pin wnd and delete.
    static void destroy(HWND pin) {
        PinData* pd = get(pin);
        if (!pd)
            return;
        SetLastError(0);
        if (!SetWindowLong(pin, 0, 0) && GetLastError())
            return;
        delete pd;
    }

    HWND callbackWnd;

    bool proxyMode;
    HWND topMostWnd;
    HWND proxyWnd;

    HWND getPinOwner() const {
        return proxyMode ? proxyWnd : topMostWnd;
    }

private:
    PinData(HWND wnd)
    :
        callbackWnd(wnd),
        proxyMode(false),
        topMostWnd(0),
        proxyWnd(0)
    {}
};


// Pin wnd proc.
//
LRESULT CALLBACK PinWndProc(HWND wnd, UINT msg, 
                            WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_NCCREATE) {
        PinData::create(wnd, app.mainWnd);
        return true;
    }
    else if (msg == WM_NCDESTROY) {
        PinData::destroy(wnd);
        return 0;
    }
    PinData* pd = PinData::get(wnd);
    if (pd) {
        switch (msg) {
            case WM_CREATE:         return EvCreate(wnd, *pd);
            case WM_DESTROY:        return EvDestroy(wnd, *pd), 0;
            case WM_TIMER:          return EvTimer(wnd, *pd, wparam), 0;
            case WM_PAINT:          return EvPaint(wnd, *pd), 0;
            case WM_LBUTTONDOWN:    return EvLClick(wnd, *pd), 0;
            case App::WM_PIN_RESETTIMER:   return EvPinResetTimer(wnd, *pd, int(wparam)), 0;
            case App::WM_PIN_ASSIGNWND:    return EvPinAssignWnd(wnd, *pd, HWND(wparam), int(lparam));
            case App::WM_PIN_GETPINNEDWND: return LRESULT(EvGetPinnedWnd(wnd, *pd));
        }
    }
    return DefWindowProc(wnd, msg, wparam, lparam);
}


static LRESULT EvCreate(HWND wnd, PinData& pd)
{
    // send 'pin created' notification
    PostMessage(pd.callbackWnd, App::WM_PINSTATUS, WPARAM(wnd), true);

    // initially place pin in the middle of the screen;
    // later it will be positioned on the pinned wnd's caption
    RECT rc;
    GetWindowRect(wnd, &rc);
    int wndW = rc.right - rc.left;
    int wndH = rc.bottom - rc.top;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(wnd, 0, (scrW-wndW)/2, (scrH-wndH)/2, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE);

    return 0;
}


static void EvDestroy(HWND wnd, PinData& pd)
{
    if (pd.topMostWnd) {
        SetWindowPos(pd.topMostWnd, HWND_NOTOPMOST, 0,0,0,0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); // NOTE: added noactivate
        pd.topMostWnd = pd.proxyWnd = 0;
        pd.proxyMode = false;
    }

    // send 'pin destroyed' notification
    PostMessage(pd.callbackWnd, App::WM_PINSTATUS, WPARAM(wnd), false);
}


// helper to collect all non-child windows of a thread
class ThreadWnds {
public:
    ThreadWnds(HWND wnd) : appWnd(wnd) {}
    int count() const { return wndList.size(); }
    HWND operator[](int n) const { return wndList[n]; }

    bool collect()
    {
        DWORD threadId = GetWindowThreadProcessId(appWnd, 0);
        return !!EnumThreadWindows(threadId, (WNDENUMPROC)enumProc, LPARAM(this));
    }

protected:
    HWND appWnd;
    std::vector<HWND> wndList;

    static BOOL CALLBACK enumProc(HWND wnd, LPARAM param)
    {
        ThreadWnds& obj = *reinterpret_cast<ThreadWnds*>(param);
        if (wnd == obj.appWnd || GetWindow(wnd, GW_OWNER) == obj.appWnd)
            obj.wndList.push_back(wnd);
        return true;  // continue enumeration
    }

};


// - get the visible, top-level windows
// - if any enabled wnds are after any disabled
//   get the last enabled wnd; otherwise exit
// - move all disabled wnds (starting from the bottom one)
//   behind the last enabled
//
static void FixPopupZOrder(HWND appWnd)
{
    // - find the most visible, disabled, top-level wnd (wnd X)
    // - find all non-disabled, top-level wnds and place them
    //   above wnd X

    // get all non-child wnds
    ThreadWnds threadWnds(appWnd);
    if (!threadWnds.collect())
        return;

    // HACK: here I'm assuming that EnumThreadWindows returns
    // HWNDs acoording to their z-order (is this documented anywhere?)

    // Reordering is needed if any disabled wnd
    // is above any enabled one.
    bool needReordering = false;
    for (int n = 1; n < threadWnds.count(); ++n) {
        if (!IsWindowEnabled(threadWnds[n-1]) && IsWindowEnabled(threadWnds[n])) {
            needReordering = true;
            break;
        }
    }

    if (!needReordering)
        return;

    // find last enabled
    HWND lastEnabled = 0;
    for (int n = threadWnds.count()-1; n >= 0; --n) {
        if (IsWindowEnabled(threadWnds[n])) {
            lastEnabled = threadWnds[n];
            break;
        }
    }

    // none enabled? bail out
    if (!lastEnabled)
        return;

    // move all disabled (starting from the last) behind the last enabled
    for (int n = threadWnds.count()-1; n >= 0; --n) {
        if (!IsWindowEnabled(threadWnds[n])) {
            SetWindowPos(threadWnds[n], lastEnabled, 0,0,0,0, 
                SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
        }
    }

}


static void EvTimer(HWND wnd, PinData& pd, UINT id)
{
    if (id != 1) return;

    // Does the app wnd still exist?
    // We must check this because, when the pinned wnd closes,
    // the pin is just hidden, not destroyed.
    if (!IsWindow(pd.topMostWnd)) {
        pd.topMostWnd = pd.proxyWnd = 0;
        pd.proxyMode = false;
        DestroyWindow(wnd);
        return;
    }

    if (pd.proxyMode
        && (!pd.proxyWnd || !IsWindowVisible(pd.proxyWnd))
        && !SelectProxy(wnd, pd))
        return;

    // bugfix: disabled proxy mode check so that FixVisible()
    // is called even on normal apps that hide the window on minimize
    // (just like our About dlg)
    if (/*pd->proxyMode &&*/ !FixVisible(wnd, pd))
        return;

    if (pd.proxyMode && !IsWindowEnabled(pd.topMostWnd))
        FixPopupZOrder(pd.topMostWnd);

    FixTopStyle(wnd, pd);
    PlaceOnCaption(wnd, pd);
}


static void EvPaint(HWND wnd, PinData&)
{
    PAINTSTRUCT ps;
    if (HDC dc = BeginPaint(wnd, &ps)) {
        if (app.pinShape.getBmp()) {
            if (HDC memDC = CreateCompatibleDC(0)) {
                if (HBITMAP orgBmp = HBITMAP(SelectObject(memDC, app.pinShape.getBmp()))) {
                    BitBlt(dc, 0,0,app.pinShape.getW(),app.pinShape.getH(), memDC, 0,0, SRCCOPY);
                    SelectObject(memDC, orgBmp);
                }
                DeleteDC(memDC);
            }
        }
        EndPaint(wnd, &ps);
    }

}


static void EvLClick(HWND wnd, PinData&)
{
    DestroyWindow(wnd);
}


static bool EvPinAssignWnd(HWND wnd, PinData& pd, HWND target, int pollRate)
{
    // this shouldn't happen; it means the pin is already used
    if (pd.topMostWnd) return false;


    pd.topMostWnd = target;

    // decide proxy mode
    if (!IsWindowVisible(target) || IsWndRectEmpty(target) || IsVCLAppWnd(target)) {
        // set proxy mode flag; we'll find a proxy wnd later
        pd.proxyMode = true;
    }

    if (pd.getPinOwner()) {
        // docs say not to set GWL_HWNDPARENT, but it works nevertheless
        // (SetParent() only works with windows of the same process)
        SetLastError(0);
        if (!SetWindowLong(wnd, GWL_HWNDPARENT, LONG(pd.getPinOwner())) && GetLastError())
            Error(wnd, ResStr(IDS_ERR_SETPINPARENTFAIL));
    }

    if (!SetWindowPos(pd.topMostWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE))
        Error(wnd, ResStr(IDS_ERR_SETTOPMOSTFAIL));

    // set wnd region
    if (app.pinShape.getRgn()) {
        HRGN rgnCopy = CreateRectRgn(0,0,0,0);
        if (CombineRgn(rgnCopy, app.pinShape.getRgn(), 0, RGN_COPY) != ERROR) {
            if (!SetWindowRgn(wnd, rgnCopy, false))
                DeleteObject(rgnCopy);
        }
    }

    // set pin size, move it on the caption, and make it visible
    SetWindowPos(wnd, 0, 0,0,app.pinShape.getW(),app.pinShape.getH(), 
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); // NOTE: added SWP_NOACTIVATE
    PlaceOnCaption(wnd, pd);
    ShowWindow(wnd, SW_SHOW);

    SetForegroundWindow(pd.topMostWnd);

    // start polling timer
    SetTimer(wnd, 1, pollRate, 0);

    return true;
}


static HWND EvGetPinnedWnd(HWND wnd, PinData& pd)
{
    return pd.topMostWnd;
}


static void EvPinResetTimer(HWND wnd, PinData& pd, int pollRate)
{
    // only set it if there's a pinned window
    if (pd.topMostWnd)
        SetTimer(wnd, 1, pollRate, 0);
}


// Patch for VCL apps (owner-of-owner problem).
// If the pinned and pin wnd visiblity states are different
// change pin state to pinned wnd state.
// Return whether the pin is visible
// (if so, the caller should perform further adjustments)
static bool FixVisible(HWND wnd, const PinData& pd)
{
    // insanity check
    if (!IsWindow(pd.topMostWnd)) return false;

    HWND pinOwner = pd.getPinOwner();
    if (!pinOwner) return false;

    // IsIconic() is crucial; without it we cannot restore the minimized
    // wnd by clicking on the taskbar button
    bool ownerVisible = IsWindowVisible(pinOwner) && !IsIconic(pinOwner);
    bool pinVisible = !!IsWindowVisible(wnd);
    if (ownerVisible != pinVisible)
        ShowWindow(wnd, ownerVisible ? SW_SHOWNOACTIVATE : SW_HIDE);

    // return whether the pin is visible now
    return ownerVisible;
}


// patch for VCL apps (clearing of WS_EX_TOPMOST bit)
static void FixTopStyle(HWND /*wnd*/, const PinData& pd)
{
    if (!(ef::Win::WndH(pd.topMostWnd).getExStyle() & WS_EX_TOPMOST))
        SetWindowPos(pd.topMostWnd, HWND_TOPMOST, 0,0,0,0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}


static void PlaceOnCaption(HWND wnd, const PinData& pd)
{
    HWND pinOwner = pd.getPinOwner();
    if (!pinOwner) return;

    // Move the pin on the owner's caption
    //

    // Calc the number of caption buttons
    int btnCnt = 0;
    LONG style   = ef::Win::WndH(pinOwner).getStyle();
    LONG exStyle = ef::Win::WndH(pinOwner).getExStyle();
    LONG minMaxMask = WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    bool hasMinOrMax  = (style & minMaxMask) != 0;
    bool hasMinAndMax = (style & minMaxMask) == minMaxMask;
    bool hasClose = (style & WS_SYSMENU) != 0;
    bool hasHelp  = (exStyle & WS_EX_CONTEXTHELP) != 0;
    if (style & WS_SYSMENU) {  // other buttons appear only if this is set
        ++btnCnt;
        if (hasMinOrMax)
            btnCnt += 2;
        // Win XP/2000 erroneously allow a non-functional help button 
        // when either (not both!) of the min/max buttons are enabled
        if (hasHelp && (!hasMinOrMax || (ef::Win::OsVer().major() == 5 && !hasMinAndMax)))
            ++btnCnt;
    }

    RECT pin, pinned;
    GetWindowRect(wnd, &pin);
    GetWindowRect(pinOwner, &pinned);
    int x = pinned.right - (btnCnt*GetSystemMetrics(SM_CXSIZE) + app.pinShape.getW() + 10); // add a small padding
    SetWindowPos(wnd, 0, x, pinned.top+3, 0, 0, 
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    // reveal VCL app wnd :)
    //if (rcPinned.top >= rcPinned.bottom || rcPinned.left >= rcPinned.right)
    //  MoveWindow(pd.pinnedWnd, rcPinned.left, rcPinned.top, 200, 100, true);

}


static bool SelectProxy(HWND wnd, const PinData& pd)
{
    HWND appWnd = pd.topMostWnd;
    if (!IsWindow(appWnd)) return false;

    DWORD thread = GetWindowThreadProcessId(appWnd, 0);
    return EnumThreadWindows(thread, (WNDENUMPROC)EnumThreadWndProc, 
        LPARAM(wnd)) && pd.getPinOwner();
}


static BOOL CALLBACK EnumThreadWndProc(HWND wnd, LPARAM param)
{
    HWND pin = HWND(param);
    PinData* pd = PinData::get(pin);
    if (!pd)
        return false;

    if (GetWindow(wnd, GW_OWNER) == pd->topMostWnd) {
        if (IsWindowVisible(wnd) && !IsIconic(wnd) && !IsWndRectEmpty(wnd)) {
            pd->proxyWnd = wnd;
            SetWindowLong(pin, GWL_HWNDPARENT, LONG(wnd));
            // we must also move it in front of the new owner
            // otherwise the wnd mgr will not do it until we select the new owner
            SetWindowPos(wnd, pin, 0,0,0,0, 
                SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            return false;   // stop enumeration
        }
    }

    return true;  // continue enumeration
}


static bool IsVCLAppWnd(HWND wnd)
{
    return strimatch(L"TApplication", ef::Win::WndH(wnd).getClassName().c_str()) 
        && IsWndRectEmpty(wnd);
}
