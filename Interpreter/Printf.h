
#ifndef PRINTF_20160922_H_
#define PRINTF_20160922_H_

#include "Util/Raise.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>

// #define CXX17_PRINTF_EXTENSIONS

namespace cxx17 {

struct ostream_writer {

	ostream_writer(std::ostream &os)
		: os_(os) {
	}

	void write(char ch) {
		os_.put(ch);
		++written;
	}

	void write(const char *p, size_t n) {
		os_.write(p, n);
		written += n;
	}

	void done() noexcept {}

	std::ostream &os_;
	size_t written = 0;
};

struct format_error : std::runtime_error {
	format_error(const char *what_arg)
		: std::runtime_error(what_arg) {};
};

namespace detail {

enum class Modifiers : uint8_t {
	None,
	Char,
	Short,
	Long,
	LongLong,
	LongDouble,
	IntMaxT,
	SizeT,
	PtrDiffT
};

struct Flags {
	uint8_t justify : 1;
	uint8_t sign : 1;
	uint8_t space : 1;
	uint8_t prefix : 1;
	uint8_t padding : 1;
	uint8_t reserved : 3;
};

static_assert(sizeof(Flags) == sizeof(uint8_t));

template <bool Upper, size_t Divisor, class T, size_t N>
std::tuple<const char *, size_t> format(char (&buf)[N], T d, int width, Flags flags) {

	if constexpr (Divisor == 10) {
		static constexpr const char digit_pairs[201] = {"00010203040506070809"
														"10111213141516171819"
														"20212223242526272829"
														"30313233343536373839"
														"40414243444546474849"
														"50515253545556575859"
														"60616263646566676869"
														"70717273747576777879"
														"80818283848586878889"
														"90919293949596979899"};

		char *it = &buf[N - 2];
		if constexpr (std::is_signed<T>::value) {
			if (d >= 0) {
				int div = d / 100;
				while (div) {
					std::memcpy(it, &digit_pairs[2 * (d - div * 100)], 2);
					d = div;
					it -= 2;
					div = d / 100;
				}

				std::memcpy(it, &digit_pairs[2 * d], 2);

				if (d < 10) {
					it++;
				}

				if (flags.space) {
					if (flags.padding) {
						while (&buf[N] - it < width - 1) {
							*--it = '0';
						}
					}
					*--it = ' ';
				} else if (flags.sign) {
					if (flags.padding) {
						while (&buf[N] - it < width - 1) {
							*--it = '0';
						}
					}
					*--it = '+';

				} else {
					if (flags.padding) {
						while (&buf[N] - it < width) {
							*--it = '0';
						}
					}
				}

			} else {
				int div = d / 100;
				while (div) {
					std::memcpy(it, &digit_pairs[-2 * (d - div * 100)], 2);
					d = div;
					it -= 2;
					div = d / 100;
				}

				std::memcpy(it, &digit_pairs[-2 * d], 2);

				if (d <= -10) {
					it--;
				}

				if (flags.padding) {
					while (&buf[N] - it < width) {
						*--it = '0';
					}
				}

				*it = '-';
			}
		} else {
			if (d >= 0) {
				int div = d / 100;
				while (div) {
					std::memcpy(it, &digit_pairs[2 * (d - div * 100)], 2);
					d = div;
					it -= 2;
					div = d / 100;
				}

				std::memcpy(it, &digit_pairs[2 * d], 2);

				if (d < 10) {
					it++;
				}

				if (flags.space) {
					if (flags.padding) {
						while (&buf[N] - it < width - 1) {
							*--it = '0';
						}
					}
					*--it = ' ';
				} else if (flags.sign) {
					if (flags.padding) {
						while (&buf[N] - it < width - 1) {
							*--it = '0';
						}
					}
					*--it = '+';

				} else {
					if (flags.padding) {
						while (&buf[N] - it < width) {
							*--it = '0';
						}
					}
				}
			}
		}

		return std::make_tuple(it, &buf[N] - it);
	} else if constexpr (Divisor == 16) {
		[[maybe_unused]] static constexpr const char xdigit_pairs_l[513] = {"000102030405060708090a0b0c0d0e0f"
																			"101112131415161718191a1b1c1d1e1f"
																			"202122232425262728292a2b2c2d2e2f"
																			"303132333435363738393a3b3c3d3e3f"
																			"404142434445464748494a4b4c4d4e4f"
																			"505152535455565758595a5b5c5d5e5f"
																			"606162636465666768696a6b6c6d6e6f"
																			"707172737475767778797a7b7c7d7e7f"
																			"808182838485868788898a8b8c8d8e8f"
																			"909192939495969798999a9b9c9d9e9f"
																			"a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
																			"b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
																			"c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
																			"d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
																			"e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
																			"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"};

		[[maybe_unused]] static constexpr const char xdigit_pairs_u[513] = {"000102030405060708090A0B0C0D0E0F"
																			"101112131415161718191A1B1C1D1E1F"
																			"202122232425262728292A2B2C2D2E2F"
																			"303132333435363738393A3B3C3D3E3F"
																			"404142434445464748494A4B4C4D4E4F"
																			"505152535455565758595A5B5C5D5E5F"
																			"606162636465666768696A6B6C6D6E6F"
																			"707172737475767778797A7B7C7D7E7F"
																			"808182838485868788898A8B8C8D8E8F"
																			"909192939495969798999A9B9C9D9E9F"
																			"A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
																			"B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
																			"C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
																			"D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
																			"E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
																			"F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF"};

		// NOTE(eteran): we include the x/X, here as an easy way to put the
		//               upper/lower case prefix for hex numbers
		[[maybe_unused]] static constexpr const char alphabet_l[] = "0123456789abcdefx";
		[[maybe_unused]] static constexpr const char alphabet_u[] = "0123456789ABCDEFX";

		typename std::make_unsigned<T>::type ud = d;

		char *p = buf + N;

		if (ud >= 0) {
			while (ud > 16) {
				p -= 2;
				if constexpr (Upper) {
					std::memcpy(p, &xdigit_pairs_u[2 * (ud & 0xff)], 2);
				} else {
					std::memcpy(p, &xdigit_pairs_l[2 * (ud & 0xff)], 2);
				}
				ud /= 256;
			}

			while (ud > 0) {
				p -= 1;
				if constexpr (Upper) {
					std::memcpy(p, &xdigit_pairs_u[2 * (ud & 0x0f) + 1], 1);
				} else {
					std::memcpy(p, &xdigit_pairs_l[2 * (ud & 0x0f) + 1], 1);
				}
				ud /= 16;
			}

			// add in any necessary padding
			if (flags.padding) {
				while (&buf[N] - p < width) {
					*--p = '0';
				}
			}

			// add the prefix as needed
			if (flags.prefix) {
				if constexpr (Upper) {
					*--p = alphabet_u[16];
				} else {
					*--p = alphabet_l[16];
				}
				*--p = '0';
			}
		}

		return std::make_tuple(p, (buf + N) - p);
	} else if constexpr (Divisor == 8) {
		static constexpr const char digit_pairs[129] = {"0001020304050607"
														"1011121314151617"
														"2021222324252627"
														"3031323334353637"
														"4041424344454647"
														"5051525354555657"
														"6061626364656667"
														"7071727374757677"};
		typename std::make_unsigned<T>::type ud      = d;

		char *p = buf + N;

		if (ud >= 0) {
			while (ud > 64) {
				p -= 2;
				std::memcpy(p, &digit_pairs[2 * (ud & 077)], 2);
				ud /= 64;
			}

			while (ud > 0) {
				p -= 1;
				std::memcpy(p, &digit_pairs[2 * (ud & 007) + 1], 1);
				ud /= 8;
			}

			// add in any necessary padding
			if (flags.padding) {
				while (&buf[N] - p < width) {
					*--p = '0';
				}
			}

			// add the prefix as needed
			if (flags.prefix) {
				*--p = '0';
			}
		}

		return std::make_tuple(p, (buf + N) - p);
	} else if constexpr (Divisor == 2) {
		static constexpr const char digit_pairs[9] = {"00011011"};

		typename std::make_unsigned<T>::type ud = d;

		char *p = buf + N;

		if (ud >= 0) {
			while (ud > 4) {
				p -= 2;
				std::memcpy(p, &digit_pairs[2 * (ud & 0x03)], 2);
				ud /= 4;
			}

			while (ud > 0) {
				p -= 1;
				std::memcpy(p, &digit_pairs[2 * (ud & 0x01) + 1], 1);
				ud /= 2;
			}

			// add in any necessary padding
			if (flags.padding) {
				while (&buf[N] - p < width) {
					*--p = '0';
				}
			}

			// add the prefix as needed
			if (flags.prefix) {
				*--p = '0';
			}
		}

		return std::make_tuple(p, (buf + N) - p);
	}

	Raise<format_error>("Invalid Base Used In Integer To String Conversion");
}

//------------------------------------------------------------------------------
// Name: itoa
// Desc: as a minor optimization, let's determine a few things up front and pass
//       them as template parameters enabling some more aggressive optimizations
//       when the division can use more efficient operations
//------------------------------------------------------------------------------
template <class T, size_t N>
std::tuple<const char *, size_t> itoa(char (&buf)[N], char base, int precision, T d, int width, Flags flags) {

	if (d == 0 && precision == 0) {
		*buf = '\0';
		return std::make_tuple(buf, 0);
	}

	switch (base) {
	case 'i':
	case 'd':
	case 'u':
		return format<false, 10>(buf, d, width, flags);
#ifdef CXX17_PRINTF_EXTENSIONS
	case 'b':
		return format<false, 2>(buf, d, width, flags);
#endif
	case 'X':
		return format<true, 16>(buf, d, width, flags);
	case 'x':
		return format<false, 16>(buf, d, width, flags);
	case 'o':
		return format<false, 8>(buf, d, width, flags);
	default:
		return format<false, 10>(buf, d, width, flags);
	}
}

//------------------------------------------------------------------------------
// Name: output_string
// Desc: prints a string to the Context object, taking into account padding flags
// Note: ch is the current format specifier
//------------------------------------------------------------------------------
template <class Context>
void output_string(char ch, const char *s_ptr, int precision, long int width, Flags flags, int len, Context &ctx) noexcept {

	if ((ch == 's' && precision >= 0 && precision < len)) {
		len = precision;
	}

	// if not left justified padding goes first...
	if (!flags.justify) {
		// spaces go before the prefix...
		while (width-- > len) {
			ctx.write(' ');
		}
	}

	// output the string
	// NOTE(eteran): len is at most strlen, possibly is less
	// so we can write len chars
	width -= len;

	ctx.write(s_ptr, len);

	// if left justified padding goes last...
	if (flags.justify) {
		while (width-- > 0) {
			ctx.write(' ');
		}
	}
}

// NOTE(eteran): Here is some code to fetch arguments of specific types.

#ifdef CXX17_PRINTF_EXTENSIONS
std::string formatted_object(std::string obj) {
	return obj;
}

template <class T>
std::string to_string(T) {
	Raise<format_error>("No to_string found for this object type");
}

template <class T>
std::string formatted_object(T obj) {
	using detail::to_string;
	using std::to_string;
	return to_string(obj);
}
#endif

template <class T>
constexpr const char *formatted_string([[maybe_unused]] T s) {
	if constexpr (std::is_convertible<T, const char *>::value) {
		return static_cast<const char *>(s);
	}
	Raise<format_error>("Non-String Argument For String Format");
}

template <class R, class T>
constexpr R formatted_pointer([[maybe_unused]] T p) {
	if constexpr (std::is_same<R, uintptr_t>::value) {
		return reinterpret_cast<uintptr_t>(p);
	} else if constexpr (std::is_convertible<T, const void *>::value) {
		return reinterpret_cast<R>(reinterpret_cast<uintptr_t>(p));
	}
	Raise<format_error>("Non-Pointer Argument For Pointer Format");
}

template <class R, class T>
constexpr R formatted_integer([[maybe_unused]] T n) {
	if constexpr (std::is_integral<T>::value) {
		return static_cast<R>(n);
	}
	Raise<format_error>("Non-Integer Argument For Integer Format");
}

//------------------------------------------------------------------------------
// Name: process_format
// Desc: prints the next argument to the Context taking into account the flags,
//       width, precision, and modifiers collected along the way. Then will
//       recursively continue processing the string
//------------------------------------------------------------------------------
template <class Context, class T, class... Ts>
int process_format(Context &ctx, const char *format, Flags flags, long int width, long int precision, Modifiers modifier, const T &arg, const Ts &...ts) {

	// enough to contain a 64-bit number in bin notation + optional prefix
	char num_buf[67];

	size_t slen;
	const char *s_ptr;

	char ch = *format;
	switch (ch) {
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'a':
	case 'A':
	case 'g':
	case 'G':
		// TODO(eteran): implement float formatting... for now, just consume the argument
		return Printf(ctx, format + 1, ts...);

	case 'p':
		precision    = 1;
		ch           = 'x';
		flags.prefix = 1;

		// NOTE(eteran): GNU printf prints "(nil)" for NULL pointers, we print 0x0
		std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_pointer<uintptr_t>(arg), width, flags);

		output_string(ch, s_ptr, precision, width, flags, slen, ctx);
		return Printf(ctx, format + 1, ts...);
	case 'x':
	case 'X':
	case 'u':
	case 'o':
#ifdef CXX17_PRINTF_EXTENSIONS
	case 'b': // extension, BINARY mode
#endif
		if (precision < 0) {
			precision = 1;
		}

		switch (modifier) {
		case Modifiers::Char:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<unsigned char>(arg), width, flags);
			break;
		case Modifiers::Short:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<unsigned short int>(arg), width, flags);
			break;
		case Modifiers::Long:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<unsigned long int>(arg), width, flags);
			break;
		case Modifiers::LongLong:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<unsigned long long int>(arg), width, flags);
			break;
		case Modifiers::IntMaxT:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<uintmax_t>(arg), width, flags);
			break;
		case Modifiers::SizeT:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<size_t>(arg), width, flags);
			break;
		case Modifiers::PtrDiffT:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<std::make_unsigned<ptrdiff_t>::type>(arg), width, flags);
			break;
		default:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<unsigned int>(arg), width, flags);
			break;
		}

		output_string(ch, s_ptr, precision, width, flags, slen, ctx);
		return Printf(ctx, format + 1, ts...);

	case 'i':
	case 'd':
		if (precision < 0) {
			precision = 1;
		}

		switch (modifier) {
		case Modifiers::Char:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<signed char>(arg), width, flags);
			break;
		case Modifiers::Short:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<short int>(arg), width, flags);
			break;
		case Modifiers::Long:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<long int>(arg), width, flags);
			break;
		case Modifiers::LongLong:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<long long int>(arg), width, flags);
			break;
		case Modifiers::IntMaxT:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<intmax_t>(arg), width, flags);
			break;
		case Modifiers::SizeT:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<std::make_signed<size_t>::type>(arg), width, flags);
			break;
		case Modifiers::PtrDiffT:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<ptrdiff_t>(arg), width, flags);
			break;
		default:
			std::tie(s_ptr, slen) = itoa(num_buf, ch, precision, formatted_integer<int>(arg), width, flags);
			break;
		}

		output_string(ch, s_ptr, precision, width, flags, slen, ctx);
		return Printf(ctx, format + 1, ts...);

	case 'c':
		// char is promoted to an int when pushed on the stack
		num_buf[0] = formatted_integer<char>(arg);
		num_buf[1] = '\0';
		s_ptr      = num_buf;
		output_string('c', s_ptr, precision, width, flags, 1, ctx);
		return Printf(ctx, format + 1, ts...);

	case 's':
		s_ptr = formatted_string(arg);
		if (!s_ptr) {
			s_ptr = "(null)";
		}
		output_string('s', s_ptr, precision, width, flags, strlen(s_ptr), ctx);
		return Printf(ctx, format + 1, ts...);

