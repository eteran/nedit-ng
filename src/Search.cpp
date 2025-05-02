
#include "Search.h"
#include "DocumentWidget.h"
#include "Highlight.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Regex.h"
#include "TextBuffer.h"
#include "UserCommands.h"
#include "Util/String.h"
#include "Util/algorithm.h"
#include "Util/utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <gsl/gsl_util>

namespace {

// Maximum length of search string history
constexpr int MaxSearchHistory = 100;

// History mechanism for search and replace strings
// TODO(eteran): this appears to be just a circular queue
Search::HistoryEntry SearchReplaceHistory[MaxSearchHistory];
int NHist     = 0;
int HistStart = 0;

/**
 * @brief
 *
 * @param string
 * @param searchString
 * @param wrap
 * @param beginPos
 * @param delimiters
 * @param defaultFlags
 * @return
 */
std::optional<Search::Result> ForwardRegexSearch(std::string_view string, std::string_view searchString, WrapMode wrap, int64_t beginPos, const char *delimiters, int defaultFlags) {

	try {
		Regex compiledRE(searchString, defaultFlags);

		// search from beginPos to end of string
		if (compiledRE.execute(string, static_cast<size_t>(beginPos), delimiters, false)) {

			Search::Result result;
			result.start    = compiledRE.startp[0] - string.data();
			result.end      = compiledRE.endp[0] - string.data();
			result.extentFW = compiledRE.extentpFW - string.data();
			result.extentBW = compiledRE.extentpBW - string.data();
			return result;
		}

		// if wrap turned off, we're done
		if (wrap == WrapMode::NoWrap) {
			return {};
		}

		// search from the beginning of the string to beginPos
		if (compiledRE.execute(string, 0, static_cast<size_t>(beginPos), delimiters, false)) {

			Search::Result result;
			result.start    = compiledRE.startp[0] - string.data();
			result.end      = compiledRE.endp[0] - string.data();
			result.extentFW = compiledRE.extentpFW - string.data();
			result.extentBW = compiledRE.extentpBW - string.data();
			return result;
		}

		return {};
	} catch (const RegexError &e) {
		Q_UNUSED(e)
		/* Note that this does not process errors from compiling the expression.
		 * It assumes that the expression was checked earlier.
		 */
		return {};
	}
}

/**
 * @brief
 *
 * @param string
 * @param searchString
 * @param wrap
 * @param beginPos
 * @param delimiters
 * @param defaultFlags
 * @return
 */
std::optional<Search::Result> BackwardRegexSearch(std::string_view string, std::string_view searchString, WrapMode wrap, int64_t beginPos, const char *delimiters, int defaultFlags) {

	try {
		Regex compiledRE(searchString, defaultFlags);

		// search from beginPos to start of file.  A negative begin pos
		// says begin searching from the far end of the file.
		if (beginPos >= 0) {
			if (compiledRE.execute(string, 0, static_cast<size_t>(beginPos), -1, -1, delimiters, true)) {

				Search::Result result;
				result.start    = compiledRE.startp[0] - string.data();
				result.end      = compiledRE.endp[0] - string.data();
				result.extentFW = compiledRE.extentpFW - string.data();
				result.extentBW = compiledRE.extentpBW - string.data();
				return result;
			}
		}

		// if wrap turned off, we're done
		if (wrap == WrapMode::NoWrap) {
			return {};
		}

		// search from the end of the string to beginPos
		if (beginPos < 0) {
			beginPos = 0;
		}

		if (compiledRE.execute(string, static_cast<size_t>(beginPos), delimiters, true)) {
			Search::Result result;
			result.start    = compiledRE.startp[0] - string.data();
			result.end      = compiledRE.endp[0] - string.data();
			result.extentFW = compiledRE.extentpFW - string.data();
			result.extentBW = compiledRE.extentpBW - string.data();
			return result;
		}

		return {};
	} catch (const RegexError &e) {
		Q_UNUSED(e)
		/* Note that this does not process errors from compiling the expression.
		 * It assumes that the expression was checked earlier.
		 */
		return {};
	}
}

/**
 * @brief
 *
 * @param string
 * @param searchString
 * @param direction
 * @param wrap
 * @param beginPos
 * @param delimiters
 * @param defaultFlags
 * @return
 */
std::optional<Search::Result> SearchRegex(std::string_view string, std::string_view searchString, Direction direction, WrapMode wrap, int64_t beginPos, const char *delimiters, int defaultFlags) {

	switch (direction) {
	case Direction::Forward:
		return ForwardRegexSearch(string, searchString, wrap, beginPos, delimiters, defaultFlags);
	case Direction::Backward:
		return BackwardRegexSearch(string, searchString, wrap, beginPos, delimiters, defaultFlags);
	}

	Q_UNREACHABLE();
}

/**
 * @brief
 *
 * @param string
 * @param searchString
 * @param caseSensitivity
 * @param direction
 * @param wrap
 * @param beginPos
 * @return
 */
std::optional<Search::Result> SearchLiteral(std::string_view string, std::string_view searchString, Direction direction, WrapMode wrap, int64_t beginPos, Qt::CaseSensitivity caseSensitivity) {

	if (searchString.empty()) {
		return {};
	}

	std::string lcString;
	std::string ucString;

	if (caseSensitivity == Qt::CaseSensitive) {
		lcString = std::string(searchString);
		ucString = std::string(searchString);
	} else {
		ucString = to_upper(searchString);
		lcString = to_lower(searchString);
	}

	const auto first = string.begin();
	const auto mid   = first + beginPos;
	const auto last  = string.end();

	auto do_search = [&](std::string_view::iterator it) -> std::optional<Search::Result> {
		if (*it == ucString[0] || *it == lcString[0]) {
			// matched first character
			auto ucPtr   = ucString.begin();
			auto lcPtr   = lcString.begin();
			auto tempPtr = it;

			while (tempPtr != last && (*tempPtr == *ucPtr || *tempPtr == *lcPtr)) {
				++tempPtr;
				++ucPtr;
				++lcPtr;

				if (ucPtr == ucString.end()) {
					// matched whole string
					Search::Result result;
					result.start    = it - string.begin();
					result.end      = tempPtr - string.begin();
					result.extentBW = result.start;
					result.extentFW = result.end;
					return result;
				}
			}
		}

		return {};
	};

	if (direction == Direction::Forward) {

		// search from beginPos to end of string
		for (auto it = mid; it != last; ++it) {
			if (std::optional<Search::Result> result = do_search(it)) {
				return result;
			}
		}

		if (wrap == WrapMode::NoWrap) {
			return {};
		}

		// search from start of file to beginPos
		for (auto it = first; it != mid; ++it) {
			if (std::optional<Search::Result> result = do_search(it)) {
				return result;
			}
		}

		return {};
	}

	// Direction::Backward
	// search from beginPos to start of file.  A negative begin pos
	// says begin searching from the far end of the file

	if (beginPos >= 0) {
		for (auto it = mid; it >= first; --it) {
			if (std::optional<Search::Result> result = do_search(it)) {
				return result;
			}
		}
	}

	if (wrap == WrapMode::NoWrap) {
		return {};
	}

	// search from end of file to beginPos
	for (auto it = last; it >= mid; --it) {
		if (std::optional<Search::Result> result = do_search(it)) {
			return result;
		}
	}

	return {};
}

/*
**  Searches for whole words (Markus Schwarzenberg).
**
**  If the first/last character of 'searchString' is a "normal
**  word character" (not contained in 'delimiters', not a whitespace)
**  then limit search to strings, who's next left/next right character
**  is contained in 'delimiters' or is a whitespace or text begin or end.
**
**  If the first/last character of searchString' itself is contained
**  in delimiters or is a white space, then the neighbour character of the
**  first/last character will not be checked, just a simple match
**  will suffice in that case.
**
*/
std::optional<Search::Result> SearchLiteralWord(std::string_view string, std::string_view searchString, Direction direction, WrapMode wrap, int64_t beginPos, const char *delimiters, Qt::CaseSensitivity caseSensitivity) {

	if (searchString.empty()) {
		return {};
	}

	std::string lcString;
	std::string ucString;
	bool cignore_L = false;
	bool cignore_R = false;

	const auto first = string.begin();
	const auto mid   = first + beginPos;
	const auto last  = string.end();

	auto do_search_word = [&](const std::string_view::iterator it) -> std::optional<Search::Result> {
		if (*it == ucString[0] || *it == lcString[0]) {

			// matched first character
			auto ucPtr   = ucString.begin();
			auto lcPtr   = lcString.begin();
			auto tempPtr = it;

			while (tempPtr != last && (*tempPtr == *ucPtr || *tempPtr == *lcPtr)) {
				++tempPtr;
				++ucPtr;
				++lcPtr;

				if (ucPtr == ucString.end() &&                                                 // matched whole string
					(cignore_R || safe_isspace(*tempPtr) || ::strchr(delimiters, *tempPtr)) && // next char right delimits word ?
					(cignore_L || it == string.begin() ||                                      // border case
					 safe_isspace(it[-1]) || ::strchr(delimiters, it[-1]))) {                  // next char left delimits word ?

					Search::Result result;
					result.start    = it - string.begin();
					result.end      = tempPtr - string.begin();
					result.extentBW = result.start;
					result.extentFW = result.end;
					return result;
				}

				// NOTE(eteran): this doesn't seem possible, but just being careful
				if (ucPtr == ucString.end()) {
					break;
				}
			}
		}

		return {};
	};

	// If there is no language mode, we use the default list of delimiters
	const QByteArray delimiterString = Preferences::GetPrefDelimiters().toLatin1();
	if (!delimiters) {
		delimiters = delimiterString.data();
	}

	if (safe_isspace(searchString.front()) || ::strchr(delimiters, searchString.front())) {
		cignore_L = true;
	}

	if (safe_isspace(searchString.back()) || ::strchr(delimiters, searchString.back())) {
		cignore_R = true;
	}

	if (caseSensitivity == Qt::CaseSensitive) {
		ucString = std::string(searchString);
		lcString = std::string(searchString);
	} else {
		ucString = to_upper(searchString);
		lcString = to_lower(searchString);
	}

	if (direction == Direction::Forward) {

		// search from beginPos to end of string
		for (auto it = mid; it != last; ++it) {
			if (std::optional<Search::Result> result = do_search_word(it)) {
				return result;
			}
		}

		if (wrap == WrapMode::NoWrap) {
			return {};
		}

		// search from start of file to beginPos
		for (auto it = first; it != mid; ++it) {
			if (std::optional<Search::Result> result = do_search_word(it)) {
				return result;
			}
		}
		return {};
	}

	// Direction::Backward
	// search from beginPos to start of file. A negative begin pos
	// says begin searching from the far end of the file
	if (beginPos >= 0) {
		for (auto it = mid; it >= first; --it) {
			if (std::optional<Search::Result> result = do_search_word(it)) {
				return result;
			}
		}
	}

	if (wrap == WrapMode::NoWrap) {
		return {};
	}

	// search from end of file to beginPos
	for (auto it = last; it >= mid; --it) {
		if (std::optional<Search::Result> result = do_search_word(it)) {
			return result;
		}
	}
	return {};
}

/*
** Search the string "string" for "searchString", beginning at "beginPos".
** "delimiters" may be used to provide an alternative set of word delimiters
** for regular expression "<" and ">" characters, or simply passed as nullptr
** for the default delimiter set.
*/
std::optional<Search::Result> SearchStringEx(std::string_view string, std::string_view searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, const char *delimiters) {
	switch (searchType) {
	case SearchType::CaseSenseWord:
		return SearchLiteralWord(string, searchString, direction, wrap, beginPos, delimiters, Qt::CaseSensitive);
	case SearchType::LiteralWord:
		return SearchLiteralWord(string, searchString, direction, wrap, beginPos, delimiters, Qt::CaseInsensitive);
	case SearchType::CaseSense:
		return SearchLiteral(string, searchString, direction, wrap, beginPos, Qt::CaseSensitive);
	case SearchType::Literal:
		return SearchLiteral(string, searchString, direction, wrap, beginPos, Qt::CaseInsensitive);
	case SearchType::Regex:
		return SearchRegex(string, searchString, direction, wrap, beginPos, delimiters, RE_DEFAULT_STANDARD);
	case SearchType::RegexNoCase:
		return SearchRegex(string, searchString, direction, wrap, beginPos, delimiters, RE_DEFAULT_CASE_INSENSITIVE);
	}

	Q_UNREACHABLE();
}

/*
** Substitutes a replace string for a string that was matched using a
** regular expression.  This was added later and is rather inefficient
** because instead of using the compiled regular expression that was used
** to make the match in the first place, it re-compiles the expression
** and redoes the search on the already-matched string.  This allows the
** code to continue using strings to represent the search and replace
** items.
*/
bool ReplaceUsingRegex(std::string_view searchStr, std::string_view replaceStr, std::string_view sourceStr, int64_t beginPos, std::string &dest, int prevChar, const char *delimiters, int defaultFlags) {
	// TODO(eteran): just return an optional<std::string>
	try {
		Regex compiledRE(searchStr, defaultFlags);
		compiledRE.execute(sourceStr, static_cast<size_t>(beginPos), sourceStr.size(), prevChar, -1, delimiters, false);
		return compiledRE.SubstituteRE(replaceStr, dest);
	} catch (const RegexError &e) {
		Q_UNUSED(e)
		return false;
	}
}

}

