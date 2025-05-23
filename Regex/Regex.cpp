/*------------------------------------------------------------------------*
 * 'CompileRE', 'ExecRE', and 'substituteRE' -- regular expression parsing
 *
 * This is a HIGHLY ALTERED VERSION of Henry Spencer's 'regcomp' and
 * 'regexec' code adapted for NEdit.
 *
 * .-------------------------------------------------------------------.
 * | ORIGINAL COPYRIGHT NOTICE:                                        |
 * |                                                                   |
 * | Copyright (c) 1986 by University of Toronto.                      |
 * | Written by Henry Spencer.  Not derived from licensed software.    |
 * |                                                                   |
 * | Permission is granted to anyone to use this software for any      |
 * | purpose on any computer system, and to redistribute it freely,    |
 * | subject to the following restrictions:                            |
 * |                                                                   |
 * | 1. The author is not responsible for the consequences of use of   |
 * |      this software, no matter how awful, even if they arise       |
 * |      from defects in it.                                          |
 * |                                                                   |
 * | 2. The origin of this software must not be misrepresented, either |
 * |      by explicit claim or by omission.                            |
 * |                                                                   |
 * | 3. Altered versions must be plainly marked as such, and must not  |
 * |      be misrepresented as being the original software.            |
 * '-------------------------------------------------------------------'
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. In addition, you may distribute version of this program linked to
 * Motif or Open Motif. See README for details.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *
 * BEWARE that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 *   -- Henry Spencer
 * (Yes, it did!) -- Christopher Conrad, Dec. 1999
 *
 * January, 1994, Mark Edel
 *    Consolidated files, changed names of external functions to avoid
 *    potential conflicts with native regcomp and regexec functions, changed
 *    error reporting to NEdit form, added multi-line and reverse searching,
 *    and added \n \t \u \U \l \L.
 *
 * June, 1996, Mark Edel
 *    Bug in NEXT macro, didn't work for expressions which compiled to over
 *    256 bytes.
 *
 * December, 1999, Christopher Conrad
 *    Reformatted code for readability, improved error output, added octal and
 *    hexadecimal escapes, added back-references (\1-\9), added positive look
 *    ahead: (?=...), added negative lookahead: (?!...),  added non-capturing
 *    parentheses: (?:...), added case insensitive constructs (?i...) and
 *    (?I...), added newline matching constructs (?n...) and (?N...), added
 *    regex comments: (?#...), added shortcut escapes: \d\D\l\L\s\S\w\W\y\Y.
 *    Added "not a word boundary" anchor \B.
 *
 * July, 2002, Eddy De Greef
 *    Added look behind, both positive (?<=...) and negative (?<!...) for
 *    bounded-length patterns.
 *
 * November, 2004, Eddy De Greef
 *    Added constrained matching (allowing specification of the logical end
 *    of the string iso. matching till \0), and fixed several (probably
 *    very old) string overrun errors that could easily result in crashes,
 *    especially in client code.
 */

#include "Regex.h"
#include "Common.h"
#include "Compile.h"
#include "Execute.h"

#include <cassert>

// Default table for determining whether a character is a word delimiter.
std::bitset<256> Regex::Default_Delimiters;

ExecuteContext eContext;
ParseContext pContext;

/* The "internal use only" fields in `Regex.h' are present to pass info from
 * `CompileRE' to `ExecRE' which permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 *   match_start     Character that must begin a match; '\0' if none obvious.
 *   anchor          Is the match anchored (at beginning-of-line only)?
 *
 * `match_start' and `anchor' permit very fast decisions on suitable starting
 * points for a match, considerably reducing the work done by ExecRE. */

/* A node is one char of opcode followed by two chars of NEXT pointer plus
 * any operands.  NEXT pointers are stored as two 8-bit pieces, high order
 * first.  The value is a positive offset from the opcode of the node
 * containing it.  An operand, if any, simply follows the node.  (Note that
 * much of the code generation knows about this implicit relationship.)
 *
 * Using two bytes for NEXT_PTR_SIZE is vast overkill for most things,
 * but allows patterns to get big without disasters. */

/**
 * @brief Execute a `Regex` structure against a string.
 *
 * @param string Text to search within
 * @param reverse If `true`, search backwards through the string.
 * @return `true` if the regex matches, `false` otherwise.
 */
