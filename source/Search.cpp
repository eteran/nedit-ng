
#include "Search.h"
#include "Util/utils.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "TruncSubstitution.h"
#include "TextBuffer.h"
#include "WrapStyle.h"
#include "Highlight.h"
#include "preferences.h"
#include "Regex.h"
#include "userCmds.h"
#include "Util/algorithm.h"
#include <gsl/gsl_util>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// Maximum length of search string history
constexpr int MAX_SEARCH_HISTORY = 100;

// History mechanism for search and replace strings
Search::HistoryEntry SearchReplaceHistory[MAX_SEARCH_HISTORY];
int NHist = 0;
int HistStart = 0;

/**
 * @brief to_upper
 * @param s
 * @return
 */
std::string to_upper(view::string_view s) {

    std::string str;
    str.reserve(s.size());
    std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
        return safe_ctype<toupper>(ch);
    });
    return str;
}

/**
 * @brief to_lower
 * @param s
 * @return
 */
std::string to_lower(view::string_view s) {

    std::string str;
    str.reserve(s.size());
    std::transform(s.begin(), s.end(), std::back_inserter(str), [](char ch) {
        return safe_ctype<tolower>(ch);
    });
    return str;
}

/**
 * @brief forwardRegexSearch
 * @param string
 * @param searchString
 * @param wrap
 * @param beginPos
 * @param result
 * @param delimiters
 * @param defaultFlags
 * @return
 */
bool forwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int64_t beginPos, Search::Result *result, const char *delimiters, int defaultFlags) {

    try {
        Regex compiledRE(searchString, defaultFlags);

        // search from beginPos to end of string
        if (compiledRE.execute(string, static_cast<size_t>(beginPos), delimiters, false)) {

            result->start    = compiledRE.startp[0] - &string[0];
            result->end      = compiledRE.endp[0]   - &string[0];
            result->extentFW = compiledRE.extentpFW - &string[0];
            result->extentBW = compiledRE.extentpBW - &string[0];
            return true;
        }

        // if wrap turned off, we're done
        if (wrap == WrapMode::NoWrap) {
            return false;
        }

        // search from the beginning of the string to beginPos
        if (compiledRE.execute(string, 0, static_cast<size_t>(beginPos), delimiters, false)) {

            result->start    = compiledRE.startp[0] - &string[0];
            result->end      = compiledRE.endp[0]   - &string[0];
            result->extentFW = compiledRE.extentpFW - &string[0];
            result->extentBW = compiledRE.extentpBW - &string[0];
            return true;
        }

        return false;
    } catch(const RegexError &e) {
        Q_UNUSED(e);
        /* Note that this does not process errors from compiling the expression.
         * It assumes that the expression was checked earlier.
         */
        return false;
    }
}

/**
 * @brief backwardRegexSearch
 * @param string
 * @param searchString
 * @param wrap
 * @param beginPos
 * @param result
 * @param delimiters
 * @param defaultFlags
 * @return
 */
bool backwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int64_t beginPos, Search::Result *result, const char *delimiters, int defaultFlags) {

    try {
        Regex compiledRE(searchString, defaultFlags);

        // search from beginPos to start of file.  A negative begin pos
        // says begin searching from the far end of the file.
        if (beginPos >= 0) {
            if (compiledRE.execute(string, 0, static_cast<size_t>(beginPos), -1, -1, delimiters, true)) {

                result->start    = compiledRE.startp[0] - &string[0];
                result->end      = compiledRE.endp[0]   - &string[0];
                result->extentFW = compiledRE.extentpFW - &string[0];
                result->extentBW = compiledRE.extentpBW - &string[0];
                return true;
            }
        }

        // if wrap turned off, we're done
        if (wrap == WrapMode::NoWrap) {
            return false;
        }

        // search from the end of the string to beginPos
        if (beginPos < 0) {
            beginPos = 0;
        }

        if (compiledRE.execute(string, static_cast<size_t>(beginPos), delimiters, true)) {

            result->start    = compiledRE.startp[0] - &string[0];
            result->end      = compiledRE.endp[0]   - &string[0];
            result->extentFW = compiledRE.extentpFW - &string[0];
            result->extentBW = compiledRE.extentpBW - &string[0];
            return true;
        }

        return false;
    } catch(const RegexError &e) {
        Q_UNUSED(e);
        /* Note that this does not process errors from compiling the expression.
         * It assumes that the expression was checked earlier.
         */
        return false;
    }
}

/**
 * @brief searchRegex
 * @param string
 * @param searchString
 * @param direction
 * @param wrap
 * @param beginPos
 * @param result
 * @param delimiters
 * @param defaultFlags
 * @return
 */
