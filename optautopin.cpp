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
    HCURSOR curs;

    TargetWndData(HWND wnd)
    {
        orgProc = WNDPROC(GetWindowLong(wnd, GWL_WNDPROC));
        if (!orgProc)
            throw 0;  // TODO: use an exception
        SetWindowLong(wnd, GWL_USERDATA, LONG(this));
        SetWindowLong(wnd, GWL_WNDPROC, LONG(TargetWndProc));

        cursShowing = true;
        curs = LoadCursor(app.inst, MAKEINTRESOURCE(IDC_BULLSEYE));
    }

    ~TargetWndData() {}

private:
    TargetWndData(const TargetWndData&);
    TargetWndData& operator=(const TargetWndData&);

};


BOOL CALLBACK TargetWndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    TargetWndData& data = *(TargetWndData*)GetWindowLong(wnd, GWL_USERDATA);

    switch (msg) {
        case WM_NCDESTROY: {
            WNDPROC orgProc = data.orgProc;
            delete &data;
            return CallWindowProc(orgProc, wnd, msg, wparam, lparam);
        }
        case WM_PAINT: {
            LRESULT ret = CallWindowProc(data.orgProc, wnd, msg, wparam, lparam);
            HDC dc = GetDC(wnd);
            if (dc) {
                if (data.cursShowing) {
                    RECT rc;
                    GetClientRect(wnd, &rc);
                    int x = (rc.right  - GetSystemMetrics(SM_CXCURSOR)) / 2;
                    int y = (rc.bottom - GetSystemMetrics(SM_CYCURSOR)) / 2;          
                    DrawIcon(dc, x+1, y+1, data.curs);
                }
                ReleaseDC(wnd, dc);
            }
            return ret;
        }
        case WM_USER: {
            data.cursShowing = wparam != 0;
            InvalidateRect(wnd, 0, true);
            UpdateWindow(wnd);
            return 0;
        }
    }

    return CallWindowProc(data.orgProc, wnd, msg, wparam, lparam);
}


void MarkWnd(HWND wnd, bool mode)
{
    const int blinkDelay = 50;  // msec
    // thickness of highlight border
    const int width = 3;
    // first val can vary; second should be zero
    const int flashes = mode ? 1 : 0;
    // amount to deflate if wnd is maximized, to make the highlight visible
    const int zoomFix = IsZoomed(wnd) ? GetSystemMetrics(SM_CXFRAME) : 0;

    HDC dc = GetWindowDC(wnd);
    if (dc) {
        int orgRop2 = SetROP2(dc, R2_XORPEN);

        HRGN rgn = CreateRectRgn(0,0,0,0);
        bool hasRgn = GetWindowRgn(wnd, rgn) != ERROR;
        const int loops = flashes*2+1;

        if (hasRgn) {
            for (int m = 0; m < loops; ++m) {
                FrameRgn(dc, rgn, HBRUSH(GetStockObject(WHITE_BRUSH)), width, width);
                GdiFlush();
                if (mode && m < loops-1)
                    Sleep(blinkDelay);
            }
        }
        else {
            RECT rc;
            GetWindowRect(wnd, &rc);    
            OffsetRect(&rc, -rc.left, -rc.top);
            InflateRect(&rc, -zoomFix, -zoomFix);

            HGDIOBJ orgPen = SelectObject(dc, GetStockObject(WHITE_PEN));
            HGDIOBJ orgBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

            RECT tmp;
            for (int m = 0; m < loops; ++m) {
                CopyRect(&tmp, &rc);
                for (int n = 0; n < width; ++n) {
                    Rectangle(dc, tmp.left, tmp.top, tmp.right, tmp.bottom);
                    InflateRect(&tmp, -1, -1);
                }
                GdiFlush();
                if (mode && m < loops-1)
                    Sleep(blinkDelay);
            }
            SelectObject(dc, orgBrush);
            SelectObject(dc, orgPen);
        }

        SetROP2(dc, orgRop2);
        ReleaseDC(wnd, dc);
        DeleteObject(rgn);
    }

}


