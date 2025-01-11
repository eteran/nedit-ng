
#ifndef UTIL_STRING_H_
#define UTIL_STRING_H_

#include <string>
#include <string_view>

class QString;

QString ensure_newline(const QString &string);
std::string to_upper(std::string_view s);
std::string to_lower(std::string_view s);

#endif
