
#ifndef PATH_ERROR_H_
#define PATH_ERROR_H_

#include <exception>

class path_error : public std::exception {
public:
	const char *what() const noexcept {
		return "path_error";
	}
};

#endif
