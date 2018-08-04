
#ifndef REGEX_H_
#define REGEX_H_

#include "RegexError.h"
#include "Constants.h"
#include "Util/string_view.h"

#include <array>
#include <cstdint>
#include <bitset>
#include <memory>


/* Flags for CompileRE default settings (Markus Schwarzenberg) */
enum RE_DEFAULT_FLAG {
	REDFLT_STANDARD = 0,
	REDFLT_CASE_INSENSITIVE = 1
	/* REDFLT_MATCH_NEWLINE = 2    Currently not used. */
};


class Regex {
public:
	Regex(view::string_view exp, int defaultFlags);
	Regex(const Regex &)            = delete;
	Regex& operator=(const Regex &) = delete;
	~Regex() noexcept               = default;

public:
	/**
	 * Match a 'Regex' structure against a string.
	 *
	 * @param string         Text to search within
	 * @param end            Pointer to the logical end of the string
	 * @param reverse        Backward search.
	 * @param prev_char      Character immediately prior to 'string'.  Set to '\n' or -1 if true beginning of text.
	 * @param succ_char      Character immediately after 'end'.  Set to '\n' or -1 if true beginning of text.
	 * @param delimiters     Word delimiters to use (nullptr for default)
	 * @param look_behind_to Boundary for look-behind; defaults to "string" if nullptr
	 * @param match_till     Boundary to where match can extend. \0 is assumed to be the boundary if not set. Lookahead can cross the boundary.
	 */
	bool ExecRE(const char *string, const char *end, bool reverse, int prev_char, int succ_char, const char *delimiters, const char *look_behind_to, const char *match_to, const char *string_end);

	/**
	 * Match a 'Regex' structure against a string.
	 *
	 * @param string  Text to search within
	 * @param reverse Backward search.
	 */
	bool execute(view::string_view string, bool reverse = false);

	/**
	 * Match a 'Regex' structure against a string.
	 *
	 * @param prog    Compiled regex
	 * @param string  Text to search within
	 * @param offset  Offset into the string to begin search
	 * @param reverse Backward search.
	 */
	bool execute(view::string_view string, size_t offset, bool reverse = false);

	/**
	 * Match a 'Regex' structure against a string.
	 *
	 * @param prog       Compiled regex
	 * @param string     Text to search within
	 * @param offset     Offset into the string to begin search
	 * @param delimiters Word delimiters to use (nullptr for default)
	 * @param reverse    Backward search.
	 */
	bool execute(view::string_view string, size_t offset, const char *delimiters, bool reverse = false);

	/**
	 * Match a 'Regex' structure against a string. Will only match things between offset and end_offset
	 *
	 * @param prog       Compiled regex
	 * @param string     Text to search within
	 * @param offset     Offset into the string to begin search
	 * @param end_offset Offset into the string to end search
	 * @param delimiters Word delimiters to use (nullptr for default)
	 * @param reverse    Backward search.
	 */
	bool execute(view::string_view string, size_t offset, size_t end_offset, const char *delimiters, bool reverse = false);

	/**
	 * Match a 'Regex' structure against a string. Will only match things between offset and end_offset
	 *
	 * @param prog       Compiled regex
	 * @param string     Text to search within
	 * @param offset     Offset into the string to begin search
	 * @param end_offset Offset into the string to end search
	 * @param delimiters Word delimiters to use (nullptr for default)
	 * @param prev       Character immediately prior to 'string'.  Set to '\n' or -1 if true beginning of text.
	 * @param succ       Character immediately after 'end'.  Set to '\n' or -1 if true beginning of text.
	 * @param reverse    Backward search.
	 */
	bool execute(view::string_view string, size_t offset, size_t end_offset, int prev, int succ, const char *delimiters, bool reverse = false);

	/**
	 * Perform substitutions after a 'Regex' match.
	 *
	 * @brief SubstituteRE
	 * @param source
	 * @param dest
	 * @return
	 */
	bool SubstituteRE(view::string_view source, std::string &dest) const noexcept;

public:
	/* Builds a default delimiter table that persists across 'ExecRE' calls that
	   is identical to 'delimiters'.*/
	static void SetDefaultWordDelimiters(view::string_view delimiters);

public:
	std::array<const char *, NSUBEXP> startp = {}; /* Captured text starting locations. */
	std::array<const char *, NSUBEXP> endp   = {}; /* Captured text ending locations. */
	const char *extentpBW       = nullptr;         /* Points to the maximum extent of text scanned by ExecRE in front of the string to achieve a match (needed because of positive look-behind.) */
	const char *extentpFW       = nullptr;         /* Points to the maximum extent of text scanned by ExecRE to achieve a match (needed because of positive look-ahead.) */
	size_t top_branch           = 0;               /* Zero-based index of the top branch that matches. Used by syntax highlighting only. */
	char match_start            = '\0';            /* Internal use only. */
	char anchor                 = '\0';            /* Internal use only. */
	std::unique_ptr<uint8_t[]> program;

public:
	static std::bitset<256> Default_Delimiters;
	static std::bitset<256> makeDelimiterTable(view::string_view delimiters);
};

#endif
