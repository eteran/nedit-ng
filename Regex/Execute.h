
#ifndef EXECUTE_H_
#define EXECUTE_H_

#include "Constants.h"
#include "Util/string_view.h"
#include <cstdint>
#include <array>
#include <bitset>
#include <memory>

// #define ENABLE_CROSS_REGEX_BACKREF

class Regex;

// Global work variables for 'ExecRE'.

template <size_t N>
using array_iterator = typename std::array<const char *, N>::iterator;

struct ExecuteContext {
	std::unique_ptr<uint32_t[]> BraceCounts;     // Define a pointer to an array to hold general (...){m,n} counts.
	const char *Reg_Input;                       // String-input pointer.
	const char *Start_Of_String;                 // Beginning of input, for ^ and < checks.
	const char *End_Of_String;                   // Logical end of input
	const char *Real_End_Of_String;              // Point that the string truly ends and we may not pass safely
	const char *Look_Behind_To;                  // Position till were look behind can safely check back
	array_iterator<NSUBEXP> Start_Ptr_Ptr;       // Pointer to 'startp' array.
	array_iterator<NSUBEXP> End_Ptr_Ptr;         // Ditto for 'endp'.
	const char *Extent_Ptr_FW;                   // Forward extent pointer
	const char *Extent_Ptr_BW;                   // Backward extent pointer
	std::array<const char *, 10> Back_Ref_Start; // Back_Ref_Start [0] and
	std::array<const char *, 10> Back_Ref_End;   // Back_Ref_End [0] are not used. This simplifies indexing.
	int Recursion_Count;                         // Recursion counter

#ifdef ENABLE_CROSS_REGEX_BACKREF
	Regex *Cross_Regex_Backref;
#endif
	uint8_t Num_Braces;                        // Number of general {m,n} constructs. {m,n} quantifiers of SIMPLE atoms are not included in this count.
	uint8_t Total_Paren;                       // Parentheses, (),  counter.
	bool Prev_Is_BOL;
	bool Succ_Is_EOL;
	bool Prev_Is_Delim;
	bool Succ_Is_Delim;
	bool Recursion_Limit_Exceeded;                // Recursion limit exceeded flag
	std::bitset<256> Current_Delimiters;          // Current delimiter table
};


extern ExecuteContext eContext;

#endif
