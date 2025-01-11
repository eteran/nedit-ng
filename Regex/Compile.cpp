
#include "Compile.h"
#include "Common.h"
#include "Constants.h"
#include "Execute.h"
#include "Opcodes.h"
#include "Regex.h"
#include "RegexError.h"
#include "Util/Compiler.h"
#include "Util/Raise.h"
#include "Util/utils.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <gsl/gsl_util>
#include <limits>

namespace {

const auto FirstPassToken = reinterpret_cast<uint8_t *>(1);

// Flags for function shortcut_escape()
enum ShortcutEscapeFlag {
	CHECK_ESCAPE       = 0, // Check an escape sequence for validity only.
	CHECK_CLASS_ESCAPE = 1, // Check the validity of an escape within a character class
	EMIT_CLASS_BYTES   = 2, // Emit equivalent character class bytes, e.g \d=0123456789
	EMIT_NODE          = 3, // Emit the appropriate node.
};

// Flags to be passed up and down via function parameters during compile.
constexpr int WORST     = 0; // Worst case. No assumptions can be made.
constexpr int HAS_WIDTH = 1; // Known never to match null string.
constexpr int SIMPLE    = 2; // Simple enough to be STAR/PLUS operand.

constexpr int NO_PAREN    = 0; // Only set by initial call to "chunk".
constexpr int PAREN       = 1; // Used for normal capturing parentheses.
constexpr int NO_CAPTURE  = 2; // Non-capturing parentheses (grouping only).
constexpr int INSENSITIVE = 3; // Case insensitive parenthetical construct
constexpr int SENSITIVE   = 4; // Case sensitive parenthetical construct
constexpr int NEWLINE     = 5; // Construct to match newlines in most cases
constexpr int NO_NEWLINE  = 6; // Construct to match newlines normally

// Largest size a compiled regex can be. Probably could be 65535UL.
constexpr size_t MaxCompiledSize = std::numeric_limits<int16_t>::max();

struct len_range {
	int32_t lower;
	int32_t upper;
};

uint8_t *chunk(int paren, int *flag_param, len_range &range_param);

const char Default_Meta_Char[] = "{.*+?[(|)^<>$";
const char ASCII_Digits[]      = "0123456789"; // Same for all locales.

// Array sizes for arrays used by function init_ansi_classes.
constexpr int WHITE_SPACE_SIZE = 16;
constexpr int ALNUM_CHAR_SIZE  = 256;

char White_Space[WHITE_SPACE_SIZE]; // Arrays used by
char Word_Char[ALNUM_CHAR_SIZE];    // functions
char Letter_Char[ALNUM_CHAR_SIZE];  // init_ansi_classes () and shortcut_escape ().

/*----------------------------------------------------------------------*
 * next_ptr - compute the address of a node's "NEXT" pointer.
 * Note: a simplified inline version is available via NEXT_PTR(),
 *----------------------------------------------------------------------*/
uint8_t *next_ptr(uint8_t *ptr) noexcept {

	if (pContext.FirstPass) {
		return nullptr;
	}

	const int offset = GET_OFFSET(ptr);

	if (offset == 0) {
		return nullptr;
	}

	if (GET_OP_CODE(ptr) == BACK) {
		return (ptr - offset);
	}

	return (ptr + offset);
}

/**
 * @brief PUT_OFFSET_L
 * @param v
 * @return
 */
template <class T>
constexpr uint8_t PUT_OFFSET_L(T v) noexcept {
	return static_cast<uint8_t>((v >> 8) & 0xff);
}

/**
 * @brief PUT_OFFSET_R
 * @param v
 * @return
 */
template <class T>
constexpr uint8_t PUT_OFFSET_R(T v) noexcept {
	return static_cast<uint8_t>(v & 0xff);
}

/**
 * @brief isQuantifier
 * @param c
 * @return
 */
bool isQuantifier(char ch) noexcept {
	return ch == '*' || ch == '+' || ch == '?' || ch == pContext.Brace_Char;
}

/*--------------------------------------------------------------------*
 * init_ansi_classes
 *
 * Generate character class sets using locale aware ANSI C functions.
 *
 *--------------------------------------------------------------------*/
bool init_ansi_classes() noexcept {

	static bool initialized = false;

	if (!initialized) {
		initialized = true; // Only need to generate character sets once.

		constexpr char Underscore = '_';
		constexpr char Newline    = '\n';

		int word_count   = 0;
		int letter_count = 0;
		int space_count  = 0;

		for (int i = 1; i < UINT8_MAX; i++) {

			const auto ch = static_cast<char>(i);

			if (safe_isalnum(ch) || ch == Underscore) {
				Word_Char[word_count++] = ch;
			}

			if (safe_isalpha(ch)) {
				Letter_Char[letter_count++] = ch;
			}

			/* Note: Whether or not newline is considered to be whitespace is
			   handled by switches within the original regex and is thus omitted
			   here. */
			if (safe_isspace(ch) && (ch != Newline)) {
				White_Space[space_count++] = ch;
			}

			/* Make sure arrays are big enough.  ("- 2" because of zero array
			   origin and we need to leave room for the '\0' terminator.) */
			if (word_count > (ALNUM_CHAR_SIZE - 2) || space_count > (WHITE_SPACE_SIZE - 2) || letter_count > (ALNUM_CHAR_SIZE - 2)) {
				reg_error("internal error #9 'init_ansi_classes'");
				return false;
			}
		}

		Word_Char[word_count]     = '\0';
		Letter_Char[letter_count] = '\0';
		White_Space[space_count]  = '\0';
	}

	return true;
}

/*----------------------------------------------------------------------*
 * emit_node
 *
 * Emit (if appropriate) the op code for a regex node atom.
 *
 * The NEXT pointer is initialized to 0x0000.
 *
 * Returns a pointer to the START of the emitted node.
 *----------------------------------------------------------------------*/
template <class T>
uint8_t *emit_node(T op_code) noexcept {

	if (pContext.FirstPass) {
		pContext.Reg_Size += NODE_SIZE<size_t>;
		return FirstPassToken;
	}

	const size_t end_offset = pContext.Code.size();
	pContext.Code.push_back(static_cast<uint8_t>(op_code));
	pContext.Code.push_back(0);
	pContext.Code.push_back(0);
	return &pContext.Code[end_offset];
}

/*----------------------------------------------------------------------*
 * emit_byte
 *
 * Emit (if appropriate) a byte of code (usually part of an operand.)
 *----------------------------------------------------------------------*/
template <class T>
void emit_byte(T ch) noexcept {

	if (pContext.FirstPass) {
		pContext.Reg_Size++;
	} else {
		pContext.Code.push_back(static_cast<uint8_t>(ch));
	}
}

/*----------------------------------------------------------------------*
 * emit_class_byte
 *
 * Emit (if appropriate) a byte of code (usually part of a character
 * class operand.)
 *----------------------------------------------------------------------*/
template <class T>
void emit_class_byte(T ch) noexcept {

	if (pContext.FirstPass) {
		pContext.Reg_Size++;

		if (pContext.Is_Case_Insensitive && safe_isalpha(ch)) {
			pContext.Reg_Size++;
		}

	} else {
		if (pContext.Is_Case_Insensitive && safe_isalpha(ch)) {
			/* For case insensitive character classes, emit both upper and lower
			 * case versions of alphabetical characters. */
			pContext.Code.push_back(static_cast<uint8_t>(safe_tolower(ch)));
			pContext.Code.push_back(static_cast<uint8_t>(safe_toupper(ch)));
		} else {
			pContext.Code.push_back(static_cast<uint8_t>(ch));
		}
	}
}

/*----------------------------------------------------------------------*
 * emit_special
 *
 * Emit nodes that need special processing.
 *----------------------------------------------------------------------*/
template <class Ch>
uint8_t *emit_special(Ch op_code, uint32_t test_val, size_t index) noexcept {

	if (pContext.FirstPass) {
		switch (op_code) {
		case POS_BEHIND_OPEN:
		case NEG_BEHIND_OPEN:
			pContext.Reg_Size += LENGTH_SIZE<size_t>; // Length of the look-behind match
			pContext.Reg_Size += NODE_SIZE<size_t>;   // Make room for the node
			break;

		case TEST_COUNT:
			pContext.Reg_Size += NEXT_PTR_SIZE<size_t>; // Make room for a test value.
			[[fallthrough]];
		case INC_COUNT:
			pContext.Reg_Size += INDEX_SIZE<size_t>; // Make room for an index value.
			[[fallthrough]];
		default:
			pContext.Reg_Size += NODE_SIZE<size_t>; // Make room for the node.
		}

		return FirstPassToken;
	}

	uint8_t *ret_val = emit_node(op_code); // Return the address for start of node.
	if (op_code == INC_COUNT || op_code == TEST_COUNT) {
		pContext.Code.push_back(index & 0xff);

		if (op_code == TEST_COUNT) {
			pContext.Code.push_back(PUT_OFFSET_L(test_val));
			pContext.Code.push_back(PUT_OFFSET_R(test_val));
		}
	} else if (op_code == POS_BEHIND_OPEN || op_code == NEG_BEHIND_OPEN) {
		pContext.Code.push_back(PUT_OFFSET_L(test_val));
		pContext.Code.push_back(PUT_OFFSET_R(test_val));
		pContext.Code.push_back(PUT_OFFSET_L(test_val));
		pContext.Code.push_back(PUT_OFFSET_R(test_val));
	}
	return ret_val;
}

/*----------------------------------------------------------------------*
 * tail - Set the next-pointer at the end of a node chain.
 *----------------------------------------------------------------------*/
void tail(uint8_t *search_from, const uint8_t *point_to) {

	if (pContext.FirstPass) {
		return;
	}

	// Find the last node in the chain (node with a null NEXT pointer)
	uint8_t *scan = search_from;

	while (uint8_t *next = next_ptr(scan)) {
		scan = next;
	}

	int64_t offset;
	if (GET_OP_CODE(scan) == BACK) {
		offset = scan - point_to;
	} else {
		offset = point_to - scan;
	}

	// Set NEXT pointer
	scan[1] = PUT_OFFSET_L(offset);
	scan[2] = PUT_OFFSET_R(offset);
}

/*----------------------------------------------------------------------*
 * insert
 *
 * Insert a node in front of already emitted node(s).  Means relocating
 * the operand. The parameter 'insert_pos' points to the location
 * where the new node is to be inserted.
 *----------------------------------------------------------------------*/
uint8_t *insert(uint8_t op, const uint8_t *insert_pos, uint32_t min, uint32_t max, uint16_t index) {

	if (pContext.FirstPass) {

		size_t insert_size = NODE_SIZE<size_t>;

		if (op == BRACE || op == LAZY_BRACE) {
			// Make room for the min and max values.
			insert_size += (2 * NEXT_PTR_SIZE<size_t>);
		} else if (op == INIT_COUNT) {
			// Make room for an index value.
			insert_size += INDEX_SIZE<size_t>;
		}

		pContext.Reg_Size += insert_size;
		return FirstPassToken;
	}

	// Where operand used to be.
	const ptrdiff_t offset = insert_pos - pContext.Code.data();

	// assemble the new node in place, then insert it
	uint8_t new_node[16];
	uint8_t *ptr = new_node;

	*ptr++ = op;
	*ptr++ = 0;
	*ptr++ = 0;

	switch (op) {
	case BRACE:
	case LAZY_BRACE:
		*ptr++ = PUT_OFFSET_L(min);
		*ptr++ = PUT_OFFSET_R(min);

		*ptr++ = PUT_OFFSET_L(max);
		*ptr++ = PUT_OFFSET_R(max);
		break;
	case INIT_COUNT:
		*ptr++ = (index & 0xff);
	}

	pContext.Code.insert(pContext.Code.begin() + offset, new_node, ptr);
	return &pContext.Code[offset]; // Return a pointer to the start of the code moved.
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
template <ShortcutEscapeFlag Flags, class Ch>
uint8_t *shortcut_escape(Ch ch, int *flag_param) {

	static const char codes[] = "ByYdDlLsSwW";

	const char *clazz = nullptr;
	auto ret_val      = FirstPassToken; // Assume success.
	const char *valid_codes;

	if (Flags == EMIT_CLASS_BYTES || Flags == CHECK_CLASS_ESCAPE) {
		valid_codes = codes + 3; // \B, \y and \Y are not allowed in classes
	} else {
		valid_codes = codes;
	}

	if (!::strchr(valid_codes, static_cast<int>(ch))) {
		return nullptr; // Not a valid shortcut escape sequence
	}

	if (Flags == CHECK_ESCAPE || Flags == CHECK_CLASS_ESCAPE) {
		return ret_val; // Just checking if this is a valid shortcut escape.
	}

	switch (ch) {
	case 'd':
	case 'D':
		if (Flags == EMIT_CLASS_BYTES) {
			clazz = ASCII_Digits;
		} else if (Flags == EMIT_NODE) {
			ret_val = (safe_islower(ch) ? emit_node(DIGIT) : emit_node(NOT_DIGIT));
		}
		break;
	case 'l':
	case 'L':
		if (Flags == EMIT_CLASS_BYTES) {
			clazz = Letter_Char;
		} else if (Flags == EMIT_NODE) {
			ret_val = (safe_islower(ch) ? emit_node(LETTER) : emit_node(NOT_LETTER));
		}
		break;
	case 's':
	case 'S':
		if (Flags == EMIT_CLASS_BYTES) {
			if (pContext.Match_Newline) {
				emit_byte('\n');
			}

			clazz = White_Space;
		} else if (Flags == EMIT_NODE) {
			if (pContext.Match_Newline) {
				ret_val = (safe_islower(ch) ? emit_node(SPACE_NL) : emit_node(NOT_SPACE_NL));
			} else {
				ret_val = (safe_islower(ch) ? emit_node(SPACE) : emit_node(NOT_SPACE));
			}
		}
		break;
	case 'w':
	case 'W':
		if (Flags == EMIT_CLASS_BYTES) {
			clazz = Word_Char;
		} else if (Flags == EMIT_NODE) {
			ret_val = (safe_islower(ch) ? emit_node(WORD_CHAR) : emit_node(NOT_WORD_CHAR));
		}
		break;

		/* Since the delimiter table is not available at regex compile time
		 * \B, \Y and \Y can only generate a node.  At run time, the delimiter
		 * table will be available for these nodes to use. */
	case 'y':
		if (Flags == EMIT_NODE) {
			ret_val = emit_node(IS_DELIM);
		} else {
			Raise<RegexError>("internal error #5 'shortcut_escape'");
		}
		break;

	case 'Y':
		if (Flags == EMIT_NODE) {
			ret_val = emit_node(NOT_DELIM);
		} else {
			Raise<RegexError>("internal error #6 'shortcut_escape'");
		}
		break;
	case 'B':
		if (Flags == EMIT_NODE) {
			ret_val = emit_node(NOT_BOUNDARY);
		} else {
			Raise<RegexError>("internal error #7 'shortcut_escape'");
		}
		break;
	default:
		/* We get here if there isn't a case for every character in
		   the string "codes" */
		Raise<RegexError>("internal error #8 'shortcut_escape'");
	}

	if (Flags == EMIT_NODE && ch != 'B') {
		*flag_param |= (HAS_WIDTH | SIMPLE);
	}

	if (clazz) {
		// Emit bytes within a character class operand.

		// TODO(eteran): maybe emit the length of the string first
		// so we don't have to depend on the NUL character during execution
		while (*clazz != '\0') {
			emit_byte(*clazz++);
		}
	}

	return ret_val;
}

/*--------------------------------------------------------------------*
 * offset_tail
 *
 * Perform a tail operation on (ptr + offset).
 *--------------------------------------------------------------------*/
void offset_tail(uint8_t *ptr, int offset, uint8_t *val) {

	if (pContext.FirstPass || !ptr) {
		return;
	}

	tail(ptr + offset, val);
}

/*--------------------------------------------------------------------*
 * branch_tail
 *
 * Perform a tail operation on (ptr + offset) but only if 'ptr' is a
 * BRANCH node.
 *--------------------------------------------------------------------*/
void branch_tail(uint8_t *ptr, int offset, uint8_t *val) {

	if (pContext.FirstPass || !ptr || GET_OP_CODE(ptr) != BRANCH) {
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
template <ShortcutEscapeFlag Flags>
uint8_t *back_ref(const char *ch, int *flag_param) {

	size_t c_offset           = 0;
	const bool is_cross_regex = false;

	uint8_t *ret_val;

#if 0 // Implement cross regex backreferences later.
	if (*ch == '~') {
		c_offset++;
		is_cross_regex = true;
	}
#endif

	// Only \1, \2, ... \9 are supported.
	if (!safe_isdigit(ch[c_offset])) {
		return nullptr;
	}

	auto paren_no = static_cast<uint8_t>(ch[c_offset] - '0');

	// Should be caught by numeric_escape.
	if (paren_no == 0) {
		return nullptr;
	}

	// Make sure parentheses for requested back-reference are complete.
	if (!is_cross_regex && !pContext.Closed_Parens[paren_no]) {
		Raise<RegexError>("\\%d is an illegal back reference", paren_no);
	}

	if (Flags == EMIT_NODE) {
		if (is_cross_regex) {
			++pContext.Reg_Parse; /* Skip past the '~' in a cross regex back
								   * reference. We only do this if we are emitting code. */

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

		emit_byte(paren_no);

		if (is_cross_regex || pContext.Paren_Has_Width[paren_no]) {
			*flag_param |= HAS_WIDTH;
		}
	} else if (Flags == CHECK_ESCAPE) {
		ret_val = FirstPassToken;
	} else {
		ret_val = nullptr;
	}

	return ret_val;
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
uint8_t *atom(int *flag_param, len_range &range_param) {

	uint8_t *ret_val;
	uint8_t test;
	int flags_local;
	len_range range_local;

	*flag_param       = WORST; // Tentatively.
	range_param.lower = 0;     // Idem
	range_param.upper = 0;

	/* Process any regex comments, e.g. '(?# match next token->)'.  The
	   terminating right parenthesis can not be escaped.  The comment stops at
	   the first right parenthesis encountered (or the end of the regex
	   string)... period.  Handles multiple sequential comments,
	   e.g. '(?# one)(?# two)...'  */

	while (*pContext.Reg_Parse == '(' && pContext.Reg_Parse[1] == '?' && *(pContext.Reg_Parse + 2) == '#') {

		pContext.Reg_Parse += 3;

		while (pContext.Reg_Parse != pContext.InputString.end() && *pContext.Reg_Parse != ')') {
			++pContext.Reg_Parse;
		}

		if (*pContext.Reg_Parse == ')') {
			++pContext.Reg_Parse;
		}

		if (pContext.Reg_Parse == pContext.InputString.end() || *pContext.Reg_Parse == ')' || *pContext.Reg_Parse == '|') {
			/* Hit end of regex string or end of parenthesized regex; have to
			 return "something" (i.e. a NOTHING node) to avoid generating an
			 error. */

			ret_val = emit_node(NOTHING);
			return ret_val;
		}
	}

	if (pContext.Reg_Parse == pContext.InputString.end()) {
		// Supposed to be caught earlier.
		Raise<RegexError>("internal error #3, 'atom'");
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
		range_param.lower = 1;
		range_param.upper = 1;
		break;
	case '(':
		if (*pContext.Reg_Parse == '?') { // Special parenthetical expression
			++pContext.Reg_Parse;
			range_local.lower = 0; // Make sure it is always used
			range_local.upper = 0;

			if (*pContext.Reg_Parse == ':') {
				++pContext.Reg_Parse;
				ret_val = chunk(NO_CAPTURE, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == '=') {
				++pContext.Reg_Parse;
				ret_val = chunk(POS_AHEAD_OPEN, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == '!') {
				++pContext.Reg_Parse;
				ret_val = chunk(NEG_AHEAD_OPEN, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == 'i') {
				++pContext.Reg_Parse;
				ret_val = chunk(INSENSITIVE, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == 'I') {
				++pContext.Reg_Parse;
				ret_val = chunk(SENSITIVE, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == 'n') {
				++pContext.Reg_Parse;
				ret_val = chunk(NEWLINE, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == 'N') {
				++pContext.Reg_Parse;
				ret_val = chunk(NO_NEWLINE, &flags_local, range_local);
			} else if (*pContext.Reg_Parse == '<') {
				++pContext.Reg_Parse;
				if (*pContext.Reg_Parse == '=') {
					++pContext.Reg_Parse;
					ret_val = chunk(POS_BEHIND_OPEN, &flags_local, range_local);
				} else if (*pContext.Reg_Parse == '!') {
					++pContext.Reg_Parse;
					ret_val = chunk(NEG_BEHIND_OPEN, &flags_local, range_local);
				} else {
					Raise<RegexError>("invalid look-behind syntax, \"(?<%c...)\"", *pContext.Reg_Parse);
				}
			} else {
				Raise<RegexError>("invalid grouping syntax, \"(?%c...)\"", *pContext.Reg_Parse);
			}
		} else { // Normal capturing parentheses
			ret_val = chunk(PAREN, &flags_local, range_local);
		}

		if (!ret_val) {
			return nullptr; // Something went wrong.
		}

		// Add HAS_WIDTH flag if it was set by call to chunk.

		*flag_param |= flags_local & HAS_WIDTH;
		range_param = range_local;
		break;
	case '|':
	case ')':
		Raise<RegexError>("internal error #3, 'atom'"); // Supposed to be
														// caught earlier.
	case '?':
	case '+':
	case '*':
		Raise<RegexError>("%c follows nothing", pContext.Reg_Parse[-1]);

	case '{':
		if (ParseContext::Enable_Counting_Quantifier) {
			Raise<RegexError>("{m,n} follows nothing");
		} else {
			ret_val = emit_node(EXACTLY); // Treat braces as literals.
			emit_byte('{');
			emit_byte('\0');
			range_param.lower = 1;
			range_param.upper = 1;
		}

		break;

	case '[': {
		uint8_t last_emit = 0x00;

		// Handle characters that can only occur at the start of a class.

		if (*pContext.Reg_Parse == '^') { // Complement of range.
			ret_val = emit_node(ANY_BUT);
			++pContext.Reg_Parse;

			/* All negated classes include newline unless escaped with
			   a "(?n)" switch. */

			if (!pContext.Match_Newline) {
				emit_byte('\n');
			}
		} else {
			ret_val = emit_node(ANY_OF);
		}

		if (*pContext.Reg_Parse == ']' || *pContext.Reg_Parse == '-') {
			/* If '-' or ']' is the first character in a class,
			   it is a literal character in the class. */

			last_emit = static_cast<uint8_t>(*pContext.Reg_Parse);
			emit_byte(*pContext.Reg_Parse);
			++pContext.Reg_Parse;
		}

		// Handle the rest of the class characters.

		while (pContext.Reg_Parse != pContext.InputString.end() && *pContext.Reg_Parse != ']') {
			if (*pContext.Reg_Parse == '-') { // Process a range, e.g [a-z].
				++pContext.Reg_Parse;

				if (*pContext.Reg_Parse == ']' || pContext.Reg_Parse == pContext.InputString.end()) {
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

					unsigned int last_value;
					unsigned int second_value = static_cast<unsigned int>(last_emit) + 1;

					if (*pContext.Reg_Parse == '\\') {
						/* Handle escaped characters within a class range.
						   Specifically disallow shortcut escapes as the end of
						   a class range.  To allow this would be ambiguous
						   since shortcut escapes represent a set of characters,
						   and it would not be clear which character of the
						   class should be treated as the "last" character. */

						++pContext.Reg_Parse;

						if ((test = numeric_escape<uint8_t>(*pContext.Reg_Parse, &pContext.Reg_Parse))) {
							last_value = test;
						} else if ((test = literal_escape<uint8_t>(*pContext.Reg_Parse))) {
							last_value = test;
						} else if (shortcut_escape<CHECK_CLASS_ESCAPE>(*pContext.Reg_Parse, nullptr)) {
							Raise<RegexError>("\\%c is not allowed as range operand", *pContext.Reg_Parse);
						} else {
							Raise<RegexError>("\\%c is an invalid char class escape sequence", *pContext.Reg_Parse);
						}
					} else {
						last_value = U_CHAR_AT(pContext.Reg_Parse);
					}

					if (pContext.Is_Case_Insensitive) {
						second_value = static_cast<unsigned int>(safe_tolower(second_value));
						last_value   = static_cast<unsigned int>(safe_tolower(last_value));
					}

					/* For case insensitive, something like [A-_] will
					   generate an error here since ranges are converted to
					   lower case. */

					if (second_value - 1 > last_value) {
						Raise<RegexError>("invalid [] range");
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

				if ((test = numeric_escape<uint8_t>(*pContext.Reg_Parse, &pContext.Reg_Parse)) != '\0') {
					emit_class_byte(test);

					last_emit = test;
				} else if ((test = literal_escape<uint8_t>(*pContext.Reg_Parse)) != '\0') {
					emit_byte(test);
					last_emit = test;
				} else if (shortcut_escape<CHECK_CLASS_ESCAPE>(*pContext.Reg_Parse, nullptr)) {

					if (pContext.Reg_Parse[1] == '-') {
						/* Specifically disallow shortcut escapes as the start
						   of a character class range (see comment above.) */

						Raise<RegexError>("\\%c not allowed as range operand", *pContext.Reg_Parse);
					} else {
						/* Emit the bytes that are part of the shortcut
						   escape sequence's range (e.g. \d = 0123456789) */

						shortcut_escape<EMIT_CLASS_BYTES>(*pContext.Reg_Parse, nullptr);
					}
				} else {
					Raise<RegexError>("\\%c is an invalid char class escape sequence", *pContext.Reg_Parse);
				}

				++pContext.Reg_Parse;

				// End of class escaped sequence code
			} else {
				emit_class_byte(*pContext.Reg_Parse); // Ordinary class character.

				last_emit = static_cast<uint8_t>(*pContext.Reg_Parse);
				++pContext.Reg_Parse;
			}
		} // End of while (Reg_Parse != Reg_Parse_End && *pContext.Reg_Parse != ']')

		if (*pContext.Reg_Parse != ']') {
			Raise<RegexError>("missing right ']'");
		}

		emit_byte('\0');

		/* NOTE: it is impossible to specify an empty class.  This is
		   because [] would be interpreted as "begin character class"
		   followed by a literal ']' character and no "end character class"
		   delimiter (']').  Because of this, it is always safe to assume
		   that a class HAS_WIDTH. */

		++pContext.Reg_Parse;
		*flag_param |= HAS_WIDTH | SIMPLE;
		range_param.lower = 1;
		range_param.upper = 1;
	}

	break; // End of character class code.

	case '\\':
		if ((ret_val = shortcut_escape<EMIT_NODE>(*pContext.Reg_Parse, flag_param))) {

			++pContext.Reg_Parse;
			range_param.lower = 1;
			range_param.upper = 1;
			break;

		} else if ((ret_val = back_ref<EMIT_NODE>(pContext.Reg_Parse, flag_param))) {
			/* Can't make any assumptions about a back-reference as to SIMPLE
			   or HAS_WIDTH.  For example (^|<) is neither simple nor has
			   width.  So we don't flip bits in flag_param here. */

			++pContext.Reg_Parse;
			// Back-references always have an unknown length
			range_param.lower = -1;
			range_param.upper = -1;
			break;
		}
		/* At this point it is apparent that the escaped character is not a
		 * shortcut escape or back-reference.  Back up one character to allow
		 * the default code to include it as an ordinary character. */

		/* Fall through to Default case to handle literal escapes and numeric
		 * escapes. */
		[[fallthrough]];
	default:
		--pContext.Reg_Parse; /* If we fell through from the above code, we are now
							   * pointing at the back slash (\) character. */
		{
			std::string_view::iterator parse_save;
			int len = 0;

			if (pContext.Is_Case_Insensitive) {
				ret_val = emit_node(SIMILAR);
			} else {
				ret_val = emit_node(EXACTLY);
			}

			/* Loop until we find a meta character, shortcut escape, back
			 * reference, or end of regex string. */

			for (; pContext.Reg_Parse != pContext.InputString.end() && !::strchr(pContext.Meta_Char, static_cast<int>(*pContext.Reg_Parse)); len++) {
				/* Save where we are in case we have to back
				   this character out. */

				parse_save = pContext.Reg_Parse;

				if (*pContext.Reg_Parse == '\\') {
					++pContext.Reg_Parse; // Point to escaped character

					if ((test = numeric_escape<uint8_t>(*pContext.Reg_Parse, &pContext.Reg_Parse))) {
						if (pContext.Is_Case_Insensitive) {
							emit_byte(tolower(test));
						} else {
							emit_byte(test);
						}
					} else if ((test = literal_escape<uint8_t>(*pContext.Reg_Parse))) {
						emit_byte(test);
					} else if (back_ref<CHECK_ESCAPE>(pContext.Reg_Parse, nullptr)) {
						// Leave back reference for next 'atom' call
						--pContext.Reg_Parse;
						break;
					} else if (shortcut_escape<CHECK_ESCAPE>(*pContext.Reg_Parse, nullptr)) {
						// Leave shortcut escape for next 'atom' call
						--pContext.Reg_Parse;
						break;
					} else {
						/* None of the above calls generated an error message
						   so generate our own here. */

						Raise<RegexError>("\\%c is an invalid escape sequence", *pContext.Reg_Parse);
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

				if (isQuantifier(*pContext.Reg_Parse) && len > 0) {
					pContext.Reg_Parse = parse_save; // Point to previous regex token.

					if (pContext.FirstPass) {
						pContext.Reg_Size--;
					} else {
						pContext.Code.pop_back();
					}
					break;
				}
			}

			if (len <= 0) {
				Raise<RegexError>("internal error #4, 'atom'");
			}

			*flag_param |= HAS_WIDTH;

			if (len == 1) {
				*flag_param |= SIMPLE;
			}

			range_param.lower = len;
			range_param.upper = len;

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
uint8_t *piece(int *flag_param, len_range &range_param) {

	uint8_t *next;
	uint32_t min_max[2] = {0, REG_INFINITY};
	int flags_local;
	int i;
	int brace_present    = 0;
	bool lazy            = false;
	bool comma_present   = false;
	int digit_present[2] = {0, 0};
	len_range range_local;

	uint8_t *ret_val = atom(&flags_local, range_local);

	if (!ret_val) {
		return nullptr; // Something went wrong.
	}

	char op_code = *pContext.Reg_Parse;

	if (!isQuantifier(op_code)) {
		*flag_param = flags_local;
		range_param = range_local;
		return ret_val;
	}

	if (op_code == '{') { // {n,m} quantifier present
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

			while (safe_isdigit(*pContext.Reg_Parse)) {
				// (6553 * 10 + 6) > 65535 (16 bit max)

				// NOTE(eteran): we're storing this into a 32-bit variable... so would be simpler
				// to just convert the number using strtoul and just check if it's too large when
				// we're done

				if ((min_max[i] == 6553UL && (*pContext.Reg_Parse - '0') <= 5) || (min_max[i] <= 6552UL)) {

					min_max[i] = (min_max[i] * 10UL) + static_cast<uint32_t>(*pContext.Reg_Parse - '0');
					++pContext.Reg_Parse;

					digit_present[i]++;
				} else {
					if (i == 0) {
						Raise<RegexError>("min operand of {%lu%c,???} > 65535", min_max[0], *pContext.Reg_Parse);
					} else {
						Raise<RegexError>("max operand of {%lu,%lu%c} > 65535", min_max[0], min_max[1], *pContext.Reg_Parse);
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

		if (digit_present[0] && (min_max[0] == 0) && !comma_present) {

			Raise<RegexError>("{0} is an invalid range");
		} else if (digit_present[0] && (min_max[0] == 0) && digit_present[1] && (min_max[1] == 0)) {

			Raise<RegexError>("{0,0} is an invalid range");
		} else if (digit_present[1] && (min_max[1] == 0)) {
			if (digit_present[0]) {
				Raise<RegexError>("{%lu,0} is an invalid range", min_max[0]);
			} else {
				Raise<RegexError>("{,0} is an invalid range");
			}
		}

		if (!comma_present) {
			min_max[1] = min_max[0]; // {x} means {x,x}
		}

		if (*pContext.Reg_Parse != '}') {
			Raise<RegexError>("{m,n} specification missing right '}'");

		} else if (min_max[1] != REG_INFINITY && min_max[0] > min_max[1]) {
			// Disallow a backward range.

			Raise<RegexError>("{%lu,%lu} is an invalid range", min_max[0], min_max[1]);
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
		if (min_max[0] == 0 && min_max[1] == REG_INFINITY) {
			op_code = '*';
		} else if (min_max[0] == 1 && min_max[1] == REG_INFINITY) {
			op_code = '+';
		} else if (min_max[0] == 0 && min_max[1] == 1) {
			op_code = '?';
		} else if (min_max[0] == 1 && min_max[1] == 1) {
			/* "x{1,1}" is the same as "x".  No need to pollute the compiled
				regex with such nonsense. */

			*flag_param = flags_local;
			range_param = range_local;
			return ret_val;
		} else if (pContext.Num_Braces > static_cast<int>(std::numeric_limits<uint8_t>::max())) {
			Raise<RegexError>("number of {m,n} constructs > %d", UINT8_MAX);
		}
	}

	if (op_code == '+') {
		min_max[0] = 1;
	}
	if (op_code == '?') {
		min_max[1] = 1;
	}

	/* It is dangerous to apply certain quantifiers to a possibly zero width
	   item. */

	if (!(flags_local & HAS_WIDTH)) {
		if (brace_present) {
			Raise<RegexError>("{%lu,%lu} operand could be empty", min_max[0], min_max[1]);
		} else {
			Raise<RegexError>("%c operand could be empty", op_code);
		}
	}

	*flag_param = (min_max[0] > 0) ? (WORST | HAS_WIDTH) : WORST;
	if (range_local.lower >= 0) {
		if (min_max[1] != REG_INFINITY) {
			range_param.lower = gsl::narrow<int32_t>(range_local.lower * min_max[0]);
			range_param.upper = gsl::narrow<int32_t>(range_local.upper * min_max[1]);
		} else {
			range_param.lower = -1; // Not a fixed-size length
			range_param.upper = -1;
		}
	} else {
		range_param.lower = -1; // Not a fixed-size length
		range_param.upper = -1;
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
		insert(lazy ? LAZY_STAR : STAR, ret_val, 0UL, 0UL, 0);

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

		tail(ret_val, emit_node(BACK));        // 1
		insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 2,4
		insert(NOTHING, ret_val, 0UL, 0UL, 0); // 3

		next = emit_node(NOTHING); // 2,3

		offset_tail(ret_val, NODE_SIZE<size_t>, next);        // 2
		tail(ret_val, next);                                  // 3
		insert(BRANCH, ret_val, 0UL, 0UL, 0);                 // 4,5
		tail(ret_val, ret_val + (2 * NODE_SIZE<size_t>));     // 4
		offset_tail(ret_val, 3 * NODE_SIZE<size_t>, ret_val); // 5

		if (op_code == '+') {
			insert(NOTHING, ret_val, 0UL, 0UL, 0);            // 6
			tail(ret_val, ret_val + (4 * NODE_SIZE<size_t>)); // 6
		}
	} else if (op_code == '*') {
		/* Node structure for (x)* construct.
		 *      ____1_____
		 *     |          \
		 *     B~ (...)~ K~ B~ N~
		 *      \      \_|2 |\_|
		 *       \__3_______|  4
		 */

		insert(BRANCH, ret_val, 0UL, 0UL, 0);                     // 1,3
		offset_tail(ret_val, NODE_SIZE<size_t>, emit_node(BACK)); // 2
		offset_tail(ret_val, NODE_SIZE<size_t>, ret_val);         // 1
		tail(ret_val, emit_node(BRANCH));                         // 3
		tail(ret_val, emit_node(NOTHING));                        // 4
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

		insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 2,4
		insert(NOTHING, ret_val, 0UL, 0UL, 0); // 3

		next = emit_node(NOTHING); // 1,2,3

		offset_tail(ret_val, 2 * NODE_SIZE<size_t>, next);  // 1
		offset_tail(ret_val, NODE_SIZE<size_t>, next);      // 2
		tail(ret_val, next);                                // 3
		insert(BRANCH, ret_val, 0UL, 0UL, 0);               // 4
		tail(ret_val, (ret_val + (2 * NODE_SIZE<size_t>))); // 4

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

		tail(ret_val, next);                           // 2
		offset_tail(ret_val, NODE_SIZE<size_t>, next); // 3
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
		tail(emit_node(BACK), ret_val);                                           // 3
		tail(ret_val, emit_node(NOTHING));                                        // 4

		next = insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 5

		tail(ret_val, next); // 5

		pContext.Num_Braces++;
	} else if (op_code == '{' && lazy) {
		if (min_max[0] == 0 && min_max[1] != REG_INFINITY) {
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

			tail(ret_val, next);                                     // 2
			insert(BRANCH, ret_val, 0UL, 0UL, pContext.Num_Braces);  // 4,6
			insert(NOTHING, ret_val, 0UL, 0UL, pContext.Num_Braces); // 5
			insert(BRANCH, ret_val, 0UL, 0UL, pContext.Num_Braces);  // 3,4,8
			tail(emit_node(BACK), ret_val);                          // 3
			tail(ret_val, ret_val + (2 * NODE_SIZE<size_t>));        // 4

			next = emit_node(NOTHING); // 5,6,7

			offset_tail(ret_val, NODE_SIZE<size_t>, next);     // 5
			offset_tail(ret_val, 2 * NODE_SIZE<size_t>, next); // 6
			offset_tail(ret_val, 3 * NODE_SIZE<size_t>, next); // 7

			next = insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 8

			tail(ret_val, next); // 8

		} else if (min_max[0] > 0 && min_max[1] == REG_INFINITY) {
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

			tail(ret_val, next);                   // 2
			tail(emit_node(BACK), ret_val);        // 3
			tail(ret_val, emit_node(BACK));        // 4
			insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 5,7
			insert(NOTHING, ret_val, 0UL, 0UL, 0); // 6

			next = emit_node(NOTHING); // 5,6

			offset_tail(ret_val, NODE_SIZE<size_t>, next);                         // 5
			tail(ret_val, next);                                                   // 6
			insert(BRANCH, ret_val, 0UL, 0UL, 0);                                  // 7,8
			tail(ret_val, ret_val + (2 * NODE_SIZE<size_t>));                      // 7
			offset_tail(ret_val, 3 * NODE_SIZE<size_t>, ret_val);                  // 8
			insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces);            // 9
			tail(ret_val, ret_val + INDEX_SIZE<size_t> + (4 * NODE_SIZE<size_t>)); // 9

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

			tail(emit_node(BACK), ret_val);        // 3
			tail(next, emit_node(BACK));           // 4
			insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 6,8
			insert(NOTHING, ret_val, 0UL, 0UL, 0); // 5
			insert(BRANCH, ret_val, 0UL, 0UL, 0);  // 8,9

			next = emit_node(NOTHING); // 5,6,7

			offset_tail(ret_val, NODE_SIZE<size_t>, next);                         // 5
			offset_tail(ret_val, 2 * NODE_SIZE<size_t>, next);                     // 6
			offset_tail(ret_val, 3 * NODE_SIZE<size_t>, next);                     // 7
			tail(ret_val, ret_val + (2 * NODE_SIZE<size_t>));                      // 8
			offset_tail(next, -NODE_SIZE<int>, ret_val);                           // 9
			insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces);            // 10
			tail(ret_val, ret_val + INDEX_SIZE<size_t> + (4 * NODE_SIZE<size_t>)); // 10
		}

		pContext.Num_Braces++;
	} else if (op_code == '{') {
		if (min_max[0] == 0 && min_max[1] != REG_INFINITY) {
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

			tail(ret_val, next);                  // 2
			insert(BRANCH, ret_val, 0UL, 0UL, 0); // 3,4,7
			tail(emit_node(BACK), ret_val);       // 3

			next = emit_node(BRANCH); // 4,5

			tail(ret_val, next);                           // 4
			tail(next, emit_node(NOTHING));                // 5,6
			offset_tail(ret_val, NODE_SIZE<size_t>, next); // 6

			next = insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 7

			tail(ret_val, next); // 7

		} else if (min_max[0] > 0 && min_max[1] == REG_INFINITY) {
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

			tail(ret_val, next);                  // 2
			tail(emit_node(BACK), ret_val);       // 3
			insert(BRANCH, ret_val, 0UL, 0UL, 0); // 4,6

			next = emit_node(BACK); // 4

			tail(next, ret_val);                           // 4
			offset_tail(ret_val, NODE_SIZE<size_t>, next); // 5
			tail(ret_val, emit_node(BRANCH));              // 6
			tail(ret_val, emit_node(NOTHING));             // 7

			insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces); // 8

			tail(ret_val, ret_val + INDEX_SIZE<size_t> + (2 * NODE_SIZE<size_t>)); // 8

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

			tail(emit_node(BACK), ret_val);       // 3
			tail(next, emit_node(BACK));          // 4
			insert(BRANCH, ret_val, 0UL, 0UL, 0); // 5,6

			next = emit_node(BRANCH); // 5,8

			tail(ret_val, next);                         // 5
			offset_tail(next, -NODE_SIZE<int>, ret_val); // 6

			next = emit_node(NOTHING); // 7,8

			offset_tail(ret_val, NODE_SIZE<size_t>, next); // 7

			offset_tail(next, -NODE_SIZE<int>, next);                              // 8
			insert(INIT_COUNT, ret_val, 0UL, 0UL, pContext.Num_Braces);            // 9
			tail(ret_val, ret_val + INDEX_SIZE<size_t> + (2 * NODE_SIZE<size_t>)); // 9
		}

		pContext.Num_Braces++;
	} else {
		/* We get here if the IS_QUANTIFIER macro is not coordinated properly
		   with this function. */

		Raise<RegexError>("internal error #2, 'piece'");
	}

	if (isQuantifier(*pContext.Reg_Parse)) {
		if (op_code == '{') {
			Raise<RegexError>("nested quantifiers, {m,n}%c", *pContext.Reg_Parse);
		} else {
			Raise<RegexError>("nested quantifiers, %c%c", op_code, *pContext.Reg_Parse);
		}
	}

	return ret_val;
}

/*----------------------------------------------------------------------*
 * alternative
 *
 * Processes one alternative of an '|' operator.  Connects the NEXT
 * pointers of each regex atom together sequentially.
 *----------------------------------------------------------------------*/
uint8_t *alternative(int *flag_param, len_range &range_param) {

	uint8_t *ret_val;
	uint8_t *chain;
	uint8_t *latest;
	int flags_local;
	len_range range_local;

	*flag_param       = WORST; // Tentatively.
	range_param.lower = 0;     // Idem
	range_param.upper = 0;

	ret_val = emit_node(BRANCH);
	chain   = nullptr;

	/* Loop until we hit the start of the next alternative, the end of this set
	   of alternatives (end of parentheses), or the end of the regex. */

	while (*pContext.Reg_Parse != '|' && *pContext.Reg_Parse != ')' && pContext.Reg_Parse != pContext.InputString.end()) {
		latest = piece(&flags_local, range_local);

		if (!latest) {
			return nullptr; // Something went wrong.
		}

		*flag_param |= flags_local & HAS_WIDTH;
		if (range_local.lower < 0) {
			// Not a fixed length
			range_param.lower = -1;
			range_param.upper = -1;
		} else if (range_param.lower >= 0) {
			range_param.lower += range_local.lower;
			range_param.upper += range_local.upper;
		}

		if (chain) { // Connect the regex atoms together sequentially.
			tail(chain, latest);
		}

		chain = latest;
	}

	if (!chain) { // Loop ran zero times.
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
uint8_t *chunk(int paren, int *flag_param, len_range &range_param) {

	uint8_t *ret_val  = nullptr;
	uint8_t *ender    = nullptr;
	size_t this_paren = 0;
	int flags_local;
	bool first = true;
	int zero_width;
	const bool old_sensitive = pContext.Is_Case_Insensitive;
	const bool old_newline   = pContext.Match_Newline;

	len_range range_local;
	bool look_only                   = false;
	uint8_t *emit_look_behind_bounds = nullptr;
	*flag_param                      = HAS_WIDTH; // Tentatively.
	range_param.lower                = 0;         // Idem
	range_param.upper                = 0;

	// Make an OPEN node, if parenthesized.

	if (paren == PAREN) {
		if (pContext.Total_Paren >= MaxSubExpr) {
			Raise<RegexError>("number of ()'s > %u", MaxSubExpr);
		}

		this_paren = pContext.Total_Paren;
		++pContext.Total_Paren;
		ret_val = emit_node(OPEN + this_paren);
	} else if (paren == POS_AHEAD_OPEN || paren == NEG_AHEAD_OPEN) {
		*flag_param = WORST; // Look ahead is zero width.
		look_only   = true;
		ret_val     = emit_node(paren);
	} else if (paren == POS_BEHIND_OPEN || paren == NEG_BEHIND_OPEN) {
		*flag_param = WORST; // Look behind is zero width.
		look_only   = true;
		// We'll overwrite the zero length later on, so we save the ptr
		ret_val                 = emit_special(paren, 0, 0);
		emit_look_behind_bounds = ret_val + NODE_SIZE<size_t>;
	} else if (paren == INSENSITIVE) {
		pContext.Is_Case_Insensitive = true;
	} else if (paren == SENSITIVE) {
		pContext.Is_Case_Insensitive = false;
	} else if (paren == NEWLINE) {
		pContext.Match_Newline = true;
	} else if (paren == NO_NEWLINE) {
		pContext.Match_Newline = false;
	}

	// Pick up the branches, linking them together.
	do {
		uint8_t *const this_branch = alternative(&flags_local, range_local);
		if (!this_branch) {
			return nullptr;
		}

		if (first) {
			first       = false;
			range_param = range_local;
			if (!ret_val) {
				ret_val = this_branch;
			}
		} else if (range_param.lower >= 0) {
			if (range_local.lower >= 0) {
				if (range_local.lower < range_param.lower) {
					range_param.lower = range_local.lower;
				}
				if (range_local.upper > range_param.upper) {
					range_param.upper = range_local.upper;
				}
			} else {
				range_param.lower = -1; // Branches have different lengths
				range_param.upper = -1;
			}
		}

		tail(ret_val, this_branch); // Connect BRANCH -> BRANCH.

		/* If any alternative could be zero width, consider the whole
		   parenthesized thing to be zero width. */

		if (!(flags_local & HAS_WIDTH)) {
			*flag_param &= ~HAS_WIDTH;
		}

		// Are there more alternatives to process?

		if (*pContext.Reg_Parse != '|') {
			break;
		}

		++pContext.Reg_Parse;
	} while (true);

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
	for (uint8_t *this_branch = ret_val; this_branch != nullptr; this_branch = next_ptr(this_branch)) {
		branch_tail(this_branch, NODE_SIZE<size_t>, ender);
	}

	// Check for proper termination.

	if (paren != NO_PAREN && *pContext.Reg_Parse++ != ')') {
		Raise<RegexError>("missing right parenthesis ')'");
	} else if (paren == NO_PAREN && pContext.Reg_Parse != pContext.InputString.end()) {
		if (*pContext.Reg_Parse == ')') {
			Raise<RegexError>("missing left parenthesis '('");
		} else {
			Raise<RegexError>("junk on end"); // "Can't happen" - NOT REACHED
		}
	}

	// Check whether look behind has a fixed size
	if (emit_look_behind_bounds) {
		if (range_param.lower < 0) {
			Raise<RegexError>("look-behind does not have a bounded size");
		}

		if (range_param.upper > 65535L) {
			Raise<RegexError>("max. look-behind size is too large (>65535)");
		}

		if (!pContext.FirstPass) {
			*emit_look_behind_bounds++ = PUT_OFFSET_L(range_param.lower);
			*emit_look_behind_bounds++ = PUT_OFFSET_R(range_param.lower);
			*emit_look_behind_bounds++ = PUT_OFFSET_L(range_param.upper);
			*emit_look_behind_bounds   = PUT_OFFSET_R(range_param.upper);
		}
	}

	// For look ahead/behind, the length must be set to zero again
	if (look_only) {
		range_param.lower = 0;
		range_param.upper = 0;
	}

	zero_width = 0;

	/* Set a bit in Closed_Parens to let future calls to function 'back_ref'
	   know that we have closed this set of parentheses. */

	if (paren == PAREN && this_paren < pContext.Closed_Parens.size()) {
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
	   (*) or question (?) quantifiers to be applied to a back-reference that
	   refers to this set of parentheses. */

	if ((*flag_param & HAS_WIDTH) && paren == PAREN && !zero_width && this_paren < pContext.Paren_Has_Width.size()) {
		pContext.Paren_Has_Width[this_paren] = true;
	}

	pContext.Is_Case_Insensitive = old_sensitive;
	pContext.Match_Newline       = old_newline;

	return ret_val;
}

}

/*----------------------------------------------------------------------*
 * Regex
 *
 * Compiles a regular expression into the internal format used by
 * 'ExecRE'.
 *
 * The default behaviour wrt. case sensitivity and newline matching can
 * be controlled through the defaultFlags argument (Markus Schwarzenberg).
 * Future extensions are possible by using other flag bits.
 * Note that currently only the case sensitivity flag is effectively used.
 *
 * Beware that the optimization and preparation code in here knows about
 * some of the structure of the compiled Regex.
 *----------------------------------------------------------------------*/
Regex::Regex(std::string_view exp, int defaultFlags) {

	Regex *const re = this;

	int flags_local;
	len_range range_local;

	if (ParseContext::Enable_Counting_Quantifier) {
		pContext.Brace_Char = '{';
		pContext.Meta_Char  = &Default_Meta_Char[0];
	} else {
		pContext.Brace_Char = '*';                   // Bypass the '{' in
		pContext.Meta_Char  = &Default_Meta_Char[1]; // Default_Meta_Char
	}

	// Initialize arrays used by function 'shortcut_escape'.
	if (!init_ansi_classes()) {
		Raise<RegexError>("internal error #1, 'CompileRE'");
	}

	pContext.FirstPass = true;
	pContext.Reg_Size  = 0UL;
	pContext.Code.clear();

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
		pContext.Is_Case_Insensitive = ((defaultFlags & RE_DEFAULT_CASE_INSENSITIVE) != 0);
#if 0 // Currently not used. Uncomment if needed.
		pContext.Match_Newline       = ((defaultFlags & RE_DEFAULT_MATCH_NEWLINE) != 0);
#else
		pContext.Match_Newline = false;
#endif

		pContext.Reg_Parse       = exp.begin();
		pContext.InputString     = exp;
		pContext.Total_Paren     = 1;
		pContext.Num_Braces      = 0;
		pContext.Closed_Parens   = 0;
		pContext.Paren_Has_Width = 0;

		emit_byte(Magic);
		emit_byte('%'); // Placeholder for num of capturing parentheses.
		emit_byte('%'); // Placeholder for num of general {m,n} constructs.

		if (!chunk(NO_PAREN, &flags_local, range_local)) {
			Raise<RegexError>("internal error #10, 'CompileRE'");
		}

		if (pass == 1) {
			if (pContext.Reg_Size >= MaxCompiledSize) {
				/* Too big for NEXT pointers NEXT_PTR_SIZE bytes long to span.
				   This is a real issue since the first BRANCH node usually points
				   to the end of the compiled regex code. */

				Raise<RegexError>("Regex > %lu bytes", MaxCompiledSize);
			}

			// NOTE(eteran): For now, we NEED this to avoid issues regarding holding pointers to reallocated space
			pContext.Code.reserve(pContext.Reg_Size);
			pContext.FirstPass = false;
		}
	}

	pContext.Code[1] = static_cast<uint8_t>(pContext.Total_Paren - 1);
	pContext.Code[2] = static_cast<uint8_t>(pContext.Num_Braces);

	assert(pContext.Code.size() == pContext.Reg_Size);

	// move over what we compiled
	re->program = std::move(pContext.Code);

	/*----------------------------------------*
	 * Dig out information for optimizations. *
	 *----------------------------------------*/

	// First BRANCH.
	uint8_t *scan = (&re->program[REGEX_START_OFFSET]);

	if (GET_OP_CODE(next_ptr(scan)) == END) { // Only one top-level choice.
		scan = OPERAND(scan);

		// Starting-point info.
		if (GET_OP_CODE(scan) == EXACTLY) {
			re->match_start = static_cast<char>(*OPERAND(scan));

		} else if (PLUS <= GET_OP_CODE(scan) && GET_OP_CODE(scan) <= LAZY_PLUS) {

			/* Allow x+ or x+? at the start of the regex to be
			   optimized. */

			if (GET_OP_CODE(scan + NODE_SIZE<size_t>) == EXACTLY) {
				re->match_start = static_cast<char>(*OPERAND(scan + NODE_SIZE<size_t>));
			}
		} else if (GET_OP_CODE(scan) == BOL) {
			re->anchor++;
		}
	}
}