bool searchRegex(view::string_view string, view::string_view searchString, Direction direction, WrapMode wrap, int64_t beginPos, Search::Result *result, const char *delimiters, int defaultFlags) {

    switch(direction) {
    case Direction::Forward:
        return forwardRegexSearch(string, searchString, wrap, beginPos, result, delimiters, defaultFlags);
    case Direction::Backward:
        return backwardRegexSearch(string, searchString, wrap, beginPos, result, delimiters, defaultFlags);
    }

    Q_UNREACHABLE();
}

/**
 * @brief searchLiteral
 * @param string
 * @param searchString
 * @param caseSense
 * @param direction
 * @param wrap
 * @param beginPos
 * @param result
 * @return
 */
bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int64_t beginPos, Search::Result *result) {

    // TODO(eteran): investigate if we can rework this in terms of std::search
    // TODO(eteran): wrapping seems to be handled at a higher level, is there any code which actually asses WrapMode::Wrap to this function?

    std::string lcString;
    std::string ucString;

    if (caseSense) {
        lcString = searchString.to_string();
        ucString = searchString.to_string();
    } else {
        ucString = to_upper(searchString);
        lcString = to_lower(searchString);
    }

    const auto first = string.begin();
    const auto mid   = first + beginPos;
    const auto last  = string.end();

    auto do_search = [&](view::string_view::iterator it) {
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
                    result->start    = it - string.begin();
                    result->end      = tempPtr - string.begin();
                    result->extentBW = result->start;
                    result->extentFW = result->end;
                    return true;
                }
            }
        }

        return false;
    };

    if (direction == Direction::Forward) {

        // search from beginPos to end of string
        for (auto it = mid; it != last; ++it) {
            if(do_search(it)) {
                return true;
            }
        }

        if (wrap == WrapMode::NoWrap) {
            return false;
        }

        // search from start of file to beginPos
        for (auto it = first; it != mid; ++it) {
            if(do_search(it)) {
                return true;
            }
        }

        return false;
    } else {
        // Direction::BACKWARD
        // search from beginPos to start of file.  A negative begin pos
        // says begin searching from the far end of the file

        if (beginPos >= 0) {
            for (auto it = mid; it >= first; --it) {
                if(do_search(it)) {
                    return true;
                }
            }
        }

        if (wrap == WrapMode::NoWrap) {
            return false;
        }

        // search from end of file to beginPos
        for (auto it = last; it >= mid; --it) {
            if(do_search(it)) {
                return true;
            }
        }

        return false;
    }
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
bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int64_t beginPos, Search::Result *result, const char *delimiters) {

    // TODO(eteran): investigate if we can rework this in terms of std::search
    // TODO(eteran): wrapping seems to be handled at a higher level, is there any code which actually asses WrapMode::Wrap to this function?

    std::string lcString;
    std::string ucString;
    bool cignore_L = false;
    bool cignore_R = false;

    const auto first = string.begin();
    const auto mid   = first + beginPos;
    const auto last  = string.end();

    auto do_search_word = [&](const view::string_view::iterator it) {
        if (*it == ucString[0] || *it == lcString[0]) {

            // matched first character
            auto ucPtr   = ucString.begin();
            auto lcPtr   = lcString.begin();
            auto tempPtr = it;

            while (tempPtr != last && (*tempPtr == *ucPtr || *tempPtr == *lcPtr)) {
                ++tempPtr;
                ++ucPtr;
                ++lcPtr;

                if (ucPtr == ucString.end() &&                                                         // matched whole string
                    (cignore_R || safe_ctype<isspace>(*tempPtr) || strchr(delimiters, *tempPtr)) &&    // next char right delimits word ?
                    (cignore_L || it == string.begin() ||                                              // border case
                     safe_ctype<isspace>(it[-1]) || strchr(delimiters, it[-1]))) {                     // next char left delimits word ?

                    result->start    = it - string.begin();
                    result->end      = tempPtr - string.begin();
                    result->extentBW = result->start;
                    result->extentFW = result->end;
                    return true;
                }

                // NOTE(eteran): this doesn't seem possible, but just being careful
                if(ucPtr == ucString.end()) {
                    break;
                }
            }
        }

        return false;
    };


    // If there is no language mode, we use the default list of delimiters
    const QByteArray delimiterString = Preferences::GetPrefDelimiters().toLatin1();
    if(!delimiters) {
        delimiters = delimiterString.data();
    }

    if (safe_ctype<isspace>(searchString.front()) || strchr(delimiters, searchString.front())) {
        cignore_L = true;
    }

    if (safe_ctype<isspace>(searchString.back()) || strchr(delimiters, searchString.back())) {
        cignore_R = true;
    }

    if (caseSense) {
        ucString = searchString.to_string();
        lcString = searchString.to_string();
    } else {
        ucString = to_upper(searchString);
        lcString = to_lower(searchString);
    }

    if (direction == Direction::Forward) {

        // search from beginPos to end of string
        for (auto it = mid; it != last; ++it) {
            if(do_search_word(it)) {
                return true;
            }
        }

        if (wrap == WrapMode::NoWrap) {
            return false;
        }

        // search from start of file to beginPos
        for (auto it = first; it != mid; ++it) {
            if(do_search_word(it)) {
                return true;
            }
        }
        return false;
    } else {
        // Direction::BACKWARD
        // search from beginPos to start of file. A negative begin pos
        // says begin searching from the far end of the file

        if (beginPos >= 0) {
            for (auto it = mid; it >= first; --it) {
                if(do_search_word(it)) {
                    return true;
                }
            }
        }

        if (wrap == WrapMode::NoWrap) {
            return false;
        }

        // search from end of file to beginPos
        for (auto it = last; it >= mid; --it) {
            if(do_search_word(it)) {
                return true;
            }
        }
        return false;
    }
}

