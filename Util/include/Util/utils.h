
#ifndef UTILS_H_
#define UTILS_H_

#include <cctype>

inline int safe_isascii(unsigned char ch) noexcept {
	return static_cast<int>(ch) < 0x80;
}

inline int safe_isalnum(unsigned char ch) noexcept {
	return std::isalnum(ch);
}

inline int safe_isspace(unsigned char ch) noexcept {
	return std::isspace(ch);
}

inline int safe_isalpha(unsigned char ch) noexcept {
	return std::isalpha(ch);
}

inline int safe_isdigit(unsigned char ch) noexcept {
	return std::isdigit(ch);
}

inline int safe_islower(unsigned char ch) noexcept {
	return std::islower(ch);
}

inline int safe_isupper(unsigned char ch) noexcept {
	return std::isupper(ch);
}

inline int safe_tolower(unsigned char ch) noexcept {
	return std::tolower(ch);
}

inline int safe_toupper(unsigned char ch) noexcept {
	return std::toupper(ch);
}

#endif
