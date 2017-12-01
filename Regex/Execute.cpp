
#include "Execute.h"
#include "Common.h"
#include "Compile.h"
#include "Constants.h"
#include "Opcodes.h"
#include "RegexError.h"
#include "Regex.h"
#include "util/utils.h"

#include <cstdio>
#include <climits>
#include <cstring>
#include <algorithm>

// Address of this used as flag.
uint8_t Compute_Size;

namespace {

int match(uint8_t *prog, int *branch_index_param);
bool attempt(Regex *prog, const char *string);

/**
 * @brief AT_END_OF_STRING
 * @param ptr
 * @return
 */
bool AT_END_OF_STRING(const char *ptr) {

    if(eContext.End_Of_String != nullptr && ptr >= eContext.End_Of_String) {
        return true;
    }

    return false;
}

/**
 * @brief GET_LOWER
 * @param p
 * @return
 */
uint16_t GET_LOWER(uint8_t *p) {
    return static_cast<uint8_t>(((p[NODE_SIZE + 0] & 0xff) << 8) + ((p[NODE_SIZE + 1]) & 0xff));
}

/**
 * @brief GET_UPPER
 * @param p
 * @return
 */
uint16_t GET_UPPER(uint8_t *p) {
    return static_cast<uint8_t>(((p[NODE_SIZE + 2] & 0xff) << 8) + ((p[NODE_SIZE + 3]) & 0xff));
}

/**
 * @brief isDelimiter
 * @param ch
 * @return
 */
bool isDelimiter(int ch) {
    unsigned int n = static_cast<unsigned int>(ch);
    if(n < eContext.Current_Delimiters.size()) {
        return eContext.Current_Delimiters[n];
    }

    return false;
}

/*----------------------------------------------------------------------*
 * greedy
 *
 * Repeatedly match something simple up to "max" times. If max <= 0
 * then match as much as possible (max = infinity).  Uses unsigned long
 * variables to maximize the amount of text matchable for unbounded
 * qualifiers like '*' and '+'.  This will allow at least 4,294,967,295
 * matches (4 Gig!) for an ANSI C compliant compiler.  If you are
 * applying a regex to something bigger than that, you shouldn't be
 * using NEdit!
 *
 * Returns the actual number of matches.
 *----------------------------------------------------------------------*/
unsigned long greedy(uint8_t *p, long max) {

    unsigned long count = REG_ZERO;

    const char *input_str = eContext.Reg_Input;
    uint8_t *operand = OPERAND(p); // Literal char or start of class characters.
    unsigned long max_cmp = (max > 0) ? static_cast<unsigned long>(max) : ULONG_MAX;

    switch (GET_OP_CODE(p)) {
    case ANY:
        /* Race to the end of the line or string. Dot DOESN'T match
           newline. */

        while (count < max_cmp && *input_str != '\n' && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case EVERY:
        // Race to the end of the line or string. Dot DOES match newline.

        while (count < max_cmp && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case EXACTLY: // Count occurrences of single character operand.
        while (count < max_cmp && *operand == *input_str && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case SIMILAR: // Case insensitive version of EXACTLY
        while (count < max_cmp && *operand == tolower(*input_str) && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case ANY_OF: // [...] character class.
        while (count < max_cmp && strchr(reinterpret_cast<char *>(operand), *input_str) != nullptr && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case ANY_BUT: /* [^...] Negated character class- does NOT normally
                     match newline (\n added usually to operand at compile
                     time.) */

        while (count < max_cmp && strchr(reinterpret_cast<char *>(operand), *input_str) == nullptr && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case IS_DELIM: /* \y (not a word delimiter char)
                       NOTE: '\n' and '\0' are always word delimiters. */

        while (count < max_cmp && isDelimiter(*input_str) && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case NOT_DELIM: /* \Y (not a word delimiter char)
                       NOTE: '\n' and '\0' are always word delimiters. */

        while (count < max_cmp && !isDelimiter(*input_str) && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case WORD_CHAR: // \w (word character, alpha-numeric or underscore)
        while (count < max_cmp && (safe_ctype<isalnum>(*input_str) || *input_str == static_cast<uint8_t>('_')) && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case NOT_WORD_CHAR: // \W (NOT a word character)
        while (count < max_cmp && !safe_ctype<isalnum>(*input_str) && *input_str != static_cast<uint8_t>('_') && *input_str != static_cast<uint8_t>('\n') && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case DIGIT: // same as [0123456789]
        while (count < max_cmp && safe_ctype<isdigit>(*input_str) && !AT_END_OF_STRING(input_str)) {
            count++;
            input_str++;
        }

        break;

    case NOT_DIGIT: // same as [^0123456789]
        while (count < max_cmp && !safe_ctype<isdigit>(*input_str) && *input_str != '\n' && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case SPACE: // same as [ \t\r\f\v]-- doesn't match newline.
        while (count < max_cmp && safe_ctype<isspace>(*input_str) && *input_str != '\n' && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case SPACE_NL: // same as [\n \t\r\f\v]-- matches newline.
        while (count < max_cmp && safe_ctype<isspace>(*input_str) && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case NOT_SPACE: // same as [^\n \t\r\f\v]-- doesn't match newline.
        while (count < max_cmp && !safe_ctype<isspace>(*input_str) && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case NOT_SPACE_NL: // same as [^ \t\r\f\v]-- matches newline.
        while (count < max_cmp && (!safe_ctype<isspace>(*input_str) || *input_str == '\n') && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case LETTER: // same as [a-zA-Z]
        while (count < max_cmp && safe_ctype<isalpha>(*input_str) && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    case NOT_LETTER: // same as [^a-zA-Z]
        while (count < max_cmp && !safe_ctype<isalpha>(*input_str) && *input_str != '\n' && !AT_END_OF_STRING(input_str)) {

            count++;
            input_str++;
        }

        break;

    default:
        /* Called inappropriately.  Only atoms that are SIMPLE should
           generate a call to greedy.  The above cases should cover
           all the atoms that are SIMPLE. */

        reg_error("internal error #10 'greedy'");
        count = 0U; // Best we can do.
    }

    // Point to character just after last matched character.

    eContext.Reg_Input = input_str;

    return count;
}

/* The next_ptr () function can consume up to 30% of the time during matching
   because it is called an immense number of times (an average of 25
   next_ptr() calls per match() call was witnessed for Perl syntax
   highlighting). Therefore it is well worth removing some of the function
   call overhead by selectively inlining the next_ptr() calls. Moreover,
   the inlined code can be simplified for matching because one of the tests,
   only necessary during compilation, can be left out.
   The net result of using this inlined version at two critical places is
   a 25% speedup (again, witnesses on Perl syntax highlighting). */

#define NEXT_PTR(in_ptr, out_ptr)                   \
    do {                                            \
        next_ptr_offset = GET_OFFSET(in_ptr);       \
        if (next_ptr_offset == 0)                   \
            out_ptr = nullptr;                      \
        else {                                      \
            if (GET_OP_CODE(in_ptr) == BACK)        \
                out_ptr = in_ptr - next_ptr_offset; \
            else                                    \
                out_ptr = in_ptr + next_ptr_offset; \
        }                                           \
    } while(0)

/*----------------------------------------------------------------------*
 * match - main matching routine
 *
 * Conceptually the strategy is simple: check to see whether the
 * current node matches, call self recursively to see whether the rest
 * matches, and then act accordingly.  In practice we make some effort
 * to avoid recursion, in particular by going through "ordinary" nodes
 * (that don't need to know whether the rest of the match failed) by a
 * loop instead of by recursion.  Returns 0 failure, 1 success.
 *----------------------------------------------------------------------*/
#define MATCH_RETURN(X)    \
    do {                   \
        --eContext.Recursion_Count; \
        return (X);        \
    } while(0)

#define CHECK_RECURSION_LIMIT()       \
    do {                              \
        if (eContext.Recursion_Limit_Exceeded) \
            MATCH_RETURN(0);          \
    } while(0)


int match(uint8_t *prog, int *branch_index_param) {

    uint8_t *scan; // Current node.
    uint8_t *next;          // Next node.
    int next_ptr_offset; // Used by the NEXT_PTR () macro

    if (++eContext.Recursion_Count > REGEX_RECURSION_LIMIT) {
        if (!eContext.Recursion_Limit_Exceeded) // Prevent duplicate errors
            reg_error("recursion limit exceeded, please respecify expression");
        eContext.Recursion_Limit_Exceeded = true;
        MATCH_RETURN(0);
    }

    scan = prog;

    while (scan) {
        NEXT_PTR(scan, next);

        switch (GET_OP_CODE(scan)) {
        case BRANCH: {
            const char *save;

            if (GET_OP_CODE(next) != BRANCH) { // No choice.
                next = OPERAND(scan);          // Avoid recursion.
            } else {
                int branch_index_local = 0;

                do {
                    save = eContext.Reg_Input;

                    if (match(OPERAND(scan), nullptr)) {
                        if (branch_index_param)
                            *branch_index_param = branch_index_local;
                        MATCH_RETURN(1);
                    }

                    CHECK_RECURSION_LIMIT();

                    ++branch_index_local;

                    eContext.Reg_Input = save; // Backtrack.
                    NEXT_PTR(scan, scan);
                } while (scan != nullptr && GET_OP_CODE(scan) == BRANCH);

                MATCH_RETURN(0); // NOT REACHED
            }
        }

        break;

        case EXACTLY: {
            uint8_t *opnd = OPERAND(scan);

            // Inline the first character, for speed.
            if (*opnd != *eContext.Reg_Input) {
                MATCH_RETURN(0);
            }

            const auto str = reinterpret_cast<const char *>(opnd);
            const size_t len = strlen(str);

            if (eContext.End_Of_String != nullptr && eContext.Reg_Input + len > eContext.End_Of_String) {
                MATCH_RETURN(0);
            }

            if (len > 1 && strncmp(str, eContext.Reg_Input, len) != 0) {
                MATCH_RETURN(0);
            }

            eContext.Reg_Input += len;
        }

        break;

        case SIMILAR: {
            uint8_t test;
            uint8_t *opnd = OPERAND(scan);

            /* Note: the SIMILAR operand was converted to lower case during
               regex compile. */

            while ((test = *opnd++) != '\0') {
                if (AT_END_OF_STRING(eContext.Reg_Input) || tolower(*eContext.Reg_Input++) != test) {
                    MATCH_RETURN(0);
                }
            }
        }

        break;

        case BOL: // '^' (beginning of line anchor)
            if (eContext.Reg_Input == eContext.Start_Of_String) {
                if (eContext.Prev_Is_BOL)
                    break;
            } else if (static_cast<int>(*(eContext.Reg_Input - 1)) == '\n') {
                break;
            }

            MATCH_RETURN(0);

        case EOL: // '$' anchor matches end of line and end of string
            if (*eContext.Reg_Input == '\n' || (AT_END_OF_STRING(eContext.Reg_Input) && eContext.Succ_Is_EOL)) {
                break;
            }

            MATCH_RETURN(0);

        case BOWORD: // '<' (beginning of word anchor)
                     /* Check to see if the current character is not a delimiter
                        and the preceding character is. */
            {
                int prev_is_delim;
                if (eContext.Reg_Input == eContext.Start_Of_String) {
                    prev_is_delim = eContext.Prev_Is_Delim;
                } else {
                    prev_is_delim = isDelimiter(*(eContext.Reg_Input - 1));
                }
                if (prev_is_delim) {
                    int current_is_delim;
                    if (AT_END_OF_STRING(eContext.Reg_Input)) {
                        current_is_delim = eContext.Succ_Is_Delim;
                    } else {
                        current_is_delim = isDelimiter(*eContext.Reg_Input);
                    }
                    if (!current_is_delim)
                        break;
                }
            }

            MATCH_RETURN(0);

        case EOWORD: // '>' (end of word anchor)
                     /* Check to see if the current character is a delimiter
                    and the preceding character is not. */
            {
                int prev_is_delim;
                if (eContext.Reg_Input == eContext.Start_Of_String) {
                    prev_is_delim = eContext.Prev_Is_Delim;
                } else {
                    prev_is_delim = isDelimiter(*(eContext.Reg_Input - 1));
                }
                if (!prev_is_delim) {
                    int current_is_delim;
                    if (AT_END_OF_STRING(eContext.Reg_Input)) {
                        current_is_delim = eContext.Succ_Is_Delim;
                    } else {
                        current_is_delim = isDelimiter(*eContext.Reg_Input);
                    }
                    if (current_is_delim)
                        break;
                }
            }

            MATCH_RETURN(0);

        case NOT_BOUNDARY: // \B (NOT a word boundary)
        {
            int prev_is_delim;
            int current_is_delim;
            if (eContext.Reg_Input == eContext.Start_Of_String) {
                prev_is_delim = eContext.Prev_Is_Delim;
            } else {
                prev_is_delim = isDelimiter(*(eContext.Reg_Input - 1));
            }
            if (AT_END_OF_STRING(eContext.Reg_Input)) {
                current_is_delim = eContext.Succ_Is_Delim;
            } else {
                current_is_delim = isDelimiter(*eContext.Reg_Input);
            }
            if (!(prev_is_delim ^ current_is_delim))
                break;
        }

            MATCH_RETURN(0);

        case IS_DELIM: // \y (A word delimiter character.)
            if (isDelimiter(*eContext.Reg_Input) && !AT_END_OF_STRING(eContext.Reg_Input)) {
                eContext.Reg_Input++;
                break;
            }

            MATCH_RETURN(0);

        case NOT_DELIM: // \Y (NOT a word delimiter character.)
            if (!isDelimiter(*eContext.Reg_Input) && !AT_END_OF_STRING(eContext.Reg_Input)) {
                eContext.Reg_Input++;
                break;
            }

            MATCH_RETURN(0);

        case WORD_CHAR: // \w (word character; alpha-numeric or underscore)
            if ((safe_ctype<isalnum>(*eContext.Reg_Input) || *eContext.Reg_Input == '_') && !AT_END_OF_STRING(eContext.Reg_Input)) {
                eContext.Reg_Input++;
                break;
            }

            MATCH_RETURN(0);

        case NOT_WORD_CHAR: // \W (NOT a word character)
            if (safe_ctype<isalnum>(*eContext.Reg_Input) || *eContext.Reg_Input == '_' || *eContext.Reg_Input == '\n' || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case ANY: // '.' (matches any character EXCEPT newline)
            if (AT_END_OF_STRING(eContext.Reg_Input) || *eContext.Reg_Input == '\n')
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case EVERY: // '.' (matches any character INCLUDING newline)
            if (AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case DIGIT: // \d, same as [0123456789]
            if (!safe_ctype<isdigit>(*eContext.Reg_Input) || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case NOT_DIGIT: // \D, same as [^0123456789]
            if (safe_ctype<isdigit>(*eContext.Reg_Input) || *eContext.Reg_Input == '\n' || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case LETTER: // \l, same as [a-zA-Z]
            if (!safe_ctype<isalpha>(*eContext.Reg_Input) || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case NOT_LETTER: // \L, same as [^0123456789]
            if (safe_ctype<isalpha>(*eContext.Reg_Input) || *eContext.Reg_Input == '\n' || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case SPACE: // \s, same as [ \t\r\f\v]
            if (!safe_ctype<isspace>(*eContext.Reg_Input) || *eContext.Reg_Input == '\n' || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case SPACE_NL: // \s, same as [\n \t\r\f\v]
            if (!safe_ctype<isspace>(*eContext.Reg_Input) || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case NOT_SPACE: // \S, same as [^\n \t\r\f\v]
            if (safe_ctype<isspace>(*eContext.Reg_Input) || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case NOT_SPACE_NL: // \S, same as [^ \t\r\f\v]
            if ((safe_ctype<isspace>(*eContext.Reg_Input) && *eContext.Reg_Input != '\n') || AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0);

            eContext.Reg_Input++;
            break;

        case ANY_OF: // [...] character class.
            if (AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0); /* Needed because strchr ()
                                    considers \0 as a member
                                    of the character set. */

            if (strchr(reinterpret_cast<char *>(OPERAND(scan)), *eContext.Reg_Input) == nullptr) {
                MATCH_RETURN(0);
            }

            eContext.Reg_Input++;
            break;

        case ANY_BUT: /* [^...] Negated character class-- does NOT normally
                      match newline (\n added usually to operand at compile
                      time.) */

            if (AT_END_OF_STRING(eContext.Reg_Input))
                MATCH_RETURN(0); // See comment for ANY_OF.

            if (strchr(reinterpret_cast<char *>(OPERAND(scan)), *eContext.Reg_Input) != nullptr) {
                MATCH_RETURN(0);
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
            unsigned long num_matched = REG_ZERO;
            unsigned long min = ULONG_MAX;
            unsigned long max = REG_ZERO;
            const char *save;
            uint8_t next_char;
            uint8_t *next_op;
            bool lazy = false;

            /* Lookahead (when possible) to avoid useless match attempts
               when we know what character comes next. */

            if (GET_OP_CODE(next) == EXACTLY) {
                next_char = *OPERAND(next);
            } else {
                next_char = '\0'; // i.e. Don't know what next character is.
            }

            next_op = OPERAND(scan);

            switch (GET_OP_CODE(scan)) {
            case LAZY_STAR:
                lazy = true;
                /* fallthrough */
            case STAR:
                min = REG_ZERO;
                max = ULONG_MAX;
                break;

            case LAZY_PLUS:
                lazy = true;
                /* fallthrough */
            case PLUS:
                min = REG_ONE;
                max = ULONG_MAX;
                break;

            case LAZY_QUESTION:
                lazy = true;
                /* fallthrough */
            case QUESTION:
                min = REG_ZERO;
                max = REG_ONE;
                break;

            case LAZY_BRACE:
                lazy = true;
                /* fallthrough */
            case BRACE:
                min = static_cast<unsigned long>(GET_OFFSET(scan + NEXT_PTR_SIZE));

                max = static_cast<unsigned long>(GET_OFFSET(scan + (2 * NEXT_PTR_SIZE)));

                if (max <= REG_INFINITY) {
                    max = ULONG_MAX;
                }

                next_op = OPERAND(scan + (2 * NEXT_PTR_SIZE));
            }

            save = eContext.Reg_Input;

            if (lazy) {
                if (min > REG_ZERO) {
                    num_matched = greedy(next_op, min);
                }
            } else {
                num_matched = greedy(next_op, max);
            }

            while (min <= num_matched && num_matched <= max) {
                if (next_char == '\0' || next_char == *eContext.Reg_Input) {
                    if (match(next, nullptr))
                        MATCH_RETURN(1);

                    CHECK_RECURSION_LIMIT();
                }

                // Couldn't or didn't match.

                if (lazy) {
                    if (!greedy(next_op, 1))
                        MATCH_RETURN(0);

                    num_matched++; // Inch forward.
                } else if (num_matched > REG_ZERO) {
                    num_matched--; // Back up.
                } else if (min == REG_ZERO && num_matched == REG_ZERO) {
                    break;
                }

                eContext.Reg_Input = save + num_matched;
            }

            MATCH_RETURN(0);
        }

        break;

        case END:
            if (eContext.Extent_Ptr_FW == nullptr || (eContext.Reg_Input - eContext.Extent_Ptr_FW) > 0) {
                eContext.Extent_Ptr_FW = eContext.Reg_Input;
            }

            MATCH_RETURN(1); // Success!
            break;

        case INIT_COUNT:
            eContext.BraceCounts[*OPERAND(scan)] = REG_ZERO;
            break;

        case INC_COUNT:
            eContext.BraceCounts[*OPERAND(scan)]++;
            break;

        case TEST_COUNT:
            if (eContext.BraceCounts[*OPERAND(scan)] < static_cast<unsigned long>(GET_OFFSET(scan + NEXT_PTR_SIZE + INDEX_SIZE))) {

                next = scan + NODE_SIZE + INDEX_SIZE + NEXT_PTR_SIZE;
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
                const uint8_t paren_no = *OPERAND(scan);

#ifdef ENABLE_CROSS_REGEX_BACKREF
                if (GET_OP_CODE (scan) == X_REGEX_BR || GET_OP_CODE (scan) == X_REGEX_BR_CI) {
                   if (eContext.Cross_Regex_Backref == nullptr)
                       MATCH_RETURN (0);

                   captured = eContext.Cross_Regex_Backref->startp [paren_no];
                   finish   = eContext.Cross_Regex_Backref->endp   [paren_no];
                } else {
#endif
                    captured = eContext.Back_Ref_Start[paren_no];
                    finish   = eContext.Back_Ref_End[paren_no];
#ifdef ENABLE_CROSS_REGEX_BACKREF
                }
#endif

                if ((captured != nullptr) && (finish != nullptr)) {
                    if (captured > finish)
                        MATCH_RETURN(0);

#ifdef ENABLE_CROSS_REGEX_BACKREF
                    if (GET_OP_CODE(scan) == BACK_REF_CI || GET_OP_CODE (scan) == X_REGEX_BR_CI) {
#else
                    if (GET_OP_CODE(scan) == BACK_REF_CI) {
#endif
                        while (captured < finish) {
                            if (AT_END_OF_STRING(eContext.Reg_Input) || tolower(*captured++) != tolower(*eContext.Reg_Input++)) {
                                MATCH_RETURN(0);
                            }
                        }
                    } else {
                        while (captured < finish) {
                            if (AT_END_OF_STRING(eContext.Reg_Input) || *captured++ != *eContext.Reg_Input++)
                                MATCH_RETURN(0);
                        }
                    }

                    break;
                } else {
                    MATCH_RETURN(0);
                }
            }

        case POS_AHEAD_OPEN:
        case NEG_AHEAD_OPEN: {
            const char *save;
            const char *saved_end;
            int answer;

            save = eContext.Reg_Input;

            /* Temporarily ignore the logical end of the string, to allow
               lookahead past the end. */
            saved_end = eContext.End_Of_String;
            eContext.End_Of_String = nullptr;

            answer = match(next, nullptr); // Does the look-ahead regex match?

            CHECK_RECURSION_LIMIT();

            if ((GET_OP_CODE(scan) == POS_AHEAD_OPEN) ? answer : !answer) {
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

                eContext.Reg_Input = save;          // Backtrack to look-ahead start.
                eContext.End_Of_String = saved_end; // Restore logical end.

                /* Jump to the node just after the (?=...) or (?!...)
                   Construct. */

                next = next_ptr(OPERAND(scan)); // Skip 1st branch
                // Skip the chain of branches inside the look-ahead
                while (GET_OP_CODE(next) == BRANCH)
                    next = next_ptr(next);
                next = next_ptr(next); // Skip the LOOK_AHEAD_CLOSE
            } else {
                eContext.Reg_Input = save;          // Backtrack to look-ahead start.
                eContext.End_Of_String = saved_end; // Restore logical end.

                MATCH_RETURN(0);
            }
        }

        break;

        case POS_BEHIND_OPEN:
        case NEG_BEHIND_OPEN: {
            const char *save;
            bool found = false;
            const char *saved_end;

            save = eContext.Reg_Input;
            saved_end = eContext.End_Of_String;

            /* Prevent overshoot (greedy matching could end past the
               current position) by tightening the matching boundary.
               Lookahead inside lookbehind can still cross that boundary. */
            eContext.End_Of_String = eContext.Reg_Input;

            uint16_t lower = GET_LOWER(scan);
            uint16_t upper = GET_UPPER(scan);

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

                int answer = match(next, nullptr); // Does the look-behind regex match?

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
            eContext.Reg_Input = save;
            eContext.End_Of_String = saved_end;

            if ((GET_OP_CODE(scan) == POS_BEHIND_OPEN) ? found : !found) {
                /* The look-behind matches, so we must jump to the next
                   node. The look-behind node is followed by a chain of
                   branches (contents of the look-behind expression), and
                   terminated by a look-behind-close node. */
                next = next_ptr(OPERAND(scan) + LENGTH_SIZE); // 1st branch
                // Skip the chained branches inside the look-ahead
                while (GET_OP_CODE(next) == BRANCH)
                    next = next_ptr(next);
                next = next_ptr(next); // Skip LOOK_BEHIND_CLOSE
            } else {
                // Not a match
                MATCH_RETURN(0);
            }
        } break;

        case LOOK_AHEAD_CLOSE:
        case LOOK_BEHIND_CLOSE:
            /* We have reached the end of the look-ahead or look-behind which
             * implies that we matched it, so return TRUE. */
            MATCH_RETURN(1);

        default:
            if ((GET_OP_CODE(scan) > OPEN) && (GET_OP_CODE(scan) < OPEN + NSUBEXP)) {

                uint8_t no = GET_OP_CODE(scan) - OPEN;
                const char *save = eContext.Reg_Input;

                if (no < 10) {
                    eContext.Back_Ref_Start[no] = save;
                    eContext.Back_Ref_End[no] = nullptr;
                }

                if (match(next, nullptr)) {
                    /* Do not set 'Start_Ptr_Ptr' if some later invocation (think
                       recursion) of the same parentheses already has. */

                    if (eContext.Start_Ptr_Ptr[no] == nullptr) {
                        eContext.Start_Ptr_Ptr[no] = save;
                    }

                    MATCH_RETURN(1);
                } else {
                    MATCH_RETURN(0);
                }
            } else if ((GET_OP_CODE(scan) > CLOSE) && (GET_OP_CODE(scan) < CLOSE + NSUBEXP)) {

                uint8_t no       = GET_OP_CODE(scan) - CLOSE;
                const char *save = eContext.Reg_Input;

                if (no < 10)
                    eContext.Back_Ref_End[no] = save;

                if (match(next, nullptr)) {
                    /* Do not set 'End_Ptr_Ptr' if some later invocation of the
                       same parentheses already has. */

                    if (eContext.End_Ptr_Ptr[no] == nullptr) {
                        eContext.End_Ptr_Ptr[no] = save;
                    }

                    MATCH_RETURN(1);
                } else {
                    MATCH_RETURN(0);
                }
            } else {
                reg_error("memory corruption, 'match'");

                MATCH_RETURN(0);
            }

            break;
        }

        scan = next;
    }

    /* We get here only if there's trouble -- normally "case END" is
       the terminating point. */

    reg_error("corrupted pointers, 'match'");

    MATCH_RETURN(0);
}

/*----------------------------------------------------------------------*
 * attempt - try match at specific point, returns: false failure, true success
 *----------------------------------------------------------------------*/
bool attempt(Regex *prog, const char *string) {

    int branch_index = 0; // Must be set to zero !

    eContext.Reg_Input     = string;
    eContext.Start_Ptr_Ptr = prog->startp.begin();
    eContext.End_Ptr_Ptr   = prog->endp.begin();

    // Reset the recursion counter.
    eContext.Recursion_Count = 0;

    // Overhead due to capturing parentheses.
    eContext.Extent_Ptr_BW = string;
    eContext.Extent_Ptr_FW = nullptr;

    std::fill_n(prog->startp.begin(), pContext.Total_Paren + 1, nullptr);
    std::fill_n(prog->endp.begin(),   pContext.Total_Paren + 1, nullptr);

    if (match((prog->program + REGEX_START_OFFSET), &branch_index)) {
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

/*
 * match a Regex against a string
 *
 * If 'end' is non-nullptr, matches may not BEGIN past end, but may extend past
 * it.  If reverse is true, 'end' must be specified, and searching begins at
 * 'end'.  "isbol" should be set to true if the beginning of the string is the
 * actual beginning of a line (since 'ExecRE' can't look backwards from the
 * beginning to find whether there was a newline before).  Likewise, "isbow"
 * asks whether the string is preceded by a word delimiter.  End of string is
 * always treated as a word and line boundary (there may be cases where it
 * shouldn't be, in which case, this should be changed).  "delimit" (if
 * non-null) specifies a null-terminated string of characters to be considered
 * word delimiters matching "<" and ">".  if "delimit" is nullptr, the default
 * delimiters (as set in SetREDefaultWordDelimiters) are used.
 * Look_behind_to indicates the position till where it is safe to
 * perform look-behind matches. If set, it should be smaller than or equal
 * to the start position of the search (pointed at by string). If it is nullptr,
 * it defaults to the start position.
 * Finally, match_to indicates the logical end of the string, till where
 * matches are allowed to extend. Note that look-ahead patterns may look
 * past that boundary. If match_to is set to nullptr, the terminating \0 is
 * assumed to correspond to the logical boundary. Match_to, if set, must be
 * larger than or equal to end, if set.
 */

/*
Notes: look_behind_to <= string <= end <= match_to

look_behind_to string            end           match_to
|              |                 |             |
+--------------+-----------------+-------------+
|  Look Behind | String Contents | Look Ahead  |
+--------------+-----------------+-------------+

*/

/**
 * @brief Regex::ExecRE
 * @param string
 * @param end
 * @param reverse
 * @param prev_char
 * @param succ_char
 * @param delimiters
 * @param look_behind_to
 * @param match_to
 * @return
 */
bool Regex::ExecRE(const char *string, const char *end, bool reverse, char prev_char, char succ_char, const char *delimiters, const char *look_behind_to, const char *match_to) {
    assert(string);

    Regex *const re = this;

    // Check validity of program.
    if (U_CHAR_AT(re->program) != MAGIC) {
        reg_error("corrupted program");
        return false;
    }

    const char *str;
    bool ret_val = false;

    // If caller has supplied delimiters, make a delimiter table
    eContext.Current_Delimiters = delimiters ? Regex::makeDelimiterTable(delimiters) : Regex::Default_Delimiters;

    // Remember the logical end of the string.
    eContext.End_Of_String = match_to;

    if (!end && reverse) {
        for (end = string; !AT_END_OF_STRING(end); end++) {
        }
        succ_char = '\n';
    } else if(!end) {
        succ_char = '\n';
    }

    // Remember the beginning of the string for matching BOL
    eContext.Start_Of_String = string;
    eContext.Look_Behind_To  = (look_behind_to ? look_behind_to : string);

    eContext.Prev_Is_BOL   = (prev_char == '\n') || (prev_char == '\0');
    eContext.Succ_Is_EOL   = (succ_char == '\n') || (succ_char == '\0');
    eContext.Prev_Is_Delim = eContext.Current_Delimiters[static_cast<uint8_t>(prev_char)];
    eContext.Succ_Is_Delim = eContext.Current_Delimiters[static_cast<uint8_t>(succ_char)];

    pContext.Total_Paren = re->program[1];
    pContext.Num_Braces  = re->program[2];

    // Reset the recursion detection flag
    eContext.Recursion_Limit_Exceeded = false;

    // Allocate memory for {m,n} construct counting variables if need be.
    if (pContext.Num_Braces > 0) {
        eContext.BraceCounts = new uint32_t[pContext.Num_Braces];
    } else {
        eContext.BraceCounts = nullptr;
    }

    /* Initialize the first nine (9) capturing parentheses start and end
       pointers to point to the start of the search string.  This is to prevent
       crashes when later trying to reference captured parens that do not exist
       in the compiled regex.  We only need to do the first nine since users
       can only specify \1, \2, ... \9. */
    std::fill_n(re->startp.begin(), 9, string);
    std::fill_n(re->endp.begin(),   9, string);

    if (!reverse) { // Forward Search
        if (re->anchor) {
            // Search is anchored at BOL

            if (attempt(re, string)) {
                ret_val = true;
                goto SINGLE_RETURN;
            }

            for (str = string; !AT_END_OF_STRING(str) && str != end && !eContext.Recursion_Limit_Exceeded; str++) {

                if (*str == '\n') {
                    if (attempt(re, str + 1)) {
                        ret_val = true;
                        break;
                    }
                }
            }

            goto SINGLE_RETURN;

        } else if (re->match_start != '\0') {
            // We know what char match must start with.

            for (str = string; !AT_END_OF_STRING(str) && str != end && !eContext.Recursion_Limit_Exceeded; str++) {

                if (*str == static_cast<uint8_t>(re->match_start)) {
                    if (attempt(re, str)) {
                        ret_val = true;
                        break;
                    }
                }
            }

            goto SINGLE_RETURN;
        } else {
            // General case

            for (str = string; !AT_END_OF_STRING(str) && str != end && !eContext.Recursion_Limit_Exceeded; str++) {

                if (attempt(re, str)) {
                    ret_val = true;
                    break;
                }
            }

            // Beware of a single $ matching \0
            if (!eContext.Recursion_Limit_Exceeded && !ret_val && AT_END_OF_STRING(str) && str != end) {
                if (attempt(re, str)) {
                    ret_val = true;
                }
            }

            goto SINGLE_RETURN;
        }
    } else { // Search reverse, same as forward, but loops run backward

        // Make sure that we don't start matching beyond the logical end
        if (eContext.End_Of_String != nullptr && end > eContext.End_Of_String) {
            end = eContext.End_Of_String;
        }

        if (re->anchor) {
            // Search is anchored at BOL

            for (str = (end - 1); str >= string && !eContext.Recursion_Limit_Exceeded; str--) {

                if (*str == '\n') {
                    if (attempt(re, str + 1)) {
                        ret_val = true;
                        goto SINGLE_RETURN;
                    }
                }
            }

            if (!eContext.Recursion_Limit_Exceeded && attempt(re, string)) {
                ret_val = true;
                goto SINGLE_RETURN;
            }

            goto SINGLE_RETURN;
        } else if (re->match_start != '\0') {
            // We know what char match must start with.

            for (str = end; str >= string && !eContext.Recursion_Limit_Exceeded; str--) {

                if (*str == static_cast<uint8_t>(re->match_start)) {
                    if (attempt(re, str)) {
                        ret_val = true;
                        break;
                    }
                }
            }

            goto SINGLE_RETURN;
        } else {
            // General case

            for (str = end; str >= string && !eContext.Recursion_Limit_Exceeded; str--) {

                if (attempt(re, str)) {
                    ret_val = true;
                    break;
                }
            }
        }
    }

SINGLE_RETURN:
    delete [] eContext.BraceCounts;

    if (eContext.Recursion_Limit_Exceeded) {
        return false;
    }

    return ret_val;
}
