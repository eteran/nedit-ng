
#ifndef STYLE_TABLE_ENTRY_H_
#define STYLE_TABLE_ENTRY_H_

#include <string>
#include <cstdint>
#include <X11/Intrinsic.h>

struct StyleTableEntry {
	std::string highlightName;
	std::string styleName;
	std::string colorName;
	bool        isBold;
	bool        isItalic;
	uint16_t    red;
	uint16_t    green;
	uint16_t    blue;
	Pixel       color;
	bool        underline;
	XFontStruct *font;
	std::string bgColorName; /* background style coloring (name may be "empty") */
	uint16_t    bgRed;
	uint16_t    bgGreen;
	uint16_t    bgBlue;
	Pixel       bgColor;
};

#endif
