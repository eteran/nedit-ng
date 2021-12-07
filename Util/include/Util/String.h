
#ifndef UTIL_STRING_H_
#define UTIL_STRING_H_

#include "Ext/string_view.h"
#include <string>

class QString;

QString ensure_newline(const QString &string);
std::string to_upper(ext::string_view s);
std::string to_lower(ext::string_view s);

#endif
