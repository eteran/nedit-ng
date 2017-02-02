
#include "Color.h"
#include <QColor>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

QColor toQColor(const Color &c) {
	return QColor(c.r / 256, c.g / 256, c.b / 256);
}

QColor toQColor(Pixel pixel) {
    return QColor((pixel >> 16) & 0xff,
                  (pixel >> 8)  & 0xff,
                  (pixel >> 0)  & 0xff);

}

Pixel toPixel(const QColor &color) {
    return color.value();
}
