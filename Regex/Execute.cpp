
#include "Execute.h"
#include "Common.h"
#include "Compile.h"
#include "Constants.h"
#include "Opcodes.h"
#include "Regex.h"
#include "RegexError.h"
#include "Util/Compiler.h"
#include "Util/utils.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

bool Match(uint8_t *prog, size_t *branch_index_param);
bool Attempt(Regex *prog, const char *string);

/**
 * @brief Get the next pointer in the regex program.
 *
 * @param ptr The current position in the regex program.
 * @return The next pointer in the regex program, or nullptr if there is no next pointer.
 *
 * @note The `NextPtr()` function can consume up to 30% of the time
 * during matching because it is called an immense number of times
 * (an average of 25 `NextPtr()` calls per `Match()` call was witnessed
 * for Perl syntax highlighting). Therefore it is well worth removing
 * some of the function call overhead by selectively inlining the `NextPtr()` calls.
 * Moreover, the inlined code can be simplified for matching because one of the tests,
 * only necessary during compilation, can be left out. The net result of using this
 * inlined version at two critical places is a 25% speedup
 * (again, witnesses on Perl syntax highlighting).
 *
 * @note This function used to be a macro, but was changed to an inline function
 * to improve type safety and maintainability.
 */
FORCE_INLINE uint8_t *NextPointer(uint8_t *ptr) noexcept {

	// NOTE(eteran): like NextPtr, but is inline
	// doesn't do "is this a first pass compile" check

	const int offset = GetOffset(ptr);

	if (offset == 0) {
		return nullptr;
	}

	if (GetOpCode(ptr) == BACK) {
		return (ptr - offset);
	}

	return (ptr + offset);
}

/**
 * @brief Determine if the end of the string has been reached.
 *
 * @param ptr The current position in the string.
 * @return `true` if the end of the string has been reached, `false` otherwise.
 */
FORCE_INLINE bool EndOfString(const char *ptr) noexcept {

	if (eContext.End_Of_String != nullptr && ptr >= eContext.End_Of_String) {
		return true;
	}

	if (ptr >= eContext.Real_End_Of_String) {
		return true;
	}

	return false;
}

/**
 * @brief Get the first 16-bit operand of a node.
 *
 * @param p The current position in the regex program.
 * @return The first 16-bit operand of a node.
 */
FORCE_INLINE uint16_t GetLower(const uint8_t *p) noexcept {
	return static_cast<uint8_t>(((p[NODE_SIZE<size_t> + 0] & 0xff) << 8) + ((p[NODE_SIZE<size_t> + 1]) & 0xff));
}

/**
 * @brief Get the second 16-bit operand of a node.
 *
 * @param p The current position in the regex program.
 * @return The second 16-bit operand of a node.
 */
FORCE_INLINE uint16_t GetUpper(const uint8_t *p) noexcept {
	return static_cast<uint8_t>(((p[NODE_SIZE<size_t> + 2] & 0xff) << 8) + ((p[NODE_SIZE<size_t> + 3]) & 0xff));
}

/**
 * @brief Check if a character is a delimiter.
 *
 * @param ch The character to check.
 * @return `true` if the character is a delimiter, `false` otherwise.
 */
bool IsDelimeter(int ch) noexcept {
	auto n = static_cast<unsigned int>(ch);
	if (n < eContext.Current_Delimiters.size()) {
		return eContext.Current_Delimiters[n];
	}

	return false;
}

/**
 * @brief Consume characters from the input string while a predicate is `true`.
 *
 * @param input The input string to consume from.
 * @param max The maximum number of characters to consume.
 * @param pred A predicate function that takes a character and returns `true` if it should be consumed.
 * @return The number of characters consumed from the input string.
 */
template <class Pred>
uint32_t GreedyConsume(const char *input, uint32_t max, Pred pred) {
	uint32_t count = 0;
	while (count < max && !EndOfString(input) && pred(*input)) {
		++count;
		++input;
	}
	return count;
}

/**
 * @brief Repeatedly match something simple up to "max" times.
 *
 * @param p The current position in the regex program.
 * @param max The maximum number of times to match.
 *            If `max` is less than or equal to zero, match as much as possible.
 *            If `max` is greater than zero, match up to `max` times.
 * @return The actual number of matches made.
 */
