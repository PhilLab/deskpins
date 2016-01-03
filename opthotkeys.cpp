#include "stdafx.h"
#include "options.h"
#include "util.h"
#include "opthotkeys.h"
#include "resource.h"


static bool CmHotkeysOn(HWND wnd)
{
    bool b = IsDlgButtonChecked(wnd, IDC_HOTKEYS_ON) == BST_CHECKED;
    EnableGroup(wnd, IDC_HOTKEYS_GROUP, b);
    PSChanged(wnd);
    return true;
}


static bool EvInitDialog(HWND wnd, HWND /*focus*/, LPARAM param)
{
    // must have a valid data ptr
    if (!param) {
        EndDialog(wnd, IDCANCEL);
        return false;
    }

    // save the data ptr
    PROPSHEETPAGE& psp = *reinterpret_cast<PROPSHEETPAGE*>(param);
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(psp.lParam)->opt;
    SetWindowLong(wnd, DWL_USER, psp.lParam);


    CheckDlgButton(wnd, IDC_HOTKEYS_ON, opt.hotkeysOn);
    CmHotkeysOn(wnd);

    opt.hotEnterPin.setUI(wnd, IDC_HOT_PINMODE);
    opt.hotTogglePin.setUI(wnd, IDC_HOT_TOGGLEPIN);

    return false;
}


static bool Validate(HWND wnd)
{
    return true;
}


static bool ChangeHotkey(HWND wnd, 
                         const HotKey& newHotkey, bool newState, 
                         const HotKey& oldHotkey, bool oldState)
{
    // get old & new state to figure out transition
    bool wasOn = oldState && oldHotkey.vk;
    bool isOn  = newState && newHotkey.vk;

    // bail out if it's still off
    if (!wasOn && !isOn)
        return true;

    // turning off
    if (wasOn && !isOn)
        return newHotkey.unset(app.mainWnd);

    // turning on OR key change
    if ((!wasOn && isOn) || (newHotkey != oldHotkey))
        return newHotkey.set(app.mainWnd);

    // same key is still on
    return true;
}


static void Apply(HWND wnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;

    bool hotkeysOn = IsDlgButtonChecked(wnd, IDC_HOTKEYS_ON) == BST_CHECKED;

    HotKey enterKey(App::HOTID_ENTERPINMODE);
    HotKey toggleKey(App::HOTID_TOGGLEPIN);

    enterKey.getUI(wnd, IDC_HOT_PINMODE);
    toggleKey.getUI(wnd, IDC_HOT_TOGGLEPIN);

    // Set hotkeys and report error if any failed.
    // Get separate success flags to avoid short-circuiting
    // (and set as many keys as possible)
    bool allKeysSet = true;
    allKeysSet &= ChangeHotkey(
        wnd, enterKey, hotkeysOn, opt.hotEnterPin, opt.hotkeysOn);
    allKeysSet &= ChangeHotkey(
        wnd, toggleKey, hotkeysOn, opt.hotTogglePin, opt.hotkeysOn);

    opt.hotkeysOn = hotkeysOn;
    opt.hotEnterPin = enterKey;
    opt.hotTogglePin = toggleKey;

    if (!allKeysSet)
        Error(wnd, ResStr(IDS_ERR_HOTKEYSSET));
}


BOOL CALLBACK OptHotkeysProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_INITDIALOG:  return EvInitDialog(wnd, HWND(wparam), lparam);
            //case WM_DESTROY:     return EvTermDialog(wnd, lparam);
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
                case PSN_APPLY: {
                    Apply(wnd);
                    return true;
                }
                case PSN_HELP: {
                    app.help.show(wnd, _T("::\\opthotkeys.htm"));
                    return true;
                }
                default:
                    return false;
            }
        }
        case WM_HELP: {
            app.help.show(wnd, _T("::\\opthotkeys.htm"));
            return true;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wparam), code = HIWORD(wparam);
            switch (id) {
                case IDC_HOTKEYS_ON:    CmHotkeysOn(wnd); return true;
                case IDC_HOT_PINMODE:
                case IDC_HOT_TOGGLEPIN: if (code == EN_CHANGE) PSChanged(wnd); return true;
                default:                return false;
            }
        }
        default:
            return false;
    }
}
