
#ifndef COMMON_H_
#define COMMON_H_

#include "Constants.h"
#include "Opcodes.h"
#include "RegexError.h"
#include "Util/Raise.h"

#include <cstdint>
#include <cstring>

template <class Ptr>
unsigned int U_CHAR_AT(Ptr p) noexcept {
	return static_cast<unsigned int>(*p);
}

template <class T>
T *OPERAND(T *p) noexcept {
	static_assert(sizeof(T) == 1, "Invalid Pointer Type");
	return p + NODE_SIZE<size_t>;
}

template <class T>
uint8_t GET_OP_CODE(T *p) noexcept {
	static_assert(sizeof(T) == 1, "Invalid Pointer Type");
	return *reinterpret_cast<uint8_t *>(p);
}

/*--------------------------------------------------------------------*
 * literal_escape
 *
 * Recognize escaped literal characters (prefixed with backslash),
 * and translate them into the corresponding character.
 *
 * Returns the proper character value or nullptr if not a valid literal
 * escape.
 *--------------------------------------------------------------------*/
template <class R, class Ch>
constexpr R literal_escape(Ch ch) noexcept {

	constexpr char valid_escape[] = {
		'a', 'b', 'e', 'f', 'n', 'r', 't', 'v', '(', ')', '-', '[', ']', '<',
		'>', '{', '}', '.', '\\', '|', '^', '$', '*', '+', '?', '&'};

	constexpr char value[] = {
		'\a', '\b', 0x1B, // Escape character in ASCII character set.
		'\f', '\n', '\r', '\t', '\v', '(', ')', '-', '[', ']', '<', '>', '{',
		'}', '.', '\\', '|', '^', '$', '*', '+', '?', '&'};

	for (int i = 0; i != sizeof(valid_escape); ++i) {
		if (ch == valid_escape[i]) {
			return static_cast<R>(value[i]);
		}
	}

	return '\0';
}

/*--------------------------------------------------------------------*
 * numeric_escape
 *
 * Implements hex and octal numeric escape sequence syntax.
 *
 * Hexadecimal Escape: \x##    Max of two digits  Must have leading 'x'.
 * Octal Escape:       \0###   Max of three digits and not greater
 *                             than 377 octal.  Must have leading zero.
 *
 * Returns the actual character value or '\0' if not a valid hex or
 * octal escape.  RegexError is thrown if \x0, \x00, \0, \00, \000, or
 * \0000 is specified.
 *--------------------------------------------------------------------*/
template <class R, class Ch, class Iterator>
R numeric_escape(Ch ch, Iterator *parse) {

	static const char digits[] = "fedcbaFEDCBA9876543210";

	static const unsigned int digit_val[] = {
		15, 14, 13, 12, 11, 10,      // Lower case Hex digits
		15, 14, 13, 12, 11, 10,      // Upper case Hex digits
		9, 8, 7, 6, 5, 4, 3, 2, 1, 0 // Decimal Digits
	};

	const char *digit_str;
	unsigned int value = 0;
	unsigned int radix = 8;
	int width          = 3; // Can not be bigger than \0377
	int pos_delta      = 14;

	switch (ch) {
	case '0':
		digit_str = digits + pos_delta; // Only use Octal digits, i.e. 0-7.
		break;

	case 'x':
	case 'X':
		width     = 2; // Can not be bigger than \0377
		radix     = 16;
		pos_delta = 0;
		digit_str = digits; // Use all of the digit characters.

		break;

	default:
		return '\0'; // Not a numeric escape
	}

	Iterator scan = *parse;
	scan++; // Only change *parse on success.

	const char *pos_ptr = ::strchr(digit_str, static_cast<int>(*scan));

	for (int i = 0; pos_ptr != nullptr && (i < width); i++) {
		const ptrdiff_t pos = (pos_ptr - digit_str) + pos_delta;
		value               = (value * radix) + digit_val[pos];

		/* If this digit makes the value over 255, treat this digit as a literal
		   character instead of part of the numeric escape.  For example, \0777
		   will be processed as \077 (an 'M') and a literal '7' character, NOT
		   511 decimal which is > 255. */

		if (value > 255) {
			// Back out calculations for last digit processed.

			value -= digit_val[pos];
			value /= radix;

			break; /* Note that scan will not be incremented and still points to
					  the digit that caused overflow.  It will be decremented by
					  the "else" below to point to the last character that is
					  considered to be part of the octal escape. */
		}

		scan++;
		pos_ptr = ::strchr(digit_str, static_cast<int>(*scan));
	}

	// Handle the case of "\0" i.e. trying to specify a nullptr character.
	if (value == 0) {
		if (ch == '0') {
			Raise<RegexError>("\\00 is an invalid octal escape");
		} else {
			Raise<RegexError>("\\%c0 is an invalid hexadecimal escape", ch);
		}
	} else {
		// Point to the last character of the number on success.

		scan--;
		*parse = scan;
	}

	return static_cast<R>(value);
}

/**
 * @brief GET_OFFSET
 * @param p
 * @return
 */
inline uint16_t GET_OFFSET(const void *p) noexcept {
	auto ptr = reinterpret_cast<const uint8_t *>(p);
	return static_cast<uint16_t>(((ptr[1] & 0xff) << 8) + (ptr[2] & 0xff));
}

#endif
