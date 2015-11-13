
#ifndef REGEX_ERROR_H_
#define REGEX_ERROR_H_

#include <exception>
#include <cstdarg>
#include <string>

class regex_error : public std::exception {
public:
	regex_error(const char *fmt, ...) {
		char buf[1024];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		error_ = buf;
	}
	
public:
	virtual const char *what() const noexcept {
		return error_.c_str();
	}
	
private:
	std::string error_;
};

#endif
