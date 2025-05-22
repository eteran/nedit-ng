
#ifndef REGEX_ERROR_H_
#define REGEX_ERROR_H_

#include "Util/Compiler.h"

#include <exception>
#include <string>

class RegexError final : public std::exception {
public:
	RegexError(const char *fmt, ...);

public:
	const char *what() const noexcept override;

private:
	std::string error_;
};

COLD_CODE
void ReportError(const char *str);

#endif