/*
** Replace all occurrences of "searchString" in "inString" with "replaceString"
** and return a string covering the range between the start of the
** first replacement (returned in "copyStart", and the end of the last
** replacement (returned in "copyEnd")
*/
std::optional<std::string> Search::ReplaceAllInString(std::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int64_t *copyStart, int64_t *copyEnd, const QString &delimiters) {

	Result searchResult;
	int64_t lastEndPos;

	// reject empty string
	if (searchString.isNull()) {
		return {};
	}

	/* rehearse the search first to determine the size of the buffer needed
	   to hold the substituted text.  No substitution done here yet */
	bool found           = true;
	const int replaceLen = replaceString.size();
	int nFound           = 0;
	int64_t removeLen    = 0;
	int64_t addLen       = 0;
	int64_t beginPos     = 0;

	*copyStart = -1;

	while (found) {
		found = SearchString(
			inString,
			searchString,
			Direction::Forward,
			searchType,
			WrapMode::NoWrap,
			beginPos,
			&searchResult,
			delimiters);

		if (found) {
			if (*copyStart < 0) {
				*copyStart = searchResult.start;
			}

			*copyEnd = searchResult.end;
			// start next after match unless match was empty, then endPos+1
			beginPos = (searchResult.start == searchResult.end) ? searchResult.end + 1 : searchResult.end;
			++nFound;
			removeLen += searchResult.end - searchResult.start;
			if (IsRegexType(searchType)) {
				std::string replaceResult;

				ReplaceUsingRE(
					searchString,
					replaceString,
					inString.substr(static_cast<size_t>(searchResult.extentBW)),
					searchResult.start - searchResult.extentBW,
					replaceResult,
					searchResult.start == 0 ? -1 : inString[static_cast<size_t>(searchResult.start) - 1],
					delimiters,
					DefaultRegexFlags(searchType));

				addLen += gsl::narrow<int64_t>(replaceResult.size());
			} else {
				addLen += replaceLen;
			}

			if (searchResult.end == gsl::narrow<int64_t>(inString.size())) {
				break;
			}
		}
	}

	if (nFound == 0) {
		return {};
	}

	const int64_t copyLen = *copyEnd - *copyStart;

	std::string outString;
	outString.reserve(static_cast<size_t>(copyLen - removeLen + addLen));

	/* Scan through the text buffer again, substituting the replace string
	   and copying the part between replaced text to the new buffer  */
	found      = true;
	beginPos   = {};
	lastEndPos = {};

	while (found) {
		found = SearchString(
			inString,
			searchString,
			Direction::Forward,
			searchType,
			WrapMode::NoWrap,
			beginPos,
			&searchResult,
			delimiters);

		if (found) {
			if (beginPos != 0) {
				outString.append(
					&inString[static_cast<size_t>(lastEndPos)],
					&inString[static_cast<size_t>(lastEndPos + (searchResult.start - lastEndPos))]);
			}

			if (IsRegexType(searchType)) {
				std::string replaceResult;

				ReplaceUsingRE(
					searchString,
					replaceString,
					inString.substr(static_cast<size_t>(searchResult.extentBW)),
					searchResult.start - searchResult.extentBW,
					replaceResult,
					searchResult.start == 0 ? -1 : inString[static_cast<size_t>(searchResult.start) - 1],
					delimiters,
					DefaultRegexFlags(searchType));

				outString.append(replaceResult);
			} else {
				outString.append(replaceString.toStdString());
			}

			lastEndPos = searchResult.end;

			// start next after match unless match was empty, then endPos+1
			beginPos = (searchResult.start == searchResult.end) ? searchResult.end + 1 : searchResult.end;
			if (searchResult.end == gsl::narrow<int64_t>(inString.size())) {
				break;
			}
		}
	}

	return outString;
}

