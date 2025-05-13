
#include "Font.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>

namespace Font {

/**
 * @brief Converts a font name string to a QFont object.
 * This function takes a font name in the format used by QFont::fromString()
 * and attempts to ensure that the font is set up with integer metrics.
 *
 * @param fontName The name of the font to convert, in the format used by QFont::fromString().
 * @return A QFont object representing the specified font.
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
	font.setStyleStrategy(QFont::NoFontMerging);
#endif
	return font;
}

/**
 * @brief Returns the maximum width of a character in the given font metrics.
 *
 * @param fm The QFontMetrics object containing the font metrics.
 * @return The maximum width of a character in the font, typically the width of 'X'.
 */
int maxWidth(const QFontMetrics &fm) {
	return Font::characterWidth(fm, QLatin1Char('X'));
}

/**
 * @brief Returns the width of a single character in the given font metrics.
 *
 * @param fm The QFontMetrics object containing the font metrics.
 * @param ch The character for which to calculate the width.
 * @return The width of the specified character in the font.
 */
int characterWidth(const QFontMetrics &fm, QChar ch) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(ch);
#else
	return fm.width(ch);
#endif
}

/**
 * @brief Returns the width of a string in the given font metrics.
 *
 * @param fm The QFontMetrics object containing the font metrics.
 * @param s The string for which to calculate the width.
 * @return The width of the specified string in the font.
 */
int stringWidth(const QFontMetrics &fm, const QString &s) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(s);
#else
	return fm.width(s);
#endif
}

/**
 * @brief Returns a list of point sizes supported by the given font.
 *
 * @param font The QFont object for which to retrieve the point sizes.
 * @return A list of integers representing the point sizes supported by the font.
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
