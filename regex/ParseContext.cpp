
#include "ParseContext.h"
#include "ExecuteContext.h"
#include "Constants.h"
#include "Common.h"
#include "Opcodes.h"
#include "regex_error.h"
#include "regularExp.h"
#include "util/raise.h"
#include "util/utils.h"
#include <cstring>
#include <algorithm>

namespace {

struct len_range {
    long lower;
    long upper;
};

uint8_t *chunk(int paren, int *flag_param, len_range *range_param);

const char Default_Meta_Char[] = "{.*+?[(|)^<>$";
const char ASCII_Digits[] = "0123456789"; // Same for all locales.

/**
 *
 */
template <class T>
constexpr uint8_t PUT_OFFSET_L(T v) {
    return static_cast<uint8_t>((v >> 8) & 0xff);
}

/**
 *
 */
template <class T>
constexpr uint8_t PUT_OFFSET_R(T v) {
    return static_cast<uint8_t>(v & 0xff);
}

/*--------------------------------------------------------------------*
 * init_ansi_classes
 *
 * Generate character class sets using locale aware ANSI C functions.
 *
 *--------------------------------------------------------------------*/
bool init_ansi_classes() {

    static bool initialized  = false;

    if (!initialized) {
        initialized = true; // Only need to generate character sets once.

        constexpr int Underscore = '_';
        constexpr int Newline    = '\n';

        int word_count   = 0;
        int letter_count = 0;
        int space_count  = 0;

        for (int i = 1; i < UINT8_MAX; i++) {
            if (safe_ctype<isalnum>(i) || i == Underscore) {
                pContext.Word_Char[word_count++] = static_cast<char>(i);
            }

            if (safe_ctype<isalpha>(i)) {
                pContext.Letter_Char[letter_count++] = static_cast<char>(i);
            }

            /* Note: Whether or not newline is considered to be whitespace is
               handled by switches within the original regex and is thus omitted
               here. */

            if (safe_ctype<isspace>(i) && (i != Newline)) {
                pContext.White_Space[space_count++] = static_cast<char>(i);
            }

            /* Make sure arrays are big enough.  ("- 2" because of zero array
               origin and we need to leave room for the nullptr terminator.) */

            if (word_count > (ALNUM_CHAR_SIZE - 2) || space_count > (WHITE_SPACE_SIZE - 2) || letter_count > (ALNUM_CHAR_SIZE - 2)) {
                reg_error("internal error #9 'init_ansi_classes'");
                return false;
            }
        }

        pContext.Word_Char[word_count]    = '\0';
        pContext.Letter_Char[word_count]  = '\0';
        pContext.White_Space[space_count] = '\0';
    }

    return true;
}

/**
 * @brief IS_QUANTIFIER
 * @param c
 * @return
 */
bool IS_QUANTIFIER(char c) {
    return c == '*' || c == '+' || c == '?' || c == pContext.Brace_Char;
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
 * octal escape.  raise<regex_error> is called if \x0, \x00, \0, \00, \000, or
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
            raise<regex_error>("\\00 is an invalid octal escape");
        } else {
            raise<regex_error>("\\%c0 is an invalid hexadecimal escape", ch);
        }
    } else {
        // Point to the last character of the number on success.

        scan--;
        *parse = scan;
    }

    return static_cast<uint8_t>(value);
}

/*----------------------------------------------------------------------*
 * emit_node
 *
 * Emit (if appropriate) the op code for a regex node atom.
 *
 * The NEXT pointer is initialized to nullptr.
 *
 * Returns a pointer to the START of the emitted node.
 *----------------------------------------------------------------------*/
template <class T>
uint8_t *emit_node(T op_code) {

    uint8_t *ret_val = pContext.Code_Emit_Ptr; // Return address of start of node

    if (ret_val == &Compute_Size) {
        pContext.Reg_Size += NODE_SIZE;
    } else {
        uint8_t *ptr = ret_val;
        *ptr++ = static_cast<uint8_t>(op_code);
        *ptr++ = '\0'; // Null "NEXT" pointer.
        *ptr++ = '\0';

        pContext.Code_Emit_Ptr = ptr;
    }

    return ret_val;
}

/*----------------------------------------------------------------------*
 * emit_byte
 *
 * Emit (if appropriate) a byte of code (usually part of an operand.)
 *----------------------------------------------------------------------*/
template <class T>
void emit_byte(T ch) {

    if (pContext.Code_Emit_Ptr == &Compute_Size) {
        pContext.Reg_Size++;
    } else {
        *pContext.Code_Emit_Ptr++ = static_cast<uint8_t>(ch);
    }
}

/*----------------------------------------------------------------------*
 * emit_class_byte
 *
 * Emit (if appropriate) a byte of code (usually part of a character
 * class operand.)
 *----------------------------------------------------------------------*/
template <class T>
void emit_class_byte(T ch) {

    if (pContext.Code_Emit_Ptr == &Compute_Size) {
        pContext.Reg_Size++;

        if (pContext.Is_Case_Insensitive && safe_ctype<isalpha>(ch))
            pContext.Reg_Size++;
    } else if (pContext.Is_Case_Insensitive && safe_ctype<isalpha>(ch)) {
        /* For case insensitive character classes, emit both upper and lower case
           versions of alphabetical characters. */

        *pContext.Code_Emit_Ptr++ = static_cast<uint8_t>(safe_ctype<tolower>(ch));
        *pContext.Code_Emit_Ptr++ = static_cast<uint8_t>(safe_ctype<toupper>(ch));
    } else {
        *pContext.Code_Emit_Ptr++ = static_cast<uint8_t>(ch);
    }
}

/*----------------------------------------------------------------------*
 * emit_special
 *
 * Emit nodes that need special processing.
 *----------------------------------------------------------------------*/
uint8_t *emit_special(uint8_t op_code, unsigned long test_val, size_t index) {

    uint8_t *ret_val = &Compute_Size;
    uint8_t *ptr;

    if (pContext.Code_Emit_Ptr == &Compute_Size) {
        switch (op_code) {
        case POS_BEHIND_OPEN:
        case NEG_BEHIND_OPEN:
            pContext.Reg_Size += LENGTH_SIZE; // Length of the look-behind match
            pContext.Reg_Size += NODE_SIZE;   // Make room for the node
            break;

        case TEST_COUNT:
            pContext.Reg_Size += NEXT_PTR_SIZE; // Make room for a test value.
            /* fallthrough */
        case INC_COUNT:
            pContext.Reg_Size += INDEX_SIZE; // Make room for an index value.
            /* fallthrough */
        default:
            pContext.Reg_Size += NODE_SIZE; // Make room for the node.
        }
    } else {
        ret_val = emit_node(op_code); // Return the address for start of node.
        ptr = pContext.Code_Emit_Ptr;

        if (op_code == INC_COUNT || op_code == TEST_COUNT) {
            *ptr++ = (uint8_t)index;

            if (op_code == TEST_COUNT) {
                *ptr++ = PUT_OFFSET_L(test_val);
                *ptr++ = PUT_OFFSET_R(test_val);
            }
        } else if (op_code == POS_BEHIND_OPEN || op_code == NEG_BEHIND_OPEN) {
            *ptr++ = PUT_OFFSET_L(test_val);
            *ptr++ = PUT_OFFSET_R(test_val);
            *ptr++ = PUT_OFFSET_L(test_val);
            *ptr++ = PUT_OFFSET_R(test_val);
        }

        pContext.Code_Emit_Ptr = ptr;
    }

    return ret_val;
}

/*----------------------------------------------------------------------*
 * tail - Set the next-pointer at the end of a node chain.
 *----------------------------------------------------------------------*/
void tail(uint8_t *search_from, uint8_t *point_to) {

    uint8_t *scan;
    uint8_t *next;

    if (search_from == &Compute_Size) {
        return;
    }

    // Find the last node in the chain (node with a null NEXT pointer)

    scan = search_from;

    for (;;) {
        next = next_ptr(scan);

        if (!next) {
            break;
        }

        scan = next;
    }

    long offset;
    if (GET_OP_CODE(scan) == BACK) {
        offset = scan - point_to;
    } else {
        offset = point_to - scan;
    }

    // Set NEXT pointer

    *(scan + 1) = PUT_OFFSET_L(offset);
    *(scan + 2) = PUT_OFFSET_R(offset);
}

/*--------------------------------------------------------------------*
 * offset_tail
 *
 * Perform a tail operation on (ptr + offset).
 *--------------------------------------------------------------------*/
void offset_tail(uint8_t *ptr, int offset, uint8_t *val) {
    if (ptr == &Compute_Size || ptr == nullptr)
        return;

    tail(ptr + offset, val);
}

/*----------------------------------------------------------------------*
 * insert
 *
 * Insert a node in front of already emitted node(s).  Means relocating
 * the operand.  Code_Emit_Ptr points one byte past the just emitted
 * node and operand.  The parameter 'insert_pos' points to the location
 * where the new node is to be inserted.
 *----------------------------------------------------------------------*/
