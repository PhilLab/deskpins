#include "stdafx.h"
#include "options.h"
#include "util.h"
#include "opthotkeys.h"
#include "resource.h"


static bool CmHotkeysOn(HWND hWnd)
{
    bool b = IsDlgButtonChecked(hWnd, IDC_HOTKEYS_ON) == BST_CHECKED;
    EnableGroup(hWnd, IDC_HOTKEYS_GROUP, b);
    PSChanged(hWnd);
    return true;
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


    CheckDlgButton(hWnd, IDC_HOTKEYS_ON, opt.hotkeysOn);
    CmHotkeysOn(hWnd);

    opt.hotEnterPin.setUI(hWnd, IDC_HOT_PINMODE);
    opt.hotTogglePin.setUI(hWnd, IDC_HOT_TOGGLEPIN);

    return false;
}


static bool Validate(HWND hWnd)
{
    return true;
}


static bool ChangeHotkey(HWND hWnd, 
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
        return newHotkey.unset(app.hMainWnd);

    // turning on OR key change
    if ((!wasOn && isOn) || (newHotkey != oldHotkey))
        return newHotkey.set(app.hMainWnd);

    // same key is still on
    return true;
}


static void Apply(HWND hWnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;

    bool hotkeysOn = IsDlgButtonChecked(hWnd, IDC_HOTKEYS_ON) == BST_CHECKED;

    HotKey hkEnter(App::HOTID_ENTERPINMODE);
    HotKey hkToggle(App::HOTID_TOGGLEPIN);

    hkEnter.getUI(hWnd, IDC_HOT_PINMODE);
    hkToggle.getUI(hWnd, IDC_HOT_TOGGLEPIN);

    // Set hotkeys and report error if any failed.
    // Get separate success flags to avoid short-circuiting
    // (and set as many keys as possible)
    bool allKeysSet = true;
    allKeysSet &= ChangeHotkey(
        hWnd, hkEnter, hotkeysOn, opt.hotEnterPin, opt.hotkeysOn);
    allKeysSet &= ChangeHotkey(
        hWnd, hkToggle, hotkeysOn, opt.hotTogglePin, opt.hotkeysOn);

    opt.hotkeysOn = hotkeysOn;
    opt.hotEnterPin = hkEnter;
    opt.hotTogglePin = hkToggle;

    if (!allKeysSet)
        Error(hWnd, ResStr(IDS_ERR_HOTKEYSSET));
}


BOOL CALLBACK OptHotkeysProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG:  return EvInitDialog(hWnd, HWND(wParam), lParam);
            //case WM_DESTROY:     return EvTermDialog(hWnd, lParam);
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
                case PSN_APPLY: {
                    Apply(hWnd);
                    return true;
                }
                case PSN_HELP: {
                    app.help.show(hWnd, _T("::\\opthotkeys.htm"));
                    return true;
                }
                default:
                    return false;
            }
        }
        case WM_HELP: {
            app.help.show(hWnd, _T("::\\opthotkeys.htm"));
            return true;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam), code = HIWORD(wParam);
            switch (id) {
                case IDC_HOTKEYS_ON:    CmHotkeysOn(hWnd); return true;
                case IDC_HOT_PINMODE:
                case IDC_HOT_TOGGLEPIN: if (code == EN_CHANGE) PSChanged(hWnd); return true;
                default:                return false;
            }
        }
        default:
            return false;
    }
}
