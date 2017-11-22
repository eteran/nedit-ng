/*******************************************************************************
*                                                                              *
* search.c -- Nirvana Editor search and replace functions                      *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* May 10, 1991                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "search.h"
#include "utils.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "TruncSubstitution.h"
#include "TextBuffer.h"
#include "WrapStyle.h"
#include "highlight.h"
#include "preferences.h"
#include "regularExp.h"
#include "selection.h"
#include "userCmds.h"
#include <gsl/gsl_util>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

int NHist = 0;


// History mechanism for search and replace strings 
SearchReplaceHistoryEntry SearchReplaceHistory[MAX_SEARCH_HISTORY];
static int HistStart = 0;

static bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters);
static bool backwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool forwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool searchRegex(view::string_view string, view::string_view searchString, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW);
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, const char *delimiters);
static std::string upCaseStringEx(view::string_view inString);
static std::string downCaseStringEx(view::string_view inString);

/*
** Count no. of writable windows, but first update the status of all files.
*/
int countWritableWindows() {

    int nWritable = 0;

    std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
    int nBefore = documents.size();

    auto first = documents.begin();
    auto last = documents.end();

    for(auto it = first; it != last; ++it) {
        DocumentWidget *document = *it;

		/* We must be very careful! The status check may trigger a pop-up
		   dialog when the file has changed on disk, and the user may destroy
		   arbitrary windows in response. */
        document->CheckForChangesToFileEx();

        std::vector<DocumentWidget *> afterDocuments = DocumentWidget::allDocuments();
        int nAfter = afterDocuments.size();

		if (nAfter != nBefore) {
			// The user has destroyed a file; start counting all over again 
			nBefore = nAfter;

            documents = afterDocuments;
            first     = documents.begin();
            last      = documents.end();
            it        = first;
			nWritable = 0;
			continue;
		}
		
        if (!document->lockReasons_.isAnyLocked()) {
			++nWritable;
		}
	}

	return nWritable;
}

/*
** Replace all occurences of "searchString" in "inString" with "replaceString"
** and return an allocated string covering the range between the start of the
** first replacement (returned in "copyStart", and the end of the last
** replacement (returned in "copyEnd")
*/
std::string ReplaceAllInStringEx(view::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int *copyStart, int *copyEnd, const QString &delimiters, bool *ok) {
    int startPos;
    int endPos;
    int lastEndPos;
    int copyLen;
    int searchExtentBW;
    int searchExtentFW;

    // reject empty string
    if (searchString.isNull()) {
        *ok = false;
        return std::string();
    }

    /* rehearse the search first to determine the size of the buffer needed
       to hold the substituted text.  No substitution done here yet */
    int replaceLen = replaceString.size();
    bool found     = true;
    int nFound     = 0;
    int removeLen  = 0;
    int addLen     = 0;
    int beginPos   = 0;

    *copyStart = -1;

    while (found) {
        found = SearchString(
                    inString,
                    searchString,
                    Direction::FORWARD,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    &startPos,
                    &endPos,
                    &searchExtentBW,
                    &searchExtentFW,
                    delimiters);

        if (found) {
            if (*copyStart < 0) {
                *copyStart = startPos;
            }

            *copyEnd = endPos;
            // start next after match unless match was empty, then endPos+1
            beginPos = (startPos == endPos) ? endPos + 1 : endPos;
            nFound++;
            removeLen += endPos - startPos;
            if (isRegexType(searchType)) {
                std::string replaceResult;

                replaceUsingREEx(
                    searchString,
                    replaceString,
                    &inString[searchExtentBW],
                    startPos - searchExtentBW,
                    replaceResult,
                    startPos == 0 ? '\0' : inString[startPos - 1],
                    delimiters,
                    defaultRegexFlags(searchType));

                addLen += replaceResult.size();
            } else {
                addLen += replaceLen;
            }

            if (endPos == gsl::narrow<int>(inString.size())) {
                break;
            }
        }
    }

    if (nFound == 0) {
        *ok = false;
        return std::string();
    }

    /* Allocate a new buffer to hold all of the new text between the first
       and last substitutions */
    copyLen = *copyEnd - *copyStart;


    std::string outString;
    outString.reserve(copyLen - removeLen + addLen);

    /* Scan through the text buffer again, substituting the replace string
       and copying the part between replaced text to the new buffer  */
    found = true;
    beginPos = 0;
    lastEndPos = 0;

    while (found) {
        found = SearchString(inString, searchString, Direction::FORWARD, searchType, WrapMode::NoWrap, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW, delimiters);
        if (found) {
            if (beginPos != 0) {

                outString.append(&inString[lastEndPos], &inString[lastEndPos + startPos - lastEndPos]);
            }

            if (isRegexType(searchType)) {
                std::string replaceResult;

                replaceUsingREEx(
                    searchString,
                    replaceString,
                    &inString[searchExtentBW],
                    startPos - searchExtentBW,
                    replaceResult,
                    startPos == 0 ? '\0' : inString[startPos - 1],
                    delimiters,
                    defaultRegexFlags(searchType));

                outString.append(replaceResult);
            } else {
                outString.append(replaceString.toStdString());
            }

            lastEndPos = endPos;

            // start next after match unless match was empty, then endPos+1
            beginPos = (startPos == endPos) ? endPos + 1 : endPos;
            if (endPos == gsl::narrow<int>(inString.size())) {
                break;
            }
        }
    }

    *ok = true;
    return outString;
}