BOOL CALLBACK APEditRuleDlgProc(
                                HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static int tracking = 0;    // 0: none, 1: title, 2: class
    static HWND hitWnd = 0;
    static SIZE minSize;
    static SIZE listMarg;
    static int  btnMarg;
    static HWND tooltip = 0;

    switch (msg) {
        case WM_INITDIALOG: {
            if (!lparam) {
                EndDialog(wnd, -1);
                return true;
            }
            SetWindowLong(wnd, DWL_USER, lparam);
            AutoPinRule& rule = *reinterpret_cast<AutoPinRule*>(lparam);

            hitWnd = 0;

            SetDlgItemText(wnd, IDC_DESCR, rule.descr.c_str());
            SetDlgItemText(wnd, IDC_TITLE, rule.ttl.c_str());
            SetDlgItemText(wnd, IDC_CLASS, rule.cls.c_str());

            try { 
                new TargetWndData(GetDlgItem(wnd, IDC_TTLPICK));
                new TargetWndData(GetDlgItem(wnd, IDC_CLSPICK));
            } 
            catch (int) {
                // oh, well...
            }

            // create tooltip for target icons
            tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, L"",
                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                wnd, 0, app.inst, 0);

            TOOLINFO ti;
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
            ti.hwnd = wnd;
            ti.hinst = app.inst;
            ti.lParam = 0;

            ti.uId = UINT_PTR(GetDlgItem(wnd, IDC_TTLPICK));
            ti.lpszText = L"Drag icon to get window title";
            SendMessage(tooltip, TTM_ADDTOOL, 0, LPARAM(&ti));

            ti.uId = UINT_PTR(GetDlgItem(wnd, IDC_CLSPICK));
            ti.lpszText = L"Drag icon to get window class";
            SendMessage(tooltip, TTM_ADDTOOL, 0, LPARAM(&ti));

            return true;
        }
        case WM_DESTROY: {
            DestroyWindow(tooltip);
            return true;
        }
        case WM_MOUSEMOVE: {
            if (tracking) {
                POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(wnd, &pt);
                HWND newWnd = GetNonChildParent(WindowFromPoint(pt));
                if (hitWnd != newWnd && !IsProgManWnd(newWnd) && !IsTaskBar(newWnd)) {
                    if (hitWnd)
                        MarkWnd(hitWnd, false);

                    hitWnd = newWnd;

                    if (tracking == 1) {
                        ef::Win::WndH w = GetDlgItem(wnd, IDC_TITLE);
                        w.setText(ef::Win::WndH(hitWnd).getText());
                        w.update();
                    }
                    else {
                        ef::Win::WndH w = GetDlgItem(wnd, IDC_CLASS);
                        w.setText(ef::Win::WndH(hitWnd).getClassName());
                        w.update();
                    }

                    MarkWnd(hitWnd, true);

                }
            }
            return true;
        }
        case WM_LBUTTONUP: {
            if (tracking) {
                if (hitWnd)
                    MarkWnd(hitWnd, false);
                ReleaseCapture();
                SendDlgItemMessage(wnd, tracking == 1 ? IDC_TTLPICK : IDC_CLSPICK, 
                    WM_USER, true, 0);
                tracking = 0;
                hitWnd = 0;
            }
            return true;
        }
        case WM_HELP: {
            app.help.show(wnd, L"::\\editruledlg.htm");
            return true;
        }
        case WM_COMMAND: {
            int id = LOWORD(wparam);
            switch (id) {
                case IDOK: {
                    AutoPinRule& rule = 
                        *reinterpret_cast<AutoPinRule*>(GetWindowLong(wnd, DWL_USER));
                    WCHAR buf[256];
                    GetDlgItemText(wnd, IDC_DESCR, buf, sizeof(buf));  rule.descr = buf;
                    GetDlgItemText(wnd, IDC_TITLE, buf, sizeof(buf));  rule.ttl = buf;
                    GetDlgItemText(wnd, IDC_CLASS, buf, sizeof(buf));  rule.cls = buf;
                    // allow empty strings (at least title can be empty...)
                    //if (rule.ttl.empty()) rule.ttl = "*";
                    //if (rule.cls.empty()) rule.cls = "*";          
                }
                case IDCANCEL:
                    EndDialog(wnd, id);
                    return true;
                case IDHELP:
                    app.help.show(wnd, L"::\\editruledlg.htm");
                    return true;
                case IDC_TTLPICK:
                case IDC_CLSPICK: {
                    if (HIWORD(wparam) == STN_CLICKED) {
                        SendDlgItemMessage(wnd, id, WM_USER, false, 0);
                        SetCapture(wnd);
                        SetCursor(LoadCursor(app.inst, MAKEINTRESOURCE(IDC_BULLSEYE)));
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

    void init(HWND wnd);
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
        return list;
    }

    void checkClick(HWND page)
    {
        DWORD xy = GetMessagePos();
        LVHITTESTINFO hti;
        hti.pt.x = GET_X_LPARAM(xy);
        hti.pt.y = GET_Y_LPARAM(xy);
        ScreenToClient(list, &hti.pt);
        int i = ListView_HitTest(list, &hti);
        if (i >= 0 && hti.flags & LVHT_ONITEMSTATEICON) {
            if (emuChk) toggleItem(i);
            PSChanged(page);
        }
    }

    void toggleItem(int i)
    {
        AutoPinRule* rule = getData(i);
        if (rule) {
            rule->enabled = !rule->enabled;
            ListView_Update(list, i);
        }
    }

    static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        RulesList* list = (RulesList*)GetWindowLong(wnd, GWL_USERDATA);
        if (!list || !list->orgProc)
            return DefWindowProc(wnd, msg, wparam, lparam);

        if (msg == WM_CHAR && wparam == VK_SPACE) {
            RulesList* list = (RulesList*)GetWindowLong(wnd, GWL_USERDATA);
            if (list)
                list->onSpace();
            return 0;
        }

        return CallWindowProc(list->orgProc, wnd, msg, wparam, lparam);    
    }

    void onSpace()
    {
        int i = ListView_GetNextItem(list, -1, LVNI_FOCUSED);
        if (i != -1)
            toggleItem(i);
    }

protected:
    HWND list;
    HIMAGELIST ilChecks;
    bool emuChk;
    WNDPROC orgProc;

    AutoPinRule* getData(int i) const;
    bool setData(int i, AutoPinRule* rule);

};


RulesList::RulesList() : list(0), emuChk(ef::Win::getDllVer(L"comctl32.dll") < ef::Win::packVer(4,70))
{
    //emuChk = true;
}


void RulesList::init(HWND wnd)
{
    list = wnd;

    // LVS_EX_CHECKBOXES is only available in comctl32 4.70+
    // (like LVS_EX_GRIDLINES and LVS_EX_FULLROWSELECT),
    // so we emulate it on earlier versions

    if (emuChk) {
        orgProc = WNDPROC(SetWindowLong(list, GWL_WNDPROC, LONG(WndProc)));
        SetWindowLong(list, GWL_USERDATA, LONG(this));

        HDC dc = CreateCompatibleDC(0);
        if (dc) {
            HBITMAP bmp = CreateCompatibleBitmap(dc, 32, 16);
            if (bmp) {
                HGDIOBJ orgBmp = SelectObject(dc, bmp);
                if (orgBmp) {
                    HFONT fnt = CreateFont(16, 0, 0,0,FW_NORMAL, false, false, false, 
                        SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                        DEFAULT_PITCH | FF_DONTCARE, L"Marlett");
                    if (fnt) {
                        HGDIOBJ orgFnt = SelectObject(dc, fnt);
                        if (orgFnt) {
                            //ab
                            RECT rc = {0,0,16,16};
                            for (int n = 0; n < 2; ++n) {
                                FillRect(dc, &rc, HBRUSH(GetStockObject(WHITE_BRUSH)));
                                SelectObject(dc, GetStockObject(NULL_BRUSH));

                                RECT rc2;
                                CopyRect(&rc2, &rc);

                                InflateRect(&rc2, -1, -1);
                                Rectangle(dc, rc2);
                                InflateRect(&rc2, -1, -1);
                                Rectangle(dc, rc2);
                                InflateRect(&rc2, -1, -1);

                                if (n == 1) {
                                    SetBkMode(dc, TRANSPARENT);
                                    DrawText(dc, L"a", 1, &rc, DT_NOPREFIX | DT_CENTER);
                                }

                                OffsetRect(&rc, 16, 0);
                            }

                            SelectObject(dc, orgFnt);
                            DeleteObject(fnt);
                        }
                    }
                    SelectObject(dc, orgBmp);

                    ilChecks = ImageList_Create(16, 16, ILC_COLOR4 | ILC_MASK, 0, 1);
                    ImageList_AddMasked(ilChecks, bmp, RGB(255,255,255));
                    ListView_SetImageList(list, ilChecks, LVSIL_STATE);
                }
                DeleteObject(bmp);
            }
            DeleteDC(dc);
        }

    }
    else {
        ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES);
    }

    ListView_SetCallbackMask(list, LVIS_STATEIMAGEMASK);

    HWND dlg = GetParent(wnd);

    // get the column text from the dummy static ctrl
    // delete it and move the top edge of the list to cover its place
    HWND dummy = GetDlgItem(dlg, IDC_DUMMY);
    std::wstring label = ef::Win::WndH(dummy).getText();
    RECT rc;
    GetWindowRect(dummy, &rc);
    DestroyWindow(dummy);  dummy = 0;
    ScreenToClient(dlg, (POINT*)&rc);  // only top needed
    int top = rc.top;

    GetWindowRect(list, &rc);
    MapWindowPoints(0, dlg, (POINT*)&rc, 2);
    rc.top = top;
    MoveWindow(list, rc, true);

    // we'll set the column size to client width
    GetClientRect(list, &rc);    

    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
    lvc.fmt  = LVCFMT_LEFT;
    lvc.cx   = rc.right - rc.left;
    lvc.pszText = const_cast<LPWSTR>(label.c_str());
    ListView_InsertColumn(list, 0, LPARAM(&lvc));
}


void RulesList::term()
{
    if (ef::Win::WndH(list).getStyle() & LVS_SHAREIMAGELISTS)
        ImageList_Destroy(ilChecks);

    if (emuChk)
        SetWindowLong(list, GWL_WNDPROC, LONG(orgProc));

    list = 0;
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

    int cnt = ListView_GetItemCount(list);
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
        ListView_DeleteItem(list, indices[n]);
}


// add a new, empty list item (with a valid data object)
//
int RulesList::addNew()
{
    AutoPinRule* rule = new AutoPinRule;

    LVITEM lvi;
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = ListView_GetItemCount(list);
    lvi.iSubItem = 0;
    lvi.pszText = LPSTR_TEXTCALLBACK;
    lvi.lParam = LPARAM(rule);
    int i = ListView_InsertItem(list, &lvi);
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
    return ListView_GetItem(list, &lvi) ? (AutoPinRule*)lvi.lParam : 0;
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
    return !!ListView_SetItem(list, &lvi);
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
    ListView_Update(list, i);
    // use this to update the focus rect size
    ListView_SetItemText(list, i, 0, LPSTR_TEXTCALLBACK);
    return true;
}


// select (highlight) an item and de-select the rest
//
void RulesList::selectSingleRule(int i)
{
    int cnt = ListView_GetItemCount(list);
    for (int n = 0; n < cnt; ++n)
        ListView_SetItemState(list, n, n==i ? LVIS_SELECTED : 0, LVIS_SELECTED);
}


// get index of first selected item
// (or -1 if none are selected)
//
int RulesList::getFirstSel() const
{
    int cnt = ListView_GetItemCount(list);
    for (int n = 0; n < cnt; ++n) {
        UINT state = ListView_GetItemState(list, n, LVIS_SELECTED);
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
    int cnt = ListView_GetItemCount(list);
    for (int n = 0; n < cnt; ++n) {
        UINT state = ListView_GetItemState(list, n, LVIS_SELECTED);
        if (state & LVIS_SELECTED)
            ret.push_back(n);
    }
    return ret;
}


void RulesList::setItemSel(int i, bool sel)
{
    UINT state = ListView_GetItemState(list, i, LVIS_SELECTED);
    bool curSel = (state & LVIS_SELECTED) != 0;
    if (curSel != sel)
        ListView_SetItemState(list, i, sel ? LVIS_SELECTED : 0, LVIS_SELECTED);
}


bool RulesList::getItemSel(int i) const
{
    UINT state = ListView_GetItemState(list, i, LVIS_SELECTED);
    return (state & LVIS_SELECTED) != 0;
}


void RulesList::moveSelUp()
{
    int cnt = ListView_GetItemCount(list);
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
        ListView_SetItemText(list, i-1, 0, LPSTR_TEXTCALLBACK);
        ListView_SetItemText(list, i, 0, LPSTR_TEXTCALLBACK);
    }

}


void RulesList::moveSelDown()
{
    int cnt = ListView_GetItemCount(list);
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
        ListView_SetItemText(list, i+1, 0, LPSTR_TEXTCALLBACK);
        ListView_SetItemText(list, i, 0, LPSTR_TEXTCALLBACK);
    }

}


