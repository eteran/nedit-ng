
#include "Color.h"
#include <QColor>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

QColor toQColor(const Color &c) {
	return QColor(c.r / 256, c.g / 256, c.b / 256);
}

QColor toQColor(Pixel pixel) {
    XColor color;
    auto d = QX11Info::display();
    color.pixel = pixel;
    XQueryColor (d, DefaultColormap(d, DefaultScreen (d)), &color);
    return toQColor({color.red, color.green, color.blue});
}
