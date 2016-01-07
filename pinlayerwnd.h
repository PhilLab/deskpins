#pragma once


class PinLayerWnd
{
public:
    static ATOM registerClass();
    static LRESULT CALLBACK proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static LPCWSTR className;

protected:
    static bool gotInitLButtonDown;

    static void evLButtonDown(HWND wnd, UINT mk, int x, int y);
};