uint8_t *insert(uint8_t op, uint8_t *insert_pos, long min, long max, size_t index) {

    uint8_t *src;
    uint8_t *dst;
    uint8_t *place;
    int insert_size = NODE_SIZE;

    if (op == BRACE || op == LAZY_BRACE) {
        // Make room for the min and max values.

        insert_size += (2 * NEXT_PTR_SIZE);
    } else if (op == INIT_COUNT) {
        // Make room for an index value .

        insert_size += INDEX_SIZE;
    }

    if (pContext.Code_Emit_Ptr == &Compute_Size) {
        pContext.Reg_Size += insert_size;
        return &Compute_Size;
    }

    src = pContext.Code_Emit_Ptr;
    pContext.Code_Emit_Ptr += insert_size;
    dst = pContext.Code_Emit_Ptr;

    // Relocate the existing emitted code to make room for the new node.

    while (src > insert_pos)
        *--dst = *--src;

    place = insert_pos; // Where operand used to be.
    *place++ = op;      // Inserted operand.
    *place++ = '\0';    // NEXT pointer for inserted operand.
    *place++ = '\0';

    if (op == BRACE || op == LAZY_BRACE) {
        *place++ = PUT_OFFSET_L(min);
        *place++ = PUT_OFFSET_R(min);

        *place++ = PUT_OFFSET_L(max);
        *place++ = PUT_OFFSET_R(max);
    } else if (op == INIT_COUNT) {
        *place++ = static_cast<uint8_t>(index);
    }

    return place; // Return a pointer to the start of the code moved.
}

/*--------------------------------------------------------------------*
 * shortcut_escape
 *
 * Implements convenient escape sequences that represent entire
 * character classes or special location assertions (similar to escapes
 * supported by Perl)
 *                                                  _
 *    \d     Digits                  [0-9]           |
 *    \D     NOT a digit             [^0-9]          | (Examples
 *    \l     Letters                 [a-zA-Z]        |  at left
 *    \L     NOT a Letter            [^a-zA-Z]       |    are
 *    \s     Whitespace              [ \t\n\r\f\v]   |    for
 *    \S     NOT Whitespace          [^ \t\n\r\f\v]  |     C
 *    \w     "Word" character        [a-zA-Z0-9_]    |   Locale)
 *    \W     NOT a "Word" character  [^a-zA-Z0-9_]  _|
 *
 *    \B     Matches any character that is NOT a word-delimiter
 *
 *    Codes for the "emit" parameter:
 *
 *    EMIT_NODE
 *       Emit a shortcut node.  Shortcut nodes have an implied set of
 *       class characters.  This helps keep the compiled regex string
 *       small.
 *
 *    EMIT_CLASS_BYTES
 *       Emit just the equivalent characters of the class.  This makes
 *       the escape usable from within a class, e.g. [a-fA-F\d].  Only
 *       \d, \D, \s, \S, \w, and \W can be used within a class.
 *
 *    CHECK_ESCAPE
 *       Only verify that this is a valid shortcut escape.
 *
 *    CHECK_CLASS_ESCAPE
 *       Same as CHECK_ESCAPE but only allows characters valid within
 *       a class.
 *
 *--------------------------------------------------------------------*/
template <class T>
uint8_t *shortcut_escape(T ch, int *flag_param, ShortcutEscapeFlags flags) {

    const char *clazz = nullptr;
    static const char codes[] = "ByYdDlLsSwW";
    auto ret_val = reinterpret_cast<uint8_t *>(1); // Assume success.
    const char *valid_codes;

    if (flags == EMIT_CLASS_BYTES || flags == CHECK_CLASS_ESCAPE) {
        valid_codes = codes + 3; // \B, \y and \Y are not allowed in classes
    } else {
        valid_codes = codes;
    }

    if (!strchr(valid_codes, static_cast<int>(ch))) {
        return nullptr; // Not a valid shortcut escape sequence
    } else if (flags == CHECK_ESCAPE || flags == CHECK_CLASS_ESCAPE) {
        return ret_val; // Just checking if this is a valid shortcut escape.
    }

    switch (ch) {
    case 'd':
    case 'D':
        if (flags == EMIT_CLASS_BYTES) {
            clazz = ASCII_Digits;
        } else if (flags == EMIT_NODE) {
            ret_val = (safe_ctype<islower>(ch) ? emit_node(DIGIT) : emit_node(NOT_DIGIT));
        }

        break;

    case 'l':
    case 'L':
        if (flags == EMIT_CLASS_BYTES) {
            clazz = pContext.Letter_Char;
        } else if (flags == EMIT_NODE) {
            ret_val = (safe_ctype<islower>(ch) ? emit_node(LETTER) : emit_node(NOT_LETTER));
        }

        break;

    case 's':
    case 'S':
        if (flags == EMIT_CLASS_BYTES) {
            if (pContext.Match_Newline)
                emit_byte('\n');

            clazz = pContext.White_Space;
        } else if (flags == EMIT_NODE) {
            if (pContext.Match_Newline) {
                ret_val = (safe_ctype<islower>(ch) ? emit_node(SPACE_NL) : emit_node(NOT_SPACE_NL));
            } else {
                ret_val = (safe_ctype<islower>(ch) ? emit_node(SPACE) : emit_node(NOT_SPACE));
            }
        }

        break;

    case 'w':
    case 'W':
        if (flags == EMIT_CLASS_BYTES) {
            clazz = pContext.Word_Char;
        } else if (flags == EMIT_NODE) {
            ret_val = (safe_ctype<islower>(ch) ? emit_node(WORD_CHAR) : emit_node(NOT_WORD_CHAR));
        }

        break;

    /* Since the delimiter table is not available at regex compile time \B,
       \Y and \Y can only generate a node.  At run time, the delimiter table
       will be available for these nodes to use. */

    case 'y':

        if (flags == EMIT_NODE) {
            ret_val = emit_node(IS_DELIM);
        } else {
            raise<regex_error>("internal error #5 'shortcut_escape'");
        }

        break;

    case 'Y':

        if (flags == EMIT_NODE) {
            ret_val = emit_node(NOT_DELIM);
        } else {
            raise<regex_error>("internal error #6 'shortcut_escape'");
        }

        break;

    case 'B':

        if (flags == EMIT_NODE) {
            ret_val = emit_node(NOT_BOUNDARY);
        } else {
            raise<regex_error>("internal error #7 'shortcut_escape'");
        }

        break;

    default:
        /* We get here if there isn't a case for every character in
           the string "codes" */

        raise<regex_error>("internal error #8 'shortcut_escape'");
    }

    if (flags == EMIT_NODE && ch != 'B') {
        *flag_param |= (HAS_WIDTH | SIMPLE);
    }

    if (clazz) {
        // Emit bytes within a character class operand.

        while (*clazz != '\0') {
            emit_byte(*clazz++);
        }
    }

    return ret_val;
}

/*--------------------------------------------------------------------*
 * branch_tail
 *
 * Perform a tail operation on (ptr + offset) but only if 'ptr' is a
 * BRANCH node.
 *--------------------------------------------------------------------*/
void branch_tail(uint8_t *ptr, int offset, uint8_t *val) {

    if (ptr == &Compute_Size || ptr == nullptr || GET_OP_CODE(ptr) != BRANCH) {
        return;
    }

    tail(ptr + offset, val);
}

/*--------------------------------------------------------------------*
 * back_ref
 *
 * Process a request to match a previous parenthesized thing.
 * Parenthetical entities are numbered beginning at 1 by counting
 * opening parentheses from left to to right.  \0 would represent
 * whole match, but would confuse numeric_escape as an octal escape,
 * so it is forbidden.
 *
 * Constructs of the form \~1, \~2, etc. are cross-regex back
 * references and are used in syntax highlighting patterns to match
 * text previously matched by another regex. *** IMPLEMENT LATER ***
 *--------------------------------------------------------------------*/