/*
** Search the null terminated string "string" for "searchString", beginning at
** "beginPos".  Returns the boundaries of the match in "startPos" and "endPos".
** searchExtentBW and searchExtentFW return the backwardmost and forwardmost
** positions used to make the match, which are usually startPos and endPos,
** but may extend further if positive lookahead or lookbehind was used in
** a regular expression match.  "delimiters" may be used to provide an
** alternative set of word delimiters for regular expression "<" and ">"
** characters, or simply passed as null for the default delimiter set.
*/
bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const QString &delimiters) {

    return SearchString(
                string,
                searchString,
                direction,
                searchType,
                wrap,
                beginPos,
                startPos,
                endPos,
                searchExtentBW,
                searchExtentFW,
                delimiters.isNull() ? nullptr : delimiters.toLatin1().data());

}

static bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters) {
	switch (searchType) {
    case SearchType::CaseSenseWord:
        return searchLiteralWord(string, searchString.toLatin1().data(), true, direction, wrap, beginPos, startPos, endPos, delimiters);
    case SearchType::LiteralWord:
        return searchLiteralWord(string, searchString.toLatin1().data(), false, direction, wrap, beginPos, startPos, endPos, delimiters);
    case SearchType::CaseSense:
        return searchLiteral(string, searchString.toLatin1().data(), true, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW);
    case SearchType::Literal:
        return searchLiteral(string, searchString.toLatin1().data(), false, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW);
    case SearchType::Regex:
        return searchRegex(string, searchString.toLatin1().data(), direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, REDFLT_STANDARD);
    case SearchType::RegexNoCase:
        return searchRegex(string, searchString.toLatin1().data(), direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, REDFLT_CASE_INSENSITIVE);
	}

    Q_UNREACHABLE();
}

