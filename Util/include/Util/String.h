
#ifndef UTIL_STRING_H_
#define UTIL_STRING_H_

#include "Util/string_view.h"
#include <string>

class QString;

QString ensure_newline(const QString &string);
std::string to_upper(view::string_view s);
std::string to_lower(view::string_view s);

#endif
