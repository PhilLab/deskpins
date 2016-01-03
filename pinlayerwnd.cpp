#include "stdafx.h"
#include "util.h"
#include "pinlayerwnd.h"


static bool gotInitLButtonDown;


static void EvLButtonDown(HWND hWnd, UINT /*mk*/, int x, int y)
{
    SetCapture(hWnd);

    if (!gotInitLButtonDown)
        gotInitLButtonDown = true;
    else {
        ReleaseCapture();
        POINT pt = { x, y };
        if (ClientToScreen(hWnd, &pt))
            PostMessage(GetParent(hWnd), App::WM_PINREQ, pt.x, pt.y);
        DestroyWindow(hWnd);
    }

}


LRESULT CALLBACK PinLayerWndProc(HWND hWnd, UINT uMsg, 
                                 WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        gotInitLButtonDown = false;
        return false;

    case WM_DESTROY:
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        if (hWnd == app.hLayerWnd) app.hLayerWnd = 0;
        return false;

    case WM_LBUTTONDOWN:
        EvLButtonDown(hWnd, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return false;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KILLFOCUS:
        ReleaseCapture();
        DestroyWindow(hWnd);
        return false;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
