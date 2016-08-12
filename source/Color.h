
#ifndef COLOR_H_
#define COLOR_H_

#include <cstdint>
typedef unsigned long Pixel;

class QColor;

struct Color {
	uint16_t r;
	uint16_t g;
	uint16_t b;
};

QColor toQColor(const Color &c);
QColor toQColor(Pixel pixel);

#endif
