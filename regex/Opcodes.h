
#ifndef OPCODES_H_
#define OPCODES_H_

#include "Constants.h"
#include <stdint.h>


/* STRUCTURE FOR A REGULAR EXPRESSION (regex) gPROGRAM'.
 *
 * This is essentially a linear encoding of a nondeterministic finite-state
 * machine or NFA (aka syntax charts or 'railroad normal form' in parsing
 * technology).  Each node is an opcode plus a NEXT pointer, possibly
 * followed by operands.  NEXT pointers of all nodes except BRANCH implement
 * concatenation; a NEXT pointer with a BRANCH on both ends of it is
 * connecting two alternatives.  (Here we have one of the subtle syntax
 * dependencies:  an individual BRANCH (as opposed to a collection of them) is
 * never concatenated with anything because of operator precedence.)  The
 * operand of some types of nodes is a literal string; for others, it is a node
 * leading into a sub-FSM.  In particular, the operand of a BRANCH node is the
 * first node of the branch. (NB this is _NOT_ a tree structure:  the tail of
 * the branch connects to the thing following the set of BRANCHes.)
 *
 * The opcodes are: */

// DEFINITION            VALUE  MEANING
enum : uint8_t {
    END          = 1, // End of program.

    // Zero width positional assertions.
    BOL          = 2, // Match position at beginning of line.
    EOL          = 3, // Match position at end of line.
    BOWORD       = 4, // Match "" representing word delimiter or BOL
    EOWORD       = 5, // Match "" representing word delimiter or EOL
    NOT_BOUNDARY = 6, // Not word boundary (\B, opposite of < and >)

    // Op codes with null terminated string operands.
    EXACTLY = 7,  // Match this string.
    SIMILAR = 8,  // Match this case insensitive string
    ANY_OF  = 9,  // Match any character in the set.
    ANY_BUT = 10, // Match any character not in the set.

    // Op codes to match any character.
    ANY   = 11,   // Match any one character (implements '.')
    EVERY = 12, // Same as ANY but matches newline.

    // Shortcut escapes, \d, \D, \l, \L, \s, \S, \w, \W, \y, \Y.
    DIGIT         = 13, // Match any digit, i.e. [0123456789]
    NOT_DIGIT     = 14, // Match any non-digit, i.e. [^0123456789]
    LETTER        = 15, // Match any letter character [a-zA-Z]
    NOT_LETTER    = 16, // Match any non-letter character [^a-zA-Z]
    SPACE         = 17, // Match any whitespace character EXCEPT \n
    SPACE_NL      = 18, // Match any whitespace character INCLUDING \n
    NOT_SPACE     = 19, // Match any non-whitespace character
    NOT_SPACE_NL  = 20, // Same as NOT_SPACE but matches newline.
    WORD_CHAR     = 21, // Match any word character [a-zA-Z0-9_]
    NOT_WORD_CHAR = 22, // Match any non-word character [^a-zA-Z0-9_]
    IS_DELIM      = 23, // Match any character that's a word delimiter
    NOT_DELIM     = 24, // Match any character NOT a word delimiter

    /* Quantifier nodes. (Only applied to SIMPLE nodes.  Quantifiers applied
     * to non SIMPLE nodes or larger atoms are implemented using
     * complex constructs.)
     */
    STAR          = 25, // Match this (simple) thing 0 or more times.
    LAZY_STAR     = 26, // Minimal matching STAR
    QUESTION      = 27, // Match this (simple) thing 0 or 1 times.
    LAZY_QUESTION = 28, // Minimal matching QUESTION
    PLUS          = 29, // Match this (simple) thing 1 or more times.
    LAZY_PLUS     = 30, // Minimal matching PLUS
    BRACE         = 31, // Match this (simple) thing m to n times.
    LAZY_BRACE    = 32, // Minimal matching BRACE

    // Nodes used to build complex constructs.
    NOTHING    = 33, // Match empty string (always matches)
    BRANCH     = 34, // Match this alternative, or the next...
    BACK       = 35, // Always matches, NEXT ptr points backward.
    INIT_COUNT = 36, // Initialize {m,n} counter to zero
    INC_COUNT  = 37, // Increment {m,n} counter by one
    TEST_COUNT = 38, // Test {m,n} counter against operand

    // Back Reference nodes.
    BACK_REF      = 39, // Match latest matched parenthesized text
    BACK_REF_CI   = 40, // Case insensitive version of BACK_REF
    X_REGEX_BR    = 41, // Cross-Regex Back-Ref for syntax highlighting
    X_REGEX_BR_CI = 42, // Case insensitive version of X_REGEX_BR_CI

    // Various nodes used to implement parenthetical constructs.
    POS_AHEAD_OPEN   = 43, // Begin positive look ahead
    NEG_AHEAD_OPEN   = 44, // Begin negative look ahead
    LOOK_AHEAD_CLOSE = 45, // End positive or negative look ahead

    POS_BEHIND_OPEN   = 46,   // Begin positive look behind
    NEG_BEHIND_OPEN   = 47,   // Begin negative look behind
    LOOK_BEHIND_CLOSE = 48, // Close look behind

    OPEN  = 49, // Open for capturing parentheses.

    //  OPEN+1 is number 1, etc.
    CLOSE  = (OPEN + NSUBEXP), // Close for capturing parentheses.

    LAST_PAREN  = (CLOSE + NSUBEXP),
};

