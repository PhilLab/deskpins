#pragma once


class Help : boost::noncopyable {
public:
    Help() {}

    bool init(HINSTANCE hInst, const ef::tstring& fname);
    HWND show(HWND hWnd, const ef::tstring& topic = ef::tstring());

protected:
    ef::tstring hlpFile;
};
