#include "stdafx.h"
#include "util.h"
#include "options.h"


const HKEY  Options::HKCU              = HKEY_CURRENT_USER;

LPCWSTR Options::REG_PATH_EF      = L"Software\\Elias Fotinis";
LPCWSTR Options::REG_APR_SUBPATH  = L"AutoPinRules";

LPCWSTR Options::REG_PINCLR       = L"PinColor";
LPCWSTR Options::REG_POLLRATE     = L"PollRate";
LPCWSTR Options::REG_TRAYDBLCLK   = L"TrayDblClk";
LPCWSTR Options::REG_AUTOPINON    = L"Enabled";
LPCWSTR Options::REG_AUTOPINDELAY = L"Delay";
LPCWSTR Options::REG_AUTOPINCOUNT = L"Count";
LPCWSTR Options::REG_AUTOPINRULE  = L"AutoPinRule%d";
LPCWSTR Options::REG_HOTKEYSON    = L"HotKeysOn";
LPCWSTR Options::REG_HOTNEWPIN    = L"HotKeyNewPin";
LPCWSTR Options::REG_HOTTOGGLEPIN = L"HotKeyTogglePin";
LPCWSTR Options::REG_LCLUI        = L"LocalizedUI";
LPCWSTR Options::REG_LCLHELP      = L"LocalizedHelp";


bool 
HotKey::load(ef::Win::RegKeyH& key, LPCWSTR val)
{
    DWORD dw;
    if (!key.getDWord(val, dw))
        return false;

    vk  = LOBYTE(dw);
    mod = HIBYTE(dw);

    if (!vk)  // oops
        mod = 0;

    return true;
}


bool 
HotKey::save(ef::Win::RegKeyH& key, LPCWSTR val) const
{
    return key.setDWord(val, MAKEWORD(vk, mod));
}


// ----------------------------------------------------------------------


bool 
AutoPinRule::match(HWND wnd) const
{
    return enabled 
        && ef::wildimatch(ttl, ef::Win::WndH(wnd).getText()) 
        && ef::wildimatch(cls, ef::Win::WndH(wnd).getClassName());
}


bool 
AutoPinRule::load(ef::Win::RegKeyH& key, int i)
{
    // convert index to str and make room for one more char (the flag)
    WCHAR val[20];
    if (_itow_s(i, val, 10) != 0)
        return false;
    WCHAR* flag = val + wcslen(val);
    *(flag+1) = L'\0';

    std::wstring tmp;
    DWORD dw;

    *flag = L'D';
    if (!key.getString(val, tmp)) return false;
    descr = tmp;

    *flag = L'T';
    if (!key.getString(val, tmp)) return false;
    ttl = tmp;

    *flag = L'C';
    if (!key.getString(val, tmp)) return false;
    cls = tmp;

    *flag = L'E';
    if (!key.getDWord(val, dw)) return false;
    enabled = dw != 0;

    return true;
}


bool 
AutoPinRule::save(ef::Win::RegKeyH& key, int i) const
{
    // convert index to str and make room for one more char (the flag)
    WCHAR val[20];
    if (_itow_s(i, val, 10) != 0)
        return false;
    WCHAR* flag = val + wcslen(val);
    *(flag+1) = L'\0';

    return (*flag = L'D', key.setString(val, descr))
        && (*flag = L'T', key.setString(val, ttl))
        && (*flag = L'C', key.setString(val, cls))
        && (*flag = L'E', key.setDWord (val, enabled));
}


// ----------------------------------------------------------------------


Options::Options() : 
    pinClr(RGB(255,0,0)),
    trackRate(100,10,1000,10),
    dblClkTray(false),
    hotkeysOn(true),
    hotEnterPin(App::HOTID_ENTERPINMODE, VK_F11, MOD_CONTROL),
    hotTogglePin(App::HOTID_TOGGLEPIN, VK_F12, MOD_CONTROL),
    autoPinOn(false),
    autoPinDelay(200,100,10000,50),
    helpFile(L"DeskPins.chm")    // init, in case key doesn't exist
{
    // set higher tracking rate for Win2K+ (higher WM_TIMER resolution)

    if (ef::Win::OsVer().major() >= HIWORD(ef::Win::OsVer::win2000))
        trackRate.value = 20;
}


