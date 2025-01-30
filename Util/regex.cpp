
#include "Util/regex.h"
#include "Regex.h"

#include <QString>

/**
 * @brief make_regex
 * @param re
 * @param flags
 * @return
 */
std::unique_ptr<Regex> make_regex(const QString &re, int flags) {
	return std::make_unique<Regex>(re.toStdString(), flags);
}
