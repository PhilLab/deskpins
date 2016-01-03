#pragma once


class Help : boost::noncopyable {
public:
    Help() {}

    bool init(HINSTANCE inst, const ef::tstring& fname);
    HWND show(HWND wnd, const ef::tstring& topic = ef::tstring());

protected:
    ef::tstring hlpFile;
};