uint8_t *back_ref(const char *c, int *flag_param, int emit) {

    int c_offset = 0;
    const int is_cross_regex = 0;

    uint8_t *ret_val;

    // Implement cross regex backreferences later.

    /* if (*c == (uint8_t) ('~')) {
       c_offset++;
       is_cross_regex++;
    } */

    int paren_no = (*(c + c_offset) - '0');

    if (!safe_ctype<isdigit>(*(c + c_offset)) || /* Only \1, \2, ... \9 are supported. */
            paren_no == 0) {                     /* Should be caught by numeric_escape. */

        return nullptr;
    }

    // Make sure parentheses for requested back-reference are complete.

    if (!is_cross_regex && !pContext.Closed_Parens[paren_no]) {
        raise<regex_error>("\\%d is an illegal back reference", paren_no);
    }

    if (emit == EMIT_NODE) {
        if (is_cross_regex) {
            ++pContext.Reg_Parse; /* Skip past the '~' in a cross regex back reference.
                            We only do this if we are emitting code. */

            if (pContext.Is_Case_Insensitive) {
                ret_val = emit_node(X_REGEX_BR_CI);
            } else {
                ret_val = emit_node(X_REGEX_BR);
            }
        } else {
            if (pContext.Is_Case_Insensitive) {
                ret_val = emit_node(BACK_REF_CI);
            } else {
                ret_val = emit_node(BACK_REF);
            }
        }

        emit_byte(static_cast<uint8_t>(paren_no));

        if (is_cross_regex || pContext.Paren_Has_Width[paren_no]) {
            *flag_param |= HAS_WIDTH;
        }
    } else if (emit == CHECK_ESCAPE) {
        ret_val = reinterpret_cast<uint8_t *>(1);
    } else {
        ret_val = nullptr;
    }

    return ret_val;
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


/*----------------------------------------------------------------------*
 * atom
 *
 * Process one regex item at the lowest level
 *
 * OPTIMIZATION:  Lumps a continuous sequence of ordinary characters
 * together so that it can turn them into a single EXACTLY node, which
 * is smaller to store and faster to run.
 *----------------------------------------------------------------------*/
uint8_t *atom(int *flag_param, len_range *range_param) {

    uint8_t *ret_val;
    uint8_t test;
    int flags_local;
    len_range range_local;

    *flag_param = WORST;    // Tentatively.
    range_param->lower = 0; // Idem
    range_param->upper = 0;

    /* Process any regex comments, e.g. '(?# match next token->)'.  The
       terminating right parenthesis can not be escaped.  The comment stops at
       the first right parenthesis encountered (or the end of the regex
       string)... period.  Handles multiple sequential comments,
       e.g. '(?# one)(?# two)...'  */

    while (*pContext.Reg_Parse == '(' && pContext.Reg_Parse[1] == '?' && *(pContext.Reg_Parse + 2) == '#') {

        pContext.Reg_Parse += 3;

        while (*pContext.Reg_Parse != ')' && pContext.Reg_Parse != pContext.Reg_Parse_End) {
            ++pContext.Reg_Parse;
        }

        if (*pContext.Reg_Parse == ')') {
            ++pContext.Reg_Parse;
        }

        if (*pContext.Reg_Parse == ')' || *pContext.Reg_Parse == '|' || pContext.Reg_Parse == pContext.Reg_Parse_End) {
            /* Hit end of regex string or end of parenthesized regex; have to
             return "something" (i.e. a NOTHING node) to avoid generating an
             error. */

            ret_val = emit_node(NOTHING);

            return ret_val;
        }
    }

    if(pContext.Reg_Parse == pContext.Reg_Parse_End) {
        // Supposed to be caught earlier.
        raise<regex_error>("internal error #3, 'atom'");
    }

    switch (*pContext.Reg_Parse++) {
    case '^':
        ret_val = emit_node(BOL);
        break;

    case '$':
        ret_val = emit_node(EOL);
        break;

    case '<':
        ret_val = emit_node(BOWORD);
        break;

    case '>':
        ret_val = emit_node(EOWORD);
        break;

    case '.':
        if (pContext.Match_Newline) {
            ret_val = emit_node(EVERY);
        } else {
            ret_val = emit_node(ANY);
        }

        *flag_param |= (HAS_WIDTH | SIMPLE);
        range_param->lower = 1;
        range_param->upper = 1;
        break;

    case '(':
        if (*pContext.Reg_Parse == '?') { // Special parenthetical expression
            ++pContext.Reg_Parse;
            range_local.lower = 0; // Make sure it is always used
            range_local.upper = 0;

            if (*pContext.Reg_Parse == ':') {
                ++pContext.Reg_Parse;
                ret_val = chunk(NO_CAPTURE, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == '=') {
                ++pContext.Reg_Parse;
                ret_val = chunk(POS_AHEAD_OPEN, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == '!') {
                ++pContext.Reg_Parse;
                ret_val = chunk(NEG_AHEAD_OPEN, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == 'i') {
                ++pContext.Reg_Parse;
                ret_val = chunk(INSENSITIVE, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == 'I') {
                ++pContext.Reg_Parse;
                ret_val = chunk(SENSITIVE, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == 'n') {
                ++pContext.Reg_Parse;
                ret_val = chunk(NEWLINE, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == 'N') {
                ++pContext.Reg_Parse;
                ret_val = chunk(NO_NEWLINE, &flags_local, &range_local);
            } else if (*pContext.Reg_Parse == '<') {
                ++pContext.Reg_Parse;
                if (*pContext.Reg_Parse == '=') {
                    ++pContext.Reg_Parse;
                    ret_val = chunk(POS_BEHIND_OPEN, &flags_local, &range_local);
                } else if (*pContext.Reg_Parse == '!') {
                    ++pContext.Reg_Parse;
                    ret_val = chunk(NEG_BEHIND_OPEN, &flags_local, &range_local);
                } else {
                    raise<regex_error>("invalid look-behind syntax, \"(?<%c...)\"", *pContext.Reg_Parse);
                }
            } else {
                raise<regex_error>("invalid grouping syntax, \"(?%c...)\"", *pContext.Reg_Parse);
            }
        } else { // Normal capturing parentheses
            ret_val = chunk(PAREN, &flags_local, &range_local);
        }

        if (ret_val == nullptr)
            return nullptr; // Something went wrong.

        // Add HAS_WIDTH flag if it was set by call to chunk.

        *flag_param |= flags_local & HAS_WIDTH;
        *range_param = range_local;

        break;

    case '|':
    case ')':
        raise<regex_error>("internal error #3, 'atom'"); // Supposed to be
                                                        // caught earlier.
    case '?':
    case '+':
    case '*':
        raise<regex_error>("%c follows nothing", pContext.Reg_Parse[-1]);

    case '{':
        if (pContext.Enable_Counting_Quantifier) {
            raise<regex_error>("{m,n} follows nothing");
        } else {
            ret_val = emit_node(EXACTLY); // Treat braces as literals.
            emit_byte('{');
            emit_byte('\0');
            range_param->lower = 1;
            range_param->upper = 1;
        }

        break;

    case '[': {
        unsigned int second_value;
        unsigned int last_value;
        uint8_t last_emit = 0;

        // Handle characters that can only occur at the start of a class.

        if (*pContext.Reg_Parse == '^') { // Complement of range.
            ret_val = emit_node(ANY_BUT);
            ++pContext.Reg_Parse;

            /* All negated classes include newline unless escaped with
               a "(?n)" switch. */

            if (!pContext.Match_Newline)
                emit_byte('\n');
        } else {
            ret_val = emit_node(ANY_OF);
        }

        if (*pContext.Reg_Parse == ']' || *pContext.Reg_Parse == '-') {
            /* If '-' or ']' is the first character in a class,
               it is a literal character in the class. */

            last_emit = *pContext.Reg_Parse;
            emit_byte(*pContext.Reg_Parse);
            ++pContext.Reg_Parse;
        }

        // Handle the rest of the class characters.

        while (pContext.Reg_Parse != pContext.Reg_Parse_End && *pContext.Reg_Parse != ']') {
            if (*pContext.Reg_Parse == '-') { // Process a range, e.g [a-z].
                ++pContext.Reg_Parse;

                if (*pContext.Reg_Parse == ']' || pContext.Reg_Parse == pContext.Reg_Parse_End) {
                    /* If '-' is the last character in a class it is a literal
                       character.  If 'Reg_Parse' points to the end of the
                       regex string, an error will be generated later. */

                    emit_byte('-');
                    last_emit = '-';
                } else {
                    /* We must get the range starting character value from the
                       emitted code since it may have been an escaped
                       character.  'second_value' is set one larger than the
                       just emitted character value.  This is done since
                       'second_value' is used as the start value for the loop
                       that emits the values in the range.  Since we have
                       already emitted the first character of the class, we do
                       not want to emit it again. */

                    second_value = ((unsigned int)last_emit) + 1;

                    if (*pContext.Reg_Parse == '\\') {
                        /* Handle escaped characters within a class range.
                           Specifically disallow shortcut escapes as the end of
                           a class range.  To allow this would be ambiguous
                           since shortcut escapes represent a set of characters,
                           and it would not be clear which character of the
                           class should be treated as the "last" character. */

                        ++pContext.Reg_Parse;

                        if ((test = numeric_escape(*pContext.Reg_Parse, &pContext.Reg_Parse))) {
                            last_value = static_cast<unsigned int>(test);
                        } else if ((test = literal_escape(*pContext.Reg_Parse))) {
                            last_value = static_cast<unsigned int>(test);
                        } else if (shortcut_escape(*pContext.Reg_Parse, nullptr, CHECK_CLASS_ESCAPE)) {
                            raise<regex_error>("\\%c is not allowed as range operand", *pContext.Reg_Parse);
                        } else {
                            raise<regex_error>("\\%c is an invalid char class escape sequence", *pContext.Reg_Parse);
                        }
                    } else {
                        last_value = U_CHAR_AT(pContext.Reg_Parse);
                    }

                    if (pContext.Is_Case_Insensitive) {
                        second_value = static_cast<unsigned int>(safe_ctype<tolower>(second_value));
                        last_value   = static_cast<unsigned int>(safe_ctype<tolower>(last_value));
                    }

                    /* For case insensitive, something like [A-_] will
                       generate an error here since ranges are converted to
                       lower case. */

                    if (second_value - 1 > last_value) {
                        raise<regex_error>("invalid [] range");
                    }

                    /* If only one character in range (e.g [a-a]) then this
                       loop is not run since the first character of any range
                       was emitted by the previous iteration of while loop. */

                    for (; second_value <= last_value; second_value++) {
                        emit_class_byte(second_value);
                    }

                    last_emit = static_cast<uint8_t>(last_value);

                    ++pContext.Reg_Parse;

                } // End class character range code.
            } else if (*pContext.Reg_Parse == '\\') {
                ++pContext.Reg_Parse;

                if ((test = numeric_escape(*pContext.Reg_Parse, &pContext.Reg_Parse)) != '\0') {
                    emit_class_byte(test);

                    last_emit = test;
                } else if ((test = literal_escape(*pContext.Reg_Parse)) != '\0') {
                    emit_byte(test);
                    last_emit = test;
                } else if (shortcut_escape(*pContext.Reg_Parse, nullptr, CHECK_CLASS_ESCAPE)) {

                    if (pContext.Reg_Parse[1] == '-') {
                        /* Specifically disallow shortcut escapes as the start
                           of a character class range (see comment above.) */

                        raise<regex_error>("\\%c not allowed as range operand", *pContext.Reg_Parse);
                    } else {
                        /* Emit the bytes that are part of the shortcut
                           escape sequence's range (e.g. \d = 0123456789) */

                        shortcut_escape(*pContext.Reg_Parse, nullptr, EMIT_CLASS_BYTES);
                    }
                } else {
                    raise<regex_error>("\\%c is an invalid char class escape sequence", *pContext.Reg_Parse);
                }

                ++pContext.Reg_Parse;

                // End of class escaped sequence code
            } else {
                emit_class_byte(*pContext.Reg_Parse); // Ordinary class character.

                last_emit = *pContext.Reg_Parse;
                ++pContext.Reg_Parse;
            }
        } // End of while (Reg_Parse != Reg_Parse_End && *pContext.Reg_Parse != ']')

        if (*pContext.Reg_Parse != ']')
            raise<regex_error>("missing right ']'");

        emit_byte('\0');

        /* NOTE: it is impossible to specify an empty class.  This is
           because [] would be interpreted as "begin character class"
           followed by a literal ']' character and no "end character class"
           delimiter (']').  Because of this, it is always safe to assume
           that a class HAS_WIDTH. */

        ++pContext.Reg_Parse;
        *flag_param |= HAS_WIDTH | SIMPLE;
        range_param->lower = 1;
        range_param->upper = 1;
    }

    break; // End of character class code.

    case '\\':
        if ((ret_val = shortcut_escape(*pContext.Reg_Parse, flag_param, EMIT_NODE))) {

            ++pContext.Reg_Parse;
            range_param->lower = 1;
            range_param->upper = 1;
            break;

        } else if ((ret_val = back_ref(pContext.Reg_Parse, flag_param, EMIT_NODE))) {
            /* Can't make any assumptions about a back-reference as to SIMPLE
               or HAS_WIDTH.  For example (^|<) is neither simple nor has
               width.  So we don't flip bits in flag_param here. */

            ++pContext.Reg_Parse;
            // Back-references always have an unknown length
            range_param->lower = -1;
            range_param->upper = -1;
            break;
        }
        /* fallthrough */

    /* At this point it is apparent that the escaped character is not a
       shortcut escape or back-reference.  Back up one character to allow
       the default code to include it as an ordinary character. */

    /* Fall through to Default case to handle literal escapes and numeric
       escapes. */

        /* fallthrough */
    default:
        --pContext.Reg_Parse; /* If we fell through from the above code, we are now
                        pointing at the back slash (\) character. */
        {
            const char *parse_save;
            int len = 0;

            if (pContext.Is_Case_Insensitive) {
                ret_val = emit_node(SIMILAR);
            } else {
                ret_val = emit_node(EXACTLY);
            }

            /* Loop until we find a meta character, shortcut escape, back
               reference, or end of regex string. */

            for (; pContext.Reg_Parse != pContext.Reg_Parse_End && !strchr(pContext.Meta_Char, static_cast<int>(*pContext.Reg_Parse)); len++) {

                /* Save where we are in case we have to back
                   this character out. */

                parse_save = pContext.Reg_Parse;

                if (*pContext.Reg_Parse == '\\') {
                    ++pContext.Reg_Parse; // Point to escaped character

                    if ((test = numeric_escape(*pContext.Reg_Parse, &pContext.Reg_Parse))) {
                        if (pContext.Is_Case_Insensitive) {
                            emit_byte(tolower(test));
                        } else {
                            emit_byte(test);
                        }
                    } else if ((test = literal_escape(*pContext.Reg_Parse))) {
                        emit_byte(test);
                    } else if (back_ref(pContext.Reg_Parse, nullptr, CHECK_ESCAPE)) {
                        // Leave back reference for next 'atom' call

                        --pContext.Reg_Parse;
                        break;
                    } else if (shortcut_escape(*pContext.Reg_Parse, nullptr, CHECK_ESCAPE)) {
                        // Leave shortcut escape for next 'atom' call

                        --pContext.Reg_Parse;
                        break;
                    } else {
                        /* None of the above calls generated an error message
                           so generate our own here. */

                        raise<regex_error>("\\%c is an invalid escape sequence", *pContext.Reg_Parse);

                    }

                    ++pContext.Reg_Parse;
                } else {
                    // Ordinary character

                    if (pContext.Is_Case_Insensitive) {
                        emit_byte(tolower(*pContext.Reg_Parse));
                    } else {
                        emit_byte(*pContext.Reg_Parse);
                    }

                    ++pContext.Reg_Parse;
                }

                /* If next regex token is a quantifier (?, +. *, or {m,n}) and
                   our EXACTLY node so far is more than one character, leave the
                   last character to be made into an EXACTLY node one character
                   wide for the multiplier to act on.  For example 'abcd* would
                   have an EXACTLY node with an 'abc' operand followed by a STAR
                   node followed by another EXACTLY node with a 'd' operand. */

                if (IS_QUANTIFIER(*pContext.Reg_Parse) && len > 0) {
                    pContext.Reg_Parse = parse_save; // Point to previous regex token.

                    if (pContext.Code_Emit_Ptr == &Compute_Size) {
                        pContext.Reg_Size--;
                    } else {
                        pContext.Code_Emit_Ptr--; // Write over previously emitted byte.
                    }

                    break;
                }
            }

            if (len <= 0)
                raise<regex_error>("internal error #4, 'atom'");

            *flag_param |= HAS_WIDTH;

            if (len == 1)
                *flag_param |= SIMPLE;

            range_param->lower = len;
            range_param->upper = len;

            emit_byte('\0');
        }
    }

    return ret_val;
}

/*----------------------------------------------------------------------*
 * piece - something followed by possible '*', '+', '?', or "{m,n}"
 *
 * Note that the branching code sequences used for the general cases of
 * *, +. ?, and {m,n} are somewhat optimized:  they use the same
 * NOTHING node as both the endmarker for their branch list and the
 * body of the last branch. It might seem that this node could be
 * dispensed with entirely, but the endmarker role is not redundant.
 *----------------------------------------------------------------------*/
uint8_t *piece(int *flag_param, len_range *range_param) {

    uint8_t *ret_val;
    uint8_t *next;
    uint8_t op_code;
    unsigned long min_max[2] = {REG_ZERO, REG_INFINITY};
    int flags_local, i, brace_present = 0;
    bool lazy = false;
    bool comma_present = false;
    int digit_present[2] = {0, 0};
    len_range range_local;

    ret_val = atom(&flags_local, &range_local);

    if (ret_val == nullptr)
        return nullptr; // Something went wrong.

    op_code = *pContext.Reg_Parse;

    if (!IS_QUANTIFIER(op_code)) {
        *flag_param = flags_local;
        *range_param = range_local;
        return  ret_val;
    } else if (op_code == '{') { // {n,m} quantifier present
        brace_present++;
        ++pContext.Reg_Parse;

        /* This code will allow specifying a counting range in any of the
           following forms:

           {m,n}  between m and n.
           {,n}   same as {0,n} or between 0 and infinity.
           {m,}   same as {m,0} or between m and infinity.
           {m}    same as {m,m} or exactly m.
           {,}    same as {0,0} or between 0 and infinity or just '*'.
           {}     same as {0,0} or between 0 and infinity or just '*'.

           Note that specifying a max of zero, {m,0} is not allowed in the regex
           itself, but it is implemented internally that way to support '*', '+',
           and {min,} constructs and signals an unlimited number. */

        for (i = 0; i < 2; i++) {
            /* Look for digits of number and convert as we go.  The numeric maximum
               value for max and min of 65,535 is due to using 2 bytes to store
               each value in the compiled regex code. */

            while (safe_ctype<isdigit>(*pContext.Reg_Parse)) {
                // (6553 * 10 + 6) > 65535 (16 bit max)

                if ((min_max[i] == 6553UL && (*pContext.Reg_Parse - '0') <= 5) || (min_max[i] <= 6552UL)) {

                    min_max[i] = (min_max[i] * 10UL) + (unsigned long)(*pContext.Reg_Parse - '0');
                    ++pContext.Reg_Parse;

                    digit_present[i]++;
                } else {
                    if (i == 0) {
                        raise<regex_error>("min operand of {%lu%c,???} > 65535", min_max[0], *pContext.Reg_Parse);
                    } else {
                        raise<regex_error>("max operand of {%lu,%lu%c} > 65535", min_max[0], min_max[1], *pContext.Reg_Parse);
                    }
                }
            }

            if (!comma_present && *pContext.Reg_Parse == ',') {
                comma_present = true;
                ++pContext.Reg_Parse;
            }
        }

        /* A max of zero can not be specified directly in the regex since it would
           signal a max of infinity.  This code specifically disallows '{0,0}',
           '{,0}', and '{0}' which really means nothing to humans but would be
           interpreted as '{0,infinity}' or '*' if we didn't make this check. */

        if (digit_present[0] && (min_max[0] == REG_ZERO) && !comma_present) {

            raise<regex_error>("{0} is an invalid range");
        } else if (digit_present[0] && (min_max[0] == REG_ZERO) && digit_present[1] && (min_max[1] == REG_ZERO)) {

            raise<regex_error>("{0,0} is an invalid range");
        } else if (digit_present[1] && (min_max[1] == REG_ZERO)) {
            if (digit_present[0]) {
                raise<regex_error>("{%lu,0} is an invalid range", min_max[0]);
            } else {
                raise<regex_error>("{,0} is an invalid range");
            }
        }

        if (!comma_present)
            min_max[1] = min_max[0]; // {x} means {x,x}

        if (*pContext.Reg_Parse != '}') {
            raise<regex_error>("{m,n} specification missing right '}'");

        } else if (min_max[1] != REG_INFINITY && min_max[0] > min_max[1]) {
            // Disallow a backward range.

            raise<regex_error>("{%lu,%lu} is an invalid range", min_max[0], min_max[1]);
        }
    }

    ++pContext.Reg_Parse;

    // Check for a minimal matching (non-greedy or "lazy") specification.

    if (*pContext.Reg_Parse == '?') {
        lazy = true;
        ++pContext.Reg_Parse;
    }

    // Avoid overhead of counting if possible

    if (op_code == '{') {
        if (min_max[0] == REG_ZERO && min_max[1] == REG_INFINITY) {
            op_code = '*';
        } else if (min_max[0] == REG_ONE && min_max[1] == REG_INFINITY) {
            op_code = '+';
        } else if (min_max[0] == REG_ZERO && min_max[1] == REG_ONE) {
            op_code = '?';
        } else if (min_max[0] == REG_ONE && min_max[1] == REG_ONE) {
            /* "x{1,1}" is the same as "x".  No need to pollute the compiled
                regex with such nonsense. */

            *flag_param = flags_local;
            *range_param = range_local;
            return ret_val;
        } else if (pContext.Num_Braces > (int)UINT8_MAX) {
            raise<regex_error>("number of {m,n} constructs > %d", UINT8_MAX);
        }
    }

    if (op_code == '+')
        min_max[0] = REG_ONE;
    if (op_code == '?')
        min_max[1] = REG_ONE;

    /* It is dangerous to apply certain quantifiers to a possibly zero width
       item. */

    if (!(flags_local & HAS_WIDTH)) {
        if (brace_present) {
            raise<regex_error>("{%lu,%lu} operand could be empty", min_max[0], min_max[1]);
        } else {
            raise<regex_error>("%c operand could be empty", op_code);
        }
    }

    *flag_param = (min_max[0] > REG_ZERO) ? (WORST | HAS_WIDTH) : WORST;
    if (range_local.lower >= 0) {
        if (min_max[1] != REG_INFINITY) {
            range_param->lower = range_local.lower * min_max[0];
            range_param->upper = range_local.upper * min_max[1];
        } else {
            range_param->lower = -1; // Not a fixed-size length
            range_param->upper = -1;
        }
    } else {
        range_param->lower = -1; // Not a fixed-size length
        range_param->upper = -1;
    }

    /*---------------------------------------------------------------------*
     *          Symbol  Legend  For  Node  Structure  Diagrams
     *---------------------------------------------------------------------*
     * (...) = general grouped thing
     * B     = (B)ranch,  K = bac(K),  N = (N)othing
     * I     = (I)nitialize count,     C = Increment (C)ount
     * T~m   = (T)est against mini(m)um- go to NEXT pointer if >= operand
     * T~x   = (T)est against ma(x)imum- go to NEXT pointer if >= operand
     * '~'   = NEXT pointer, \___| = forward pointer, |___/ = Backward pointer
     *---------------------------------------------------------------------*/

    if (op_code == '*' && (flags_local & SIMPLE)) {
        insert((lazy ? LAZY_STAR : STAR), ret_val, 0UL, 0UL, 0);

    } else if (op_code == '+' && (flags_local & SIMPLE)) {
        insert(lazy ? LAZY_PLUS : PLUS, ret_val, 0UL, 0UL, 0);

    } else if (op_code == '?' && (flags_local & SIMPLE)) {
        insert(lazy ? LAZY_QUESTION : QUESTION, ret_val, 0UL, 0UL, 0);

    } else if (op_code == '{' && (flags_local & SIMPLE)) {
        insert(lazy ? LAZY_BRACE : BRACE, ret_val, min_max[0], min_max[1], 0);

    } else if ((op_code == '*' || op_code == '+') && lazy) {
        /*  Node structure for (x)*?    Node structure for (x)+? construct.
         *  construct.                  (Same as (x)*? except for initial
         *                              forward jump into parenthesis.)
         *
         *                                  ___6____
         *   _______5_______               /________|______
         *  | _4__        1_\             /| ____   |     _\
         *  |/    |       / |\           / |/    |  |    / |\
         *  B~ N~ B~ (...)~ K~ N~       N~ B~ N~ B~ (...)~ K~ N~
         *      \  \___2_______|               \  \___________|
         *       \_____3_______|                \_____________|
         *
         */

        tail(ret_val, emit_node(BACK));              // 1
        (void)insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 2,4
        (void)insert(NOTHING, ret_val, 0UL, 0UL, 0); // 3

        next = emit_node(NOTHING); // 2,3

        offset_tail(ret_val, NODE_SIZE, next);        // 2
        tail(ret_val, next);                          // 3
        insert(BRANCH, ret_val, 0UL, 0UL, 0);         // 4,5
        tail(ret_val, ret_val + (2 * NODE_SIZE));     // 4
        offset_tail(ret_val, 3 * NODE_SIZE, ret_val); // 5

        if (op_code == '+') {
            insert(NOTHING, ret_val, 0UL, 0UL, 0);    // 6
            tail(ret_val, ret_val + (4 * NODE_SIZE)); // 6
        }
    } else if (op_code == '*') {
        /* Node structure for (x)* construct.
         *      ____1_____
         *     |          \
         *     B~ (...)~ K~ B~ N~
         *      \      \_|2 |\_|
         *       \__3_______|  4
         */

        insert(BRANCH, ret_val, 0UL, 0UL, 0);             // 1,3
        offset_tail(ret_val, NODE_SIZE, emit_node(BACK)); // 2
        offset_tail(ret_val, NODE_SIZE, ret_val);         // 1
        tail(ret_val, emit_node(BRANCH));                 // 3
        tail(ret_val, emit_node(NOTHING));                // 4
    } else if (op_code == '+') {
        /* Node structure for (x)+ construct.
         *
         *      ____2_____
         *     |          \
         *     (...)~ B~ K~ B~ N~
         *          \_|\____|\_|
         *          1     3    4
         */

        next = emit_node(BRANCH); // 1

        tail(ret_val, next);               // 1
        tail(emit_node(BACK), ret_val);    // 2
        tail(next, emit_node(BRANCH));     // 3
        tail(ret_val, emit_node(NOTHING)); // 4
    } else if (op_code == '?' && lazy) {
        /* Node structure for (x)?? construct.
         *       _4__        1_
         *      /    |       / |
         *     B~ N~ B~ (...)~ N~
         *         \  \___2____|
         *          \_____3____|
         */

        (void)insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 2,4
        (void)insert(NOTHING, ret_val, 0UL, 0UL, 0); // 3

        next = emit_node(NOTHING); // 1,2,3

        offset_tail(ret_val, 2 * NODE_SIZE, next);  // 1
        offset_tail(ret_val, NODE_SIZE, next);      // 2
        tail(ret_val, next);                        // 3
        insert(BRANCH, ret_val, 0UL, 0UL, 0);       // 4
        tail(ret_val, (ret_val + (2 * NODE_SIZE))); // 4

    } else if (op_code == '?') {
        /* Node structure for (x)? construct.
         *       ___1____  _2
         *      /        |/ |
         *     B~ (...)~ B~ N~
         *             \__3_|
         */

        insert(BRANCH, ret_val, 0UL, 0UL, 0); // 1
        tail(ret_val, emit_node(BRANCH));     // 1

        next = emit_node(NOTHING); // 2,3

        tail(ret_val, next);                   // 2
        offset_tail(ret_val, NODE_SIZE, next); // 3
    } else if (op_code == '{' && min_max[0] == min_max[1]) {
        /* Node structure for (x){m}, (x){m}?, (x){m,m}, or (x){m,m}? constructs.
         * Note that minimal and maximal matching mean the same thing when we
         * specify the minimum and maximum to be the same value.
         *       _______3_____
         *      |    1_  _2   \
         *      |    / |/ |    \
         *   I~ (...)~ C~ T~m K~ N~
         *    \_|          \_____|
         *     5              4
         */

        tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces));         // 1
        tail(ret_val, emit_special(TEST_COUNT, min_max[0], pContext.Num_Braces)); // 2
        tail(emit_node(BACK), ret_val);                                  // 3
        tail(ret_val, emit_node(NOTHING));                               // 4

        next = insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 5

        tail(ret_val, next); // 5

        pContext.Num_Braces++;
    } else if (op_code == '{' && lazy) {
        if (min_max[0] == REG_ZERO && min_max[1] != REG_INFINITY) {
            /* Node structure for (x){0,n}? or {,n}? construct.
             *       _________3____________
             *    8_| _4__        1_  _2   \
             *    / |/    |       / |/ |    \
             *   I~ B~ N~ B~ (...)~ C~ T~x K~ N~
             *          \  \            \__7__|
             *           \  \_________6_______|
             *            \______5____________|
             */

            tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces)); // 1

            next = emit_special(TEST_COUNT, min_max[0], pContext.Num_Braces); // 2,7

            tail(ret_val, next);                                  // 2
            insert(BRANCH,  ret_val, 0UL, 0UL, pContext.Num_Braces);   // 4,6
            insert(NOTHING, ret_val, 0UL, 0UL, pContext.Num_Braces);   // 5
            insert(BRANCH,  ret_val, 0UL, 0UL, pContext.Num_Braces);   // 3,4,8
            tail(emit_node(BACK), ret_val);                       // 3
            tail(ret_val, ret_val + (2 * NODE_SIZE));             // 4

            next = emit_node(NOTHING); // 5,6,7

            offset_tail(ret_val, NODE_SIZE, next);     // 5
            offset_tail(ret_val, 2 * NODE_SIZE, next); // 6
            offset_tail(ret_val, 3 * NODE_SIZE, next); // 7

            next = insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 8

            tail(ret_val, next); // 8

        } else if (min_max[0] > REG_ZERO && min_max[1] == REG_INFINITY) {
            /* Node structure for (x){m,}? construct.
             *       ______8_________________
             *      |         _______3_____  \
             *      | _7__   |    1_  _2   \  \
             *      |/    |  |    / |/ |    \  \
             *   I~ B~ N~ B~ (...)~ C~ T~m K~ K~ N~
             *    \_____\__\_|          \_4___|  |
             *       9   \  \_________5__________|
             *            \_______6______________|
             */

            tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces)); // 1

            next = emit_special(TEST_COUNT, min_max[0], pContext.Num_Braces); // 2,4

            tail(ret_val, next);                         // 2
            tail(emit_node(BACK), ret_val);              // 3
            tail(ret_val, emit_node(BACK));              // 4
            insert(BRANCH,  ret_val, 0UL, 0UL, 0);  // 5,7
            insert(NOTHING, ret_val, 0UL, 0UL, 0); // 6

            next = emit_node(NOTHING); // 5,6

            offset_tail(ret_val, NODE_SIZE, next);                   // 5
            tail(ret_val, next);                                     // 6
            insert(BRANCH, ret_val, 0UL, 0UL, 0);              // 7,8
            tail(ret_val, ret_val + (2 * NODE_SIZE));                // 7
            offset_tail(ret_val, 3 * NODE_SIZE, ret_val);            // 8
            insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 9
            tail(ret_val, ret_val + INDEX_SIZE + (4 * NODE_SIZE));   // 9

        } else {
            /* Node structure for (x){m,n}? construct.
             *       ______9_____________________
             *      |         _____________3___  \
             *      | __8_   |    1_  _2       \  \
             *      |/    |  |    / |/ |        \  \
             *   I~ B~ N~ B~ (...)~ C~ T~x T~m K~ K~ N~
             *    \_____\__\_|          \   \__4__|  |
             *      10   \  \            \_7_________|
             *            \  \_________6_____________|
             *             \_______5_________________|
             */

            tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces)); // 1

            next = emit_special(TEST_COUNT, min_max[1], pContext.Num_Braces); // 2,7

            tail(ret_val, next); // 2

            next = emit_special(TEST_COUNT, min_max[0], pContext.Num_Braces); // 4

            tail(emit_node(BACK), ret_val);              // 3
            tail(next, emit_node(BACK));                 // 4
            (void)insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 6,8
            (void)insert(NOTHING, ret_val, 0UL, 0UL, 0); // 5
            (void)insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 8,9

            next = emit_node(NOTHING); // 5,6,7

            offset_tail(ret_val, NODE_SIZE, next);                 // 5
            offset_tail(ret_val, 2 * NODE_SIZE, next);             // 6
            offset_tail(ret_val, 3 * NODE_SIZE, next);             // 7
            tail(ret_val, ret_val + (2 * NODE_SIZE));              // 8
            offset_tail(next, -NODE_SIZE, ret_val);                // 9
            insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces);     // 10
            tail(ret_val, ret_val + INDEX_SIZE + (4 * NODE_SIZE)); // 10
        }

        pContext.Num_Braces++;
    } else if (op_code == '{') {
        if (min_max[0] == REG_ZERO && min_max[1] != REG_INFINITY) {
            /* Node structure for (x){0,n} or (x){,n} construct.
             *
             *       ___3____________
             *      |       1_  _2   \   5_
             *      |       / |/ |    \  / |
             *   I~ B~ (...)~ C~ T~x K~ B~ N~
             *    \_|\            \_6___|__|
             *    7   \________4________|
             */

            tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces)); // 1

            next = emit_special(TEST_COUNT, min_max[1], pContext.Num_Braces); // 2,6

            tail(ret_val, next);                        // 2
            (void)insert(BRANCH, ret_val, 0UL, 0UL, 0); // 3,4,7
            tail(emit_node(BACK), ret_val);             // 3

            next = emit_node(BRANCH); // 4,5

            tail(ret_val, next);                   // 4
            tail(next, emit_node(NOTHING));        // 5,6
            offset_tail(ret_val, NODE_SIZE, next); // 6

            next = insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 7

            tail(ret_val, next); // 7

        } else if (min_max[0] > REG_ZERO && min_max[1] == REG_INFINITY) {
            /* Node structure for (x){m,} construct.
             *       __________4________
             *      |    __3__________  \
             *     _|___|    1_  _2   \  \    _7
             *    / | 8 |    / |/ |    \  \  / |
             *   I~ B~  (...)~ C~ T~m K~ K~ B~ N~
             *       \             \_5___|  |
             *        \__________6__________|
             */

            tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces)); // 1

            next = emit_special(TEST_COUNT, min_max[0], pContext.Num_Braces); // 2

            tail(ret_val, next);                        // 2
            tail(emit_node(BACK), ret_val);             // 3
            insert(BRANCH, ret_val, 0UL, 0UL, 0); // 4,6

            next = emit_node(BACK); // 4

            tail(next, ret_val);                   // 4
            offset_tail(ret_val, NODE_SIZE, next); // 5
            tail(ret_val, emit_node(BRANCH));      // 6
            tail(ret_val, emit_node(NOTHING));     // 7

            insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 8

            tail(ret_val, ret_val + INDEX_SIZE + (2 * NODE_SIZE)); // 8

        } else {
            /* Node structure for (x){m,n} construct.
             *       _____6________________
             *      |   _____________3___  \
             *    9_|__|    1_  _2       \  \    _8
             *    / |  |    / |/ |        \  \  / |
             *   I~ B~ (...)~ C~ T~x T~m K~ K~ B~ N~
             *       \            \   \__4__|  |  |
             *        \            \_7_________|__|
             *         \_________5_____________|
             */

            tail(ret_val, emit_special(INC_COUNT, 0UL, pContext.Num_Braces)); // 1

            next = emit_special(TEST_COUNT, min_max[1], pContext.Num_Braces); // 2,4

            tail(ret_val, next); // 2

            next = emit_special(TEST_COUNT, min_max[0], pContext.Num_Braces); // 4

            tail(emit_node(BACK), ret_val);             // 3
            tail(next, emit_node(BACK));                // 4
            insert(BRANCH, ret_val, 0UL, 0UL, 0); // 5,6

            next = emit_node(BRANCH); // 5,8

            tail(ret_val, next);                    // 5
            offset_tail(next, -NODE_SIZE, ret_val); // 6

            next = emit_node(NOTHING); // 7,8

            offset_tail(ret_val, NODE_SIZE, next); // 7

            offset_tail(next, -NODE_SIZE, next);                     // 8
            insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 9
            tail(ret_val, ret_val + INDEX_SIZE + (2 * NODE_SIZE));   // 9
        }

        pContext.Num_Braces++;
    } else {
        /* We get here if the IS_QUANTIFIER macro is not coordinated properly
           with this function. */

        raise<regex_error>("internal error #2, 'piece'");
    }

    if (IS_QUANTIFIER(*pContext.Reg_Parse)) {
        if (op_code == '{') {
            raise<regex_error>("nested quantifiers, {m,n}%c", *pContext.Reg_Parse);
        } else {
            raise<regex_error>("nested quantifiers, %c%c", op_code, *pContext.Reg_Parse);
        }
    }

    return ret_val;
}

