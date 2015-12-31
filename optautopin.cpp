#include "stdafx.h"
#include "options.h"
#include "util.h"
#include "apputils.h"
#include "optautopin.h"
#include "resource.h"


BOOL CALLBACK TargetWndProc(HWND, UINT, WPARAM, LPARAM);


struct TargetWndData {
    WNDPROC orgProc;
    bool    cursShowing;
    HCURSOR hCurs;

    TargetWndData(HWND hWnd)
    {
        orgProc = WNDPROC(GetWindowLong(hWnd, GWL_WNDPROC));
        if (!orgProc)
            throw 0;  // TODO: use an exception
        SetWindowLong(hWnd, GWL_USERDATA, LONG(this));
        SetWindowLong(hWnd, GWL_WNDPROC, LONG(TargetWndProc));

        cursShowing = true;
        hCurs = LoadCursor(app.hInst, MAKEINTRESOURCE(IDC_BULLSEYE));
    }

    ~TargetWndData() {}

private:
    TargetWndData(const TargetWndData&);
    TargetWndData& operator=(const TargetWndData&);

};


BOOL CALLBACK TargetWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TargetWndData& data = *(TargetWndData*)GetWindowLong(hWnd, GWL_USERDATA);

    switch (uMsg) {
        case WM_NCDESTROY: {
            WNDPROC orgProc = data.orgProc;
            delete &data;
            return CallWindowProc(orgProc, hWnd, uMsg, wParam, lParam);
        }
        case WM_PAINT: {
            LRESULT ret = CallWindowProc(data.orgProc, hWnd, uMsg, wParam, lParam);
            HDC hDC = GetDC(hWnd);
            if (hDC) {
                if (data.cursShowing) {
                    RECT rc;
                    GetClientRect(hWnd, &rc);
                    int x = (rc.right  - GetSystemMetrics(SM_CXCURSOR)) / 2;
                    int y = (rc.bottom - GetSystemMetrics(SM_CYCURSOR)) / 2;          
                    DrawIcon(hDC, x+1, y+1, data.hCurs);
                }
                ReleaseDC(hWnd, hDC);
            }
            return ret;
        }
        case WM_USER: {
            data.cursShowing = wParam != 0;
            InvalidateRect(hWnd, 0, true);
            UpdateWindow(hWnd);
            return 0;
        }
    }

    return CallWindowProc(data.orgProc, hWnd, uMsg, wParam, lParam);
}


