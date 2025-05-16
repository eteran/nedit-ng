
#ifndef TEXT_BUFFER_FWD_H_
#define TEXT_BUFFER_FWD_H_

#include <cstdint>
#include <string>

template <class Ch = char, class Tr = std::char_traits<Ch>>
class BasicTextBuffer;

using TextBuffer = BasicTextBuffer<char>;

// TODO(eteran): it seems that std::char_traits<uint8_t> is actually not
// supported by the C++ standard even though it has worked in practice.
// So we have to implement our own traits class for uint8_t to be standard
// compliant.
namespace std {
template <>
struct char_traits<uint8_t> {
	using char_type  = uint8_t;
	using int_type   = int;
	using off_type   = std::streamoff;
	using pos_type   = std::streampos;
	using state_type = mbstate_t;

	constexpr static void assign(char_type &c1, const char_type &c2) noexcept { c1 = c2; }
	constexpr static bool eq(char_type c1, char_type c2) noexcept { return c1 == c2; }
	constexpr static bool lt(char_type c1, char_type c2) noexcept { return c1 < c2; }
	constexpr static int_type not_eof(int_type ch) noexcept { return eq_int_type(ch, eof()) ? ~eof() : ch; }
	constexpr static char_type to_char_type(int_type ch) noexcept { return char_type(ch); }
	constexpr static int_type to_int_type(char_type ch) noexcept { return int_type(ch); }
	constexpr static bool eq_int_type(int_type c1, int_type c2) noexcept { return c1 == c2; }
	constexpr static int_type eof() noexcept { return int_type(EOF); }

	constexpr static int compare(const char_type *s1, const char_type *s2, size_t n) noexcept {
		for (size_t i = 0; i < n; ++i) {
			if (s1[i] != s2[i]) {
				return s1[i] < s2[i] ? -1 : 1;
			}
		}
		return 0;
	}

	constexpr static size_t length(const char_type *s) noexcept {
		size_t n = 0;
		while (*s++ != char_type(0)) {
			++n;
		}
		return n;
	}

	constexpr static const char_type *find(const char_type *s, size_t n, const char_type &ch) noexcept {
		for (size_t i = 0; i < n; ++i) {
			if (s[i] == ch) {
				return &s[i];
			}
		}
		return nullptr;
	}

	constexpr static char_type *move(char_type *s1, const char_type *s2, size_t n) noexcept {
		if (n == 0 || s1 == s2) {
			return s1;
		}

		if (s1 < s2) {
			return copy(s1, s2, n);
		}

		while (n--) {
			assign(s1[n], s2[n]);
		}
		return s1;
	}

	constexpr static char_type *copy(char_type *s1, const char_type *s2, size_t n) noexcept {
		for (size_t i = 0; i < n; ++i) {
			assign(s1[i], s2[i]);
		}
		return s1;
	}

	constexpr static char_type *assign(char_type *s, size_t n, char_type ch) noexcept {
		for (size_t i = 0; i < n; ++i) {
			assign(s[i], ch);
		}
		return s;
	}
};

}

using UTextBuffer = BasicTextBuffer<uint8_t, std::char_traits<uint8_t>>;

#endif
