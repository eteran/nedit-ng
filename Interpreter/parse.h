
#ifndef PARSE_H_
#define PARSE_H_

#include <string>

struct Program;

Program *ParseMacro(const std::string &expr, std::string *msg, int *stoppedAt);

#endif