uint32_t Greedy(uint8_t *p, uint32_t max) {

	uint32_t count = 0;

	const char *const input_str = eContext.Reg_Input;
	const uint8_t *operand      = Operand(p); // Literal char or start of class characters.
	const uint32_t max_cmp      = (max > 0) ? max : std::numeric_limits<uint32_t>::max();

	switch (GetOpCode(p)) {
	case ANY:
		// Race to the end of the line or string. Dot DOESN'T match newline.
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return ch != '\n'; });
		break;
	case EVERY:
		// Race to the end of the line or string. Dot DOES match newline.
		count = GreedyConsume(input_str, max_cmp, [](char ch) { (void)ch; return true; });
		break;
	case EXACTLY:
		// Count occurrences of single character operand.
		count = GreedyConsume(input_str, max_cmp, [operand](char ch) { return static_cast<char>(*operand) == ch; });
		break;
	case SIMILAR:
		// Case insensitive version of EXACTLY
		count = GreedyConsume(input_str, max_cmp, [operand](char ch) { return static_cast<char>(*operand) == safe_tolower(ch); });
		break;
	case ANY_OF:
		// [...] character class.
		count = GreedyConsume(input_str, max_cmp, [operand](char ch) { return ::strchr(reinterpret_cast<const char *>(operand), ch) != nullptr; });
		break;
	case ANY_BUT:
		/* [^...] Negated character class- does NOT normally match newline
		 * (\n added usually to operand at compile time.) */
		count = GreedyConsume(input_str, max_cmp, [operand](char ch) { return ::strchr(reinterpret_cast<const char *>(operand), ch) == nullptr; });
		break;
	case IS_DELIM:
		/* \y (not a word delimiter char)
		 * NOTE: '\n' and '\0' are always word delimiters. */
		count = GreedyConsume(input_str, max_cmp, IsDelimeter);
		break;
	case NOT_DELIM:
		/* \Y (not a word delimiter char)
		 * NOTE: '\n' and '\0' are always word delimiters. */
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return !IsDelimeter(ch); });
		break;
	case WORD_CHAR:
		// \w (word character, alpha-numeric or underscore)
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return (safe_isalnum(ch) || ch == '_'); });
		break;
	case NOT_WORD_CHAR:
		// \W (NOT a word character)
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return !safe_isalnum(ch) && ch != '_' && ch != '\n'; });
		break;
	case DIGIT:
		// same as [0123456789]
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return safe_isdigit(ch); });
		break;
	case NOT_DIGIT:
		// same as [^0123456789]
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return !safe_isdigit(ch) && ch != '\n'; });
		break;
	case SPACE:
		// same as [ \t\r\f\v]-- doesn't match newline.
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return safe_isspace(ch) && ch != '\n'; });
		break;
	case SPACE_NL:
		// same as [\n \t\r\f\v]-- matches newline.
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return safe_isspace(ch); });
		break;
	case NOT_SPACE:
		// same as [^\n \t\r\f\v]-- doesn't match newline.
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return !safe_isspace(ch); });
		break;
	case NOT_SPACE_NL:
		// same as [^ \t\r\f\v]-- matches newline.
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return (!safe_isspace(ch) || ch == '\n'); });
		break;
	case LETTER:
		// same as [a-zA-Z]
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return safe_isalpha(ch); });
		break;
	case NOT_LETTER:
		// same as [^a-zA-Z]
		count = GreedyConsume(input_str, max_cmp, [](char ch) { return !safe_isalpha(ch) && ch != '\n'; });
		break;
	default:
		/* Called inappropriately.  Only atoms that are SIMPLE should generate
		 * a call to Greedy.  The above cases should cover all the atoms that
		 * are SIMPLE. */
		ReportError("internal error #10 'Greedy'");
		count = 0U; // Best we can do.
	}

	// Point to character just after last matched character.
	eContext.Reg_Input = input_str + count;
	return count;
}

#define MATCH_RETURN(X)             \
	do {                            \
		--eContext.Recursion_Count; \
		return (X);                 \
	} while (0)

#define CHECK_RECURSION_LIMIT()                  \
	do {                                         \
		if (eContext.Recursion_Limit_Exceeded) { \
			MATCH_RETURN(false);                 \
		}                                        \
	} while (0)

/**
 * @brief The main matching routine.
 * Conceptually the strategy is simple: check to see whether the
 * current node matches, call self recursively to see whether the rest
 * matches, and then act accordingly. In practice we make some effort
 * to avoid recursion, in particular by going through "ordinary" nodes
 * (that don't need to know whether the rest of the match failed) by a
 * loop instead of by recursion.
 *
 * @param prog The regex program to match against the input string.
 * @param branch_index_param If not `nullptr`, this will be set to the index of the branch that matched.
 * @return `true` if the match is successful, `false` otherwise.
 */