Options::~Options()
{
}


bool 
Options::save() const
{
    std::wstring appKeyPath = std::wstring(REG_PATH_EF) + L'\\' + App::APPNAME;
    ef::Win::AutoRegKeyH key = ef::Win::RegKeyH::create(HKCU, appKeyPath);
    if (!key) return false;

    key.setDWord(REG_PINCLR, pinClr < 7 ? 0 : pinClr);   // HACK: see load()
    key.setDWord(REG_POLLRATE, trackRate.value);
    key.setDWord(REG_TRAYDBLCLK, dblClkTray);

    key.setDWord(REG_HOTKEYSON, hotkeysOn);
    hotEnterPin.save(key, REG_HOTNEWPIN);
    hotTogglePin.save(key, REG_HOTTOGGLEPIN);

    ef::Win::AutoRegKeyH apKey = ef::Win::RegKeyH::create(key, REG_APR_SUBPATH);
    if (apKey) {
        apKey.setDWord(REG_AUTOPINON, autoPinOn);
        apKey.setDWord(REG_AUTOPINDELAY, autoPinDelay.value);
        apKey.setDWord(REG_AUTOPINCOUNT, autoPinRules.size());
        for (int n = 0; n < int(autoPinRules.size()); ++n)
            autoPinRules[n].save(apKey, n);
    }

    key.setString(REG_LCLUI, uiFile);
    key.setString(REG_LCLHELP, helpFile);

    return true;
}


bool
Options::load()
{
    std::wstring appKeyPath = std::wstring(REG_PATH_EF) + L'\\' + App::APPNAME;
    ef::Win::AutoRegKeyH key = ef::Win::RegKeyH::open(HKCU, appKeyPath);
    if (!key) return false;

    DWORD dw;
    std::wstring buf;

    if (key.getDWord(REG_PINCLR, dw)) {
        pinClr = COLORREF(dw & 0xFFFFFF);
        // HACK: Prior to version 1.3 the color was an index (0..6), 
        //       but from v1.3+ a full 24-bit clr value is used.
        //       To avoid interpreting the old setting as an RGB
        //       (and thus producing a nasty black clr)
        //       we have to translate the older index values into real RGB.
        //       Also, in save() we replace COLORREFs < 0x000007 with 0 (black)
        //       to get this straight (and hope no-one notices :D ).
        // FIXME: no longer needed
        if (pinClr > 0 && pinClr < 7) {
            using namespace StdClr;
            const COLORREF clr[6] = {red, yellow, lime, cyan, blue, magenta};
            pinClr = clr[pinClr-1];
        }
    }
    if (key.getDWord(REG_POLLRATE, dw) && trackRate.inRange(dw))
        trackRate = dw;
    if (key.getDWord(REG_TRAYDBLCLK, dw))
        dblClkTray = dw != 0;

    if (key.getDWord(REG_HOTKEYSON, dw))
        hotkeysOn = dw != 0;
    hotEnterPin.load(key, REG_HOTNEWPIN);
    hotTogglePin.load(key, REG_HOTTOGGLEPIN);

    ef::Win::AutoRegKeyH apKey = ef::Win::RegKeyH::open(key, REG_APR_SUBPATH);
    if (apKey) {
        if (apKey.getDWord(REG_AUTOPINON, dw))
            autoPinOn = dw != 0;
        if (apKey.getDWord(REG_AUTOPINDELAY, dw) && autoPinDelay.inRange(dw))
            autoPinDelay = dw;
        if (apKey.getDWord(REG_AUTOPINCOUNT, dw)) {
            for (int n = 0; n < int(dw); ++n) {
                AutoPinRule rule;
                if (rule.load(apKey, n))
                    autoPinRules.push_back(rule);
            }
        }
    }

    if (key.getString(REG_LCLUI, buf))
        uiFile = buf;
    if (key.getString(REG_LCLHELP, buf))
        helpFile = buf;

    if (helpFile.empty())
        helpFile = L"DeskPins.chm";

    return true;
}
