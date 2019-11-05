
#include "Util/String.h"
#include "Util/utils.h"
#include <QString>
#include <cctype>

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
 * @brief to_upper
 * @param s
 * @return
 */
std::string to_upper(view::string_view s) {

	std::string str;
	str.reserve(s.size());
	std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
		return safe_ctype<std::toupper>(ch);
	});
	return str;
}

/**
 * @brief to_lower
 * @param s
 * @return
 */
std::string to_lower(view::string_view s) {

	std::string str;
	str.reserve(s.size());
	std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
		return safe_ctype<std::tolower>(ch);
	});
	return str;
}
