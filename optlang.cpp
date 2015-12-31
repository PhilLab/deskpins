#include "stdafx.h"
#include "options.h"
#include "util.h"
#include "optlang.h"
#include "resource.h"


const DWORD CHM_MARKER_SIG = 0xefda7a00;  // from chmmark.py


struct Data {
    tstring fname, dispName, descr;
    Data(const tstring& fname_    = tstring(), 
        const tstring& dispName_ = tstring(), 
        const tstring& descr_    = tstring())
        : fname(fname_), dispName(dispName_), descr(descr_) {}
};


tstring getComboSel(HWND hWnd)
{
    tstring ret;
    int n = SendMessage(hWnd, CB_GETCURSEL, 0, 0);
    if (n != CB_ERR) {
        int i = SendMessage(hWnd, CB_GETITEMDATA, n, 0);
        if (i != CB_ERR && i != 0) {
            ret.assign(reinterpret_cast<Data*>(i)->fname);
        }
    }
    return ret;
}


tstring getLangFileDescr(const tstring& path, const tstring& file)
{
    HINSTANCE hInst = file.empty() ? app.hInst : LoadLibrary((path+file).c_str());
    tchar buf[100];
    if (!hInst || !LoadString(hInst, IDS_LANG, buf, sizeof(buf)))
        *buf = '\0';
    if (!file.empty() && hInst)   // release hInst if we had to load it
        FreeLibrary(hInst);
    return buf;
}


void loadLangFiles(HWND hCombo, const tstring& path, const tstring& cur)
{
    std::vector<tstring> files = GetFiles(path + _T("lang*.dll"));
    files.push_back(tstring());    // special entry

    for (int n = 0; n < int(files.size()); ++n) {
        Data* data = new Data;
        data->fname    = files[n];
        data->dispName = files[n].empty() ? ef::fileSpec(ef::Win::getModulePath(app.hInst)) : files[n];
        data->descr    = getLangFileDescr(path, files[n]);
        int i = SendMessage(hCombo, CB_ADDSTRING, 0, LPARAM(data));
        if (i == CB_ERR)
            delete data;
        else if (strimatch(data->fname.c_str(), cur.c_str()))
            SendMessage(hCombo, CB_SETCURSEL, i, 0);
    }

}


static bool ReadFileBack(HANDLE hFile, void* buf, int bytes)
{
    DWORD read;
    return SetFilePointer(hFile, -bytes, 0, FILE_CURRENT) != -1
        && ReadFile(hFile, buf, bytes, &read, 0)
        && int(read) == bytes
        && SetFilePointer(hFile, -bytes, 0, FILE_CURRENT) != -1;
}