static RulesList rlist;


// update controls state, depending on current selection
//
void UIUpdate(HWND wnd)
{
    int cnt = ListView_GetItemCount(rlist);
    int selCnt = ListView_GetSelectedCount(rlist);
    EnableWindow(GetDlgItem(wnd, IDC_EDIT),   selCnt == 1);
    EnableWindow(GetDlgItem(wnd, IDC_REMOVE), selCnt > 0);
    EnableWindow(GetDlgItem(wnd, IDC_UP),     selCnt > 0 && 
        !rlist.getItemSel(0));
    EnableWindow(GetDlgItem(wnd, IDC_DOWN),   selCnt > 0 && 
        !rlist.getItemSel(cnt-1));
}


static bool CmAutoPinOn(HWND wnd)
{
    bool b = IsDlgButtonChecked(wnd, IDC_AUTOPIN_ON) == BST_CHECKED;
    EnableGroup(wnd, IDC_AUTOPIN_GROUP, b);

    // if turned on, disable some buttons
    if (b) UIUpdate(wnd);

    PSChanged(wnd);
    return true;
}


static bool EvInitDialog(HWND wnd, HWND focus, LPARAM param)
{
    // must have a valid data ptr
    if (!param) {
        EndDialog(wnd, IDCANCEL);
        return false;
    }

    // save the data ptr
    PROPSHEETPAGE& psp = *reinterpret_cast<PROPSHEETPAGE*>(param);
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(psp.lParam)->opt;
    SetWindowLong(wnd, DWL_USER, psp.lParam);


    CheckDlgButton(wnd, IDC_AUTOPIN_ON, opt.autoPinOn);
    CmAutoPinOn(wnd);  // simulate check to setup other ctrls

    SendDlgItemMessage(wnd, IDC_RULE_DELAY_UD, UDM_SETRANGE, 0, 
        MAKELONG(opt.autoPinDelay.maxV,opt.autoPinDelay.minV));
    SendDlgItemMessage(wnd, IDC_RULE_DELAY_UD, UDM_SETPOS, 0, 
        MAKELONG(opt.autoPinDelay.value,0));


    rlist.init(GetDlgItem(wnd, IDC_LIST));
    rlist.setAll(opt.autoPinRules);

    UIUpdate(wnd);

    return false;
}


