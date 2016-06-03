
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <QString>

enum FontType {
	PLAIN_FONT, 
	ITALIC_FONT, 
	BOLD_FONT, 
	BOLD_ITALIC_FONT
};

class HighlightStyle {
public:
	QString name;
	QString color;
	QString bgColor;
	int     font;
};

#endif
