
#ifndef STYLE_TABLE_ENTRY_H_
#define STYLE_TABLE_ENTRY_H_

#include <QColor>
#include <QString>

class StyleTableEntry {
public:
	QString      highlightName;
	QString      styleName;
	QString      colorName;
    QString      bgColorName;
	QColor       color;
    QColor       bgColor;
    bool         isBold;
    bool         isItalic;
    bool         underline;
};

#endif
