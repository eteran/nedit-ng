
#ifndef REGEX_ERROR_H_
#define REGEX_ERROR_H_

#include <exception>
#include <string>
#include <Util/Compiler.h>

class RegexError final : public std::exception {
public:
	RegexError(const char *fmt, ...);

public:
	const char *what() const noexcept override;

private:
	std::string error_;
};

COLD_CODE
void reg_error(const char *str);

#endif
