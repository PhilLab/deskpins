#pragma once


class Help : boost::noncopyable {
public:
    Help() {}

    bool init(HINSTANCE inst, const std::wstring& fname);
    HWND show(HWND wnd, const std::wstring& topic = L"");

protected:
    std::wstring hlpFile;
};
