
#ifndef STYLE_TABLE_ENTRY_H_
#define STYLE_TABLE_ENTRY_H_

#include <QColor>
#include <QFont>
#include <QString>

class StyleTableEntry {
public:
	QString      highlightName;
	QString      styleName;
	QString      colorName;
	bool         isBold;
	bool         isItalic;
	QColor       color;
	bool         underline;
    QFont        font;
	QString      bgColorName; // background style coloring (name may be "empty")
	QColor       bgColor;
};

#endif