/**
 * @brief
 *
 * @param string
 * @param searchString
 * @param direction
 * @param searchType
 * @param wrap
 * @param beginPos
 * @param delimiters
 * @return
 */
std::optional<Search::Result> Search::SearchString(std::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, const QString &delimiters) {
	return SearchStringEx(string, searchString.toStdString(), direction, searchType, wrap, beginPos, delimiters.isNull() ? nullptr : delimiters.toLatin1().data());
}

/**
 * @brief
 *
 * @param string
 * @param searchString
 * @param direction
 * @param searchType
 * @param wrap
 * @param beginPos
 * @param result
 * @param delimiters
 * @return
 */
bool Search::SearchString(std::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, Result *result, const QString &delimiters) {

	assert(result);

	if (std::optional<Result> r = SearchString(string, searchString, direction, searchType, wrap, beginPos, delimiters)) {
		*result = *r;
		return true;
	}

	return false;
}

/**
 * @brief
 *
 * @param searchStr
 * @param replaceStr
 * @param sourceStr
 * @param beginPos
 * @param dest
 * @param prevChar
 * @param delimiters
 * @param defaultFlags
 * @return
 */
bool Search::ReplaceUsingRE(const QString &searchStr, const QString &replaceStr, std::string_view sourceStr, int64_t beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags) {
	return ReplaceUsingRegex(
		searchStr.toStdString(),
		replaceStr.toStdString(),
		sourceStr,
		beginPos,
		dest,
		prevChar,
		delimiters.isNull() ? nullptr : delimiters.toLatin1().data(),
		defaultFlags);
}