void MarkWnd(HWND hWnd, bool mode)
{
    const int blinkDelay = 50;  // msec
    // thickness of highlight border
    const int width = 3;
    // first val can vary; second should be zero
    const int flashes = mode ? 1 : 0;
    // amount to deflate if wnd is maximized, to make the highlight visible
    const int zoomFix = IsZoomed(hWnd) ? GetSystemMetrics(SM_CXFRAME) : 0;

    HDC hDC = GetWindowDC(hWnd);
    if (hDC) {
        int orgRop2 = SetROP2(hDC, R2_XORPEN);

        HRGN hRgn = CreateRectRgn(0,0,0,0);
        bool hasRgn = GetWindowRgn(hWnd, hRgn) != ERROR;
        const int loops = flashes*2+1;

        if (hasRgn) {
            for (int m = 0; m < loops; ++m) {
                FrameRgn(hDC, hRgn, HBRUSH(GetStockObject(WHITE_BRUSH)), width, width);
                GdiFlush();
                if (mode && m < loops-1)
                    Sleep(blinkDelay);
            }
        }
        else {
            RECT rc;
            GetWindowRect(hWnd, &rc);    
            OffsetRect(&rc, -rc.left, -rc.top);
            InflateRect(&rc, -zoomFix, -zoomFix);

            HGDIOBJ orgPen = SelectObject(hDC, GetStockObject(WHITE_PEN));
            HGDIOBJ orgBrush = SelectObject(hDC, GetStockObject(NULL_BRUSH));

            RECT rcTmp;
            for (int m = 0; m < loops; ++m) {
                CopyRect(&rcTmp, &rc);
                for (int n = 0; n < width; ++n) {
                    Rectangle(hDC, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
                    InflateRect(&rcTmp, -1, -1);
                }
                GdiFlush();
                if (mode && m < loops-1)
                    Sleep(blinkDelay);
            }
            SelectObject(hDC, orgBrush);
            SelectObject(hDC, orgPen);
        }

        SetROP2(hDC, orgRop2);
        ReleaseDC(hWnd, hDC);
        DeleteObject(hRgn);
    }

}


BOOL CALLBACK APEditRuleDlgProc(
                                HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static int tracking = 0;    // 0: none, 1: title, 2: class
    static HWND hHitWnd = 0;
    static SIZE minSize;
    static SIZE listMarg;
    static int  btnMarg;
    static HWND hTooltip = 0;

    switch (uMsg) {
        case WM_INITDIALOG: {
            if (!lParam) {
                EndDialog(hWnd, -1);
                return true;
            }
            SetWindowLong(hWnd, DWL_USER, lParam);
            AutoPinRule& rule = *reinterpret_cast<AutoPinRule*>(lParam);

            hHitWnd = 0;

            SetDlgItemText(hWnd, IDC_DESCR, rule.descr.c_str());
            SetDlgItemText(hWnd, IDC_TITLE, rule.ttl.c_str());
            SetDlgItemText(hWnd, IDC_CLASS, rule.cls.c_str());

            try { 
                new TargetWndData(GetDlgItem(hWnd, IDC_TTLPICK));
                new TargetWndData(GetDlgItem(hWnd, IDC_CLSPICK));
            } 
            catch (int) {
                // oh, well...
            }

            // create tooltip for target icons
            hTooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, _T(""),
                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hWnd, 0, app.hInst, 0);

            TOOLINFO ti;
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
            ti.hwnd = hWnd;
            ti.hinst = app.hInst;
            ti.lParam = 0;

            ti.uId = UINT_PTR(GetDlgItem(hWnd, IDC_TTLPICK));
            ti.lpszText = _T("Drag icon to get window title");
            SendMessage(hTooltip, TTM_ADDTOOL, 0, LPARAM(&ti));

            ti.uId = UINT_PTR(GetDlgItem(hWnd, IDC_CLSPICK));
            ti.lpszText = _T("Drag icon to get window class");
            SendMessage(hTooltip, TTM_ADDTOOL, 0, LPARAM(&ti));


            return true;
        }
        case WM_DESTROY: {
            DestroyWindow(hTooltip);
            return true;
        }
        case WM_MOUSEMOVE: {
            if (tracking) {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ClientToScreen(hWnd, &pt);
                HWND hNewWnd = GetNonChildParent(WindowFromPoint(pt));
                if (hHitWnd != hNewWnd && !IsProgManWnd(hNewWnd) && !IsTaskBar(hNewWnd)) {
                    if (hHitWnd)
                        MarkWnd(hHitWnd, false);

                    hHitWnd = hNewWnd;

                    if (tracking == 1) {
                        ef::Win::WndH w = GetDlgItem(hWnd, IDC_TITLE);
                        w.setText(ef::Win::WndH(hHitWnd).getText());
                        w.update();
                    }
                    else {
                        ef::Win::WndH w = GetDlgItem(hWnd, IDC_CLASS);
                        w.setText(ef::Win::WndH(hHitWnd).getClassName());
                        w.update();
                    }

                    MarkWnd(hHitWnd, true);

                }
            }
            return true;
        }
        case WM_LBUTTONUP: {
            if (tracking) {
                if (hHitWnd)
                    MarkWnd(hHitWnd, false);
                ReleaseCapture();
                SendDlgItemMessage(hWnd, tracking == 1 ? IDC_TTLPICK : IDC_CLSPICK, 
                    WM_USER, true, 0);
                tracking = 0;
                hHitWnd = 0;
            }
            return true;
        }
        case WM_HELP: {
            app.help.show(hWnd, _T("::\\editruledlg.htm"));
            return true;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDOK: {
                    AutoPinRule& rule = 
                        *reinterpret_cast<AutoPinRule*>(GetWindowLong(hWnd, DWL_USER));
                    tchar buf[256];
                    GetDlgItemText(hWnd, IDC_DESCR, buf, sizeof(buf));  rule.descr = buf;
                    GetDlgItemText(hWnd, IDC_TITLE, buf, sizeof(buf));  rule.ttl = buf;
                    GetDlgItemText(hWnd, IDC_CLASS, buf, sizeof(buf));  rule.cls = buf;
                    // allow empty strings (at least title can be empty...)
                    //if (rule.ttl.empty()) rule.ttl = "*";
                    //if (rule.cls.empty()) rule.cls = "*";          
                }
                case IDCANCEL:
                    EndDialog(hWnd, id);
                    return true;
                case IDHELP:
                    app.help.show(hWnd, _T("::\\editruledlg.htm"));
                    return true;
                case IDC_TTLPICK:
                case IDC_CLSPICK: {
                    if (HIWORD(wParam) == STN_CLICKED) {
                        SendDlgItemMessage(hWnd, id, WM_USER, false, 0);
                        SetCapture(hWnd);
                        SetCursor(LoadCursor(app.hInst, MAKEINTRESOURCE(IDC_BULLSEYE)));
                        tracking = id == IDC_TTLPICK ? 1 : 2;
                    }
                    return true;
                }
                default: {
                    return false;
                }
            }
        }
        default:
            return false;
        }

}


