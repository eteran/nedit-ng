
#include "Util/regex.h"
#include "Regex.h"

#include <QString>

/**
 * @brief Creates a new Regex object with the given regular expression and flags.
 *
 * @param re The regular expression to compile.
 * @param flags The flags to use for the regex compilation.
 * @return The compiled Regex object.
 */
std::unique_ptr<Regex> make_regex(const QString &re, int flags) {
	return std::make_unique<Regex>(re.toStdString(), flags);
}