/*----------------------------------------------------------------------*
 * alternative
 *
 * Processes one alternative of an '|' operator.  Connects the NEXT
 * pointers of each regex atom together sequentialy.
 *----------------------------------------------------------------------*/
uint8_t *alternative(int *flag_param, len_range *range_param) {

    uint8_t *ret_val;
    uint8_t *chain;
    uint8_t *latest;
    int flags_local;
    len_range range_local;

    *flag_param = WORST;    // Tentatively.
    range_param->lower = 0; // Idem
    range_param->upper = 0;

    ret_val = emit_node(BRANCH);
    chain = nullptr;

    /* Loop until we hit the start of the next alternative, the end of this set
       of alternatives (end of parentheses), or the end of the regex. */

    while (*pContext.Reg_Parse != '|' && *pContext.Reg_Parse != ')' && pContext.Reg_Parse != pContext.Reg_Parse_End) {
        latest = piece(&flags_local, &range_local);

        if(!latest)
            return nullptr; // Something went wrong.

        *flag_param |= flags_local & HAS_WIDTH;
        if (range_local.lower < 0) {
            // Not a fixed length
            range_param->lower = -1;
            range_param->upper = -1;
        } else if (range_param->lower >= 0) {
            range_param->lower += range_local.lower;
            range_param->upper += range_local.upper;
        }

        if (chain) { // Connect the regex atoms together sequentialy.
            tail(chain, latest);
        }

        chain = latest;
    }

    if(!chain) { // Loop ran zero times.
        emit_node(NOTHING);
    }

    return ret_val;
}

