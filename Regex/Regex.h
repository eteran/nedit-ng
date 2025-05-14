
#ifndef REGEX_H_
#define REGEX_H_

#include "Constants.h"
#include "RegexError.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

/* Flags for CompileRE default settings (Markus Schwarzenberg) */
enum RE_DEFAULT_FLAG {
	RE_DEFAULT_STANDARD         = 0,
	RE_DEFAULT_CASE_INSENSITIVE = 1
	/* RE_DEFAULT_MATCH_NEWLINE = 2    Currently not used. */
};

class Regex {
public:
	Regex(std::string_view exp, int defaultFlags);
	Regex(const Regex &)            = delete;
	Regex &operator=(const Regex &) = delete;
	~Regex()                        = default;

public:

	bool ExecRE(const char *start, const char *end, bool reverse, int prev_char, int succ_char, const char *delimiters, const char *look_behind_to, const char *match_to, const char *string_end);
	bool execute(std::string_view string, bool reverse = false);
	bool execute(std::string_view string, size_t offset, bool reverse = false);
	bool execute(std::string_view string, size_t offset, const char *delimiters, bool reverse = false);
	bool execute(std::string_view string, size_t offset, size_t end_offset, const char *delimiters, bool reverse = false);
	bool execute(std::string_view string, size_t offset, size_t end_offset, int prev, int succ, const char *delimiters, bool reverse = false);
	bool SubstituteRE(std::string_view source, std::string &dest) const;
	bool isValid() const noexcept;

public:
	static void SetDefaultWordDelimiters(std::string_view delimiters);

public:
	std::array<const char *, MaxSubExpr> startp = {};      /* Captured text starting locations. */
	std::array<const char *, MaxSubExpr> endp   = {};      /* Captured text ending locations. */
	const char *extentpBW                       = nullptr; /* Points to the maximum extent of text scanned by ExecRE in front of the string to achieve a match (needed because of positive look-behind.) */
	const char *extentpFW                       = nullptr; /* Points to the maximum extent of text scanned by ExecRE to achieve a match (needed because of positive look-ahead.) */
	size_t top_branch                           = 0;       /* Zero-based index of the top branch that matches. Used by syntax highlighting only. */
	char match_start                            = '\0';    /* Internal use only. */
	char anchor                                 = '\0';    /* Internal use only. */
	std::vector<uint8_t> program;

public:
	static std::bitset<256> Default_Delimiters;
	static std::bitset<256> makeDelimiterTable(std::string_view delimiters);
};

#endif