static_assert(LAST_PAREN <= UINT8_MAX, "Too many parentheses for storage in an uint8_t (LAST_PAREN too big.)");

/* OPCODE NOTES:
   ------------

   All nodes consist of an 8 bit op code followed by 2 bytes that make up a 16
   bit NEXT pointer.  Some nodes have a null terminated character string operand
   following the NEXT pointer.  Other nodes may have an 8 bit index operand.
   The TEST_COUNT node has an index operand followed by a 16 bit test value.
   The BRACE and LAZY_BRACE nodes have two 16 bit values for min and max but no
   index value.

   SIMILAR
      Operand(s): null terminated string

      Implements a case insensitive match of a string.  Mostly intended for use
      in syntax highlighting patterns for keywords of languages like FORTRAN
      and Ada that are case insensitive.  The regex text in this node is
      converted to lower case during regex compile.

   DIGIT, NOT_DIGIT, LETTER, NOT_LETTER, SPACE, NOT_SPACE, WORD_CHAR,
   NOT_WORD_CHAR
      Operand(s): None

      Implements shortcut escapes \d, \D, \l, \L, \s, \S, \w, \W.  The locale
      aware ANSI functions isdigit(), isalpha(), isalnum(), and isspace() are
      used to implement these in the hopes of increasing portability.

   NOT_BOUNDARY
      Operand(s): None

      Implements \B as a zero width assertion that the current character is
      NOT on a word boundary.  Word boundaries are defined to be the position
      between two characters where one of those characters is one of the
      dynamically defined word delimiters, and the other character is not.

   IS_DELIM
      Operand(s): None

      Implements \y as any character that is one of the dynamically
      specified word delimiters.

   NOT_DELIM
      Operand(s): None

      Implements \Y as any character that is NOT one of the dynamically
      specified word delimiters.

   STAR, PLUS, QUESTION, and complex '*', '+', and '?'
      Operand(s): None (Note: NEXT pointer is usually zero.  The code that
                        processes this node skips over it.)

      Complex (parenthesized) versions implemented as circular BRANCH
      structures using BACK.  SIMPLE versions (one character per match) are
      implemented separately for speed and to minimize recursion.

   BRACE, LAZY_BRACE
      Operand(s): minimum value (2 bytes), maximum value (2 bytes)

      Implements the {m,n} construct for atoms that are SIMPLE.

   BRANCH
      Operand(s): None

      The set of branches constituting a single choice are hooked together
      with their NEXT pointers, since precedence prevents anything being
      concatenated to any individual branch.  The NEXT pointer of the last
      BRANCH in a choice points to the thing following the whole choice.  This
      is also where the final NEXT pointer of each individual branch points;
      each branch starts with the operand node of a BRANCH node.

   BACK
      Operand(s): None

      Normal NEXT pointers all implicitly point forward.  Back implicitly
      points backward.  BACK exists to make loop structures possible.

   INIT_COUNT
      Operand(s): index (1 byte)

      Initializes the count array element referenced by the index operand.
      This node is used to build general (i.e. parenthesized) {m,n} constructs.

   INC_COUNT
      Operand(s): index (1 byte)

      Increments the count array element referenced by the index operand.
      This node is used to build general (i.e. parenthesized) {m,n} constructs.

   TEST_COUNT
      Operand(s): index (1 byte), test value (2 bytes)

      Tests the current value of the count array element specified by the
      index operand against the test value.  If the current value is less than
      the test value, control passes to the node after that TEST_COUNT node.
      Otherwise control passes to the node referenced by the NEXT pointer for
      the TEST_COUNT node.  This node is used to build general (i.e.
      parenthesized) {m,n} constructs.

   BACK_REF, BACK_REF_CI
      Operand(s): index (1 byte, value 1-9)

      Implements back references.  This node will attempt to match whatever text
      was most recently captured by the index'th set of parentheses.
      BACK_REF_CI is case insensitive version.

   X_REGEX_BR, X_REGEX_BR_CI
      (NOT IMPLEMENTED YET)

      Operand(s): index (1 byte, value 1-9)

      Implements back references into a previously matched but separate regular
      expression.  This is used by syntax highlighting patterns. This node will
      attempt to match whatever text was most captured by the index'th set of
      parentheses of the separate regex passed to ExecRE. X_REGEX_BR_CI is case
      insensitive version.

   POS_AHEAD_OPEN, NEG_AHEAD_OPEN, LOOK_AHEAD_CLOSE

      Operand(s): None

      Implements positive and negative look ahead.  Look ahead is an assertion
      that something is either there or not there.   Once this is determined the
      regex engine backtracks to where it was just before the look ahead was
      encountered, i.e. look ahead is a zero width assertion.

   POS_BEHIND_OPEN, NEG_BEHIND_OPEN, LOOK_BEHIND_CLOSE

      Operand(s): 2x2 bytes for OPEN (match boundaries), None for CLOSE

      Implements positive and negative look behind.  Look behind is an assertion
      that something is either there or not there in front of the current
      position.  Look behind is a zero width assertion, with the additional
      constraint that it must have a bounded length (for complexity and
      efficiency reasons; note that most other implementation even impose
      fixed length).

   OPEN, CLOSE

      Operand(s): None

      OPEN  + n = Start of parenthesis 'n', CLOSE + n = Close of parenthesis
      'n', and are numbered at compile time.
 */

#endif
