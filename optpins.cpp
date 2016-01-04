#include "stdafx.h"
#include "options.h"
#include "util.h"
#include "optpins.h"
#include "resource.h"


#define OWNERDRAW_CLR_BUTTON  0


static HBRUSH GetPinClrBrush(HWND wnd)
{
    HWND ctrl = GetDlgItem(wnd, IDC_PIN_COLOR_BOX);
    return HBRUSH(GetWindowLong(ctrl, GWL_USERDATA));
}


static COLORREF GetPinClr(HWND wnd)
{
    HBRUSH brush = GetPinClrBrush(wnd);
    LOGBRUSH lb;
    return GetObject(brush, sizeof(LOGBRUSH), &lb) ? lb.lbColor : 0;
}


static void SetPinClrBrush(HWND wnd, COLORREF clr)
{
    DeleteObject(GetPinClrBrush(wnd));

    HWND ctrl = GetDlgItem(wnd, IDC_PIN_COLOR_BOX);
    SetWindowLong(ctrl, GWL_USERDATA, LONG(CreateSolidBrush(clr)));
    InvalidateRect(ctrl, 0, true);
}


static bool EvInitDialog(HWND wnd, HWND focus, LPARAM lparam)
{
    // must have a valid data ptr
    if (!lparam) {
        EndDialog(wnd, IDCANCEL);
        return false;
    }

    // save the data ptr
    PROPSHEETPAGE& psp = *reinterpret_cast<PROPSHEETPAGE*>(lparam);
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(psp.lParam)->opt;
    SetWindowLong(wnd, DWL_USER, psp.lParam);

#if OWNERDRAW_CLR_BUTTON
    LONG style = GetWindowLong(GetDlgItem(wnd, IDC_PIN_COLOR), GWL_STYLE);
    style |= BS_OWNERDRAW;
    SetWindowLong(GetDlgItem(wnd, IDC_PIN_COLOR), GWL_STYLE, style);
#endif

    SetPinClrBrush(wnd, opt.pinClr);

    SendDlgItemMessage(wnd, IDC_POLL_RATE_UD, UDM_SETRANGE, 0, 
        MAKELONG(opt.trackRate.maxV,opt.trackRate.minV));
    SendDlgItemMessage(wnd, IDC_POLL_RATE_UD, UDM_SETPOS, 0, 
        MAKELONG(opt.trackRate.value,0));

    CheckDlgButton(wnd, 
        opt.dblClkTray ? IDC_TRAY_DOUBLE_CLICK : IDC_TRAY_SINGLE_CLICK,
        BST_CHECKED);

    return false;
}


static void EvTermDialog(HWND wnd)
{
    DeleteObject(GetPinClrBrush(wnd));
}


#if OWNERDRAW_CLR_BUTTON

