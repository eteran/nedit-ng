
#include "Util/String.h"
#include "Util/utils.h"

#include <QString>

/*
** If "string" is not terminated with a newline character, return a
** string which does end in a newline.
**
** (The macro language requires newline terminators for statements, but the
** text widget doesn't force it like the NEdit text buffer does, so this might
** avoid some confusion.)
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
 * @brief
 *
 * @param s
 * @return
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
 * @brief
 *
 * @param s
 * @return
 */
std::string to_lower(std::string_view s) {

	std::string str;
	str.reserve(s.size());
	std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
		return safe_tolower(ch);
	});
	return str;
}
