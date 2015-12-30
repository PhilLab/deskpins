#include "stdafx.h"
#include "util.h"
#include "pinshape.h"
#include "pinwnd.h"
#include "resource.h"


static LRESULT EvCreate(HWND hWnd);
static void EvDestroy(HWND hWnd);
static void EvTimer(HWND hWnd, UINT id);
static void EvPaint(HWND hWnd);
static void EvLClick(HWND hWnd);
static bool EvPinAssignWnd(HWND hWnd, HWND hTarget, int pollRate);
static HWND EvGetPinnedWnd(HWND hWnd);
static void EvPinResetTimer(HWND hWnd, int pollRate);

struct PinData;   // forward decl

static bool SetPinData(HWND hWnd, PinData* pd);
static bool IsVCLAppWnd(HWND hWnd);
static bool SelectProxy(HWND hWnd, const PinData& pd);
static BOOL CALLBACK EnumThreadWndProc(HWND hWnd, LPARAM lParam);
static void FixTopStyle(HWND /*hWnd*/, const PinData& pd);
static void PlaceOnCaption(HWND hWnd, const PinData& pd);
static bool FixVisible(HWND hWnd, const PinData& pd);


// ugly but handy macros for event handlers

#define GETPINDATAREF_ONFAILRET(pdVar, pinWnd)                \
    LONG _pinDataRaw_ = GetWindowLong((pinWnd), 0);             \
    if (!_pinDataRaw_) return;                                  \
    PinData& pdVar = *reinterpret_cast<PinData*>(_pinDataRaw_)

#define GETPINDATAREF_ONFAILRETVAL(pdVar, pinWnd, ret)        \
    LONG _pinDataRaw_ = GetWindowLong((pinWnd), 0);             \
    if (!_pinDataRaw_) return ret;                              \
    PinData& pdVar = *reinterpret_cast<PinData*>(_pinDataRaw_)


// struct assigned to each pin wnd
//
struct PinData {
    HWND hCallbackWnd;

    bool proxyMode;
    HWND hTopMostWnd;
    HWND hProxyWnd;

    PinData(HWND hWnd)
        :
    hCallbackWnd(hWnd),
        proxyMode(false),
        hTopMostWnd(0),
        hProxyWnd(0)
    {
    }

    HWND getPinOwner() const {
        return proxyMode ? hProxyWnd : hTopMostWnd;
    }

};


// Pin wnd proc
//
LRESULT CALLBACK PinWndProc(HWND hWnd, UINT uMsg, 
                            WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CREATE:         return EvCreate(hWnd);
        case WM_DESTROY:        EvDestroy(hWnd); break;
        case WM_TIMER:          EvTimer(hWnd, wParam); break;
        case WM_PAINT:          EvPaint(hWnd); break;
        case WM_LBUTTONDOWN:    EvLClick(hWnd); break;
        case App::WM_PIN_RESETTIMER:   EvPinResetTimer(hWnd, int(wParam)); break;
        case App::WM_PIN_ASSIGNWND:    return EvPinAssignWnd(hWnd, HWND(wParam), int(lParam));
        case App::WM_PIN_GETPINNEDWND: return LRESULT(EvGetPinnedWnd(hWnd));
        default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;  
}


static LRESULT EvCreate(HWND hWnd)
{
    // create and store the pin data struct
    PinData& pd = *new PinData(app.hMainWnd);
    if (!SetPinData(hWnd, &pd)) {
        delete &pd;
        return -1;
    }

    // send 'pin created' notification
    PostMessage(pd.hCallbackWnd, App::WM_PINSTATUS, WPARAM(hWnd), true);

    // initially place pin in the middle of the screen;
    // later it will be positioned on the pinned wnd's caption
    RECT rc;
    GetWindowRect(hWnd, &rc);
    int wndW = rc.right - rc.left;
    int wndH = rc.bottom - rc.top;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hWnd, 0, (scrW-wndW)/2, (scrH-wndH)/2, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE);

    return 0;
}


