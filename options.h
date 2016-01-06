#pragma once

#include "util.h"
#include "resource.h"



// hotkey type
//
struct HotKey {
    int  id;
    UINT vk;
    UINT mod;

    explicit HotKey(int id_, UINT vk_ = 0, UINT mod_ = 0)
        : id(id_), vk(vk_), mod(mod_) {}

    bool operator==(const HotKey& other) const {
        return vk == other.vk && mod == other.mod;
    }
    bool operator!=(const HotKey& other) const {
        return !(*this == other);
    }

    bool set(HWND wnd) const
    {
        return !!RegisterHotKey(wnd, id, mod, vk);
    }

    bool unset(HWND wnd) const
    {
        return !!UnregisterHotKey(wnd, id);
    }

    bool load(ef::Win::RegKeyH& key, LPCWSTR val);
    bool save(ef::Win::RegKeyH& key, LPCWSTR val) const;

    void getUI(HWND wnd, int id)
    {
        LRESULT res = SendDlgItemMessage(wnd, id, HKM_GETHOTKEY, 0, 0);
        vk  = LOBYTE(res);
        mod = HIBYTE(res);
        if (!vk)
            mod = 0;
    }

    void setUI(HWND wnd, int id) const
    {
        SendDlgItemMessage(wnd, id, HKM_SETHOTKEY, MAKEWORD(vk, mod), 0);
    }

};


struct AutoPinRule {
    std::wstring descr;
    std::wstring ttl;
    std::wstring cls;
    bool enabled;

    AutoPinRule(const std::wstring& d = std::wstring(ResStr(IDS_NEWRULEDESCR)), 
        const std::wstring& t = L"", 
        const std::wstring& c = L"", 
        bool b = true) : descr(d), ttl(t), cls(c), enabled(b) {}

    bool match(HWND wnd) const;

    bool load(ef::Win::RegKeyH& key, int i);
    bool save(ef::Win::RegKeyH& key, int i) const;
    static void remove(ef::Win::RegKeyH& key, int i);
};


template <typename T>
struct ScalarOption {
    T value, minV, maxV, step;
    ScalarOption(T value_, T min_, T max_, T step_ = T(1)) : 
    value(value_), minV(min_), maxV(max_), step(step_)
    {
        if (maxV < minV)
            maxV = minV;
        capValue();
    }

    void capValue()
    {
        if (value < minV)
            value = minV;
        else if (value > maxV)
            value = maxV;
    }

    bool inRange(T t) const
    {
        return minV <= t && t <= maxV;
    }

    ScalarOption& operator=(T t)
    {
        value = t;
        capValue();
        return *this;
    }

    bool operator!=(const ScalarOption& other) const
    {
        return value != other.value;
    }

    // get value from ctrl (use min val on error)
    // making sure it's in range
    T getUI(HWND wnd, int id)
    {
        if (!std::numeric_limits<T>::is_integer) {
            // only integers are supported for now
            return minV;
        }

        BOOL xlated;
        T t = static_cast<T>(GetDlgItemInt(wnd, id, &xlated, 
            std::numeric_limits<T>::is_signed));
        if (!xlated || t < minV)
            t = minV;
        else if (t > maxV)
            t = maxV;

        return t;
    }

    // show warning and focus ctrl if out of range
    bool validateUI(HWND wnd, int id)
    {
        if (!std::numeric_limits<T>::is_integer) {
            // only integers are supported for now
            return false;
        }

        BOOL xlated;
        T t = static_cast<T>(GetDlgItemInt(wnd, id, &xlated, 
            std::numeric_limits<T>::is_signed));
        if (xlated && inRange(t))
            return true;

        // report error
        HWND prevSib = GetWindow(GetDlgItem(wnd, id), GW_HWNDPREV);
        std::wstring label = RemAccel(ef::Win::WndH(prevSib).getText());
        ResStr str(IDS_WRN_UIRANGE, 256, DWORD_PTR(label.c_str()), DWORD(minV), DWORD(maxV));
        Warning(wnd, str);
        SetFocus(GetDlgItem(wnd, id));
        return false;
    }

};

typedef ScalarOption<int>        IntOption;
typedef std::vector<AutoPinRule> AutoPinRules;

class Options {
public:
    // pins
    COLORREF      pinClr;
    IntOption     trackRate;
    bool          dblClkTray;
    bool          runOnStartup;
    // hotkeys
    bool          hotkeysOn;
    HotKey        hotEnterPin, hotTogglePin;
    // autopin
    bool          autoPinOn;
    AutoPinRules  autoPinRules;
    IntOption     autoPinDelay;
    // lang
    std::wstring  uiFile;     // empty means built-in exe resources
    std::wstring  helpFile;   // empty defaults to 'deskpins.chm'

    Options();
    ~Options();

    bool save() const;
    bool load();

protected:
    // constants
    static const HKEY   HKCU;

    static LPCWSTR REG_PATH_EF;
    static LPCWSTR REG_APR_SUBPATH;
    static LPCWSTR REG_PATH_RUN;

    static LPCWSTR REG_PINCLR;
    static LPCWSTR REG_POLLRATE;
    static LPCWSTR REG_TRAYDBLCLK;
    static LPCWSTR REG_AUTOPINON;
    static LPCWSTR REG_AUTOPINDELAY;
    static LPCWSTR REG_AUTOPINCOUNT;
    static LPCWSTR REG_AUTOPINRULE;
    static LPCWSTR REG_HOTKEYSON;
    static LPCWSTR REG_HOTNEWPIN;
    static LPCWSTR REG_HOTTOGGLEPIN;
    static LPCWSTR REG_LCLUI;
    static LPCWSTR REG_LCLHELP;

    // utilities
    bool REGOK(DWORD err) { return err == ERROR_SUCCESS; }
};


class WindowCreationMonitor;


struct OptionsPropSheetData {
    Options& opt;
    WindowCreationMonitor& winCreMon;
};
