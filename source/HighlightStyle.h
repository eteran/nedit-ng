
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <string>
#include <QString>

struct HighlightStyle {
	std::string name;
	QString     color;
	QString     bgColor;
	int         font;
};

#endif