/*
** Search the null terminated string "string" for "searchString", beginning at
** "beginPos".  Returns the boundaries of the match in "startPos" and "endPos".
** extentBW and extentFW return the backwardmost and forwardmost
** positions used to make the match, which are usually startPos and endPos,
** but may extend further if positive lookahead or lookbehind was used in
** a regular expression match.  "delimiters" may be used to provide an
** alternative set of word delimiters for regular expression "<" and ">"
** characters, or simply passed as null for the default delimiter set.
*/
bool SearchStringEx(view::string_view string, view::string_view searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, Search::Result *result, const char *delimiters) {
    switch (searchType) {
    case SearchType::CaseSenseWord:
        return searchLiteralWord(string, searchString, /*caseSense=*/true, direction, wrap, beginPos, result, delimiters);
    case SearchType::LiteralWord:
        return searchLiteralWord(string, searchString, /*caseSense=*/false, direction, wrap, beginPos, result, delimiters);
    case SearchType::CaseSense:
        return searchLiteral(string, searchString, /*caseSense=*/true, direction, wrap, beginPos, result);
    case SearchType::Literal:
        return searchLiteral(string, searchString, /*caseSense=*/false, direction, wrap, beginPos, result);
    case SearchType::Regex:
        return searchRegex(string, searchString, direction, wrap, beginPos, result, delimiters, REDFLT_STANDARD);
    case SearchType::RegexNoCase:
        return searchRegex(string, searchString, direction, wrap, beginPos, result, delimiters, REDFLT_CASE_INSENSITIVE);
    }

    Q_UNREACHABLE();
}

/*
** Substitutes a replace string for a string that was matched using a
** regular expression.  This was added later and is rather ineficient
** because instead of using the compiled regular expression that was used
** to make the match in the first place, it re-compiles the expression
** and redoes the search on the already-matched string.  This allows the
** code to continue using strings to represent the search and replace
** items.
*/
bool replaceUsingRegex(view::string_view searchStr, view::string_view replaceStr, view::string_view sourceStr, int64_t beginPos, std::string &dest, int prevChar, const char *delimiters, int defaultFlags) {
    try {
        Regex compiledRE(searchStr, defaultFlags);
        compiledRE.execute(sourceStr, static_cast<size_t>(beginPos), sourceStr.size(), prevChar, -1, delimiters, false);
        return compiledRE.SubstituteRE(replaceStr, dest);
    } catch(const RegexError &e) {
        Q_UNUSED(e);
        return false;
    }
}

}