tstring getHelpFileDescr(const tstring& path, const tstring& file)
{
    // Data layout:
    //  {CHM file data...}
    //  WCHAR data[size]  // no NULs
    //  DWORD size
    //  DWORD sig

    tstring ret;
    DWORD sig, len;
    ef::Win::AutoFileH hFile = ef::Win::FileH::create(path + file, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (hFile != INVALID_HANDLE_VALUE &&
        hFile.setPosFromEnd32(0) &&
        ReadFileBack(hFile, &sig, sizeof(sig)) && 
        sig == CHM_MARKER_SIG &&
        ReadFileBack(hFile, &len, sizeof(len)))
    {
        boost::scoped_array<char> buf(new char[len]);
        if (ReadFileBack(hFile, buf.get(), len)) {
            boost::scoped_array<wchar_t> wbuf(new wchar_t[len]);
            if (MultiByteToWideChar(CP_THREAD_ACP, 0, buf.get(), len, wbuf.get(), len/*, 0, 0*/))
                ret.assign(wbuf.get(), len);
        }
    }
    return ret;
}


void loadHelpFiles(HWND hCombo, const tstring& path, const tstring& cur)
{
    std::vector<tstring> files = GetFiles(path + _T("DeskPins*.chm"));

    for (int n = 0; n < int(files.size()); ++n) {
        Data* data = new Data;
        data->fname =    files[n];
        data->dispName = files[n];
        data->descr    = getHelpFileDescr(path, files[n]);
        int i = SendMessage(hCombo, CB_ADDSTRING, 0, LPARAM(data));
        if (i == CB_ERR)
            delete data;
        else if (strimatch(data->fname.c_str(), cur.c_str()))
            SendMessage(hCombo, CB_SETCURSEL, i, 0);

    }

}


static bool EvInitDialog(HWND hWnd, HWND /*hFocus*/, LPARAM lParam)
{
    // must have a valid data ptr
    if (!lParam) {
        EndDialog(hWnd, IDCANCEL);
        return true;
    }

    // save the data ptr
    PROPSHEETPAGE& psp = *reinterpret_cast<PROPSHEETPAGE*>(lParam);
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(psp.lParam)->opt;
    SetWindowLong(hWnd, DWL_USER, psp.lParam);

    tstring exePath = ef::dirSpec(ef::Win::getModulePath(app.hInst));
    if (!exePath.empty()) {
#ifdef DEBUG
        loadLangFiles(GetDlgItem(hWnd, IDC_UILANG),   exePath + _T("..\\localization\\"), opt.uiFile);
        loadHelpFiles(GetDlgItem(hWnd, IDC_HELPLANG), exePath + _T("..\\help\\"),         opt.helpFile);
#else
        loadLangFiles(GetDlgItem(hWnd, IDC_UILANG),   exePath, opt.uiFile);
        loadHelpFiles(GetDlgItem(hWnd, IDC_HELPLANG), exePath, opt.helpFile);
#endif
    }

    return true;
}


static bool Validate(HWND hWnd)
{
    return true;
}


static void Apply(HWND hWnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;

    tstring uiFile = getComboSel(GetDlgItem(hWnd, IDC_UILANG));
    if (opt.uiFile != uiFile) {
        if (app.loadResMod(uiFile.c_str(), hWnd))
            opt.uiFile = uiFile;
        else
            opt.uiFile = _T("");
    }

    opt.helpFile = getComboSel(GetDlgItem(hWnd, IDC_HELPLANG));
    app.help.init(app.hInst, opt.helpFile);
}


BOOL CALLBACK OptLangProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG:  return EvInitDialog(hWnd, HWND(wParam), lParam);
        case WM_NOTIFY: {
            NMHDR nmhdr = *reinterpret_cast<NMHDR*>(lParam);
            switch (nmhdr.code) {
                case PSN_SETACTIVE: {
                    HWND hTab = PropSheet_GetTabControl(nmhdr.hwndFrom);
                    app.optionPage = SendMessage(hTab, TCM_GETCURSEL, 0, 0);
                    SetWindowLong(hWnd, DWL_MSGRESULT, 0);
                    break;
                }
                case PSN_KILLACTIVE: {
                    SetWindowLong(hWnd, DWL_MSGRESULT, !Validate(hWnd));
                    break;
                }
                case PSN_APPLY: {
                    Apply(hWnd);
                    break;
                }
                case PSN_HELP: {
                    app.help.show(hWnd, _T("::\\optlang.htm"));
                    break;
                }
                default:
                    return false;
            }
            break;
        }
        case WM_COMMAND: {
            WORD code = HIWORD(wParam), id = LOWORD(wParam);
            if (code == CBN_SELCHANGE) {
                if (id == IDC_UILANG || id == IDC_HELPLANG)
                    PSChanged(hWnd);
            }
            break;
        }
        case WM_HELP: {
            app.help.show(hWnd, _T("::\\optlang.htm"));
            break;
        }
        case WM_DELETEITEM: {
            if (wParam == IDC_UILANG || wParam == IDC_HELPLANG)
                delete (Data*)(LPDELETEITEMSTRUCT(lParam)->itemData);
            break;
        }
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT& mis = *(MEASUREITEMSTRUCT*)lParam;
            mis.itemHeight = ef::Win::FontH(ef::Win::FontH::getStockDefaultGui()).getHeight() + 2;
            break;
        }
        case WM_DRAWITEM: { 
            DRAWITEMSTRUCT& dis = *(DRAWITEMSTRUCT*)lParam;
            RECT& rc = dis.rcItem;
            HDC dc = dis.hDC;      
            if (dis.itemID != -1) {
                bool sel = dis.itemState & ODS_SELECTED;
                FillRect(dc, &rc, GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));

                if (dis.itemData) {
                    Data& data = *reinterpret_cast<Data*>(dis.itemData);

                    UINT     orgAlign  = SetTextAlign(dc, TA_LEFT);
                    COLORREF orgTxtClr = SetTextColor(dc, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
                    int      orgBkMode = SetBkMode(dc, TRANSPARENT);

                    TextOut(dc, rc.left+2, rc.top+1, data.descr.c_str(), data.descr.length());
                    SetTextAlign(dc, TA_RIGHT);
                    if (!sel) SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
                    TextOut(dc, rc.right-2, rc.top+1, data.dispName.c_str(), data.dispName.length());

                    SetBkMode   (dc, orgBkMode);
                    SetTextColor(dc, orgTxtClr);
                    SetTextAlign(dc, orgAlign);
                }
            }
            break;
        }
        case WM_COMPAREITEM: {
            COMPAREITEMSTRUCT& cis = *(COMPAREITEMSTRUCT*)lParam;
            if (wParam == IDC_UILANG || wParam == IDC_HELPLANG) {
                Data* d1 = (Data*)cis.itemData1;
                Data* d2 = (Data*)cis.itemData2;
                if (d1 && d2) {
                    int order = CompareString(cis.dwLocaleId, NORM_IGNORECASE, 
                        d1->descr.c_str(), d1->descr.length(), 
                        d2->descr.c_str(), d2->descr.length());
                    if (order) return order - 2;    // convert 1/2/3 to -1/0/1
                }
            }
            return 0;
        }
        default:
            return false;
    }

    return true;
}