// TODO: add vistual styles support
static bool EvDrawItem(HWND wnd, UINT id, DRAWITEMSTRUCT* dis)
{
    if (id != IDC_PIN_COLOR)
        return false;

    RECT rc = dis->rcItem;
    HDC dc = dis->hDC;
    bool pressed = dis->itemState & ODS_SELECTED;

    DrawFrameControl(dc, &rc, DFC_BUTTON, 
        DFCS_BUTTONPUSH | DFCS_ADJUSTRECT | (pressed ? DFCS_PUSHED : 0));

    InflateRect(&rc, -1, -1);
    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dc, &rc);
    InflateRect(&rc, -1, -1);

    if (pressed)
        OffsetRect(&rc, 1, 1);

    int h = rc.bottom - rc.left;
    RECT rc2;
    CopyRect(&rc2, &rc);
    rc2.right = rc2.left + 2*h;
    InflateRect(&rc2, -1, -1);
    DrawEdge(dc, &rc2, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
    COLORREF clr = (COLORREF)GetWindowLong(GetDlgItem(wnd, IDC_PIN_COLOR), GWL_USERDATA);
    HBRUSH brush = CreateSolidBrush(clr);
    FillRect(dc, &rc2, brush);
    DeleteObject(brush);

    rc.left = rc2.right + 4;
    WCHAR buf[40];
    GetDlgItemText(wnd, id, buf, sizeof(buf));
    DrawText(dc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetWindowLong(wnd, DWL_MSGRESULT, true);
    return true;
}

#endif


static bool Validate(HWND wnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;
    return opt.trackRate.validateUI(wnd, IDC_POLL_RATE);
}


static void CmPinClr(HWND wnd)
{
    static COLORREF userClrs[16] = {0};

    CHOOSECOLOR cc;
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = wnd;
    cc.rgbResult    = GetPinClr(wnd);
    cc.lpCustColors = userClrs;
    cc.Flags        = CC_RGBINIT | CC_SOLIDCOLOR;   //CC_ANYCOLOR
    if (ChooseColor(&cc)) {
        SetPinClrBrush(wnd, cc.rgbResult);
        PSChanged(wnd);
    }
}


static BOOL CALLBACK EnumWndProcPinsUpdate(HWND wnd, LPARAM)
{
    if (strimatch(App::WNDCLS_PIN, ef::Win::WndH(wnd).getClassName().c_str()))
        InvalidateRect(wnd, 0, false);
    return true;    // continue
}


static void UpdatePinWnds()
{
    EnumWindows((WNDENUMPROC)EnumWndProcPinsUpdate, 0);
}


static BOOL CALLBACK ResetPinTimersEnumProc(HWND wnd, LPARAM param)
{
    if (strimatch(App::WNDCLS_PIN, ef::Win::WndH(wnd).getClassName().c_str()))
        SendMessage(wnd, App::WM_PIN_RESETTIMER, param, 0);
    return true;    // continue
}


static void Apply(HWND wnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;

    COLORREF clr = GetPinClr(wnd);
    if (opt.pinClr != clr) {
        app.createSmClrIcon(opt.pinClr = clr);
        app.trayIcon.setIcon(app.smClrIcon);
        if (app.pinShape.initImage(clr)) UpdatePinWnds();
    }

    int rate = opt.trackRate.getUI(wnd, IDC_POLL_RATE);
    if (opt.trackRate.value != rate)
        EnumWindows(ResetPinTimersEnumProc, opt.trackRate.value = rate);

    opt.dblClkTray = IsDlgButtonChecked(wnd, IDC_TRAY_DOUBLE_CLICK) == BST_CHECKED;
}


BOOL CALLBACK OptPinsProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

    switch (msg) {
        case WM_INITDIALOG:  return EvInitDialog(wnd, HWND(wparam), lparam);
        case WM_DESTROY:     EvTermDialog(wnd); return true;
        case WM_NOTIFY: {
            NMHDR nmhdr = *reinterpret_cast<NMHDR*>(lparam);
            switch (nmhdr.code) {
                case PSN_SETACTIVE: {
                    HWND tab = PropSheet_GetTabControl(nmhdr.hwndFrom);
                    app.optionPage = SendMessage(tab, TCM_GETCURSEL, 0, 0);
                    SetWindowLong(wnd, DWL_MSGRESULT, 0);
                    return true;
                }
                case PSN_KILLACTIVE: {
                    SetWindowLong(wnd, DWL_MSGRESULT, !Validate(wnd));
                    return true;
                }
                case PSN_APPLY:
                    Apply(wnd);
                    return true;
                case PSN_HELP: {
                    app.help.show(wnd, L"::\\optpins.htm");
                    return true;
                }
                case UDN_DELTAPOS: {
                    if (wparam == IDC_POLL_RATE_UD) {
                        NM_UPDOWN& nmud = *(NM_UPDOWN*)lparam;
                        Options& opt = 
                            reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;
                        nmud.iDelta *= opt.trackRate.step;
                        SetWindowLong(wnd, DWL_MSGRESULT, FALSE);   // allow change
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
            app.help.show(wnd, L"::\\optpins.htm");
            return true;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wparam), code = HIWORD(wparam);
            switch (id) {
                case IDC_PIN_COLOR:         CmPinClr(wnd); return true;
                case IDC_PIN_COLOR_BOX:     if (code == STN_DBLCLK) CmPinClr(wnd); return true;
                case IDC_TRAY_SINGLE_CLICK:
                case IDC_TRAY_DOUBLE_CLICK: PSChanged(wnd); return true;
                case IDC_POLL_RATE:         if (code == EN_CHANGE) PSChanged(wnd); return true;
                default:                    return false;
            }
        }
#if OWNERDRAW_CLR_BUTTON
        case WM_DRAWITEM:
            return EvDrawItem(wnd, wparam, (DRAWITEMSTRUCT*)lparam);
#endif
        case WM_CTLCOLORSTATIC:
            if (HWND(lparam) == GetDlgItem(wnd, IDC_PIN_COLOR_BOX))
                return BOOL(GetPinClrBrush(wnd));
            return 0;
        default:
            return false;
    }

}