/*
**  Searches for whole words (Markus Schwarzenberg).
**
**  If the first/last character of `searchString' is a "normal
**  word character" (not contained in `delimiters', not a whitespace)
**  then limit search to strings, who's next left/next right character
**  is contained in `delimiters' or is a whitespace or text begin or end.
**
**  If the first/last character of `searchString' itself is contained
**  in delimiters or is a white space, then the neighbour character of the
**  first/last character will not be checked, just a simple match
**  will suffice in that case.
**
*/
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, const char *delimiters) {

	std::string lcString;
	std::string ucString;
    bool cignore_L = false;
    bool cignore_R = false;

    auto do_search_word2 = [&](const char *filePtr) {
		if (*filePtr == ucString[0] || *filePtr == lcString[0]) {
			// matched first character 
			auto ucPtr = ucString.begin();
			auto lcPtr = lcString.begin();
			const char *tempPtr = filePtr;

			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;

                if (ucPtr == ucString.end() &&                                                                   // matched whole string
                    (cignore_R || safe_ctype<isspace>(*tempPtr) || strchr(delimiters, *tempPtr)) &&    // next char right delimits word ?
                    (cignore_L || filePtr == &string[0] ||                                                       // border case
                     safe_ctype<isspace>(filePtr[-1]) || strchr(delimiters, filePtr[-1]))) {           // next char left delimits word ?

                    *startPos = gsl::narrow<int>(filePtr - &string[0]);
                    *endPos   = gsl::narrow<int>(tempPtr - &string[0]);
					return true;
				}
			}
		}
		
        return false;
	};


	// If there is no language mode, we use the default list of delimiters 
	QByteArray delimiterString = GetPrefDelimiters().toLatin1();
	if(!delimiters) {
		delimiters = delimiterString.data();
    }

    if (safe_ctype<isspace>(searchString[0]) || strchr(delimiters, searchString[0])) {
		cignore_L = true;
	}

    if (safe_ctype<isspace>(searchString[searchString.size() - 1]) || strchr(delimiters, searchString[searchString.size() - 1])) {
		cignore_R = true;
	}

	if (caseSense) {
		ucString = searchString.to_string();
		lcString = searchString.to_string();
	} else {
		ucString = upCaseStringEx(searchString);
		lcString = downCaseStringEx(searchString);
	}

    if (direction == Direction::FORWARD) {
		// search from beginPos to end of string 
		for (auto filePtr = string.begin() + beginPos; filePtr != string.end(); filePtr++) {
            if(do_search_word2(filePtr)) {
				return true;
			}
		}
        if (wrap == WrapMode::NoWrap)
            return false;

		// search from start of file to beginPos 
		for (auto filePtr = string.begin(); filePtr <= string.begin() + beginPos; filePtr++) {
            if(do_search_word2(filePtr)) {
				return true;
			}
		}
        return false;
	} else {
        // Direction::BACKWARD
		// search from beginPos to start of file. A negative begin pos 
		// says begin searching from the far end of the file 
		if (beginPos >= 0) {
			for (auto filePtr = string.begin() + beginPos; filePtr >= string.begin(); filePtr--) {
                if(do_search_word2(filePtr)) {
					return true;
				}
			}
		}
        if (wrap == WrapMode::NoWrap)
            return false;

		// search from end of file to beginPos 
		for (auto filePtr = string.begin() + string.size(); filePtr >= string.begin() + beginPos; filePtr--) {
            if(do_search_word2(filePtr)) {
				return true;
			}
		}
        return false;
	}
}

