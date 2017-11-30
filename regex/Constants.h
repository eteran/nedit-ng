
#ifndef CONSTANTS_H_
#define CONSTANTS_H_

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

#endif
