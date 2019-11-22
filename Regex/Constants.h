
#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstdint>

/* The first byte of the Regex internal 'program' is a magic number to help
   gaurd against corrupted data; the compiled regex code really begins in the
   second byte. */
constexpr uint8_t Magic = 0234;

// Number of text capturing parentheses allowed.
constexpr auto MaxSubExpr = 50u;

/*
 * Measured recursion limits:
 *    Linux:      +/-  40 000 (up to 110 000)
 *    Solaris:    +/-  85 000
 *    HP-UX 11:   +/- 325 000
 *
 * So 10 000 ought to be safe.
 */
constexpr int RecursionLimit = 10000;

constexpr int OP_CODE_SIZE  = 1;
constexpr int NEXT_PTR_SIZE = 2;
constexpr int INDEX_SIZE    = 1;
constexpr int LENGTH_SIZE   = 4;
constexpr int NODE_SIZE     = NEXT_PTR_SIZE + OP_CODE_SIZE;

constexpr auto REG_INFINITY = 0UL;

/* Number of bytes to offset from the beginning of the regex program to the start
   of the actual compiled regex code, i.e. skipping over the MAGIC number and
   the two counters at the front.  */
constexpr int REGEX_START_OFFSET = 3;

#endif