static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW) {


	std::string lcString;
	std::string ucString;

    if (caseSense) {
        lcString = searchString.to_string();
        ucString = searchString.to_string();
    } else {
        ucString = upCaseStringEx(searchString);
        lcString = downCaseStringEx(searchString);
    }

    auto do_search2 = [&](const char *filePtr) {
		if (*filePtr == ucString[0] || *filePtr == lcString[0]) {
			// matched first character 
			auto ucPtr   = ucString.begin();
			auto lcPtr   = lcString.begin();
			const char *tempPtr = filePtr;

			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;
				if (ucPtr == ucString.end()) {
					// matched whole string 
                    *startPos = gsl::narrow<int>(filePtr - &string[0]);
                    *endPos   = gsl::narrow<int>(tempPtr - &string[0]);
					if(searchExtentBW) {
						*searchExtentBW = *startPos;
					}

					if(searchExtentFW) {
						*searchExtentFW = *endPos;
					}
					return true;
				}
			}
		}

        return false;
	};



    if (direction == Direction::FORWARD) {

		auto first = string.begin();
		auto mid   = first + beginPos;
		auto last  = string.end();

		// search from beginPos to end of string 
		for (auto filePtr = mid; filePtr != last; ++filePtr) {
            if(do_search2(filePtr)) {
				return true;
			}
		}

        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from start of file to beginPos 
        // NOTE(eteran): this used to include "mid", but that seems redundant given that we already looked there
		//               in the first loop
		for (auto filePtr = first; filePtr != mid; ++filePtr) {
            if(do_search2(filePtr)) {
				return true;
			}
		}

        return false;
	} else {
        // Direction::BACKWARD
		// search from beginPos to start of file.  A negative begin pos	
		// says begin searching from the far end of the file 

		auto first = string.begin();
		auto mid   = first + beginPos;
		auto last  = string.end();

		if (beginPos >= 0) {
			for (auto filePtr = mid; filePtr >= first; --filePtr) {
                if(do_search2(filePtr)) {
					return true;
				}
			}
		}

        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from end of file to beginPos 
		// how to get the text string length from the text widget (under 1.1)
		for (auto filePtr = last; filePtr >= mid; --filePtr) {
            if(do_search2(filePtr)) {
				return true;
			}
		}

        return false;
	}
}

static bool searchRegex(view::string_view string, view::string_view searchString, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {
    if (direction == Direction::FORWARD)
		return forwardRegexSearch(string, searchString, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, defaultFlags);
	else
		return backwardRegexSearch(string, searchString, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, defaultFlags);
}

static bool forwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {

	try {
        regexp compiledRE(searchString, defaultFlags);

		// search from beginPos to end of string 
		if (compiledRE.execute(string, beginPos, delimiters, false)) {

            *startPos = gsl::narrow<int>(compiledRE.startp[0] - &string[0]);
            *endPos   = gsl::narrow<int>(compiledRE.endp[0]   - &string[0]);

			if(searchExtentFW) {
                *searchExtentFW = gsl::narrow<int>(compiledRE.extentpFW - &string[0]);
			}

			if(searchExtentBW) {
                *searchExtentBW = gsl::narrow<int>(compiledRE.extentpBW - &string[0]);
			}

			return true;
		}

		// if wrap turned off, we're done 
        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from the beginning of the string to beginPos 
		if (compiledRE.execute(string, 0, beginPos, delimiters, false)) {

            *startPos = gsl::narrow<int>(compiledRE.startp[0] - &string[0]);
            *endPos   = gsl::narrow<int>(compiledRE.endp[0]   - &string[0]);

			if(searchExtentFW) {
                *searchExtentFW = gsl::narrow<int>(compiledRE.extentpFW - &string[0]);
			}

			if(searchExtentBW) {
                *searchExtentBW = gsl::narrow<int>(compiledRE.extentpBW - &string[0]);
			}
			return true;
		}

        return false;
	} catch(const regex_error &e) {
        Q_UNUSED(e);
		/* Note that this does not process errors from compiling the expression.
		 * It assumes that the expression was checked earlier.
		 */
        return false;
	}
}

static bool backwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {

	try {
		regexp compiledRE(searchString, defaultFlags);

		// search from beginPos to start of file.  A negative begin pos	
		// says begin searching from the far end of the file.		
		if (beginPos >= 0) {

            // NOTE(eteran): why do we use NUL as the previous char, and not string[beginPos - 1] (assuming that beginPos > 0)?
			if (compiledRE.execute(string, 0, beginPos, '\0', '\0', delimiters, true)) {

                *startPos = gsl::narrow<int>(compiledRE.startp[0] - &string[0]);
                *endPos   = gsl::narrow<int>(compiledRE.endp[0]   - &string[0]);

				if(searchExtentFW) {
                    *searchExtentFW = gsl::narrow<int>(compiledRE.extentpFW - &string[0]);
				}

				if(searchExtentBW) {
                    *searchExtentBW = gsl::narrow<int>(compiledRE.extentpBW - &string[0]);
				}

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

		if (compiledRE.execute(string, beginPos, delimiters, true)) {

            *startPos = gsl::narrow<int>(compiledRE.startp[0] - &string[0]);
            *endPos   = gsl::narrow<int>(compiledRE.endp[0]   - &string[0]);

			if(searchExtentFW) {
                *searchExtentFW = gsl::narrow<int>(compiledRE.extentpFW - &string[0]);
			}

			if(searchExtentBW) {
                *searchExtentBW = gsl::narrow<int>(compiledRE.extentpBW - &string[0]);
			}

			return true;
		}

        return false;
	} catch(const regex_error &e) {
        Q_UNUSED(e);
        return false;
	}
}

static std::string upCaseStringEx(view::string_view inString) {

	std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
        return safe_ctype<toupper>(ch);
	});
	return str;
}

static std::string downCaseStringEx(view::string_view inString) {

    std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
        return safe_ctype<tolower>(ch);
	});
	return str;
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
bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const char *delimiters, int defaultFlags) {
    try {
        regexp compiledRE(searchStr, defaultFlags);
        compiledRE.execute(sourceStr, beginPos, sourceStr.size(), prevChar, '\0', delimiters, false);
        return compiledRE.SubstituteRE(replaceStr, dest);
    } catch(const regex_error &e) {
        Q_UNUSED(e);
        return false;
    }
}