static void EvDestroy(HWND hWnd)
{
    GETPINDATAREF_ONFAILRET(pd, hWnd);

    if (pd.hTopMostWnd) {
        SetWindowPos(pd.hTopMostWnd, HWND_NOTOPMOST, 0,0,0,0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); // NOTE: added noactivate
        pd.hTopMostWnd = pd.hProxyWnd = 0;
        pd.proxyMode = false;
    }

    // send 'pin destroyed' notification
    PostMessage(pd.hCallbackWnd, App::WM_PINSTATUS, WPARAM(hWnd), false);

    // remove and destroy the pin data struct
    SetPinData(hWnd, 0);
    delete &pd;
}


//static HWND static_hAppWnd = 0;
//
//static BOOL CALLBACK EnumThreadWndCollectProc(HWND hWnd, LPARAM lParam)
//{
//    vector<HWND>& wndList = *(vector<HWND>*)lParam;
//    if (hWnd == static_hAppWnd || GetWindow(hWnd, GW_OWNER) == static_hAppWnd)
//    wndList.push_back(hWnd);
//    return true;  // continue enum
//}


// helper to collect all non-child windows of a thread
class ThreadWnds {
public:
    ThreadWnds(HWND hWnd) : appWnd(hWnd) {}
    int count() const { return wndList.size(); }
    HWND operator[](int n) const { return wndList[n]; }

    bool collect()
    {
        DWORD threadId = GetWindowThreadProcessId(appWnd, 0);
        return EnumThreadWindows(threadId, (WNDENUMPROC)enumProc, LPARAM(this));
    }

protected:
    HWND appWnd;
    std::vector<HWND> wndList;

    static BOOL CALLBACK enumProc(HWND hWnd, LPARAM lParam)
    {
        ThreadWnds& obj = *reinterpret_cast<ThreadWnds*>(lParam);
        if (hWnd == obj.appWnd || GetWindow(hWnd, GW_OWNER) == obj.appWnd)
            obj.wndList.push_back(hWnd);
        return true;  // continue enum
    }

};


// - get the visible, top-level windows
// - if any enabled wnds are after any disabled
//   get the last enabled wnd; otherwise exit
// - move all disabled wnds (starting from the bottom one)
//   behind the last enabled
//
static void FixPopupZOrder(HWND hAppWnd)
{
    // - find the most visible, disabled, top-level wnd (wnd X)
    // - find all non-disabled, top-level wnds and place them
    //   above wnd X

    // get all non-child wnds
    ThreadWnds threadWnds(hAppWnd);
    if (!threadWnds.collect())
        return;

    //vector<HWND> wndList;
    //DWORD idThread = GetWindowThreadProcessId(hAppWnd, 0);
    //static_hAppWnd = hAppWnd;
    //if (!EnumThreadWindows(idThread, (WNDENUMPROC)EnumThreadWndCollectProc,
    //  LPARAM(&wndList))) return;

    //typedef vector<HWND>::const_iterator It;

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
    HWND hLastEnabled = 0;
    for (int n = threadWnds.count()-1; n >= 0; --n) {
        if (IsWindowEnabled(threadWnds[n])) {
            hLastEnabled = threadWnds[n];
            break;
        }
    }

    // none enabled? bail out
    if (!hLastEnabled)
        return;

    // move all disabled (starting from the last) behind the last enabled
    for (int n = threadWnds.count()-1; n >= 0; --n) {
        if (!IsWindowEnabled(threadWnds[n])) {
            SetWindowPos(threadWnds[n], hLastEnabled, 0,0,0,0, 
                SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
        }
    }

}


static void EvTimer(HWND hWnd, UINT id)
{
    GETPINDATAREF_ONFAILRET(pd, hWnd);

    if (id != 1) return;

    // Does the app wnd still exist?
    // We must check this because, when the pinned wnd closes,
    // the pin is just hidden, not destroyed.
    if (!IsWindow(pd.hTopMostWnd)) {
        pd.hTopMostWnd = pd.hProxyWnd = 0;
        pd.proxyMode = false;
        DestroyWindow(hWnd);
        return;
    }

    if (pd.proxyMode
        && (!pd.hProxyWnd || !IsWindowVisible(pd.hProxyWnd))
        && !SelectProxy(hWnd, pd))
        return;

    // bugfix: disabled proxy mode check so that FixVisible()
    // is called even on normal apps that hide the window on minimize
    // (just like our About dlg)
    if (/*pd->proxyMode &&*/ !FixVisible(hWnd, pd))
        return;

    if (pd.proxyMode && !IsWindowEnabled(pd.hTopMostWnd))
        FixPopupZOrder(pd.hTopMostWnd);

    FixTopStyle(hWnd, pd);
    PlaceOnCaption(hWnd, pd);
}


static void EvPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    if (HDC hDC = BeginPaint(hWnd, &ps)) {
        if (app.pinShape.getBmp()) {
            if (HDC hMemDC = CreateCompatibleDC(0)) {
                if (HBITMAP hOrgBmp = HBITMAP(SelectObject(hMemDC, app.pinShape.getBmp()))) {
                    BitBlt(hDC, 0,0,app.pinShape.getW(),app.pinShape.getH(), hMemDC, 0,0, SRCCOPY);
                    SelectObject(hMemDC, hOrgBmp);
                }
                DeleteDC(hMemDC);
            }
        }
        EndPaint(hWnd, &ps);
    }

}