bool Regex::execute(std::string_view string, bool reverse) {
	return execute(string, 0, reverse);
}

/**
 * @brief Execute a `Regex` structure against a string starting at a specific offset.
 *
 * @param string Text to search within
 * @param offset Offset into the string to begin search.
 * @param reverse If `true`, search backwards through the string.
 * @return `true` if the regex matches, `false` otherwise.
 */
bool Regex::execute(std::string_view string, size_t offset, bool reverse) {
	return execute(string, offset, nullptr, reverse);
}

/**
 * @brief Execute a `Regex` structure against a string starting at a specific offset,
 *        using a specific set of delimiters.
 *
 * @param string Text to search within
 * @param offset Offset into the string to begin search.
 * @param delimiters Word delimiters to use (nullptr for default).
 * @param reverse If `true`, search backwards through the string.
 * @return `true` if the regex matches, `false` otherwise.
 */
bool Regex::execute(std::string_view string, size_t offset, const char *delimiters, bool reverse) {
	return execute(string, offset, string.size(), delimiters, reverse);
}

/**
 * @brief Execute a `Regex` structure against a substring of a string,
 *        starting at a specific offset and ending at a specific end_offset,
 *        using a specific set of delimiters.
 *
 * @param string Text to search within
 * @param offset Offset into the string to begin search.
 * @param end_offset Offset into the string to end search.
 * @param delimiters Word delimiters to use (nullptr for default).
 * @param reverse If `true`, search backwards through the string.
 * @return `true` if the regex matches, `false` otherwise.
 */
bool Regex::execute(std::string_view string, size_t offset, size_t end_offset, const char *delimiters, bool reverse) {
	return execute(
		string,
		offset,
		end_offset,
		(offset == 0) ? -1 : string[offset - 1],
		(end_offset == string.size()) ? -1 : string[end_offset],
		delimiters,
		reverse);
}

/**
 * @brief Execute a `Regex` structure against a substring of a string,
 * 	  starting at a specific offset and ending at a specific end_offset,
 *    using a specific set of delimiters,
 *    and considering characters immediately before and after the substring.
 *
 * @param string Text to search within
 * @param offset Offset into the string to begin search.
 * @param end_offset Offset into the string to end search.
 * @param prev Character immediately prior to `string`. Set to '\n' or -1 if `string` starts at the beginning of text.
 * @param succ Character immediately after `end`. Set to '\n' or -1 if `string` ends at the ending of text.
 * @param delimiters Word delimiters to use (nullptr for default).
 * @param reverse If `true`, search backwards through the string.
 * @return `true` if the regex matches, `false` otherwise.
 */
bool Regex::execute(std::string_view string, size_t offset, size_t end_offset, int prev, int succ, const char *delimiters, bool reverse) {
	assert(offset <= end_offset);
	assert(end_offset <= string.size());
	return ExecRE(
		string.data() + offset,
		string.data() + end_offset,
		reverse,
		prev,
		succ,
		delimiters,
		string.data(),
		string.data() + string.size(),
		string.data() + string.size());
}

/**
 * @brief Set the default word delimiters for regex operations.
 *
 * @param delimiters The characters to be used as word delimiters.
 */
void Regex::SetDefaultWordDelimiters(std::string_view delimiters) {
	Default_Delimiters = makeDelimiterTable(delimiters);
}

/**
 * @brief Create a bitset table of delimiters from a string.
 *
 * @param delimiters A string containing characters that are considered delimiters.
 * @return A bitset where each bit represents whether a character is a delimiter.
 */
std::bitset<256> Regex::makeDelimiterTable(std::string_view delimiters) {

	std::bitset<256> table;

	for (const char ch : delimiters) {
		table[static_cast<size_t>(ch)] = true;
	}

	table['\0'] = true; // These
	table['\t'] = true; // characters
	table['\n'] = true; // are always
	table[' ']  = true; // delimiters.

	return table;
}

/**
 * @brief Check if the regex program is valid.
 *
 * @return `true` if the regex program is valid, `false` otherwise.
 */
bool Regex::isValid() const noexcept {
	if (program.empty()) {
		return false;
	}

	return (program[0] == Magic);
}
