
#ifndef STYLE_TABLE_ENTRY_H_
#define STYLE_TABLE_ENTRY_H_

#include <QString>
#include <cstdint>
#include <X11/Intrinsic.h>

class StyleTableEntry {
public:
	QString     highlightName;
	QString     styleName;
	QString     colorName;
	bool        isBold;
	bool        isItalic;
	Pixel       color;
	bool        underline;
	XFontStruct *font;
	QString bgColorName; /* background style coloring (name may be "empty") */
	Pixel       bgColor;
};

#endif
