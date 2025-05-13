
#ifndef UTIL_STRING_H_
#define UTIL_STRING_H_

#include <string>
#include <string_view>

class QString;

QString EnsureNewline(const QString &string);
std::string ToUpper(std::string_view s);
std::string ToLower(std::string_view s);

#endif