#ifdef CXX17_PRINTF_EXTENSIONS
	case '?': {
		std::string s = formatted_object(arg);
		output_string('s', s.data(), precision, width, flags, s.size(), ctx);
	}
		return Printf(ctx, format + 1, ts...);
#endif

	case 'n':
		switch (modifier) {
		case Modifiers::Char:
			*formatted_pointer<signed char *>(arg) = ctx.written;
			break;
		case Modifiers::Short:
			*formatted_pointer<short int *>(arg) = ctx.written;
			break;
		case Modifiers::Long:
			*formatted_pointer<long int *>(arg) = ctx.written;
			break;
		case Modifiers::LongLong:
			*formatted_pointer<long long int *>(arg) = ctx.written;
			break;
		case Modifiers::IntMaxT:
			*formatted_pointer<intmax_t *>(arg) = ctx.written;
			break;
		case Modifiers::SizeT:
			*formatted_pointer<std::make_signed<size_t>::type *>(arg) = ctx.written;
			break;
		case Modifiers::PtrDiffT:
			*formatted_pointer<ptrdiff_t *>(arg) = ctx.written;
			break;
		default:
			*formatted_pointer<int *>(arg) = ctx.written;
			break;
		}

		return Printf(ctx, format + 1, ts...);

	default:
		ctx.write('%');
		[[fallthrough]];
	case '\0':
	case '%':
		ctx.write(ch);
		break;
	}

	return Printf(ctx, format + 1, ts...);
}

