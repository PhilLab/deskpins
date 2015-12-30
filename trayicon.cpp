#include "stdafx.h"
#include "trayicon.h"


TrayIcon::TrayIcon(UINT msg_, UINT id_)
:
hWnd(0), msg(msg_), id(id_)
{
}


TrayIcon::~TrayIcon()
{
    destroy();
}


// can only be called once
bool TrayIcon::setWnd(HWND hWnd_)
{
    if (hWnd) return false;

    hWnd = hWnd_;
    return true;
}


bool TrayIcon::create(HICON hIcon, LPCTSTR tip)
{
    NOTIFYICONDATA nid;
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = hWnd;
    nid.uID              = id;
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = msg;
    nid.hIcon            = hIcon;
    lstrcpyn(nid.szTip, tip, sizeof(nid.szTip));

    return Shell_NotifyIcon(NIM_ADD, &nid);
}


bool TrayIcon::destroy()
{
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd   = hWnd;
    nid.uID    = id;

    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

bool TrayIcon::setTip(LPCTSTR tip)
{
    NOTIFYICONDATA nid;
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = hWnd;
    nid.uID              = id;
    nid.uFlags           = NIF_TIP;
    lstrcpyn(nid.szTip, tip, sizeof(nid.szTip));

    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}


bool TrayIcon::setIcon(HICON hIcon)
{
    NOTIFYICONDATA nid;
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = hWnd;
    nid.uID              = id;
    nid.uFlags           = NIF_ICON;
    nid.hIcon            = hIcon;

    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

