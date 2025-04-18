
#include "RegexError.h"

#include <QByteArray>
#include <QtDebug>

/**
 * @brief
 *
 * @param fmt
 */
RegexError::RegexError(const char *fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	qvsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	error_ = buf;
}

/**
 * @brief
 *
 * @return
 */
const char *RegexError::what() const noexcept {
	return error_.c_str();
}

/**
 * @brief
 *
 * @param str
 */
void reg_error(const char *str) {
	qCritical("NEdit: Internal error processing regular expression (%s)", str);
}