/*
** Store the search and replace strings, and search type for later recall.
** If replaceString is nullptr, duplicate the last replaceString used.
** Contiguous incremental searches share the same history entry (each new
** search modifies the current search string, until a non-incremental search
** is made.  To mark the end of an incremental search, call SaveSearchHistory
** again with an empty search string and isIncremental==false.
*/
void Search::SaveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental) {

	static bool currentItemIsIncremental = false;

	/* Cancel accumulation of contiguous incremental searches (even if the
	   information is not worthy of saving) if search is not incremental */
	if (!isIncremental) {
		currentItemIsIncremental = false;
	}

	// Don't save empty search strings
	if (searchString.isEmpty()) {
		return;
	}

	const int index = HistoryIndex(1);

	// If replaceString is nullptr, duplicate the last one (if any)
	if (replaceString.isNull()) {
		replaceString = (index != -1) ? SearchReplaceHistory[index].replace : QString();
	}

	/* Compare the current search and replace strings against the saved ones.
	   If they are identical, don't bother saving */
	if (index != -1 && searchType == SearchReplaceHistory[index].type && SearchReplaceHistory[index].search == searchString && SearchReplaceHistory[index].replace == replaceString) {
		return;
	}

	/* If the current history item came from an incremental search, and the
	   new one is also incremental, just update the entry */
	if (currentItemIsIncremental && isIncremental) {
		if (index != -1) {
			HistoryEntry *entry = &SearchReplaceHistory[index];

			entry->search = searchString;
			entry->type   = searchType;
		}
		return;
	}

	currentItemIsIncremental = isIncremental;

	if (NHist == 0) {
		for (MainWindow *window : MainWindow::allWindows()) {
			window->ui.action_Find_Again->setEnabled(true);
			window->ui.action_Replace_Find_Again->setEnabled(true);
			window->ui.action_Replace_Again->setEnabled(true);
		}
	}

	/* If there are more than MaxSearchHistory strings saved, recycle
	   some space, free the entry that's about to be overwritten */
	if (NHist != MaxSearchHistory) {
		++NHist;
	}

	HistoryEntry *entry = &SearchReplaceHistory[HistStart];
	Q_ASSERT(entry);

	entry->search  = searchString;
	entry->replace = replaceString;
	entry->type    = searchType;

	++HistStart;

	if (HistStart >= MaxSearchHistory) {
		HistStart = 0;
	}
}

