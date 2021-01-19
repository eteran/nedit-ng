
#include "Font.h"
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>

namespace Font {

/**
 * @brief fromString
 * @param fontName
 * @return
 */
QFont fromString(const QString &fontName) {
	QFont font;
	font.fromString(fontName);
	font.setStyleName(QString());
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_DEPRECATED
	// NOTE(eteran): unfortunately, this line seems to matter
	// despite being marked as deprecated, and Qt doesn't suggest
	// a meaningful alternative
	// See https://github.com/eteran/nedit-ng/issues/287
	// See https://github.com/eteran/nedit-ng/issues/274
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
	QT_WARNING_POP
	return font;
}

/**
 * @brief maxWidth
 * @param fm
 * @return
 */
int maxWidth(const QFontMetrics &fm) {
	return Font::characterWidth(fm, QLatin1Char('X'));
}

/**
 * @brief characterWidth
 * @param fm
 * @param ch
 * @return
 */
int characterWidth(const QFontMetrics &fm, QChar ch) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(ch);
#else
	return fm.width(ch);
#endif
}

/**
 * @brief stringWidth
 * @param fm
 * @param s
 * @return
 */
int stringWidth(const QFontMetrics &fm, const QString &s) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(s);
#else
	return fm.width(s);
#endif
}

/**
 * @brief pointSizes
 * @param font
 * @return
 */
QList<int> pointSizes(const QFont &font) {

	// attemps to return the list of point sizes supported by this font
	// but that list CAN be empty. If it is, just fall back on the standard
	// list of sizes which may or may be usable... but it's arguably better
	// than an empty list

	QFontDatabase database;
	QFontInfo fi(font);
	QList<int> sizes = database.pointSizes(fi.family(), fi.styleName());
	if (sizes.empty()) {
		sizes = QFontDatabase::standardSizes();
	}
	return sizes;
}

}