//------------------------------------------------------------------------------
// Name: get_modifier
// Desc: gets the modifier, if any, from the format string, then calls
//       process_format
//------------------------------------------------------------------------------
template <class Context, class T, class... Ts>
int get_modifier(Context &ctx, const char *format, Flags flags, long int width, long int precision, const T &arg, const Ts &...ts) {

	Modifiers modifier = Modifiers::None;

	switch (*format) {
	case 'h':
		modifier = Modifiers::Short;
		++format;
		if (*format == 'h') {
			modifier = Modifiers::Char;
			++format;
		}
		break;
	case 'l':
		modifier = Modifiers::Long;
		++format;
		if (*format == 'l') {
			modifier = Modifiers::LongLong;
			++format;
		}
		break;
	case 'L':
		modifier = Modifiers::LongDouble;
		++format;
		break;
	case 'j':
		modifier = Modifiers::IntMaxT;
		++format;
		break;
	case 'z':
		modifier = Modifiers::SizeT;
		++format;
		break;
	case 't':
		modifier = Modifiers::PtrDiffT;
		++format;
		break;
	default:
		break;
	}

	return process_format(ctx, format, flags, width, precision, modifier, arg, ts...);
}

//------------------------------------------------------------------------------
// Name: get_precision
// Desc: gets the precision, if any, either from the format string or as an arg
//       as needed, then calls get_modifier
//------------------------------------------------------------------------------
template <class Context, class T, class... Ts>
int get_precision(Context &ctx, const char *format, Flags flags, long int width, const T &arg, const Ts &...ts) {

	// default to non-existant
	long int p = -1;

	if (*format == '.') {

		++format;
		if (*format == '*') {
			++format;
			// pull an int off the stack for processing
			p = formatted_integer<long int>(arg);
			if constexpr (sizeof...(ts) > 0) {
				return get_modifier(ctx, format, flags, width, p, ts...);
			}
			Raise<format_error>("Internal Error");
		} else {
			char *endptr;
			p      = std::strtol(format, &endptr, 10);
			format = endptr;
			return get_modifier(ctx, format, flags, width, p, arg, ts...);
		}
	}

	return get_modifier(ctx, format, flags, width, p, arg, ts...);
}

