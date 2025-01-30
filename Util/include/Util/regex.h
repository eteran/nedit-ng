
#ifndef UTIL_REGEX_H_
#define UTIL_REGEX_H_

#include <memory>

class QString;
class Regex;

std::unique_ptr<Regex> make_regex(const QString &re, int flags);

#endif
