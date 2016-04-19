
#ifndef HIGHLIGHT_STYLE_H_
#define HIGHLIGHT_STYLE_H_

#include <QString>
#include <string>

enum FontType {
	PLAIN_FONT, 
	ITALIC_FONT, 
	BOLD_FONT, 
	BOLD_ITALIC_FONT
};

class HighlightStyle {
public:
	std::string name; // not QString yet for Motif List Widget needs
	QString color;
	QString bgColor;
	int     font;
};

#endif