//------------------------------------------------------------------------------
// Name: get_width
// Desc: gets the width if any, either from the format string or as an arg as
//       needed, then calls get_precision
//------------------------------------------------------------------------------
template <class Context, class T, class... Ts>
int get_width(Context &ctx, const char *format, Flags flags, const T &arg, const Ts &...ts) {

	int width = 0;

	if (*format == '*') {
		++format;
		// pull an int off the stack for processing
		width = formatted_integer<long int>(arg);
		if constexpr (sizeof...(ts) > 0) {
			return get_precision(ctx, format, flags, width, ts...);
		}
		Raise<format_error>("Internal Error");
	} else {
		char *endptr;
		width  = std::strtol(format, &endptr, 10);
		format = endptr;
		return get_precision(ctx, format, flags, width, arg, ts...);
	}
}

//------------------------------------------------------------------------------
// Name: get_flags
// Desc: gets the flags, if any, from the format string, then calls get_width
//------------------------------------------------------------------------------
template <class Context, class... Ts>
int get_flags(Context &ctx, const char *format, const Ts &...ts) {

	Flags f   = {0, 0, 0, 0, 0, 0};
	bool done = false;

	// skip past the % char
	++format;
	do {
		switch (*format) {
		case '-':
			// justify, overrides padding
			f.justify = 1;
			f.padding = 0;
			++format;
			break;
		case '+':
			// sign, overrides space
			f.sign  = 1;
			f.space = 0;
			++format;
			break;
		case ' ':
			if (!f.sign) {
				f.space = 1;
			}
			++format;
			break;
		case '#':
			f.prefix = 1;
			++format;
			break;
		case '0':
			if (!f.justify) {
				f.padding = 1;
			}
			++format;
			break;
		default:
			done = true;
		}
	} while (!done);

	return get_width(ctx, format, f, ts...);
}

}

