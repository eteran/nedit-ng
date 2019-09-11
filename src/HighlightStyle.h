
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <QString>
#include "Font.h"

struct HighlightStyle {
	QString name;
	QString color;
	QString bgColor;
	int     font = Font::Plain;
};

#endif
