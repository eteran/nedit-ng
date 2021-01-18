
#include "Font.h"
#include <QFont>
#include <QFontMetrics>

namespace Font {

QFont fromString(const QString &fontName) {
	QFont font;
	font.fromString(fontName);
	font.setStyleName(QString());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
QT_WARNING_POP
	return font;
}

int maxWidth(const QFontMetrics &fm) {
	return Font::characterWidth(fm, QLatin1Char('X'));
}

int characterWidth(const QFontMetrics &fm, QChar ch) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(ch);
#else
	return fm.width(ch);
#endif
}

int stringWidth(const QFontMetrics &fm, const QString &s) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(s);
#else
	return fm.width(s);
#endif
}

}