//------------------------------------------------------------------------------
// Name: Printf
//------------------------------------------------------------------------------
template <class Context, class... Ts>
int Printf(Context &ctx, const char *format, const Ts &...ts) {

	assert(format);

	if constexpr (sizeof...(ts) > 0) {
		while (*format != '\0') {
			if (*format == '%') {
				// %[flag][width][.precision][length]char

				// this recurses into get_width -> get_precision -> get_length -> process_format
				return detail::get_flags(ctx, format, ts...);
			} else {
				// do long strips of non-format chars in bulk
				const char *first = format;
				size_t count      = 0;
				do {
					++format;
					++count;
				} while (*format != '%');

				ctx.write(first, count);
				continue;
			}

			++format;
		}

		// clean up any trailing stuff
		return Printf(ctx, format + 1, ts...);
	} else {
		for (; *format; ++format) {
			if (*format != '%' || *++format == '%') {
				ctx.write(*format);
				continue;
			}

			Raise<format_error>("Bad Format");
		}

		// this will usually null terminate the string
		ctx.done();

		// return the amount of bytes that should have been written if there was sufficient space
		return ctx.written;
	}
}

//------------------------------------------------------------------------------
// Name: snprintf
// Desc: implementation of a snprintf compatible interface
//------------------------------------------------------------------------------
template <class... Ts>
int sprintf(std::ostream &os, const char *format, const Ts &...ts) {
	ostream_writer ctx(os);
	return Printf(ctx, format, ts...);
}

}

#endif
