
#include "RegexError.h"
#include <cstdarg>

/**
 * @brief RegexError::RegexError
 * @param fmt
 */
RegexError::RegexError(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    error_ = buf;
}

/**
 * @brief RegexError::what
 * @return
 */
const char *RegexError::what() const noexcept {
    return error_.c_str();
}

/**
 * @brief reg_error
 * @param str
 */
void reg_error(const char *str) {
    fprintf(stderr, "nedit: Internal error processing regular expression (%s)\n", str);
}
