
#ifndef PARSE_H_
#define PARSE_H_

#include "interpret.h"

Program *ParseMacro(const char *expr, const char **msg, const char **stoppedAt);

#endif
