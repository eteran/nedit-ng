
#ifndef COMPILE_H_
#define COMPILE_H_

#include "Reader.h"

#include <bitset>
#include <cstdint>
#include <string_view>
#include <vector>

class Regex;

// Global work variables for 'CompileRE'.
struct ParseContext {
	Reader Reg_Parse; // Input scan ptr (scans user's regex)
	std::string_view InputString;
	std::vector<uint8_t> Code;
	const char *Meta_Char;
	size_t Reg_Size;                 // Size of compiled regex code.
	std::bitset<64> Closed_Parens;   // Bit flags indicating () closure.
	std::bitset<64> Paren_Has_Width; // Bit flags indicating ()'s that are known to not match the empty string
	uint16_t Num_Braces;             // Number of general {m,n} constructs. {m,n} quantifiers of SIMPLE atoms are not included in this count.
	uint16_t Total_Paren;            // Parentheses, (),  counter.
	bool FirstPass;
	bool Is_Case_Insensitive;
	bool Match_Newline;
	static constexpr bool Enable_Counting_Quantifier = true;
	char Brace_Char;
};

extern ParseContext pContext;

#endif
