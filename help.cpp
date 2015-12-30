#include "stdafx.h"
#include "util.h"
#include "help.h"
#include "resource.h"


bool Help::init(HINSTANCE hInst, const tstring& fname) 
{
    hlpFile = tstring();

    tstring path = ef::dirSpec(ef::Win::getModulePath(hInst));
    if (path.empty())
        return false;

#if defined(DEBUG) || defined(_DEBUG)
    path += _T("..\\help\\");
#endif

    hlpFile = path + fname;
    return true;
}


// if 'topic' is empty, use just the filename (goes to default topic)
// otherwise, append it to file spec
//   topic format is:  "::\someTopic.html"  or
//                     "::\someTopic.html#namedAnchor"
HWND Help::show(HWND hWnd, const tstring& topic /*= tstring()*/)
{
    if (hlpFile.empty()) {
        Error(hWnd, ResStr(IDS_ERR_HELPMISSING));
        return 0;
    }

    return ef::Win::HTMLHelp::obj().dispTopic(hWnd, (hlpFile + topic).c_str());
}
