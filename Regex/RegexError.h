
#ifndef REGEX_ERROR_H_
#define REGEX_ERROR_H_

#include <exception>
#include <string>

class RegexError : public std::exception {
public:
	RegexError(const char *fmt, ...);

public:
	const char *what() const noexcept override;

private:
	std::string error_;
};


void reg_error(const char *str);

#endif