// A class to handle interaction with the list control
class RulesList {
public:
    RulesList();

    void init(HWND hWnd);
    void term();

    void setAll(const AutoPinRules& rules);
    void getAll(AutoPinRules& rules);
    void remove(std::vector<int> indices);

    int  addNew();
    bool getRule(int i, AutoPinRule& rule) const;
    bool setRule(int i, const AutoPinRule& rule);

    void selectSingleRule(int i);
    int  getFirstSel() const;
    std::vector<int> getSel() const;
    void setItemSel(int i, bool sel);
    bool getItemSel(int i) const;

    void moveSelUp();
    void moveSelDown();

    operator HWND() const
    {
        return hList;
    }

    void checkClick(HWND hPage)
    {
        DWORD xy = GetMessagePos();
        LVHITTESTINFO hti;
        hti.pt.x = GET_X_LPARAM(xy);
        hti.pt.y = GET_Y_LPARAM(xy);
        ScreenToClient(hList, &hti.pt);
        int i = ListView_HitTest(hList, &hti);
        if (i >= 0 && hti.flags & LVHT_ONITEMSTATEICON) {
            if (emuChk) toggleItem(i);
            PSChanged(hPage);
        }
    }

    void toggleItem(int i)
    {
        AutoPinRule* rule = getData(i);
        if (rule) {
            rule->enabled = !rule->enabled;
            ListView_Update(hList, i);
        }
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        RulesList* list = (RulesList*)GetWindowLong(hWnd, GWL_USERDATA);
        if (!list || !list->orgProc)
            return DefWindowProc(hWnd, uMsg, wParam, lParam);

        if (uMsg == WM_CHAR && wParam == VK_SPACE) {
            RulesList* list = (RulesList*)GetWindowLong(hWnd, GWL_USERDATA);
            if (list)
                list->onSpace();
            return 0;
        }

        return CallWindowProc(list->orgProc, hWnd, uMsg, wParam, lParam);    
    }

    void onSpace()
    {
        int i = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
        if (i != -1)
            toggleItem(i);
    }

protected:
    HWND hList;
    HIMAGELIST ilChecks;
    bool emuChk;
    WNDPROC orgProc;

    AutoPinRule* getData(int i) const;
    bool setData(int i, AutoPinRule* rule);

};


//RulesList::RulesList() : hList(0), emuChk(SysVer::comctl() < PACKVERSION(4,70))
RulesList::RulesList() : hList(0), emuChk(ef::Win::getDllVer(_T("comctl32.dll")) < ef::Win::packVer(4,70))
{
    //emuChk = true;
}


