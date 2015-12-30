// dphook.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

HINSTANCE hInst;

// shared data
#pragma data_seg(".shared")
HHOOK hHook      = 0;
HWND  hWndEvSink = 0;
int   msg        = 0;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.shared,RWS")


BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    hInst = hinstDLL;
    return TRUE;
}


LRESULT CALLBACK ShellProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HSHELL_WINDOWCREATED) {
        PostMessage(hWndEvSink, msg, wParam, 0);
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}


extern "C" {
    __declspec(dllexport) bool init(HWND hWnd, int msgId);
    __declspec(dllexport) bool term();
}


__declspec(dllexport) bool init(HWND hWnd, int msgId)
{
    hHook      = SetWindowsHookEx(WH_SHELL, ShellProc, hInst, 0);
    hWndEvSink = hWnd;
    msg        = msgId;
    return hHook;
}


__declspec(dllexport) bool term()
{
    bool ok = UnhookWindowsHookEx(hHook);
    hHook      = 0;
    hWndEvSink = 0;
    msg        = 0;
    return ok;
}
