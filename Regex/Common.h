
#ifndef COMMON_H_
#define COMMON_H_

#include "Constants.h"
#include "Opcodes.h"
#include "RegexError.h"
#include "util/raise.h"

#include <cstdint>
#include <cstring>

extern uint8_t Compute_Size;

template <class T>
unsigned int U_CHAR_AT(T *p) {
    return static_cast<unsigned int>(*p);
}

template <class T>
inline uint8_t *OPERAND(T *p) {
    return reinterpret_cast<uint8_t *>(p) + NODE_SIZE;
}

template <class T>
inline uint8_t GET_OP_CODE(T *p) {
    return *p;
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
template <class T>
char literal_escape(T ch) {

    static const uint8_t valid_escape[] = {
        'a', 'b', 'e', 'f', 'n', 'r', 't', 'v', '(', ')', '-', '[', ']', '<',
        '>', '{', '}', '.', '\\', '|', '^', '$', '*', '+', '?', '&', '\0'
    };

    static const uint8_t value[] = {
        '\a', '\b', 0x1B, // Escape character in ASCII character set.
        '\f', '\n', '\r', '\t', '\v', '(', ')', '-', '[', ']', '<', '>', '{',
        '}', '.', '\\', '|', '^', '$', '*', '+', '?', '&', '\0'
    };

    for (int i = 0; valid_escape[i] != '\0'; i++) {
        if (static_cast<uint8_t>(ch) == valid_escape[i]) {
            return static_cast<char>(value[i]);
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
 * Returns the actual character value or nullptr if not a valid hex or
 * octal escape.  raise<RegexError> is called if \x0, \x00, \0, \00, \000, or
 * \0000 is specified.
 *--------------------------------------------------------------------*/
template <class T>
char numeric_escape(T ch, const char **parse) {

    static const char digits[] = "fedcbaFEDCBA9876543210";

    static const unsigned int digit_val[] = {
        15, 14, 13, 12, 11, 10,            // Lower case Hex digits
        15, 14, 13, 12, 11, 10,            // Upper case Hex digits
        9,  8,  7,  6,  5,  4,  3, 2, 1, 0 // Decimal Digits
    };

    const char *scan;
    const char *pos_ptr;
    const char *digit_str;
    unsigned int value = 0;
    unsigned int radix = 8;
    int width = 3; // Can not be bigger than \0377
    int pos_delta = 14;
    int i;

    switch (ch) {
    case '0':
        digit_str = digits + pos_delta; // Only use Octal digits, i.e. 0-7.
        break;

    case 'x':
    case 'X':
        width = 2; // Can not be bigger than \0377
        radix = 16;
        pos_delta = 0;
        digit_str = digits; // Use all of the digit characters.

        break;

    default:
        return '\0'; // Not a numeric escape
    }

    scan = *parse;
    scan++; // Only change *parse on success.

    pos_ptr = strchr(digit_str, static_cast<int>(*scan));

    for (i = 0; pos_ptr != nullptr && (i < width); i++) {
        const long pos = (pos_ptr - digit_str) + pos_delta;
        value = (value * radix) + digit_val[pos];

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
        pos_ptr = strchr(digit_str, static_cast<int>(*scan));
    }

    // Handle the case of "\0" i.e. trying to specify a nullptr character.

    if (value == 0) {
        if (ch == '0') {
            raise<RegexError>("\\00 is an invalid octal escape");
        } else {
            raise<RegexError>("\\%c0 is an invalid hexadecimal escape", ch);
        }
    } else {
        // Point to the last character of the number on success.

        scan--;
        *parse = scan;
    }

    return static_cast<uint8_t>(value);
}

/**
 * @brief GET_OFFSET
 * @param p
 * @return
 */
template <class T>
uint16_t GET_OFFSET(T *p) {
    auto ptr = reinterpret_cast<uint8_t *>(p);
    return static_cast<uint16_t>(((ptr[1] & 0xff) << 8) + (ptr[2] & 0xff));
}

/*----------------------------------------------------------------------*
 * next_ptr - compute the address of a node's "NEXT" pointer.
 * Note: a simplified inline version is available via the NEXT_PTR() macro,
 *       but that one is only to be used at time-critical places (see the
 *       description of the macro).
 *----------------------------------------------------------------------*/
template <class T>
uint8_t *next_ptr(T *ptr) {

    if (ptr == &Compute_Size) {
        return nullptr;
    }

    const int offset = GET_OFFSET(ptr);

    if (offset == 0) {
        return nullptr;
    }

    if (GET_OP_CODE(ptr) == BACK) {
        return (ptr - offset);
    } else {
        return (ptr + offset);
    }
}

#endif