static void EvLClick(HWND hWnd)
{
    DestroyWindow(hWnd);
}


static bool SetPinData(HWND hWnd, PinData* pd)
{
    SetLastError(0);
    return SetWindowLong(hWnd, 0, LONG(pd)) || !GetLastError();
}


static bool EvPinAssignWnd(HWND hWnd, HWND hTarget, int pollRate)
{
    GETPINDATAREF_ONFAILRETVAL(pd, hWnd, false);

    // this shouldn't happen; it means the pin is already used
    if (pd.hTopMostWnd) return false;


    pd.hTopMostWnd = hTarget;

    // decide proxy mode
    if (!IsWindowVisible(hTarget) || IsWndRectEmpty(hTarget) || IsVCLAppWnd(hTarget)) {
        // set proxy mode flag; we'll find a proxy wnd later
        pd.proxyMode = true;
    }

    if (pd.getPinOwner()) {
        // docs say not to set GWL_HWNDPARENT, but it works nevertheless
        // (SetParent() only works with windows of the same process)
        SetLastError(0);
        if (!SetWindowLong(hWnd, GWL_HWNDPARENT, LONG(pd.getPinOwner())) && GetLastError())
            Error(hWnd, ResStr(IDS_ERR_SETPINPARENTFAIL));
    }

    if (!SetWindowPos(pd.hTopMostWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE))
        Error(hWnd, ResStr(IDS_ERR_SETTOPMOSTFAIL));

    // set wnd region
    if (app.pinShape.getRgn()) {
        HRGN hRgnCopy = CreateRectRgn(0,0,0,0);
        if (CombineRgn(hRgnCopy, app.pinShape.getRgn(), 0, RGN_COPY) != ERROR) {
            if (!SetWindowRgn(hWnd, hRgnCopy, false))
                DeleteObject(hRgnCopy);
        }
    }

    // set pin size, move it on the caption, and make it visible
    SetWindowPos(hWnd, 0, 0,0,app.pinShape.getW(),app.pinShape.getH(), 
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); // NOTE: added SWP_NOACTIVATE
    PlaceOnCaption(hWnd, pd);
    ShowWindow(hWnd, SW_SHOW);

    SetForegroundWindow(pd.hTopMostWnd);

    // start polling timer
    SetTimer(hWnd, 1, pollRate, 0);

    return true;
}


static HWND EvGetPinnedWnd(HWND hWnd)
{
    GETPINDATAREF_ONFAILRETVAL(pd, hWnd, 0);

    return pd.hTopMostWnd;
}


static void EvPinResetTimer(HWND hWnd, int pollRate)
{
    GETPINDATAREF_ONFAILRET(pd, hWnd);

    // only set it if there's a pinned window
    if (pd.hTopMostWnd)
        SetTimer(hWnd, 1, pollRate, 0);
}


// Patch for VCL apps (owner-of-owner problem).
// If the pinned and pin wnd visiblity states are different
// change pin state to pinned wnd state.
// Return whether the pin is visible
// (if so, the caller should perform further adjustments)
static bool FixVisible(HWND hWnd, const PinData& pd)
{
    // insanity check
    if (!IsWindow(pd.hTopMostWnd)) return false;

    HWND hPinOwner = pd.getPinOwner();
    if (!hPinOwner) return false;

    // IsIconic() is crucial; without it we cannot restore the minimized
    // wnd by clicking on the taskbar button
    bool ownerVisible = IsWindowVisible(hPinOwner) && !IsIconic(hPinOwner);
    bool pinVisible = IsWindowVisible(hWnd);
    if (ownerVisible != pinVisible)
        ShowWindow(hWnd, ownerVisible ? SW_SHOWNOACTIVATE : SW_HIDE);

    // return whether the pin is visible now
    return ownerVisible;
}


