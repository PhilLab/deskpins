#include "stdafx.h"
#include "util.h"
#include "pinlayerwnd.h"


static bool gotInitLButtonDown;


static void evLButtonDown(HWND wnd, UINT /*mk*/, int x, int y)
{
    SetCapture(wnd);

    if (!gotInitLButtonDown)
        gotInitLButtonDown = true;
    else {
        ReleaseCapture();
        POINT pt = { x, y };
        if (ClientToScreen(wnd, &pt))
            PostMessage(GetParent(wnd), App::WM_PINREQ, pt.x, pt.y);
        DestroyWindow(wnd);
    }

}


LRESULT CALLBACK pinLayerWndProc(HWND wnd, UINT msg, 
                                 WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE:
        gotInitLButtonDown = false;
        return false;

    case WM_DESTROY:
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        if (wnd == app.layerWnd) app.layerWnd = 0;
        return false;

    case WM_LBUTTONDOWN:
        evLButtonDown(wnd, wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return false;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KILLFOCUS:
        ReleaseCapture();
        DestroyWindow(wnd);
        return false;
    }

    return DefWindowProc(wnd, msg, wparam, lparam);
}
