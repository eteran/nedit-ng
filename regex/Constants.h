
#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstdint>

/* The first byte of the regexp internal 'program' is a magic number to help
   gaurd against corrupted data; the compiled regex code really begins in the
   second byte. */
constexpr uint8_t MAGIC = 0234;

// Number of text capturing parentheses allowed.
constexpr auto NSUBEXP = 50u;

/*
 * Measured recursion limits:
 *    Linux:      +/-  40 000 (up to 110 000)
 *    Solaris:    +/-  85 000
 *    HP-UX 11:   +/- 325 000
 *
 * So 10 000 ought to be safe.
 */
constexpr int REGEX_RECURSION_LIMIT = 10000;

constexpr int OP_CODE_SIZE  = 1;
constexpr int NEXT_PTR_SIZE = 2;
constexpr int INDEX_SIZE    = 1;
constexpr int LENGTH_SIZE   = 4;
constexpr int NODE_SIZE     = NEXT_PTR_SIZE + OP_CODE_SIZE;

// Flags to be passed up and down via function parameters during compile.
constexpr int WORST       = 0; // Worst case. No assumptions can be made.
constexpr int HAS_WIDTH   = 1; // Known never to match null string.
constexpr int SIMPLE      = 2; // Simple enough to be STAR/PLUS operand.

constexpr int NO_PAREN    = 0; // Only set by initial call to "chunk".
constexpr int PAREN       = 1; // Used for normal capturing parentheses.
constexpr int NO_CAPTURE  = 2; // Non-capturing parentheses (grouping only).
constexpr int INSENSITIVE = 3; // Case insensitive parenthetical construct
constexpr int SENSITIVE   = 4; // Case sensitive parenthetical construct
constexpr int NEWLINE     = 5; // Construct to match newlines in most cases
constexpr int NO_NEWLINE  = 6; // Construct to match newlines normally

const auto REG_INFINITY   = 0UL;
const auto REG_ZERO       = 0UL;
const auto REG_ONE        = 1UL;

// Flags for function shortcut_escape()
enum ShortcutEscapeFlags {
    CHECK_ESCAPE       = 0, // Check an escape sequence for validity only.
    CHECK_CLASS_ESCAPE = 1, // Check the validity of an escape within a character class
    EMIT_CLASS_BYTES   = 2, // Emit equivalent character class bytes, e.g \d=0123456789
    EMIT_NODE          = 3, // Emit the appropriate node.
};

/* Number of bytes to offset from the beginning of the regex program to the start
   of the actual compiled regex code, i.e. skipping over the MAGIC number and
   the two counters at the front.  */
constexpr int REGEX_START_OFFSET = 3;

// Largest size a compiled regex can be. Probably could be 65535UL.
constexpr auto MAX_COMPILED_SIZE  = 32767UL;

#endif