void RulesList::init(HWND hWnd)
{
    hList = hWnd;

    // LVS_EX_CHECKBOXES is only available in comctl32 4.70+
    // (like LVS_EX_GRIDLINES and LVS_EX_FULLROWSELECT),
    // so we emulate it on earlier versions

    if (emuChk) {
        orgProc = WNDPROC(SetWindowLong(hList, GWL_WNDPROC, LONG(WndProc)));
        SetWindowLong(hList, GWL_USERDATA, LONG(this));

        HDC hDC = CreateCompatibleDC(0);
        if (hDC) {
            HBITMAP hBmp = CreateCompatibleBitmap(hDC, 32, 16);
            if (hBmp) {
                HGDIOBJ orgBmp = SelectObject(hDC, hBmp);
                if (orgBmp) {
                    HFONT hFnt = CreateFont(16, 0, 0,0,FW_NORMAL, false, false, false, SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Marlett"));
                    if (hFnt) {
                        HGDIOBJ orgFnt = SelectObject(hDC, hFnt);
                        if (orgFnt) {
                            //ab
                            RECT rc = {0,0,16,16};
                            for (int n = 0; n < 2; ++n) {
                                FillRect(hDC, &rc, HBRUSH(GetStockObject(WHITE_BRUSH)));
                                SelectObject(hDC, GetStockObject(NULL_BRUSH));

                                RECT rc2;
                                CopyRect(&rc2, &rc);

                                InflateRect(&rc2, -1, -1);
                                Rectangle(hDC, rc2);
                                InflateRect(&rc2, -1, -1);
                                Rectangle(hDC, rc2);
                                InflateRect(&rc2, -1, -1);

                                //SetTextColor(hDC, RGB(0,255,0));
                                //SetBkColor(hDC, RGB(0,255,0));
                                if (n == 1) {
                                    SetBkMode(hDC, TRANSPARENT);
                                    DrawText(hDC, _T("a"), 1, &rc, DT_NOPREFIX | DT_CENTER);
                                }

                                OffsetRect(&rc, 16, 0);
                            }

                            SelectObject(hDC, orgFnt);
                            DeleteObject(hFnt);
                        }
                    }
                    SelectObject(hDC, orgBmp);

                    ilChecks = ImageList_Create(16, 16, ILC_COLOR4 | ILC_MASK, 0, 1);
                    ImageList_AddMasked(ilChecks, hBmp, RGB(255,255,255));
                    ListView_SetImageList(hList, ilChecks, LVSIL_STATE);
                }
                DeleteObject(hBmp);
            }
            DeleteDC(hDC);
        }

    }
    else {
        ListView_SetExtendedListViewStyle(hList, LVS_EX_CHECKBOXES 
            /*| LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT*/);
    }

    ListView_SetCallbackMask(hList, LVIS_STATEIMAGEMASK);

    HWND hDlg = GetParent(hWnd);

    // get the column text from the dummy static ctrl
    // delete it and move the top edge of the list to cover its place
    HWND hDummy = GetDlgItem(hDlg, IDC_DUMMY);
    tstring label = ef::Win::WndH(hDummy).getText();
    RECT rc;
    GetWindowRect(hDummy, &rc);
    DestroyWindow(hDummy);  hDummy = 0;
    ScreenToClient(hDlg, (POINT*)&rc);  // only top needed
    int top = rc.top;

    GetWindowRect(hList, &rc);
    MapWindowPoints(0, hDlg, (POINT*)&rc, 2);
    rc.top = top;
    MoveWindow(hList, rc, true);

    // we'll set the column size to client width
    GetClientRect(hList, &rc);    

    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
    lvc.fmt  = LVCFMT_LEFT;
    lvc.cx   = rc.right - rc.left;
    lvc.pszText = const_cast<tchar*>(label.c_str());
    ListView_InsertColumn(hList, 0, LPARAM(&lvc));
}


void RulesList::term()
{
    if (ef::Win::WndH(hList).getStyle() & LVS_SHAREIMAGELISTS)
        ImageList_Destroy(ilChecks);

    if (emuChk)
        SetWindowLong(hList, GWL_WNDPROC, LONG(orgProc));

    hList = 0;
}


// set all the rules to the list
//
void RulesList::setAll(const AutoPinRules& rules)
{
    for (int n = 0; n < int(rules.size()); ++n) {
        int i = addNew();
        if (i < 0)
            continue;
        AutoPinRule* rule = getData(i);
        if (!rule)
            continue;
        *rule = rules[n];
    }
}


// get all the rules from the list
//
void RulesList::getAll(AutoPinRules& rules)
{
    rules.clear();

    int cnt = ListView_GetItemCount(hList);
    for (int n = 0; n < cnt; ++n) {
        const AutoPinRule* rule = getData(n);
        if (!rule)
            continue;
        rules.push_back(*rule);
    }
}


void RulesList::remove(std::vector<int> indices)
{
    for (int n = indices.size()-1; n >= 0; --n)
        ListView_DeleteItem(hList, indices[n]);
}


// add a new, empty list item (with a valid data object)
//
int RulesList::addNew()
{
    AutoPinRule* rule = new AutoPinRule;

    LVITEM lvi;
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = ListView_GetItemCount(hList);
    lvi.iSubItem = 0;
    lvi.pszText = LPSTR_TEXTCALLBACK;
    lvi.lParam = LPARAM(rule);
    int i = ListView_InsertItem(hList, &lvi);
    if (i < 0)
        delete rule;

    return i;
}


// get the data object of an item
//
AutoPinRule* RulesList::getData(int i) const
{
    LVITEM lvi;
    lvi.mask = LVIF_PARAM;
    lvi.iItem = i;
    lvi.iSubItem = 0;
    return ListView_GetItem(hList, &lvi) ? (AutoPinRule*)lvi.lParam : 0;
}


// set the data object of an item
//
bool RulesList::setData(int i, AutoPinRule* rule)
{
    LVITEM lvi;
    lvi.mask = LVIF_PARAM;
    lvi.iItem = i;
    lvi.iSubItem = 0;
    lvi.lParam = LPARAM(rule);
    return !!ListView_SetItem(hList, &lvi);
}


// get a rule from the list
//
bool RulesList::getRule(int i, AutoPinRule& rule) const
{
    const AutoPinRule* r = getData(i);
    if (!r)
        return false;
    rule = *r;
    return true;
}


// set a rule to the list
//
bool RulesList::setRule(int i, const AutoPinRule& rule)
{
    AutoPinRule* r = getData(i);
    if (!r)
        return false;
    *r = rule;
    ListView_Update(hList, i);
    // use this to update the focus rect size
    ListView_SetItemText(hList, i, 0, LPSTR_TEXTCALLBACK);
    return true;
}


// select (highlight) an item and de-select the rest
//
void RulesList::selectSingleRule(int i)
{
    int cnt = ListView_GetItemCount(hList);
    for (int n = 0; n < cnt; ++n)
        ListView_SetItemState(hList, n, n==i ? LVIS_SELECTED : 0, LVIS_SELECTED);
}


// get index of first selected item
// (or -1 if none are selected)
//
int RulesList::getFirstSel() const
{
    int cnt = ListView_GetItemCount(hList);
    for (int n = 0; n < cnt; ++n) {
        UINT state = ListView_GetItemState(hList, n, LVIS_SELECTED);
        if (state & LVIS_SELECTED)
            return n;
    }
    return -1;
}


// get indices of all selected items
//
std::vector<int> RulesList::getSel() const
{
    std::vector<int> ret;
    int cnt = ListView_GetItemCount(hList);
    for (int n = 0; n < cnt; ++n) {
        UINT state = ListView_GetItemState(hList, n, LVIS_SELECTED);
        if (state & LVIS_SELECTED)
            ret.push_back(n);
    }
    return ret;
}


void RulesList::setItemSel(int i, bool sel)
{
    UINT state = ListView_GetItemState(hList, i, LVIS_SELECTED);
    bool curSel = (state & LVIS_SELECTED) != 0;
    if (curSel != sel)
        ListView_SetItemState(hList, i, sel ? LVIS_SELECTED : 0, LVIS_SELECTED);
}


bool RulesList::getItemSel(int i) const
{
    UINT state = ListView_GetItemState(hList, i, LVIS_SELECTED);
    return (state & LVIS_SELECTED) != 0;
}


void RulesList::moveSelUp()
{
    int cnt = ListView_GetItemCount(hList);
    std::vector<int> sel = getSel();
    // nop if:
    // - nothing's selected
    // - everything's selected
    // - the first item is selected
    if (!sel.size() || sel.size() == cnt || sel.front() == 0)
        return;

    // swap each selected item with the one before it
    for (int n = 0; n < int(sel.size()); ++n) {
        int i = sel[n];
        // move data
        AutoPinRule* tmp = getData(i-1);
        setData(i-1, getData(i));
        setData(i,   tmp);
        // move selection
        setItemSel(i-1, true);
        setItemSel(i,   false);
        // notify for text change
        ListView_SetItemText(hList, i-1, 0, LPSTR_TEXTCALLBACK);
        ListView_SetItemText(hList, i, 0, LPSTR_TEXTCALLBACK);
    }

}


void RulesList::moveSelDown()
{
    int cnt = ListView_GetItemCount(hList);
    std::vector<int> sel = getSel();
    // nop if:
    // - nothing's selected
    // - everything's selected
    // - the last item is selected
    if (!sel.size() || sel.size() == cnt || sel.back() == cnt-1)
        return;

    // swap each selected item with the one after it
    for (int n = sel.size()-1; n >= 0 ; --n) {
        int i = sel[n];
        // move data
        AutoPinRule* tmp = getData(i+1);
        setData(i+1, getData(i));
        setData(i,   tmp);
        // move selection
        setItemSel(i+1, true);
        setItemSel(i,   false);
        // notify for text change
        ListView_SetItemText(hList, i+1, 0, LPSTR_TEXTCALLBACK);
        ListView_SetItemText(hList, i, 0, LPSTR_TEXTCALLBACK);
    }

}



static RulesList list;



// update controls state, depending on current selection
//
void UIUpdate(HWND hWnd)
{
    int cnt = ListView_GetItemCount(list);
    int selCnt = ListView_GetSelectedCount(list);
    EnableWindow(GetDlgItem(hWnd, IDC_EDIT),   selCnt == 1);
    EnableWindow(GetDlgItem(hWnd, IDC_REMOVE), selCnt > 0);
    EnableWindow(GetDlgItem(hWnd, IDC_UP),     selCnt > 0 && 
        !list.getItemSel(0));
    EnableWindow(GetDlgItem(hWnd, IDC_DOWN),   selCnt > 0 && 
        !list.getItemSel(cnt-1));
}


static bool CmAutoPinOn(HWND hWnd)
{
    bool b = IsDlgButtonChecked(hWnd, IDC_AUTOPIN_ON) == BST_CHECKED;
    EnableGroup(hWnd, IDC_AUTOPIN_GROUP, b);

    // if turned on, disable some buttons
    if (b) UIUpdate(hWnd);

    PSChanged(hWnd);
    return true;
}


static bool EvInitDialog(HWND hWnd, HWND /*hFocus*/, LPARAM lParam)
{
    // must have a valid data ptr
    if (!lParam) {
        EndDialog(hWnd, IDCANCEL);
        return false;
    }

    // save the data ptr
    PROPSHEETPAGE& psp = *reinterpret_cast<PROPSHEETPAGE*>(lParam);
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(psp.lParam)->opt;
    SetWindowLong(hWnd, DWL_USER, psp.lParam);


    CheckDlgButton(hWnd, IDC_AUTOPIN_ON, opt.autoPinOn);
    CmAutoPinOn(hWnd);  // simulate check to setup other ctrls

    SendDlgItemMessage(hWnd, IDC_RULE_DELAY_UD, UDM_SETRANGE, 0, 
        MAKELONG(opt.autoPinDelay.maxV,opt.autoPinDelay.minV));
    SendDlgItemMessage(hWnd, IDC_RULE_DELAY_UD, UDM_SETPOS, 0, 
        MAKELONG(opt.autoPinDelay.value,0));


    list.init(GetDlgItem(hWnd, IDC_LIST));
    list.setAll(opt.autoPinRules);

    UIUpdate(hWnd);

    return false;
}


static void Apply(HWND hWnd, WindowCreationMonitor& winCreMon)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;

    bool autoPinOn = IsDlgButtonChecked(hWnd, IDC_AUTOPIN_ON) == BST_CHECKED;
    if (opt.autoPinOn != autoPinOn) {
        if (!autoPinOn)
            winCreMon.term();
        else if (!winCreMon.init(app.hMainWnd, App::WM_QUEUEWINDOW)) {
            Error(hWnd, ResStr(IDS_ERR_HOOKDLL));
            autoPinOn = false;
        }
    }

    opt.autoPinDelay.value = opt.autoPinDelay.getUI(hWnd, IDC_RULE_DELAY);

    if (opt.autoPinOn = autoPinOn)
        SetTimer(app.hMainWnd, App::TIMERID_AUTOPIN, opt.autoPinDelay.value, 0);
    else
        KillTimer(app.hMainWnd, App::TIMERID_AUTOPIN);

    list.getAll(opt.autoPinRules);
}