bool Match(uint8_t *prog, size_t *branch_index_param) {

	if (++eContext.Recursion_Count > RecursionLimit) {
		// Prevent duplicate errors
		if (!eContext.Recursion_Limit_Exceeded) {
			ReportError("recursion limit exceeded, please re-specify expression");
		}

		eContext.Recursion_Limit_Exceeded = true;
		MATCH_RETURN(false);
	}

	// Current node.
	uint8_t *scan = prog;

	while (scan) {
		uint8_t *next = NextPointer(scan);

		switch (GetOpCode(scan)) {
		case BRANCH:
			if (GetOpCode(next) != BRANCH) { // No choice.
				next = Operand(scan);        // Avoid recursion.
			} else {
				size_t branch_index_local = 0;

				do {
					const char *save = eContext.Reg_Input;

					if (Match(Operand(scan), nullptr)) {
						if (branch_index_param) {
							*branch_index_param = branch_index_local;
						}
						MATCH_RETURN(true);
					}

					CHECK_RECURSION_LIMIT();

					++branch_index_local;

					eContext.Reg_Input = save; // Backtrack.
					scan               = NextPointer(scan);
				} while (scan != nullptr && GetOpCode(scan) == BRANCH);

				MATCH_RETURN(false); // NOT REACHED
			}
			break;

		case EXACTLY: {
			uint8_t *opnd = Operand(scan);

			// Inline the first character, for speed.
			if (EndOfString(eContext.Reg_Input) || static_cast<char>(*opnd) != *eContext.Reg_Input) {
				MATCH_RETURN(false);
			}

			const auto str   = reinterpret_cast<const char *>(opnd);
			const size_t len = strlen(str);

			if (eContext.End_Of_String != nullptr && eContext.Reg_Input + len > eContext.End_Of_String) {
				MATCH_RETURN(false);
			}

			if (len > 1 && strncmp(str, eContext.Reg_Input, len) != 0) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input += len;
		} break;

		case SIMILAR: {
			uint8_t test;
			uint8_t *opnd = Operand(scan);

			/* Note: the SIMILAR operand was converted to lower case during
				   regex compile. */
			while ((test = *opnd++) != '\0') {
				if (EndOfString(eContext.Reg_Input) || safe_tolower(*eContext.Reg_Input++) != test) {
					MATCH_RETURN(false);
				}
			}
		} break;

		case BOL: // '^' (beginning of line anchor)
			if (eContext.Reg_Input == eContext.Start_Of_String) {
				if (eContext.Prev_Is_BOL) {
					break;
				}
			} else if (eContext.Reg_Input[-1] == '\n') {
				break;
			}

			MATCH_RETURN(false);

		case EOL: // '$' anchor matches end of line and end of string
			if ((EndOfString(eContext.Reg_Input) && eContext.Succ_Is_EOL) || *eContext.Reg_Input == '\n') {
				break;
			}

			MATCH_RETURN(false);

		case BOWORD: // '<' (beginning of word anchor)
					 /* Check to see if the current character is not a delimiter and the preceding character is. */
			{
				bool prev_is_delim;
				if (eContext.Reg_Input == eContext.Start_Of_String) {
					prev_is_delim = eContext.Prev_Is_Delim;
				} else {
					prev_is_delim = IsDelimeter(eContext.Reg_Input[-1]);
				}

				if (prev_is_delim) {
					bool current_is_delim;
					if (EndOfString(eContext.Reg_Input)) {
						current_is_delim = eContext.Succ_Is_Delim;
					} else {
						current_is_delim = IsDelimeter(*eContext.Reg_Input);
					}

					if (!current_is_delim) {
						break;
					}
				}
			}

			MATCH_RETURN(false);

		case EOWORD: // '>' (end of word anchor)
					 /* Check to see if the current character is a delimiter and the preceding character is not. */
			{
				bool prev_is_delim;
				if (eContext.Reg_Input == eContext.Start_Of_String) {
					prev_is_delim = eContext.Prev_Is_Delim;
				} else {
					prev_is_delim = IsDelimeter(eContext.Reg_Input[-1]);
				}

				if (!prev_is_delim) {
					bool current_is_delim;
					if (EndOfString(eContext.Reg_Input)) {
						current_is_delim = eContext.Succ_Is_Delim;
					} else {
						current_is_delim = IsDelimeter(*eContext.Reg_Input);
					}

					if (current_is_delim) {
						break;
					}
				}
			}

			MATCH_RETURN(false);

		case NOT_BOUNDARY: // \B (NOT a word boundary)
		{
			bool prev_is_delim;
			bool current_is_delim;

			if (eContext.Reg_Input == eContext.Start_Of_String) {
				prev_is_delim = eContext.Prev_Is_Delim;
			} else {
				prev_is_delim = IsDelimeter(eContext.Reg_Input[-1]);
			}

			if (EndOfString(eContext.Reg_Input)) {
				current_is_delim = eContext.Succ_Is_Delim;
			} else {
				current_is_delim = IsDelimeter(*eContext.Reg_Input);
			}

			if (!(prev_is_delim ^ current_is_delim)) {
				break;
			}
		}
			MATCH_RETURN(false);

		case IS_DELIM: // \y (A word delimiter character.)
			if (!EndOfString(eContext.Reg_Input) && IsDelimeter(*eContext.Reg_Input)) {
				eContext.Reg_Input++;
				break;
			}

			MATCH_RETURN(false);

		case NOT_DELIM: // \Y (NOT a word delimiter character.)
			if (!EndOfString(eContext.Reg_Input) && !IsDelimeter(*eContext.Reg_Input)) {
				eContext.Reg_Input++;
				break;
			}

			MATCH_RETURN(false);

		case WORD_CHAR: // \w (word character; alpha-numeric or underscore)
			if (!EndOfString(eContext.Reg_Input) && (safe_isalnum(*eContext.Reg_Input) || *eContext.Reg_Input == '_')) {
				eContext.Reg_Input++;
				break;
			}

			MATCH_RETURN(false);

		case NOT_WORD_CHAR: // \W (NOT a word character)
			if (EndOfString(eContext.Reg_Input) || safe_isalnum(*eContext.Reg_Input) || *eContext.Reg_Input == '_' || *eContext.Reg_Input == '\n') {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case ANY: // '.' (matches any character EXCEPT newline)
			if (EndOfString(eContext.Reg_Input) || *eContext.Reg_Input == '\n') {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case EVERY: // '.' (matches any character INCLUDING newline)
			if (EndOfString(eContext.Reg_Input)) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case DIGIT: // \d, same as [0123456789]
			if (EndOfString(eContext.Reg_Input) || !safe_isdigit(*eContext.Reg_Input)) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case NOT_DIGIT: // \D, same as [^0123456789]
			if (EndOfString(eContext.Reg_Input) || safe_isdigit(*eContext.Reg_Input) || *eContext.Reg_Input == '\n') {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case LETTER: // \l, same as [a-zA-Z]
			if (EndOfString(eContext.Reg_Input) || !safe_isalpha(*eContext.Reg_Input)) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case NOT_LETTER: // \L, same as [^0123456789]
			if (EndOfString(eContext.Reg_Input) || safe_isalpha(*eContext.Reg_Input) || *eContext.Reg_Input == '\n') {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case SPACE: // \s, same as [ \t\r\f\v]
			if (EndOfString(eContext.Reg_Input) || !safe_isspace(*eContext.Reg_Input) || *eContext.Reg_Input == '\n') {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case SPACE_NL: // \s, same as [\n \t\r\f\v]
			if (EndOfString(eContext.Reg_Input) || !safe_isspace(*eContext.Reg_Input)) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case NOT_SPACE: // \S, same as [^\n \t\r\f\v]
			if (EndOfString(eContext.Reg_Input) || safe_isspace(*eContext.Reg_Input)) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case NOT_SPACE_NL: // \S, same as [^ \t\r\f\v]
			if (EndOfString(eContext.Reg_Input) || (safe_isspace(*eContext.Reg_Input) && *eContext.Reg_Input != '\n')) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case ANY_OF: // [...] character class.
			if (EndOfString(eContext.Reg_Input)) {
				MATCH_RETURN(false); /* Needed because strchr () considers \0
										as a member of the character set. */
			}

			if (::strchr(reinterpret_cast<char *>(Operand(scan)), *eContext.Reg_Input) == nullptr) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case ANY_BUT: /* [^...] Negated character class-- does NOT normally
					  match newline (\n added usually to operand at compile
					  time.) */

			if (EndOfString(eContext.Reg_Input)) {
				MATCH_RETURN(false); // See comment for ANY_OF.
			}

			if (::strchr(reinterpret_cast<char *>(Operand(scan)), *eContext.Reg_Input) != nullptr) {
				MATCH_RETURN(false);
			}

			eContext.Reg_Input++;
			break;

		case NOTHING:
		case BACK:
			break;

		case STAR:
		case PLUS:
		case QUESTION:
		case BRACE:

		case LAZY_STAR:
		case LAZY_PLUS:
		case LAZY_QUESTION:
		case LAZY_BRACE: {
			uint32_t num_matched = 0;
			uint32_t min         = std::numeric_limits<uint32_t>::max();
			uint32_t max         = 0;
			const char *save;
			uint8_t next_char;
			uint8_t *next_op;
			bool lazy = false;

			/* Lookahead (when possible) to avoid useless match attempts
			   when we know what character comes next. */

			if (GetOpCode(next) == EXACTLY) {
				next_char = *Operand(next);
			} else {
				next_char = '\0'; // i.e. Don't know what next character is.
			}

			next_op = Operand(scan);

			switch (GetOpCode(scan)) {
			case LAZY_STAR:
				lazy = true;
				[[fallthrough]];
			case STAR:
				min = 0;
				max = std::numeric_limits<uint32_t>::max();
				break;

			case LAZY_PLUS:
				lazy = true;
				[[fallthrough]];
			case PLUS:
				min = 1;
				max = std::numeric_limits<uint32_t>::max();
				break;

			case LAZY_QUESTION:
				lazy = true;
				[[fallthrough]];
			case QUESTION:
				min = 0;
				max = 1;
				break;

			case LAZY_BRACE:
				lazy = true;
				[[fallthrough]];
			case BRACE:
				min = static_cast<uint32_t>(GetOffset(scan + NEXT_PTR_SIZE<size_t>));
				max = static_cast<uint32_t>(GetOffset(scan + (2 * NEXT_PTR_SIZE<size_t>)));

				if (max <= REG_INFINITY) {
					max = std::numeric_limits<uint32_t>::max();
				}

				next_op = Operand(scan + (2 * NEXT_PTR_SIZE<size_t>));
			}

			save = eContext.Reg_Input;

			if (lazy) {
				if (min > 0) {
					num_matched = Greedy(next_op, min);
				}
			} else {
				num_matched = Greedy(next_op, max);
			}

			while (min <= num_matched && num_matched <= max) {
				if (next_char == '\0' || (!EndOfString(eContext.Reg_Input) && static_cast<char>(next_char) == *eContext.Reg_Input)) {
					if (Match(next, nullptr)) {
						MATCH_RETURN(true);
					}

					CHECK_RECURSION_LIMIT();
				}

				// Couldn't or didn't match.
				if (lazy) {
					if (!Greedy(next_op, 1)) {
						MATCH_RETURN(false);
					}

					num_matched++; // Inch forward.
				} else if (num_matched > 0) {
					num_matched--; // Back up.
				} else if (min == 0 && num_matched == 0) {
					break;
				}

				eContext.Reg_Input = save + num_matched;
			}

			MATCH_RETURN(false);
		}

		break;

		case END:
			if (eContext.Extent_Ptr_FW == nullptr || (eContext.Reg_Input - eContext.Extent_Ptr_FW) > 0) {
				eContext.Extent_Ptr_FW = eContext.Reg_Input;
			}

			MATCH_RETURN(true); // Success!
			break;

		case INIT_COUNT:
			eContext.BraceCounts[*Operand(scan)] = 0;
			break;

		case INC_COUNT:
			eContext.BraceCounts[*Operand(scan)]++;
			break;

		case TEST_COUNT:
			if (eContext.BraceCounts[*Operand(scan)] < static_cast<uint32_t>(GetOffset(scan + NEXT_PTR_SIZE<size_t> + INDEX_SIZE<size_t>))) {
				next = scan + NODE_SIZE<size_t> + INDEX_SIZE<size_t> + NEXT_PTR_SIZE<size_t>;
			}
			break;

		case BACK_REF:
		case BACK_REF_CI:
#ifdef ENABLE_CROSS_REGEX_BACKREF
		case X_REGEX_BR:
		case X_REGEX_BR_CI: // *** IMPLEMENT LATER
#endif
		{
			const char *captured;
			const char *finish;
			const uint8_t paren_no = *Operand(scan);

#ifdef ENABLE_CROSS_REGEX_BACKREF
			if (GetOpCode(scan) == X_REGEX_BR || GetOpCode(scan) == X_REGEX_BR_CI) {
				if (eContext.Cross_Regex_Backref == nullptr) {
					MATCH_RETURN(0);
				}

				captured = eContext.Cross_Regex_Backref->startp[paren_no];
				finish   = eContext.Cross_Regex_Backref->endp[paren_no];
			} else {
#endif
				captured = eContext.Back_Ref_Start[paren_no];
				finish   = eContext.Back_Ref_End[paren_no];
#ifdef ENABLE_CROSS_REGEX_BACKREF
			}
#endif

			if ((captured != nullptr) && (finish != nullptr)) {
				if (captured > finish) {
					MATCH_RETURN(false);
				}

#ifdef ENABLE_CROSS_REGEX_BACKREF
				if (GetOpCode(scan) == BACK_REF_CI || GetOpCode(scan) == X_REGEX_BR_CI) {
#else
				if (GetOpCode(scan) == BACK_REF_CI) {
#endif
					while (captured < finish) {
						if (EndOfString(eContext.Reg_Input) || safe_tolower(*captured++) != safe_tolower(*eContext.Reg_Input++)) {
							MATCH_RETURN(false);
						}
					}
				} else {
					while (captured < finish) {
						if (EndOfString(eContext.Reg_Input) || *captured++ != *eContext.Reg_Input++) {
							MATCH_RETURN(false);
						}
					}
				}

				break;
			}

			MATCH_RETURN(false);
		}

		case POS_AHEAD_OPEN:
		case NEG_AHEAD_OPEN: {

			const char *save = eContext.Reg_Input;

			/* Temporarily ignore the logical end of the string, to allow
			   lookahead past the end. */
			const char *saved_end  = eContext.End_Of_String;
			eContext.End_Of_String = nullptr;

			const bool answer = Match(next, nullptr); // Does the look-ahead regex match?

			CHECK_RECURSION_LIMIT();

			if ((GetOpCode(scan) == POS_AHEAD_OPEN) ? answer : !answer) {
				/* Remember the last (most to the right) character position
				   that we consume in the input for a successful match.  This
				   is info that may be needed should an attempt be made to
				   match the exact same text at the exact same place.  Since
				   look-aheads backtrack, a regex with a trailing look-ahead
				   may need more text than it matches to accomplish a
				   re-match. */

				if (eContext.Extent_Ptr_FW == nullptr || (eContext.Reg_Input - eContext.Extent_Ptr_FW) > 0) {
					eContext.Extent_Ptr_FW = eContext.Reg_Input;
				}

				eContext.Reg_Input     = save;      // Backtrack to look-ahead start.
				eContext.End_Of_String = saved_end; // Restore logical end.

				/* Jump to the node just after the (?=...) or (?!...)
				   Construct. */

				next = NextPointer(Operand(scan)); // Skip 1st branch
				// Skip the chain of branches inside the look-ahead
				while (GetOpCode(next) == BRANCH) {
					next = NextPointer(next);
				}

				next = NextPointer(next); // Skip the LOOK_AHEAD_CLOSE
			} else {
				eContext.Reg_Input     = save;      // Backtrack to look-ahead start.
				eContext.End_Of_String = saved_end; // Restore logical end.

				MATCH_RETURN(false);
			}
		}

		break;

		case POS_BEHIND_OPEN:
		case NEG_BEHIND_OPEN: {
			const char *save;
			bool found = false;
			const char *saved_end;

			save      = eContext.Reg_Input;
			saved_end = eContext.End_Of_String;

			/* Prevent overshoot (greedy matching could end past the
			   current position) by tightening the matching boundary.
			   Lookahead inside lookbehind can still cross that boundary. */
			eContext.End_Of_String = eContext.Reg_Input;

			const uint16_t lower = GetLower(scan);
			const uint16_t upper = GetUpper(scan);

			/* Start with the shortest match first. This is the most
			   efficient direction in general.
			   Note! Negative look behind is _very_ tricky when the length
			   is not constant: we have to make sure the expression doesn't
			   match for _any_ of the starting positions. */
			for (uint32_t offset = lower; offset <= upper; ++offset) {
				eContext.Reg_Input = save - offset;

				if (eContext.Reg_Input < eContext.Look_Behind_To) {
					// No need to look any further
					break;
				}

				const bool answer = Match(next, nullptr); // Does the look-behind regex match?

				CHECK_RECURSION_LIMIT();

				/* The match must have ended at the current position;
				   otherwise it is invalid */
				if (answer && eContext.Reg_Input == save) {
					// It matched, exactly far enough
					found = true;

					/* Remember the last (most to the left) character position
					   that we consume in the input for a successful match.
					   This is info that may be needed should an attempt be
					   made to match the exact same text at the exact same
					   place. Since look-behind backtracks, a regex with a
					   leading look-behind may need more text than it matches
					   to accomplish a re-match. */

					if (eContext.Extent_Ptr_BW == nullptr || (eContext.Extent_Ptr_BW - (save - offset)) > 0) {
						eContext.Extent_Ptr_BW = save - offset;
					}

					break;
				}
			}

			// Always restore the position and the logical string end.
			eContext.Reg_Input     = save;
			eContext.End_Of_String = saved_end;

			if ((GetOpCode(scan) == POS_BEHIND_OPEN) ? found : !found) {
				/* The look-behind matches, so we must jump to the next
				   node. The look-behind node is followed by a chain of
				   branches (contents of the look-behind expression), and
				   terminated by a look-behind-close node. */
				next = NextPointer(Operand(scan) + LENGTH_SIZE<size_t>); // 1st branch

				// Skip the chained branches inside the look-ahead
				while (GetOpCode(next) == BRANCH) {
					next = NextPointer(next);
				}

				next = NextPointer(next); // Skip LOOK_BEHIND_CLOSE
			} else {
				// Not a match
				MATCH_RETURN(false);
			}
		} break;

		case LOOK_AHEAD_CLOSE:
		case LOOK_BEHIND_CLOSE:
			/* We have reached the end of the look-ahead or look-behind which
			 * implies that we matched it, so return true. */
			MATCH_RETURN(true);

		default:
			if ((GetOpCode(scan) > OPEN) && (GetOpCode(scan) < OPEN + MaxSubExpr)) {

				const uint8_t no = GetOpCode(scan) - OPEN;
				const char *save = eContext.Reg_Input;

				if (no < 10) {
					eContext.Back_Ref_Start[no] = save;
					eContext.Back_Ref_End[no]   = nullptr;
				}

				if (Match(next, nullptr)) {
					/* Do not set 'Start_Ptr_Ptr' if some later invocation (think
					   recursion) of the same parentheses already has. */

					if (eContext.Start_Ptr_Ptr[no] == nullptr) {
						eContext.Start_Ptr_Ptr[no] = save;
					}

					MATCH_RETURN(true);
				} else {
					MATCH_RETURN(false);
				}
			} else if ((GetOpCode(scan) > CLOSE) && (GetOpCode(scan) < CLOSE + MaxSubExpr)) {

				const uint8_t no = GetOpCode(scan) - CLOSE;
				const char *save = eContext.Reg_Input;

				if (no < 10) {
					eContext.Back_Ref_End[no] = save;
				}

				if (Match(next, nullptr)) {
					/* Do not set 'End_Ptr_Ptr' if some later invocation of the
					   same parentheses already has. */

					if (eContext.End_Ptr_Ptr[no] == nullptr) {
						eContext.End_Ptr_Ptr[no] = save;
					}

					MATCH_RETURN(true);
				} else {
					MATCH_RETURN(false);
				}
			} else {
				ReportError("memory corruption, 'match'");
				MATCH_RETURN(false);
			}

			break;
		}

		scan = next;
	}

	/* We get here only if there's trouble -- normally "case END" is
	   the terminating point. */

	ReportError("corrupted pointers, 'match'");
	MATCH_RETURN(false);
}

/**
 * @brief Attempt to match at a specific point.
 *
 * @param prog The regex program to match against the input string.
 * @param string The input string to match against the regex program.
 * @return `true` if the match is successful, `false` otherwise.
 */
bool Attempt(Regex *prog, const char *string) {

	size_t branch_index = 0; // Must be set to zero !

	eContext.Reg_Input     = string;
	eContext.Start_Ptr_Ptr = prog->startp.begin();
	eContext.End_Ptr_Ptr   = prog->endp.begin();

	// Reset the recursion counter.
	eContext.Recursion_Count = 0;

	// Overhead due to capturing parentheses.
	eContext.Extent_Ptr_BW = string;
	eContext.Extent_Ptr_FW = nullptr;

	std::fill_n(prog->startp.begin(), eContext.Total_Paren + 1, nullptr);
	std::fill_n(prog->endp.begin(), eContext.Total_Paren + 1, nullptr);

	if (Match((&prog->program[REGEX_START_OFFSET]), &branch_index)) {
		prog->startp[0]  = string;
		prog->endp[0]    = eContext.Reg_Input;     // <-- One char AFTER
		prog->extentpBW  = eContext.Extent_Ptr_BW; //     matched string!
		prog->extentpFW  = eContext.Extent_Ptr_FW;
		prog->top_branch = branch_index;

		return true;
	}

	return false;
}

}

/**
 * @brief Match a Regex against a string.
 *
 * @param start The logical start of the string to match against.
 * @param end The logical end of the string to match against. If `nullptr`, the
 *            physical end of the string is used. Matches may not BEGIN past this
 *            point, but may extend past it.
 * @param reverse If `true`, the search is performed in reverse, starting at `end`
 * @param prev_char The character before the start of the match, or -1 if there is no
 *                  character before the start of the match (e.g. at the beginning
 *                  of the string).
 * @param succ_char The character after the end of the match, or -1 if there is no
 *                  character after the end of the match (e.g. at the end of the
 *                  string).
 * @param delimiters A null-terminated string of characters to be considered
 *                   word delimiters matching "<" and ">". If `nullptr`, the
 *                   default delimiters (as set in SetREDefaultWordDelimiters)
 *                   are used.
 * @param look_behind_to The position till where it is safe to perform
 *                       look-behind matches. If `nullptr`, it defaults to the
 *                       start position of the search (pointed at by `start`).
 *                       If set, it must be smaller than or equal to `start`.
 * @param match_to The logical end of the string, till where matches are allowed
 *                 to extend. If `nullptr`, the physical end of the string is used.
 * @param string_end The physical end of the string. Must not be `nullptr` as it is used
 *                   to determine the physical end of the string regardless of '\0' termination.
 * @return `true` if the match is successful, `false` otherwise.
 */
bool Regex::ExecRE(const char *start, const char *end, bool reverse, int prev_char, int succ_char, const char *delimiters, const char *look_behind_to, const char *match_to, const char *string_end) {

	/*
	Notes: look_behind_to <= start <= end <= match_to

	look_behind_to start             end           match_to
	|              |                 |             |
	+--------------+-----------------+-------------+
	|  Look Behind | String Contents | Look Ahead  |
	+--------------+-----------------+-------------+
	*/

	Regex *const re = this;

	// Check validity of program.
	if (!re->isValid()) {
		ReportError("corrupted program");
		return false;
	}

	const char *str;
	bool ret_val = false;

	// If caller has supplied delimiters, make a delimiter table
	eContext.Current_Delimiters = delimiters ? Regex::makeDelimiterTable(delimiters) : Regex::Default_Delimiters;

	// Remember the logical and physical end of the string.
	eContext.End_Of_String      = match_to;
	eContext.Real_End_Of_String = string_end;

	if (!end && reverse) {
		for (end = start; !EndOfString(end); end++) {
		}
		succ_char = '\n';
	} else if (!end) {
		succ_char = '\n';
	}

	// Remember the beginning of the string for matching BOL
	eContext.Start_Of_String = start;
	eContext.Look_Behind_To  = (look_behind_to ? look_behind_to : start);

	eContext.Prev_Is_BOL   = (prev_char == '\n') || (prev_char == -1);
	eContext.Succ_Is_EOL   = (succ_char == '\n') || (succ_char == -1);
	eContext.Prev_Is_Delim = (prev_char == -1) || eContext.Current_Delimiters[static_cast<uint8_t>(prev_char)];
	eContext.Succ_Is_Delim = (succ_char == -1) || eContext.Current_Delimiters[static_cast<uint8_t>(succ_char)];

	eContext.Total_Paren = re->program[1];
	eContext.Num_Braces  = re->program[2];

	// Reset the recursion detection flag
	eContext.Recursion_Limit_Exceeded = false;

	// Allocate memory for {m,n} construct counting variables if need be.
	if (eContext.Num_Braces > 0) {
		eContext.BraceCounts = std::make_unique<uint32_t[]>(eContext.Num_Braces);
	}

	/* Initialize the first nine (9) capturing parentheses start and end
	   pointers to point to the start of the search string.  This is to prevent
	   crashes when later trying to reference captured parens that do not exist
	   in the compiled regex.  We only need to do the first nine since users
	   can only specify \1, \2, ... \9. */
	std::fill_n(re->startp.begin(), 9, start);
	std::fill_n(re->endp.begin(), 9, start);

	auto checked_return = [](bool value) {
		if (eContext.Recursion_Limit_Exceeded) {
			return false;
		}

		return value;
	};

	if (!reverse) { // Forward Search
		if (re->anchor) {
			// Search is anchored at BOL
			if (Attempt(re, start)) {
				ret_val = true;
				return checked_return(ret_val);
			}

			for (str = start; !EndOfString(str) && str != end && !eContext.Recursion_Limit_Exceeded; str++) {

				if (*str == '\n') {
					if (Attempt(re, str + 1)) {
						ret_val = true;
						break;
					}
				}
			}

			return checked_return(ret_val);
		}

		if (re->match_start != '\0') {
			// We know what char match must start with.
			for (str = start; !EndOfString(str) && str != end && !eContext.Recursion_Limit_Exceeded; str++) {

				if (*str == re->match_start) {
					if (Attempt(re, str)) {
						ret_val = true;
						break;
					}
				}
			}

			return checked_return(ret_val);
		}

		// General case
		for (str = start; !EndOfString(str) && str != end && !eContext.Recursion_Limit_Exceeded; str++) {

			if (Attempt(re, str)) {
				ret_val = true;
				break;
			}
		}

		// Beware of a single $ matching \0
#if 1 // NOTE(eteran): possible fix for issue #97
		if (!eContext.Recursion_Limit_Exceeded && !ret_val && EndOfString(str)) {
#else
		if (!eContext.Recursion_Limit_Exceeded && !ret_val && EndOfString(str) && str != end) {
#endif
			if (Attempt(re, str)) {
				ret_val = true;
			}
		}

		return checked_return(ret_val);
	}

	// Search reverse, same as forward, but loops run backward

	// Make sure that we don't start matching beyond the logical end
	if (eContext.End_Of_String != nullptr && end > eContext.End_Of_String) {
		end = eContext.End_Of_String;
	}

	if (re->anchor) {
		// Search is anchored at BOL
		for (str = (end - 1); str >= start && !eContext.Recursion_Limit_Exceeded; str--) {
			if (*str == '\n') {
				if (Attempt(re, str + 1)) {
					ret_val = true;
					return checked_return(ret_val);
				}
			}
		}

		if (!eContext.Recursion_Limit_Exceeded && Attempt(re, start)) {
			ret_val = true;
			return checked_return(ret_val);
		}

		return checked_return(ret_val);
	}

	if (re->match_start != '\0') {
		// We know what char match must start with.
		for (str = end; str >= start && !eContext.Recursion_Limit_Exceeded; str--) {
			if (*str == re->match_start) {
				if (Attempt(re, str)) {
					ret_val = true;
					break;
				}
			}
		}

		return checked_return(ret_val);
	}

	// General case
	for (str = end; str >= start && !eContext.Recursion_Limit_Exceeded; str--) {
		if (Attempt(re, str)) {
			ret_val = true;
			break;
		}
	}

	return checked_return(ret_val);
}
