
#include "Color.h"
#include <QColor>

#if 0
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
#endif