static void Apply(HWND wnd, WindowCreationMonitor& winCreMon)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;

    bool autoPinOn = IsDlgButtonChecked(wnd, IDC_AUTOPIN_ON) == BST_CHECKED;
    if (opt.autoPinOn != autoPinOn) {
        if (!autoPinOn)
            winCreMon.term();
        else if (!winCreMon.init(app.mainWnd, App::WM_QUEUEWINDOW)) {
            Error(wnd, ResStr(IDS_ERR_HOOKDLL));
            autoPinOn = false;
        }
    }

    opt.autoPinDelay.value = opt.autoPinDelay.getUI(wnd, IDC_RULE_DELAY);

    if (opt.autoPinOn = autoPinOn)
        SetTimer(app.mainWnd, App::TIMERID_AUTOPIN, opt.autoPinDelay.value, 0);
    else
        KillTimer(app.mainWnd, App::TIMERID_AUTOPIN);

    rlist.getAll(opt.autoPinRules);
}


static bool Validate(HWND wnd)
{
    Options& opt = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;
    return opt.autoPinDelay.validateUI(wnd, IDC_RULE_DELAY);
}


BOOL CALLBACK OptAutoPinProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_INITDIALOG: {
            return EvInitDialog(wnd, HWND(wparam), lparam);
        }
        case WM_DESTROY: {
            rlist.term();
            return true;
        }
        case WM_NOTIFY: {
            NMHDR nmhdr = *reinterpret_cast<NMHDR*>(lparam);
            switch (nmhdr.code) {
            case PSN_SETACTIVE: {
                HWND tab = PropSheet_GetTabControl(nmhdr.hwndFrom);
                app.optionPage = SendMessage(tab, TCM_GETCURSEL, 0, 0);
                SetWindowLong(wnd, DWL_MSGRESULT, 0);
                return true;
            }
            case PSN_KILLACTIVE: {
                SetWindowLong(wnd, DWL_MSGRESULT, !Validate(wnd));
                return true;
            }
            case PSN_APPLY: {
                WindowCreationMonitor& winCreMon = reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->winCreMon;
                Apply(wnd, winCreMon);
                return true;
            }
            case PSN_HELP: {
                app.help.show(wnd, L"::\\optautopin.htm");
                return true;
            }
            case UDN_DELTAPOS: {
                if (wparam == IDC_RULE_DELAY_UD) {
                    NM_UPDOWN& nmud = *(NM_UPDOWN*)lparam;
                    Options& opt = 
                        reinterpret_cast<OptionsPropSheetData*>(GetWindowLong(wnd, DWL_USER))->opt;
                    nmud.iDelta *= opt.autoPinDelay.step;
                    SetWindowLong(wnd, DWL_MSGRESULT, FALSE);   // allow change
                    return true;
                }
                else
                    return false;
            }
            case LVN_ITEMCHANGED: {
                NMLISTVIEW& nmlv = *(NMLISTVIEW*)lparam;
                if (nmlv.uChanged == LVIF_STATE) UIUpdate(wnd);
                return true;
            }
            case NM_CLICK: {
                rlist.checkClick(wnd);
                return true;
            }
            case NM_DBLCLK: {
                int i = ListView_GetNextItem(rlist, -1, LVNI_FOCUSED);
                if (i != -1) {
                    // If an item has been focused and then we click on the empty area
                    // below the last item, we'll still get NM_DBLCLK.
                    // So, we must also check if the focused item is also selected.
                    if (rlist.getItemSel(i)) {
                        // we can't just send ourselves a IDC_EDIT cmd
                        // because there may be more selected items
                        // (if the CTRL is held)
                        AutoPinRule rule;
                        if (rlist.getRule(i, rule)) {
                            int res = LocalizedDialogBoxParam(IDD_EDIT_AUTOPIN_RULE, 
                                wnd, APEditRuleDlgProc, LPARAM(&rule));
                            if (res == IDOK) {
                                rlist.setRule(i, rule);
                                PSChanged(wnd);
                            }
                        }
                    }
                }
                return true;
            }
            case LVN_DELETEITEM: {
                NMLISTVIEW& nmlv = *(NMLISTVIEW*)lparam;
                LVITEM lvi;
                lvi.mask = TVIF_PARAM;
                lvi.iItem = nmlv.iItem;
                lvi.iSubItem = 0;
                if (ListView_GetItem(nmlv.hdr.hwndFrom, &lvi))
                    delete (AutoPinRule*)lvi.lParam;
                return true;
            }
            case LVN_GETDISPINFO: {
                NMLVDISPINFO& di = *(NMLVDISPINFO*)lparam;
                const AutoPinRule* rule = (AutoPinRule*)di.item.lParam;
                if (!rule)
                    return false;
                // fill in the requested fields
                if (di.item.mask & LVIF_TEXT) {
                    //di.item.cchTextMax is 260..264 bytes
                    std::wstring s = rule->descr;
                    if (int(s.length()) > di.item.cchTextMax-1)
                        s = s.substr(0, di.item.cchTextMax-1);
                    wcscpy_s(di.item.pszText, di.item.cchTextMax, s.c_str());
                }
                if (di.item.mask & LVIF_STATE) {
                    di.item.stateMask = LVIS_STATEIMAGEMASK;
                    di.item.state = INDEXTOSTATEIMAGEMASK(rule->enabled ? 2 : 1);
                }
                return true;
            }
            case LVN_ITEMCHANGING: {
                NMLISTVIEW& nm = *(NMLISTVIEW*)lparam;
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
            app.help.show(wnd, L"::\\optautopin.htm");
            return true;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wparam), code = HIWORD(wparam);
            switch (id) {
                case IDC_AUTOPIN_ON:    return CmAutoPinOn(wnd);
                case IDC_ADD: {
                    AutoPinRule rule;
                    int res = LocalizedDialogBoxParam(IDD_EDIT_AUTOPIN_RULE, 
                        wnd, APEditRuleDlgProc, LPARAM(&rule));
                    if (res == IDOK) {
                        int i = rlist.addNew();
                        if (i >= 0 && rlist.setRule(i, rule))
                            rlist.selectSingleRule(i);
                        PSChanged(wnd);
                    }
                    return true;
                }
                case IDC_EDIT: {
                    AutoPinRule rule;
                    int i = rlist.getFirstSel();
                    if (i >= 0 && rlist.getRule(i, rule)) {
                        int res = LocalizedDialogBoxParam(IDD_EDIT_AUTOPIN_RULE, 
                            wnd, APEditRuleDlgProc, LPARAM(&rule));
                        if (res == IDOK) {
                            rlist.setRule(i, rule);
                            PSChanged(wnd);
                        }
                    }
                    return true;
                }
                case IDC_REMOVE: {
                    rlist.remove(rlist.getSel());
                    PSChanged(wnd);
                    return true;
                }
                case IDC_UP: {
                    rlist.moveSelUp();
                    PSChanged(wnd);
                    return true;
                }
                case IDC_DOWN: {
                    rlist.moveSelDown();
                    PSChanged(wnd);
                    return true;
                }
                case IDC_RULE_DELAY:
                    if (code == EN_CHANGE)
                        PSChanged(wnd);
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }

}
