
#include "RegexError.h"

#include <QByteArray>
#include <QtDebug>

/**
 * @brief RegexError constructor.
 *
 * @param fmt Format string for the error message.
 * @param ... Variable arguments for the format string.
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
 * @brief Returns the error message.
 *
 * @return The error message string.
 */
const char *RegexError::what() const noexcept {
	return error_.c_str();
}

/**
 * @brief Logs an internal error message for regular expressions.
 *
 * @param str The error message string.
 */
void reg_error(const char *str) {
	qCritical("NEdit: Internal error processing regular expression (%s)", str);
}