/*----------------------------------------------------------------------*
 * chunk                                                                *
 *                                                                      *
 * Process main body of regex or process a parenthesized "thing".       *
 *                                                                      *
 * Caller must absorb opening parenthesis.                              *
 *                                                                      *
 * Combining parenthesis handling with the base level of regular        *
 * expression is a trifle forced, but the need to tie the tails of the  *
 * branches to what follows makes it hard to avoid.                     *
 *----------------------------------------------------------------------*/
uint8_t *chunk(int paren, int *flag_param, len_range *range_param) {

    uint8_t *ret_val = nullptr;
    uint8_t *this_branch;
    uint8_t *ender = nullptr;
    size_t this_paren = 0;
    int flags_local;
    int first = 1;
    int zero_width;
    int old_sensitive = pContext.Is_Case_Insensitive;
    int old_newline = pContext.Match_Newline;
    len_range range_local;
    int look_only = 0;
    uint8_t *emit_look_behind_bounds = nullptr;

    *flag_param = HAS_WIDTH; // Tentatively.
    range_param->lower = 0;  // Idem
    range_param->upper = 0;

    // Make an OPEN node, if parenthesized.

    if (paren == PAREN) {
        if (pContext.Total_Paren >= NSUBEXP) {
            raise<regex_error>("number of ()'s > %d", static_cast<int>(NSUBEXP));
        }

        this_paren = pContext.Total_Paren;
        pContext.Total_Paren++;
        ret_val = emit_node(OPEN + this_paren);
    } else if (paren == POS_AHEAD_OPEN || paren == NEG_AHEAD_OPEN) {
        *flag_param = WORST; // Look ahead is zero width.
        look_only = 1;
        ret_val = emit_node(paren);
    } else if (paren == POS_BEHIND_OPEN || paren == NEG_BEHIND_OPEN) {
        *flag_param = WORST; // Look behind is zero width.
        look_only = 1;
        // We'll overwrite the zero length later on, so we save the ptr
        ret_val = emit_special(paren, 0, 0);
        emit_look_behind_bounds = ret_val + NODE_SIZE;
    } else if (paren == INSENSITIVE) {
        pContext.Is_Case_Insensitive = 1;
    } else if (paren == SENSITIVE) {
        pContext.Is_Case_Insensitive = 0;
    } else if (paren == NEWLINE) {
        pContext.Match_Newline = 1;
    } else if (paren == NO_NEWLINE) {
        pContext.Match_Newline = 0;
    }

    // Pick up the branches, linking them together.

    do {
        this_branch = alternative(&flags_local, &range_local);

        if (this_branch == nullptr)
            return nullptr;

        if (first) {
            first = 0;
            *range_param = range_local;
            if (ret_val == nullptr)
                ret_val = this_branch;
        } else if (range_param->lower >= 0) {
            if (range_local.lower >= 0) {
                if (range_local.lower < range_param->lower)
                    range_param->lower = range_local.lower;
                if (range_local.upper > range_param->upper)
                    range_param->upper = range_local.upper;
            } else {
                range_param->lower = -1; // Branches have different lengths
                range_param->upper = -1;
            }
        }

        tail(ret_val, this_branch); // Connect BRANCH -> BRANCH.

        /* If any alternative could be zero width, consider the whole
           parenthisized thing to be zero width. */

        if (!(flags_local & HAS_WIDTH))
            *flag_param &= ~HAS_WIDTH;

        // Are there more alternatives to process?

        if (*pContext.Reg_Parse != '|')
            break;

        ++pContext.Reg_Parse;
    } while (1);

    // Make a closing node, and hook it on the end.

    if (paren == PAREN) {
        ender = emit_node(CLOSE + this_paren);

    } else if (paren == NO_PAREN) {
        ender = emit_node(END);

    } else if (paren == POS_AHEAD_OPEN || paren == NEG_AHEAD_OPEN) {
        ender = emit_node(LOOK_AHEAD_CLOSE);

    } else if (paren == POS_BEHIND_OPEN || paren == NEG_BEHIND_OPEN) {
        ender = emit_node(LOOK_BEHIND_CLOSE);

    } else {
        ender = emit_node(NOTHING);
    }

    tail(ret_val, ender);

    // Hook the tails of the branch alternatives to the closing node.

    for (this_branch = ret_val; this_branch != nullptr;) {
        branch_tail(this_branch, NODE_SIZE, ender);
        this_branch = next_ptr(this_branch);
    }

    // Check for proper termination.

    if (paren != NO_PAREN && *pContext.Reg_Parse++ != ')') {
        raise<regex_error>("missing right parenthesis ')'");
    } else if (paren == NO_PAREN && pContext.Reg_Parse != pContext.Reg_Parse_End) {
        if (*pContext.Reg_Parse == ')') {
            raise<regex_error>("missing left parenthesis '('");
        } else {
            raise<regex_error>("junk on end"); // "Can't happen" - NOTREACHED
        }
    }

    // Check whether look behind has a fixed size

    if (emit_look_behind_bounds) {
        if (range_param->lower < 0) {
            raise<regex_error>("look-behind does not have a bounded size");
        }
        if (range_param->upper > 65535L) {
            raise<regex_error>("max. look-behind size is too large (>65535)");
        }
        if (pContext.Code_Emit_Ptr != &Compute_Size) {
            *emit_look_behind_bounds++ = PUT_OFFSET_L(range_param->lower);
            *emit_look_behind_bounds++ = PUT_OFFSET_R(range_param->lower);
            *emit_look_behind_bounds++ = PUT_OFFSET_L(range_param->upper);
            *emit_look_behind_bounds = PUT_OFFSET_R(range_param->upper);
        }
    }

    // For look ahead/behind, the length must be set to zero again
    if (look_only) {
        range_param->lower = 0;
        range_param->upper = 0;
    }

    zero_width = 0;

    /* Set a bit in Closed_Parens to let future calls to function 'back_ref'
       know that we have closed this set of parentheses. */

    if (paren == PAREN && this_paren <= pContext.Closed_Parens.size()) {
        pContext.Closed_Parens[this_paren] = true;

        /* Determine if a parenthesized expression is modified by a quantifier
           that can have zero width. */

        if (*pContext.Reg_Parse == '?' || *pContext.Reg_Parse == '*') {
            zero_width++;
        } else if (*pContext.Reg_Parse == '{' && pContext.Brace_Char == '{') {
            if (pContext.Reg_Parse[1] == ',' || pContext.Reg_Parse[1] == '}') {
                zero_width++;
            } else if (pContext.Reg_Parse[1] == '0') {
                int i = 2;

                while (pContext.Reg_Parse[i] == '0') {
                    i++;
                }

                if (pContext.Reg_Parse[i] == ',') {
                    zero_width++;
                }
            }
        }
    }

    /* If this set of parentheses is known to never match the empty string, set
       a bit in Paren_Has_Width to let future calls to function back_ref know
       that this set of parentheses has non-zero width.  This will allow star
       (*) or question (?) quantifiers to be aplied to a back-reference that
       refers to this set of parentheses. */

    if ((*flag_param & HAS_WIDTH) && paren == PAREN && !zero_width && this_paren <= pContext.Paren_Has_Width.size()) {

        pContext.Paren_Has_Width[this_paren] = true;
    }

    pContext.Is_Case_Insensitive = old_sensitive;
    pContext.Match_Newline = old_newline;

    return ret_val;
}

}


