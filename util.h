#pragma once

#include "common.h"


namespace Util {

    namespace Sys {
        bool isWin8orGreater();
        std::vector<std::wstring> getFiles(std::wstring mask);
        bool getScrSize(SIZE& sz);
        bool readFileBack(HANDLE file, void* buf, int bytes);
    }

    namespace Wnd {
        bool isChild(HWND wnd);
        bool isWndRectEmpty(HWND wnd);
        HWND getNonChildParent(HWND wnd);
        HWND getTopParent(HWND wnd);
        bool isProgManWnd(HWND wnd);
        bool isTaskBar(HWND wnd);
        bool isTopMost(HWND wnd);
        BOOL moveWindow(HWND wnd, const RECT& rc, BOOL repaint = TRUE);
        bool psChanged(HWND page);
        void enableGroup(HWND wnd, int id, bool mode);
        bool isVCLAppWnd(HWND wnd);
    }

    namespace App {
        void error(HWND wnd, LPCWSTR s);
        void warning(HWND wnd, LPCWSTR s);
        void pinWindow(HWND wnd, HWND hitWnd, int trackRate, bool silent = false);
        HWND hasPin(HWND wnd);
        void togglePin(HWND wnd, HWND target, int trackRate);
        void markWnd(HWND wnd, bool mode);
        std::wstring getLangFileDescr(const std::wstring& path, const std::wstring& file);
        std::wstring getHelpFileDescr(const std::wstring& path, const std::wstring& name);
    }

    namespace Text {
        inline bool strmatch(LPCWSTR s1, LPCWSTR s2) {
            return wcscmp(s1, s2) == 0;
        }
        inline bool strimatch(LPCWSTR s1, LPCWSTR s2) {
            return _wcsicmp(s1, s2) == 0;
        }
        std::wstring remAccel(std::wstring s);
        std::wstring substrAfterLast(const std::wstring& s, const std::wstring& delim);
    }

    namespace Gfx {
        HRGN makeRegionFromBmp(HBITMAP bmp, COLORREF clrMask);
        BOOL rectangle(HDC dc, const RECT& rc);
        bool getBmpSize(HBITMAP bmp, SIZE& sz);
        bool remapBmpColors(HBITMAP bmp, COLORREF clrs[][2], int cnt);
        bool rectContains(const RECT& rc1, const RECT& rc2);
        bool getBmpSizeAndBpp(HBITMAP bmp, SIZE& sz, int& bpp);
    }

    namespace Res {
        HMENU LoadLocalizedMenu(LPCTSTR lpMenuName);
        HMENU LoadLocalizedMenu(WORD id);
        int   LocalizedDialogBoxParam(LPCTSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
        int   LocalizedDialogBoxParam(WORD id, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
        HWND  CreateLocalizedDialog(LPCTSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
        HWND  CreateLocalizedDialog(WORD id, HWND hWndParent, DLGPROC lpDialogFunc);

        // Resource string loading and formatting.
        // Automatically uses loaded language DLL, if any.
        //
        class ResStr : boost::noncopyable {
        public:
            ResStr(DWORD id, int bufLen = 256) {
                str = new WCHAR[bufLen];
                if (!app.resMod || !LoadString(app.resMod, id, str, bufLen))
                    LoadString(app.inst, id, str, bufLen);
            }

            ResStr(DWORD id, int bufLen, DWORD p1) {
                initFmt(id, bufLen, &p1);
            }

            ResStr(DWORD id, int bufLen, DWORD p1, DWORD p2) {
                DWORD params[] = {p1,p2};
                initFmt(id, bufLen, params);
            }

            ResStr(DWORD id, int bufLen, DWORD p1, DWORD p2, DWORD p3) {
                DWORD params[] = {p1,p2,p3};
                initFmt(id, bufLen, params);
            }

            ResStr(DWORD id, int bufLen, DWORD* params) {
                initFmt(id, bufLen, params);
            }

            ~ResStr() {
                delete[] str;
            }

            operator LPCWSTR() const {
                return str;
            }

        private:
            LPWSTR str;

            void initFmt(DWORD id, int bufLen, DWORD* params) {
                str = new WCHAR[bufLen];
                if (!app.resMod || !LoadString(app.resMod, id, str, bufLen))
                    LoadString(app.inst, id, str, bufLen);

                DWORD flags = FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY;
                va_list* va = reinterpret_cast<va_list*>(params);
                if (!FormatMessage(flags, std::wstring(str).c_str(), 0, 0, str, bufLen, va))
                    *str = 0;
            }
        };

    }

    namespace Clr {
        enum {
            black   = 0x000000, maroon  = 0x000080, green   = 0x008000, olive   = 0x008080, 
            navy    = 0x800000, purple  = 0x800080, teal    = 0x808000, silver  = 0xc0c0c0, 
            gray    = 0x808080, red     = 0x0000ff, lime    = 0x00ff00, yellow  = 0x00ffff,
            blue    = 0xff0000, magenta = 0xff00ff, cyan    = 0xffff00, white   = 0xffffff,
        };
        COLORREF light(COLORREF clr);
        COLORREF dark(COLORREF clr);
    }

}
