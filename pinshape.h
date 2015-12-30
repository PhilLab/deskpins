#pragma once


class PinShape : boost::noncopyable {
public:
    PinShape();
    ~PinShape();

    bool initShape();
    bool initImage(COLORREF clr);

    HBITMAP getBmp() const { return hBmp;  }
    HRGN    getRgn() const { return hRgn;  }
    int     getW()   const { return sz.cx; }
    int     getH()   const { return sz.cy; }

protected:
    HBITMAP hBmp;
    HRGN    hRgn;
    SIZE    sz;
};
