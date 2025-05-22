
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
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_GCC("-Wformat-security")
	QT_WARNING_DISABLE_GCC("-Wformat-nonliteral")
	QT_WARNING_DISABLE_CLANG("-Wformat-security")
	QT_WARNING_DISABLE_CLANG("-Wformat-nonliteral")
	vsnprintf(buf, sizeof(buf), fmt, ap);
	QT_WARNING_POP
	// NOTE(eteran): this warning is not needed for this function because
	// we happen to know that the inputs for `RegexError` always originate
	// from string constants.
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
void ReportError(const char *str) {
	qCritical("NEdit: Internal error processing regular expression (%s)", str);
}