bool replaceUsingREEx(const QString &searchStr, const QString &replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags) {
    return replaceUsingREEx(
                searchStr.toLatin1().data(),
                replaceStr.toLatin1().data(),
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
** again with an empty search string and isIncremental==False.
*/
void saveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental) {

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

	// If replaceString is nullptr, duplicate the last one (if any) 
    if(replaceString.isNull()) {
        replaceString = (NHist >= 1) ? SearchReplaceHistory[historyIndex(1)].replace : QLatin1String("");
    }

	/* Compare the current search and replace strings against the saved ones.
	   If they are identical, don't bother saving */
    if (NHist >= 1 && searchType == SearchReplaceHistory[historyIndex(1)].type && SearchReplaceHistory[historyIndex(1)].search == searchString && SearchReplaceHistory[historyIndex(1)].replace == replaceString) {
		return;
	}

	/* If the current history item came from an incremental search, and the
	   new one is also incremental, just update the entry */
	if (currentItemIsIncremental && isIncremental) {

        SearchReplaceHistory[historyIndex(1)].search = searchString;
        SearchReplaceHistory[historyIndex(1)].type   = searchType;
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

    SearchReplaceHistory[HistStart].search  = searchString;
    SearchReplaceHistory[HistStart].replace = replaceString;
    SearchReplaceHistory[HistStart].type    = searchType;

    HistStart++;

    if (HistStart >= MAX_SEARCH_HISTORY) {
        HistStart = 0;
    }
}

/*
** return an index into the circular buffer arrays of history information
** for search strings, given the number of saveSearchHistory cycles back from
** the current time.
*/
int historyIndex(int nCycles) {

    if (nCycles > NHist || nCycles <= 0) {
		return -1;
    }

    int index = HistStart - nCycles;
    if (index < 0) {
        index = MAX_SEARCH_HISTORY + index;
    }

    return index;
}


/*
** Checks whether a search mode in one of the regular expression modes.
*/
bool isRegexType(SearchType searchType) {
    return searchType == SearchType::Regex || searchType == SearchType::RegexNoCase;
}

/*
** Returns the default flags for regular expression matching, given a
** regular expression search mode.
*/
int defaultRegexFlags(SearchType searchType) {
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

/**
 * @brief make_regex
 * @param re
 * @param flags
 * @return
 */
std::unique_ptr<regexp> make_regex(const QString &re, int flags) {
    return std::make_unique<regexp>(re.toStdString(), flags);
}

