#include "stdafx.h"
#include "options.h"
#include "util.h"
#include "optpins.h"
#include "resource.h"


static HBRUSH GetPinClrBrush(HWND hWnd)
{
    HWND hCtrl = GetDlgItem(hWnd, IDC_PIN_COLOR_BOX);
    return HBRUSH(GetWindowLong(hCtrl, GWL_USERDATA));
}


static COLORREF GetPinClr(HWND hWnd)
{
    HBRUSH hBrush = GetPinClrBrush(hWnd);
    LOGBRUSH lb;
    return GetObject(hBrush, sizeof(LOGBRUSH), &lb) ? lb.lbColor : 0;
}


static void SetPinClrBrush(HWND hWnd, COLORREF clr)
{
    DeleteObject(GetPinClrBrush(hWnd));

    HWND hCtrl = GetDlgItem(hWnd, IDC_PIN_COLOR_BOX);
    SetWindowLong(hCtrl, GWL_USERDATA, LONG(CreateSolidBrush(clr)));
    InvalidateRect(hCtrl, 0, true);
}


static bool EvInitDialog(HWND hWnd, HWND /*hFocus*/, LPARAM lParam)
{
    // must have a valid data ptr
    if (!lParam) {
        EndDialog(hWnd, IDCANCEL);
        return false;
    }

    // save the data ptr
    PROPSHEETPAGE& psp = *reinterpret_cast<PROPSHEETPAGE*>(lParam);
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(psp.lParam)->opt;
    SetWindowLong(hWnd, DWL_USER, psp.lParam);


    SetPinClrBrush(hWnd, opt.pinClr);

    SendDlgItemMessage(hWnd, IDC_POLL_RATE_UD, UDM_SETRANGE, 0, 
        MAKELONG(opt.trackRate.maxV,opt.trackRate.minV));
    SendDlgItemMessage(hWnd, IDC_POLL_RATE_UD, UDM_SETPOS, 0, 
        MAKELONG(opt.trackRate.value,0));

    CheckDlgButton(hWnd, 
        opt.dblClkTray ? IDC_TRAY_DOUBLE_CLICK : IDC_TRAY_SINGLE_CLICK,
        BST_CHECKED);

    return false;
}


static void EvTermDialog(HWND hWnd)
{
    DeleteObject(GetPinClrBrush(hWnd));
}


/*
static bool EvDrawItem(HWND hWnd, UINT id, DRAWITEMSTRUCT* dis)
{
    if (id != IDC_PIN_COLOR)
        return false;

    RECT rc = dis->rcItem;
    HDC hDC = dis->hDC;
    bool pressed = dis->itemState & ODS_SELECTED;

    DrawFrameControl(hDC, &rc, DFC_BUTTON, 
        DFCS_BUTTONPUSH | DFCS_ADJUSTRECT | (pressed ? DFCS_PUSHED : 0));

    InflateRect(&rc, -1, -1);
    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(hDC, &rc);
    InflateRect(&rc, -1, -1);

    if (pressed)
        OffsetRect(&rc, 1, 1);

    int h = rc.bottom - rc.left;
    RECT rc2;
    CopyRect(&rc2, &rc);
    rc2.right = rc2.left + 2*h;
    InflateRect(&rc2, -1, -1);
    DrawEdge(hDC, &rc2, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
    COLORREF clr = (COLORREF)GetWindowLong(GetDlgItem(hWnd, IDC_PIN_COLOR), GWL_USERDATA);
    HBRUSH hBrush = CreateSolidBrush(clr);
    FillRect(hDC, &rc2, hBrush);
    DeleteObject(hBrush);

    rc.left = rc2.right + 4;
    char buf[40];
    GetDlgItemText(hWnd, id, buf, sizeof(buf));
    DrawText(hDC, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetWindowLong(hWnd, DWL_MSGRESULT, true);
    return true;
}*/


static bool Validate(HWND hWnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;
    return opt.trackRate.validateUI(hWnd, IDC_POLL_RATE);
}


static void CmPinClr(HWND hWnd)
{
    static COLORREF userClrs[16] = {0};

    CHOOSECOLOR cc;
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = hWnd;
    cc.rgbResult    = GetPinClr(hWnd);
    cc.lpCustColors = userClrs;
    cc.Flags        = CC_RGBINIT | CC_SOLIDCOLOR;   //CC_ANYCOLOR
    if (ChooseColor(&cc)) {
        SetPinClrBrush(hWnd, cc.rgbResult);
        PSChanged(hWnd);
    }
}