// patch for VCL apps (clearing of WS_EX_TOPMOST bit)
static void FixTopStyle(HWND /*hWnd*/, const PinData& pd)
{
    if (!(ef::Win::WndH(pd.hTopMostWnd).getExStyle() & WS_EX_TOPMOST))
        SetWindowPos(pd.hTopMostWnd, HWND_TOPMOST, 0,0,0,0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}


static void PlaceOnCaption(HWND hWnd, const PinData& pd)
{
    HWND hPinOwner = pd.getPinOwner();
    if (!hPinOwner) return;

    // Move the pin on the owner's caption
    //

    // Calc the number of caption buttons
    int btnCnt = 0;
    LONG style   = ef::Win::WndH(hPinOwner).getStyle();
    LONG exStyle = ef::Win::WndH(hPinOwner).getExStyle();
    LONG minMaxMask = WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    bool hasMinOrMax  = (style & minMaxMask) != 0;
    bool hasMinAndMax = (style & minMaxMask) == minMaxMask;
    bool hasClose = style & WS_SYSMENU;
    bool hasHelp  = exStyle & WS_EX_CONTEXTHELP;
    if (style & WS_SYSMENU) {  // other buttons appear only if this is set
        ++btnCnt;
        if (hasMinOrMax)
            btnCnt += 2;
        // Win XP/2000 erroneously allow a non-functional help button 
        // when either (not both!) of the min/max buttons are enabled
        if (hasHelp && (!hasMinOrMax || (ef::Win::OsVer().major() == 5 && !hasMinAndMax)))
            ++btnCnt;
    }

    RECT rcPin, rcPinned;
    GetWindowRect(hWnd, &rcPin);
    GetWindowRect(hPinOwner, &rcPinned);
    int x = rcPinned.right - (btnCnt*GetSystemMetrics(SM_CXSIZE) + app.pinShape.getW() + 10); // add a small padding
    SetWindowPos(hWnd, 0, x, rcPinned.top+3, 0, 0, 
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    // reveal VCL app wnd :)
    //if (rcPinned.top >= rcPinned.bottom || rcPinned.left >= rcPinned.right)
    //  MoveWindow(pd.hPinnedWnd, rcPinned.left, rcPinned.top, 200, 100, true);

}


static bool SelectProxy(HWND hWnd, const PinData& pd)
{
    HWND hAppWnd = pd.hTopMostWnd;
    if (!IsWindow(hAppWnd)) return false;

    DWORD idThread = GetWindowThreadProcessId(hAppWnd, 0);
    return EnumThreadWindows(idThread, (WNDENUMPROC)EnumThreadWndProc, 
        LPARAM(hWnd)) && pd.getPinOwner();
}


static BOOL CALLBACK EnumThreadWndProc(HWND hWnd, LPARAM lParam)
{
    HWND hPin = HWND(lParam);
    GETPINDATAREF_ONFAILRETVAL(pd, hPin, false);

    if (GetWindow(hWnd, GW_OWNER) == pd.hTopMostWnd) {
        if (IsWindowVisible(hWnd) && !IsIconic(hWnd) && !IsWndRectEmpty(hWnd)) {
            pd.hProxyWnd = hWnd;
            SetWindowLong(hPin, GWL_HWNDPARENT, LONG(hWnd));
            // we must also move it in front of the new owner
            // otherwise the wnd mgr will not do it until we select the new owner
            SetWindowPos(hWnd, hPin, 0,0,0,0, 
                SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            return false;   // stop enum
        }
    }

    return true;  // continue enum
}


static bool IsVCLAppWnd(HWND hWnd)
{
    return strimatch(_T("TApplication"), ef::Win::WndH(hWnd).getClassName().c_str()) 
        && IsWndRectEmpty(hWnd);
}