/*
** Replace all occurences of "searchString" in "inString" with "replaceString"
** and return a string covering the range between the start of the
** first replacement (returned in "copyStart", and the end of the last
** replacement (returned in "copyEnd")
*/
boost::optional<std::string> Search::ReplaceAllInStringEx(view::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int64_t *copyStart, int64_t *copyEnd, const QString &delimiters) {

    Result searchResult;
    int64_t lastEndPos;    

    // reject empty string
    if (searchString.isNull()) {
        return boost::none;
    }

    /* rehearse the search first to determine the size of the buffer needed
       to hold the substituted text.  No substitution done here yet */
    bool found       = true;
    int replaceLen   = replaceString.size();
    int nFound       = 0;
    int removeLen    = 0;
    int addLen       = 0;
    int64_t beginPos = 0;

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
            nFound++;
            removeLen += searchResult.end - searchResult.start;
            if (isRegexType(searchType)) {
                std::string replaceResult;

                replaceUsingRE(
                    searchString,
                    replaceString,
                    substr(inString, static_cast<size_t>(searchResult.extentBW)),
                    searchResult.start - searchResult.extentBW,
                    replaceResult,
                    searchResult.start == 0 ? -1 : inString[static_cast<size_t>(searchResult.start) - 1],
                    delimiters,
                    defaultRegexFlags(searchType));

                addLen += replaceResult.size();
            } else {
                addLen += replaceLen;
            }

            if (searchResult.end == gsl::narrow<int64_t>(inString.size())) {
                break;
            }
        }
    }

    if (nFound == 0) {
        return boost::none;
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

            if (isRegexType(searchType)) {
                std::string replaceResult;

                replaceUsingRE(
                    searchString,
                    replaceString,
                    substr(inString, static_cast<size_t>(searchResult.extentBW)),
                    searchResult.start - searchResult.extentBW,
                    replaceResult,
                    searchResult.start == 0 ? -1 : inString[static_cast<size_t>(searchResult.start) - 1],
                    delimiters,
                    defaultRegexFlags(searchType));

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
 * @brief Search::SearchString
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
bool Search::SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, Result *result, const QString &delimiters) {

    assert(result);

    return SearchStringEx(
                string,
                searchString.toStdString(),
                direction,
                searchType,
                wrap,
                beginPos,
                result,
                delimiters.isNull() ? nullptr : delimiters.toLatin1().data());

}

bool Search::replaceUsingRE(const QString &searchStr, const QString &replaceStr, view::string_view sourceStr, int64_t beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags) {
    return replaceUsingRegex(
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
** is made.  To mark the end of an incremental search, call saveSearchHistory
** again with an empty search string and isIncremental==false.
*/
void Search::saveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental) {

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

    const int index = historyIndex(1);

	// If replaceString is nullptr, duplicate the last one (if any) 
    if(replaceString.isNull()) {
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
        if(index != -1) {
            HistoryEntry *entry = &SearchReplaceHistory[index];

            entry->search = searchString;
            entry->type   = searchType;
        }
		return;
	}

	currentItemIsIncremental = isIncremental;

    if (NHist == 0) {
        for(MainWindow *window : MainWindow::allWindows()) {
            window->ui.action_Find_Again->setEnabled(true);
            window->ui.action_Replace_Find_Again->setEnabled(true);
            window->ui.action_Replace_Again->setEnabled(true);
		}
	}

    /* If there are more than MAX_SEARCH_HISTORY strings saved, recycle
       some space, free the entry that's about to be overwritten */
    if (NHist != MAX_SEARCH_HISTORY) {
        NHist++;
    }

    HistoryEntry *entry = &SearchReplaceHistory[HistStart];
    Q_ASSERT(entry);

    entry->search  = searchString;
    entry->replace = replaceString;
    entry->type    = searchType;

    ++HistStart;

    if (HistStart >= MAX_SEARCH_HISTORY) {
        HistStart = 0;
    }
}

/*
** Checks whether a search mode in one of the regular expression modes.
*/
bool Search::isRegexType(SearchType searchType) {
    return searchType == SearchType::Regex || searchType == SearchType::RegexNoCase;
}

/*
** Returns the default flags for regular expression matching, given a
** regular expression search mode.
*/
int Search::defaultRegexFlags(SearchType searchType) {
	switch (searchType) {
    case SearchType::Regex:
		return REDFLT_STANDARD;
    case SearchType::RegexNoCase:
		return REDFLT_CASE_INSENSITIVE;
	default:
		// We should never get here, but just in case ... 
		return REDFLT_STANDARD;
	}
}

/*
** return an index into the circular buffer arrays of history information
** for search strings, given the number of saveSearchHistory cycles back from
** the current time.
*/
int Search::historyIndex(int nCycles) {

    if (nCycles > NHist || nCycles <= 0) {
        return -1;
    }

    int index = HistStart - nCycles;
    if (index < 0) {
        index = MAX_SEARCH_HISTORY + index;
    }

    return index;
}

/**
 * @brief Search::HistoryByIndex
 * @param index
 */
auto Search::HistoryByIndex(int index) -> HistoryEntry * {

    if (NHist < 1) {
        return nullptr;
    }

    const int n = historyIndex(index);
    if (n == -1) {
        return nullptr;
    }

    return &SearchReplaceHistory[n];
}