static bool Validate(HWND hWnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;
    return opt.autoPinDelay.validateUI(hWnd, IDC_RULE_DELAY);
}


BOOL CALLBACK OptAutoPinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            return EvInitDialog(hWnd, HWND(wParam), lParam);
        }
        case WM_DESTROY: {
            list.term();
            return true;
        }
        case WM_NOTIFY: {
            NMHDR nmhdr = *reinterpret_cast<NMHDR*>(lParam);
            switch (nmhdr.code) {
            case PSN_SETACTIVE: {
                HWND hTab = PropSheet_GetTabControl(nmhdr.hwndFrom);
                app.optionPage = SendMessage(hTab, TCM_GETCURSEL, 0, 0);
                SetWindowLong(hWnd, DWL_MSGRESULT, 0);
                return true;
            }
            case PSN_KILLACTIVE: {
                SetWindowLong(hWnd, DWL_MSGRESULT, !Validate(hWnd));
                return true;
            }
            case PSN_APPLY: {
                WindowCreationMonitor& winCreMon = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->winCreMon;
                Apply(hWnd, winCreMon);
                return true;
            }
            case PSN_HELP: {
                app.help.show(hWnd, _T("::\\optautopin.htm"));
                return true;
            }
            case UDN_DELTAPOS: {
                if (wParam == IDC_RULE_DELAY_UD) {
                    NM_UPDOWN& nmud = *(NM_UPDOWN*)lParam;
                    Options& opt = 
                        reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(hWnd, DWL_USER))->opt;
                    nmud.iDelta *= opt.autoPinDelay.step;
                    SetWindowLong(hWnd, DWL_MSGRESULT, FALSE);   // allow change
                    return true;
                }
                else
                    return false;
            }
            case LVN_ITEMCHANGED: {
                NMLISTVIEW& nmlv = *(NMLISTVIEW*)lParam;
                if (nmlv.uChanged == LVIF_STATE) UIUpdate(hWnd);
                return true;
            }
            case NM_CLICK: {
                list.checkClick(hWnd);
                return true;
            }
            case NM_DBLCLK: {
                int i = ListView_GetNextItem(list, -1, LVNI_FOCUSED);
                if (i != -1) {
                    // If an item has been focused and then we click on the empty area
                    // below the last item, we'll still get NM_DBLCLK.
                    // So, we must also check if the focused item is also selected.
                    if (list.getItemSel(i)) {
                        // we can't just send ourselves a IDC_EDIT cmd
                        // because there may be more selected items
                        // (if the CTRL is held)
                        AutoPinRule rule;
                        if (list.getRule(i, rule)) {
                            int res = LocalizedDialogBoxParam(IDD_EDIT_AUTOPIN_RULE, 
                                hWnd, APEditRuleDlgProc, LPARAM(&rule));
                            if (res == IDOK) {
                                list.setRule(i, rule);
                                PSChanged(hWnd);
                            }
                        }
                    }
                }
                return true;
            }
            case LVN_DELETEITEM: {
                NMLISTVIEW& nmlv = *(NMLISTVIEW*)lParam;
                LVITEM lvi;
                lvi.mask = TVIF_PARAM;
                lvi.iItem = nmlv.iItem;
                lvi.iSubItem = 0;
                if (ListView_GetItem(nmlv.hdr.hwndFrom, &lvi))
                    delete (AutoPinRule*)lvi.lParam;
                return true;
            }
            case LVN_GETDISPINFO: {
                NMLVDISPINFO& di = *(NMLVDISPINFO*)lParam;
                const AutoPinRule* rule = (AutoPinRule*)di.item.lParam;
                if (!rule)
                    return false;
                // fill in the requested fields
                if (di.item.mask & LVIF_TEXT) {
                    //di.item.cchTextMax is 260..264 bytes
                    tstring s = rule->descr;
                    if (int(s.length()) > di.item.cchTextMax-1)
                        s = s.substr(0, di.item.cchTextMax-1);
                    _tcscpy_s(di.item.pszText, di.item.cchTextMax, s.c_str());
                }
                if (di.item.mask & LVIF_STATE) {
                    di.item.stateMask = LVIS_STATEIMAGEMASK;
                    di.item.state = INDEXTOSTATEIMAGEMASK(rule->enabled ? 2 : 1);
                }
                return true;
            }
            case LVN_ITEMCHANGING: {
                NMLISTVIEW& nm = *(NMLISTVIEW*)lParam;
                AutoPinRule* rule = (AutoPinRule*)nm.lParam;
                if (nm.uChanged & LVIF_STATE && nm.uNewState & LVIS_STATEIMAGEMASK) {
                    rule->enabled = !rule->enabled;
                    ListView_Update(nm.hdr.hwndFrom, nm.iItem);
                }
                return true;
            }
            default:
                return false;
            }
        }
        case WM_HELP: {
            app.help.show(hWnd, _T("::\\optautopin.htm"));
            return true;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam), code = HIWORD(wParam);
            switch (id) {
                case IDC_AUTOPIN_ON:    return CmAutoPinOn(hWnd);
                case IDC_ADD: {
                    AutoPinRule rule;
                    int res = LocalizedDialogBoxParam(IDD_EDIT_AUTOPIN_RULE, 
                        hWnd, APEditRuleDlgProc, LPARAM(&rule));
                    if (res == IDOK) {
                        int i = list.addNew();
                        if (i >= 0 && list.setRule(i, rule))
                            list.selectSingleRule(i);
                        PSChanged(hWnd);
                    }
                    return true;
                }
                case IDC_EDIT: {
                    AutoPinRule rule;
                    int i = list.getFirstSel();
                    if (i >= 0 && list.getRule(i, rule)) {
                        int res = LocalizedDialogBoxParam(IDD_EDIT_AUTOPIN_RULE, 
                            hWnd, APEditRuleDlgProc, LPARAM(&rule));
                        if (res == IDOK) {
                            list.setRule(i, rule);
                            PSChanged(hWnd);
                        }
                    }
                    return true;
                }
                case IDC_REMOVE: {
                    list.remove(list.getSel());
                    PSChanged(hWnd);
                    return true;
                }
                case IDC_UP: {
                    list.moveSelUp();
                    PSChanged(hWnd);
                    return true;
                }
                case IDC_DOWN: {
                    list.moveSelDown();
                    PSChanged(hWnd);
                    return true;
                }
                case IDC_RULE_DELAY:
                    if (code == EN_CHANGE)
                        PSChanged(hWnd);
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }

}
