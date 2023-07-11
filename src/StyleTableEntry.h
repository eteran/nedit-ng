
#ifndef STYLE_TABLE_ENTRY_H_
#define STYLE_TABLE_ENTRY_H_

#include <QColor>
#include <QString>

struct StyleTableEntry {
	QString highlightName;
	QString styleName;
	QString colorName;
	QString bgColorName;
	QColor color;
	QColor bgColor;
	bool isBold;
	bool isItalic;
	bool isUnderlined;
};

#endif
