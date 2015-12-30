#include "stdafx.h"
#include "util.h"
#include "pinshape.h"
#include "resource.h"


PinShape::PinShape() : hBmp(0), hRgn(0) {
    sz.cx = sz.cy = 1;
}


PinShape::~PinShape() {
    DeleteObject(hBmp);
    DeleteObject(hRgn);
}


bool PinShape::initImage(COLORREF clr) {
    if (hBmp) {
        DeleteObject(hBmp);
        hBmp = 0;
    }

    COLORREF clrMap[][2] = {
        { CLR_WHITE, Light(clr) }, 
        { CLR_LGRAY, clr        }, 
        { CLR_DGRAY, Dark(clr)  }
    };
    return (hBmp = LoadBitmap(app.hInst, MAKEINTRESOURCE(IDB_PIN)))
        && remapBmpColors(hBmp, clrMap, ARRSIZE(clrMap));
}


bool PinShape::initShape() {
    if (hRgn) {
        DeleteObject(hRgn);
        hRgn = 0;
        sz.cx = sz.cy = 1;
    }

    if (HBITMAP hBmp = LoadBitmap(app.hInst, MAKEINTRESOURCE(IDB_PIN))) {
        if (!getBmpSize(hBmp, sz))
            sz.cx = sz.cy = 1;
        hRgn = ef::Win::RgnH::create(hBmp, RGB(255,0,255));
        DeleteObject(hBmp);
    }

    return hRgn;
}
