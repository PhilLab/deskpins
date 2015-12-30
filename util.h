#pragma once

#include "common.h"

#define CLR_WHITE     RGB(255,255,255)
#define CLR_BLACK     RGB(  0,  0,  0)
#define CLR_LGRAY     RGB(192,192,192)
#define CLR_DGRAY     RGB(128,128,128)

#define CLR_LRED      RGB(255,  0,  0)
#define CLR_LYELLOW   RGB(255,255,  0)
#define CLR_LGREEN    RGB(  0,255,  0)
#define CLR_LCYAN     RGB(  0,255,255)
#define CLR_LBLUE     RGB(  0,  0,255)
#define CLR_LMAGENTA  RGB(255,  0,255)

#define CLR_DRED      RGB(128,  0,  0)
#define CLR_DYELLOW   RGB(128,128,  0)
#define CLR_DGREEN    RGB(  0,128,  0)
#define CLR_DCYAN     RGB(  0,128,128)
#define CLR_DBLUE     RGB(  0,  0,128)
#define CLR_DMAGENTA  RGB(128,  0,128)

bool IsWndRectEmpty(HWND hWnd);
HWND GetNonChildParent(HWND hWnd);
HWND GetTopParent(HWND hWnd);
bool IsProgManWnd(HWND hWnd);
bool IsTaskBar(HWND hWnd);
bool IsTopMost(HWND hWnd);
void Error(HWND hWnd, const ef::tchar* s);
void Warning(HWND hWnd, const ef::tchar* s);
bool GetScrSize(SIZE& szScr);

inline bool strmatch(const ef::tchar* s1, const ef::tchar* s2)
{
    return _tcscmp(s1, s2) == 0;
}

inline bool strimatch(const ef::tchar* s1, const ef::tchar* s2)
{
    return _tcsicmp(s1, s2) == 0;
}

HRGN MakeRegionFromBmp(HBITMAP hBmp, COLORREF clrMask);
void PinWindow(HWND hWnd, HWND hHitWnd, int trackRate, bool silent = false);
void TogglePin(HWND hWnd, HWND hTarget, int trackRate);

HMENU LoadLocalizedMenu(LPCTSTR lpMenuName);
HMENU LoadLocalizedMenu(WORD id);
int   LocalizedDialogBoxParam(LPCTSTR lpTemplateName, HWND hWndParent, 
                              DLGPROC lpDialogFunc, LPARAM dwInitParam);
int   LocalizedDialogBoxParam(WORD id, HWND hWndParent, 
                              DLGPROC lpDialogFunc, LPARAM dwInitParam);
HWND  CreateLocalizedDialog  (LPCTSTR lpTemplate, HWND hWndParent, 
                              DLGPROC lpDialogFunc);
HWND  CreateLocalizedDialog  (WORD id, HWND hWndParent, 
                              DLGPROC lpDialogFunc);


bool RectContains(const RECT& rc1, const RECT& rc2);
void EnableGroup(HWND hWnd, int id, bool mode);

std::vector<ef::tstring> GetFiles(ef::tstring mask);


COLORREF Light(COLORREF clr);
COLORREF Dark(COLORREF clr);


#define PACKVERSION(major,minor) MAKELONG(minor,major)


// automatically handles language
class ResStr {
public:
    ResStr(DWORD id, int bufLen = 256) {
        str = new ef::tchar[bufLen];
        if (!app.hResMod || !LoadString(app.hResMod, id, str, bufLen))
            LoadString(app.hInst, id, str, bufLen);
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

    ResStr(const ResStr& other)
    {
        str = new ef::tchar[_tcslen(other.str) + 1];
        _tcscpy(str, other.str);
    }

    ResStr& operator=(const ResStr& other)
    {
        if (*this != other) {
            delete[] str;
            str = new ef::tchar[_tcslen(other.str) + 1];
            _tcscpy(str, other.str);
        }
    }

    ~ResStr() {
        delete[] str;
    }

    operator const ef::tchar*() const {
        return str;
    }

    operator ef::tchar*() {
        return str;
    }

protected:
    void initFmt(DWORD id, int bufLen, DWORD* params) {
        str = new ef::tchar[bufLen];
        if (!app.hResMod || !LoadString(app.hResMod, id, str, bufLen))
            LoadString(app.hInst, id, str, bufLen);

        DWORD flags = FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY;
        va_list* va = reinterpret_cast<va_list*>(params);
        if (!FormatMessage(flags, tstring(str).c_str(), 0, 0, str, bufLen, va))
            *str = 0;
    }

private:
    ef::tchar* str;

};


BOOL MoveWindow(HWND hWnd, const RECT& rc, BOOL repaint = TRUE);
BOOL Rectangle(HDC hDC, const RECT& rc);


bool PSChanged(HWND hPage);
ef::tstring RemAccel(ef::tstring s);

bool getBmpSize(HBITMAP hBmp, SIZE& sz);

bool remapBmpColors(HBITMAP hBmp, COLORREF clrs[][2], int cnt);


ef::tstring substrAfterLast(const ef::tstring& s, const ef::tstring& delim);
