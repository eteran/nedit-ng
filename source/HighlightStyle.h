
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <QString>
#include "FontType.h"

struct HighlightStyle {
	QString name;
	QString color;
	QString bgColor;
    int     font = PLAIN_FONT;
};

#endif
