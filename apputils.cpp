#include "stdafx.h"
#include "util.h"
#include "options.h"
#include "apputils.h"


HWINEVENTHOOK EventHookWindowCreationMonitor::hook = 0;
HWND EventHookWindowCreationMonitor::wnd = 0;
int EventHookWindowCreationMonitor::msgId = 0;


void PendingWindows::add(HWND wnd)
{
    Entry entry(wnd, GetTickCount());
    entries.push_back(entry);
}


void PendingWindows::check(HWND wnd, const Options& opt)
{
    for (int n = entries.size()-1; n >= 0; --n) {
        if (timeToChkWnd(entries[n].time, opt)) {
            if (checkWnd(entries[n].wnd, opt))
                Util::App::pinWindow(wnd, entries[n].wnd, opt.trackRate.value, true);
            entries.erase(entries.begin() + n);
        }
    }
}


bool PendingWindows::timeToChkWnd(DWORD t, const Options& opt)
{
    // ticks are unsigned, so wrap-around is ok
    return GetTickCount() - t >= DWORD(opt.autoPinDelay.value);
}


bool PendingWindows::checkWnd(HWND target, const Options& opt)
{
    for (int n = 0; n < int(opt.autoPinRules.size()); ++n)
        if (opt.autoPinRules[n].match(target))
            return true;
    return false;
}

