
#include "Color.h"
#include <QColor>

QColor toQColor(const Color &c) {
	return QColor(c.r / 256, c.g / 256, c.b / 256);
}
