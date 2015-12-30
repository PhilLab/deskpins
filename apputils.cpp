#include "stdafx.h"
#include "util.h"
#include "options.h"
#include "apputils.h"


HWINEVENTHOOK EventHookWindowCreationMonitor::hook = 0;
HWND EventHookWindowCreationMonitor::hWnd = 0;
int EventHookWindowCreationMonitor::msgId = 0;


/*bool HookDll::init(HWND hWnd, int msgId)
{
    if (!hDll) {
        hDll = LoadLibrary(_T("dphook.dll"));
        if (!hDll)
            return false;
    }

    dllInitFunc = initF(GetProcAddress(hDll, "init"));
    dllTermFunc = termF(GetProcAddress(hDll, "term"));
    if (!dllInitFunc || !dllTermFunc) {
        term();
        return false;
    }

    return dllInitFunc(hWnd, msgId);
}


bool HookDll::term()
{
    if (!hDll)
        return true;

    if (dllTermFunc)
        dllTermFunc();

    bool ok = FreeLibrary(hDll);
    if (ok)
        hDll = 0;

    return ok;
}*/


//-------------------------------------------------------------


void PendingWindows::add(HWND hWnd)
{
    Entry entry(hWnd, GetTickCount());
    entries.push_back(entry);
}


void PendingWindows::check(HWND hWnd, const Options& opt)
{
    for (int n = entries.size()-1; n >= 0; --n) {
        if (timeToChkWnd(entries[n].time, opt)) {
            if (checkWnd(entries[n].hWnd, opt))
                PinWindow(hWnd, entries[n].hWnd, opt.trackRate.value, true);
            entries.erase(entries.begin() + n);
        }
    }
}


bool PendingWindows::timeToChkWnd(DWORD t, const Options& opt)
{
    // ticks are unsigned, so wrap-around is ok
    return GetTickCount() - t >= DWORD(opt.autoPinDelay.value);
}


bool PendingWindows::checkWnd(HWND hTarget, const Options& opt)
{
    for (int n = 0; n < int(opt.autoPinRules.size()); ++n)
        if (opt.autoPinRules[n].match(hTarget))
            return true;
    return false;
}

