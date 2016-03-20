
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <string>
#include <QString>
#include "nullable_string.h"

struct HighlightStyle {
	std::string name;
	std::string color;
	QString     bgColor;
	int         font;
};

#endif
