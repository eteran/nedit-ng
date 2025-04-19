
#include "Util/String.h"
#include "Util/utils.h"

#include <QString>

/**
 * @brief Ensures that the given string ends with a newline character.
 *
 * @param string the input string to check.
 * @return The input string with a newline appended if it does not already end with one.
 *
 * @note The macro language requires newline terminators for statements, but the
 * text widget doesn't force it like the NEdit text buffer does, so this might
 * avoid some confusion.
 * @note If the input string is null, an empty QString is returned.
 */
QString ensure_newline(const QString &string) {

	if (string.isNull()) {
		return QString();
	}

	if (string.endsWith(QLatin1Char('\n'))) {
		return string;
	}

	return string + QLatin1Char('\n');
}

/**
 * @brief Converts a string to uppercase.
 *
 * @param s the input string to convert.
 * @return A new string with all characters converted to uppercase.
 */
std::string to_upper(std::string_view s) {

	std::string str;
	str.reserve(s.size());
	std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
		return safe_toupper(ch);
	});
	return str;
}

/**
 * @brief Converts a string to lowercase.
 *
 * @param s the input string to convert.
 * @return A new string with all characters converted to lowercase.
 */
std::string to_lower(std::string_view s) {

	std::string str;
	str.reserve(s.size());
	std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
		return safe_tolower(ch);
	});
	return str;
}
