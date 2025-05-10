
#ifndef UTILS_H_
#define UTILS_H_

#include <cctype>
#include <type_traits>

template <class Integer>
using IsInteger = std::enable_if_t<std::is_integral<Integer>::value>;

template <class Ch, class = IsInteger<Ch>>
int safe_isascii(Ch ch) noexcept {
	return static_cast<int>(ch) < 0x80;
}

template <class Ch, class = IsInteger<Ch>>
int safe_isalnum(Ch ch) noexcept {
	return std::isalnum(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_isspace(Ch ch) noexcept {
	return std::isspace(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_isalpha(Ch ch) noexcept {
	return std::isalpha(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_isdigit(Ch ch) noexcept {
	return std::isdigit(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_islower(Ch ch) noexcept {
	return std::islower(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_isupper(Ch ch) noexcept {
	return std::isupper(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_tolower(Ch ch) noexcept {
	return std::tolower(static_cast<unsigned char>(ch));
}

template <class Ch, class = IsInteger<Ch>>
int safe_toupper(Ch ch) noexcept {
	return std::toupper(static_cast<unsigned char>(ch));
}

#endif
