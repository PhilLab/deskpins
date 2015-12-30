#pragma once


class TrayIcon : boost::noncopyable {
public:
    TrayIcon(UINT msg_, UINT id_);
    ~TrayIcon();

    bool setWnd(HWND hWnd_);

    bool create(HICON hIcon, LPCTSTR tip);
    bool destroy();

    bool setTip(LPCTSTR tip);
    bool setIcon(HICON hIcon);

private:
    HWND hWnd;
    UINT id;
    UINT msg;
};