/*
** Checks whether a search mode in one of the regular expression modes.
*/
bool Search::IsRegexType(SearchType searchType) {
	return searchType == SearchType::Regex || searchType == SearchType::RegexNoCase;
}

/*
** Returns the default flags for regular expression matching, given a
** regular expression search mode.
*/
int Search::DefaultRegexFlags(SearchType searchType) {
	switch (searchType) {
	case SearchType::Regex:
		return RE_DEFAULT_STANDARD;
	case SearchType::RegexNoCase:
		return RE_DEFAULT_CASE_INSENSITIVE;
	default:
		// We should never get here, but just in case ...
		return RE_DEFAULT_STANDARD;
	}
}

/*
** return an index into the circular buffer arrays of history information
** for search strings, given the number of SaveSearchHistory cycles back from
** the current time.
*/
int Search::HistoryIndex(int nCycles) {

	if (nCycles > NHist || nCycles <= 0) {
		return -1;
	}

	int index = HistStart - nCycles;
	if (index < 0) {
		index = MaxSearchHistory + index;
	}

	return index;
}

/**
 * @brief
 *
 * @param index
 */
auto Search::HistoryByIndex(int index) -> HistoryEntry * {

	if (NHist < 1) {
		return nullptr;
	}

	const int n = HistoryIndex(index);
	if (n == -1) {
		return nullptr;
	}

	return &SearchReplaceHistory[n];
}