static BOOL CALLBACK EnumWndProcPinsUpdate(HWND hWnd, LPARAM)
{
    if (strimatch(App::WNDCLS_PIN, ef::Win::WndH(hWnd).getClassName().c_str()))
        InvalidateRect(hWnd, 0, false);
    return true;    // continue enumeration
}


static void UpdatePinWnds()
{
    EnumWindows((WNDENUMPROC)EnumWndProcPinsUpdate, 0);
}


static BOOL CALLBACK ResetPinTimersEnumProc(HWND hWnd, LPARAM lParam)
{
    if (strimatch(App::WNDCLS_PIN, ef::Win::WndH(hWnd).getClassName().c_str()))
        SendMessage(hWnd, App::WM_PIN_RESETTIMER, lParam, 0);
    return true;    // continue enumeration
}


static void Apply(HWND hWnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;

    COLORREF clr = GetPinClr(hWnd);
    if (opt.pinClr != clr) {
        app.createSmClrIcon(opt.pinClr = clr);
        app.trayIcon.setIcon(app.hSmClrIcon);
        if (app.pinShape.initImage(clr)) UpdatePinWnds();
    }

    int rate = opt.trackRate.getUI(hWnd, IDC_POLL_RATE);
    if (opt.trackRate.value != rate)
        EnumWindows(ResetPinTimersEnumProc, opt.trackRate.value = rate);

    opt.dblClkTray = IsDlgButtonChecked(hWnd, IDC_TRAY_DOUBLE_CLICK) == BST_CHECKED;
}


BOOL CALLBACK OptPinsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    switch (uMsg) {
        case WM_INITDIALOG:  return EvInitDialog(hWnd, HWND(wParam), lParam);
        case WM_DESTROY:     EvTermDialog(hWnd); return true;
        case WM_NOTIFY: {
            NMHDR nmhdr = *reinterpret_cast<NMHDR*>(lParam);
            switch (nmhdr.code) {
                case PSN_SETACTIVE: {
                    HWND hTab = PropSheet_GetTabControl(nmhdr.hwndFrom);
                    app.optionPage = SendMessage(hTab, TCM_GETCURSEL, 0, 0);
                    SetWindowLong(hWnd, DWL_MSGRESULT, 0);
                    return true;
                }
                case PSN_KILLACTIVE: {
                    SetWindowLong(hWnd, DWL_MSGRESULT, !Validate(hWnd));
                    return true;
                }
                case PSN_APPLY:
                    Apply(hWnd);
                    return true;
                case PSN_HELP: {
                    app.help.show(hWnd, _T("::\\optpins.htm"));
                    return true;
                }
                case UDN_DELTAPOS: {
                    if (wParam == IDC_POLL_RATE_UD) {
                        NM_UPDOWN& nmud = *(NM_UPDOWN*)lParam;
                        Options& opt = 
                            reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;
                        nmud.iDelta *= opt.trackRate.step;
                        SetWindowLong(hWnd, DWL_MSGRESULT, FALSE);   // allow change
                        return true;
                    }
                    else
                        return false;
                }
                default:
                    return false;
            }
        }
        case WM_HELP: {
            app.help.show(hWnd, _T("::\\optpins.htm"));
            return true;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam), code = HIWORD(wParam);
            switch (id) {
                case IDC_PIN_COLOR:         CmPinClr(hWnd); return true;
                case IDC_PIN_COLOR_BOX:     if (code == STN_DBLCLK) CmPinClr(hWnd); return true;
                case IDC_TRAY_SINGLE_CLICK:
                case IDC_TRAY_DOUBLE_CLICK: PSChanged(hWnd); return true;
                case IDC_POLL_RATE:         if (code == EN_CHANGE) PSChanged(hWnd); return true;
                default:                    return false;
            }
        }
        //case WM_DRAWITEM:
        //    return EvDrawItem(hWnd, wParam, (DRAWITEMSTRUCT*)lParam);
        case WM_CTLCOLORSTATIC:
            if (HWND(lParam) == GetDlgItem(hWnd, IDC_PIN_COLOR_BOX))
                return BOOL(GetPinClrBrush(hWnd));
            return 0;
        default:
            return false;
    }

}