void create_regex(regexp *re, view::string_view exp, int defaultFlags) {
    // NOTE(eteran): previously uninitialized
    std::fill(re->startp.begin(), re->startp.end(), nullptr);
    std::fill(re->endp.begin(), re->endp.end(), nullptr);
    re->extentpBW = nullptr;
    re->extentpFW = nullptr;
    re->top_branch = 0;

    int flags_local;
    len_range range_local;

    if (pContext.Enable_Counting_Quantifier) {
        pContext.Brace_Char = '{';
        pContext.Meta_Char = &Default_Meta_Char[0];
    } else {
        pContext.Brace_Char = '*';                  // Bypass the '{' in
        pContext.Meta_Char = &Default_Meta_Char[1]; // Default_Meta_Char
    }

    // Initialize arrays used by function 'shortcut_escape'.
    if (!init_ansi_classes()) {
        raise<regex_error>("internal error #1, 'CompileRE'");
    }

    pContext.Code_Emit_Ptr = &Compute_Size;
    pContext.Reg_Size = 0UL;

    /* We can't allocate space until we know how big the compiled form will be,
       but we can't compile it (and thus know how big it is) until we've got a
       place to put the code.  So we cheat: we compile it twice, once with code
       generation turned off and size counting turned on, and once "for real".
       This also means that we don't allocate space until we are sure that the
       thing really will compile successfully, and we never have to move the
       code and thus invalidate pointers into it.  (Note that it has to be in
       one piece because free() must be able to free it all.) */

    for (int pass = 1; pass <= 2; pass++) {
        /*-------------------------------------------*
         * FIRST  PASS: Determine size and legality. *
         * SECOND PASS: Emit code.                   *
         *-------------------------------------------*/

        /*  Schwarzenberg:
         *  If defaultFlags = 0 use standard defaults:
         *    Is_Case_Insensitive: Case sensitive is the default
         *    Match_Newline:       Newlines are NOT matched by default
         *                         in character classes
         */
        pContext.Is_Case_Insensitive = ((defaultFlags & REDFLT_CASE_INSENSITIVE) ? 1 : 0);
        pContext.Match_Newline = 0; /* ((defaultFlags & REDFLT_MATCH_NEWLINE)   ? 1 : 0);
                              Currently not used. Uncomment if needed. */

        pContext.Reg_Parse       = exp.begin();
        pContext.Reg_Parse_End   = exp.end();
        pContext.Total_Paren     = 1;
        pContext.Num_Braces      = 0;
        pContext.Closed_Parens   = 0;
        pContext.Paren_Has_Width = 0;

        emit_byte(MAGIC);
        emit_byte('%'); // Placeholder for num of capturing parentheses.
        emit_byte('%'); // Placeholder for num of general {m,n} constructs.

        if (chunk(NO_PAREN, &flags_local, &range_local) == nullptr) {
            raise<regex_error>("internal error #10, 'CompileRE'");
        }

        if (pass == 1) {
            if (pContext.Reg_Size >= MAX_COMPILED_SIZE) {
                /* Too big for NEXT pointers NEXT_PTR_SIZE bytes long to span.
                   This is a real issue since the first BRANCH node usually points
                   to the end of the compiled regex code. */

                raise<regex_error>("regexp > %lu bytes", MAX_COMPILED_SIZE);
            }

            // Allocate memory.

            re->program = new uint8_t[pContext.Reg_Size];
            pContext.Code_Emit_Ptr = re->program;
        }
    }

    re->program[1] = static_cast<uint8_t>(pContext.Total_Paren - 1);
    re->program[2] = static_cast<uint8_t>(pContext.Num_Braces);

    /*----------------------------------------*
     * Dig out information for optimizations. *
     *----------------------------------------*/

    re->match_start = '\0'; // Worst-case defaults.
    re->anchor = 0;

    // First BRANCH.

    uint8_t *scan = (re->program + REGEX_START_OFFSET);

    if (GET_OP_CODE(next_ptr(scan)) == END) { // Only one top-level choice.
        scan = OPERAND(scan);

        // Starting-point info.

        if (GET_OP_CODE(scan) == EXACTLY) {
            re->match_start = *OPERAND(scan);

        } else if (PLUS <= GET_OP_CODE(scan) && GET_OP_CODE(scan) <= LAZY_PLUS) {

            /* Allow x+ or x+? at the start of the regex to be
               optimized. */

            if (GET_OP_CODE(scan + NODE_SIZE) == EXACTLY) {
                re->match_start = *OPERAND(scan + NODE_SIZE);
            }
        } else if (GET_OP_CODE(scan) == BOL) {
            re->anchor++;
        }
    }
}

