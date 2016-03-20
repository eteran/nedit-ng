
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <string>
#include <QString>

struct HighlightStyle {
	std::string name;
	std::string color;
	QString     bgColor;
	int         font;
};

#endif
