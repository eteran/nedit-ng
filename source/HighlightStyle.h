
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <string>
#include "nullable_string.h"

struct HighlightStyle {
	std::string     name;
	std::string     color;
	nullable_string bgColor;
	int             font;
};

#endif
