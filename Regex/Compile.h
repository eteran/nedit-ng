
#ifndef COMPILE_H_
#define COMPILE_H_

#include "RegexError.h"
#include "util/string_view.h"
#include <bitset>

class Regex;

// Array sizes for arrays used by function init_ansi_classes.
constexpr int WHITE_SPACE_SIZE = 16;
constexpr int ALNUM_CHAR_SIZE  = 256;

// Global work variables for 'CompileRE'.
struct ParseContext {
    view::string_view::iterator Reg_Parse;     // Input scan ptr (scans user's regex)
    view::string_view::iterator Reg_Parse_End;
    uint8_t *Code_Emit_Ptr;                    // When Code_Emit_Ptr is set to &Compute_Size no code is emitted. Instead, the size of code that WOULD have been generated is accumulated in Reg_Size.  Otherwise, Code_Emit_Ptr points to where compiled regex code is to be written.
    const char *Meta_Char;
    size_t Reg_Size;                           // Size of compiled regex code.
    size_t Total_Paren;                        // Parentheses, (),  counter.
    size_t Num_Braces;                         // Number of general {m,n} constructs. {m,n} quantifiers of SIMPLE atoms are not included in this count.
    std::bitset<32> Closed_Parens;             // Bit flags indicating () closure.
    std::bitset<32> Paren_Has_Width;           // Bit flags indicating ()'s that are known to not match the empty string
    int Is_Case_Insensitive;
    int Match_Newline;
    int Enable_Counting_Quantifier = 1;
    char White_Space[WHITE_SPACE_SIZE];        // Arrays used by
    char Word_Char[ALNUM_CHAR_SIZE];           // functions
    char Letter_Char[ALNUM_CHAR_SIZE];         // init_ansi_classes () and shortcut_escape ().
    char Brace_Char;
};

extern ParseContext pContext;

#endif
