
#include "Font.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>

namespace Font {

/**
 * @brief
 *
 * @param fontName
 * @return
 */
QFont fromString(const QString &fontName) {
	QFont font;
	font.fromString(fontName);
	font.setStyleName(QString());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_DEPRECATED
	// NOTE(eteran): unfortunately, this line seems to matter
	// despite being marked as deprecated, and Qt doesn't suggest
	// a meaningful alternative
	// See https://github.com/eteran/nedit-ng/issues/287
	// See https://github.com/eteran/nedit-ng/issues/274
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
	QT_WARNING_POP
#else
	font.setHintingPreference(QFont::PreferFullHinting);
#endif
	return font;
}

/**
 * @brief
 *
 * @param fm
 * @return
 */
int maxWidth(const QFontMetrics &fm) {
	return Font::characterWidth(fm, QLatin1Char('X'));
}

/**
 * @brief
 *
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
 * @brief
 *
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
 * @brief
 *
 * @param font
 * @return
 */
QList<int> pointSizes(const QFont &font) {

	// attempts to return the list of point sizes supported by this font
	// but that list CAN be empty. If it is, just fall back on the standard
	// list of sizes which may or may be usable... but it's arguably better
	// than an empty list
	const QFontInfo fi(font);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QFontDatabase database;
	QList<int> sizes = database.pointSizes(fi.family(), fi.styleName());
#else
	QList<int> sizes = QFontDatabase::pointSizes(fi.family(), fi.styleName());
#endif

	if (sizes.empty()) {
		sizes = QFontDatabase::standardSizes();
	}
	return sizes;
}

}
