
#ifndef TEXT_BUFFER_FWD_H_
#define TEXT_BUFFER_FWD_H_

#include <cstdint>
#include <string>

template <class Ch = char, class Tr = std::char_traits<Ch>>
class BasicTextBuffer;

using TextBuffer = BasicTextBuffer<char>;

struct char_traits_u8 {
	using char_type  = uint8_t;
	using int_type   = int;
	using off_type   = std::streamoff;
	using pos_type   = std::streampos;
	using state_type = mbstate_t;

	static void constexpr assign(char_type &c1, const char_type &c2) noexcept { c1 = c2; }
	static constexpr bool eq(char_type c1, char_type c2) noexcept { return c1 == c2; }
	static constexpr bool lt(char_type c1, char_type c2) noexcept { return c1 < c2; }
	static constexpr int_type not_eof(int_type ch) noexcept { return eq_int_type(ch, eof()) ? ~eof() : ch; }
	static constexpr char_type to_char_type(int_type ch) noexcept { return char_type(ch); }
	static constexpr int_type to_int_type(char_type ch) noexcept { return int_type(ch); }
	static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept { return c1 == c2; }
	static constexpr int_type eof() noexcept { return int_type(EOF); }

	static constexpr int compare(const char_type *s1, const char_type *s2, size_t n) {
		for (; n; --n, ++s1, ++s2) {
			if (lt(*s1, *s2)) {
				return -1;
			}

			if (lt(*s2, *s1)) {
				return 1;
			}
		}
		return 0;
	}

	static constexpr size_t length(const char_type *s) {
		size_t len = 0;
		for (; !eq(*s, char_type(0)); ++s) {
			++len;
		}
		return len;
	}

	static constexpr const char_type *find(const char_type *s, size_t n, const char_type &ch) {
		for (; n; --n) {
			if (eq(*s, ch)) {
				return s;
			}
			++s;
		}

		return nullptr;
	}

	static char_type *move(char_type *s1, const char_type *s2, size_t n) {
		if (n == 0) {
			return s1;
		}

		if (s1 < s2) {
			return copy(s1, s2, n);
		}

		char_type *r = s1;

		if (s2 < s1) {
			s1 += n;
			s2 += n;
			for (; n; --n) {
				assign(*--s1, *--s2);
			}
		}
		return r;
	}

	static char_type *copy(char_type *s1, const char_type *s2, size_t n) {
		char_type *r = s1;
		for (; n; --n, ++s1, ++s2) {
			assign(*s1, *s2);
		}
		return r;
	}

	static char_type *assign(char_type *s, size_t n, char_type ch) {
		char_type *r = s;
		for (; n; --n, ++s) {
			assign(*s, ch);
		}
		return r;
	}
};

// TODO(eteran): it seems that technically, std::char_traits<uint8_t> is not
// supported by the C++ standard, but it works in practice. So we had to
// implement our own traits class for uint8_t to be standard
// compliant. (Adapted from LLVM's libc++ std::char_traits<uint8_t>)
using UTextBuffer = BasicTextBuffer<uint8_t, char_traits_u8>;

#endif