/**
 * @brief perform_substitution
 * @param re
 * @param source
 * @param dest
 * @return
 */
bool perform_substitution(const regexp *re, view::string_view source, std::string &dest) {
    char test;

    if (U_CHAR_AT(re->program) != MAGIC) {
        reg_error("damaged regexp passed to 'SubstituteRE'");
        return false;
    }

    auto src = source.begin();
    auto dst = std::back_inserter(dest);

    while (src != source.end()) {

        char c = *src++;

        char chgcase = '\0';
        int paren_no = -1;

        if (c == '\\') {
            // Process any case altering tokens, i.e \u, \U, \l, \L.

            if (*src == 'u' || *src == 'U' || *src == 'l' || *src == 'L') {
                chgcase = *src++;

                if (src == source.end()) {
                    break;
                }

                c = *src++;
            }
        }

        if (c == '&') {
            paren_no = 0;
        } else if (c == '\\') {
            /* Can not pass register variable '&src' to function 'numeric_escape'
               so make a non-register copy that we can take the address of. */

            decltype(src) src_alias = src;

            if ('1' <= *src && *src <= '9') {
                paren_no = *src++ - '0';

            } else if ((test = literal_escape(*src)) != '\0') {
                c = test;
                src++;

            } else if ((test = numeric_escape(*src, &src_alias)) != '\0') {
                c   = test;
                src = src_alias;
                src++;

                /* NOTE: if an octal escape for zero is attempted (e.g. \000), it
                   will be treated as a literal string. */
            } else if (src == source.end()) {
                /* If '\' is the last character of the replacement string, it is
                   interpreted as a literal backslash. */

                c = '\\';
            } else {
                c = *src++; // Allow any escape sequence (This is
            }               // INCONSISTENT with the 'CompileRE'
        }                   // mind set of issuing an error!

        if (paren_no < 0) { // Ordinary character.
            *dst++ = c;
        } else if (re->startp[paren_no] != nullptr && re->endp[paren_no]) {

            /* The tokens \u and \l only modify the first character while the
             * tokens \U and \L modify the entire string. */
            switch(chgcase) {
            case 'u':
                {
                    int count = 0;
                    std::transform(re->startp[paren_no], re->endp[paren_no], dst, [&count](char ch) -> int {
                        if(count++ == 0) {
                            return safe_ctype<toupper>(ch);
                        } else {
                            return ch;
                        }
                    });
                }
                break;
            case 'U':
                std::transform(re->startp[paren_no], re->endp[paren_no], dst, [](char ch) {
                    return safe_ctype<toupper>(ch);
                });
                break;
            case 'l':
                {
                    int count = 0;
                    std::transform(re->startp[paren_no], re->endp[paren_no], dst, [&count](char ch) -> int {
                        if(count++ == 0) {
                            return safe_ctype<tolower>(ch);
                        } else {
                            return ch;
                        }
                    });
                }
                break;
            case 'L':
                std::transform(re->startp[paren_no], re->endp[paren_no], dst, [](char ch) {
                    return safe_ctype<tolower>(ch);
                });
                break;
            default:
                std::copy(re->startp[paren_no], re->endp[paren_no], dst);
                break;
            }

        }
    }

    return true;
}
