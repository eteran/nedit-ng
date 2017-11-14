/*******************************************************************************
*                                                                              *
* search.h -- Nirvana Editor Search Header File                                *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef SEARCH_H_
#define SEARCH_H_

#include "Direction.h"
#include "SearchType.h"
#include "WrapMode.h"
#include "util/string_view.h"
#include "regex/regularExp.h"
#include <QString>
#include <memory>

class DocumentWidget;
class MainWindow;
class TextArea;

// Maximum length of search string history
constexpr int MAX_SEARCH_HISTORY = 100;

// Maximum length of search/replace strings
constexpr int SEARCHMAX = 5119;

bool ReplaceAllEx(MainWindow *window, DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType);
bool ReplaceAndSearchEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWrap);
bool ReplaceFindSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
bool ReplaceSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
bool SearchAndReplaceEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWrap);
bool SearchAndSelectIncrementalEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap, bool continued);
bool SearchAndSelectSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
bool SearchAndSelectEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap);
bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const QString &delimiters);
bool SearchWindowEx(MainWindow *window, DocumentWidget *document, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW);
std::string ReplaceAllInStringEx(view::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int *copyStart, int *copyEnd, const QString &delimiters, bool *ok);
void DoFindDlogEx(MainWindow *window, DocumentWidget *document, Direction direction, bool keepDialogs, SearchType searchType);
void DoFindReplaceDlogEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, bool keepDialogs, SearchType searchType);
void ReplaceInSelectionEx(MainWindow *window, DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType);
void SearchForSelectedEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, SearchType searchType, WrapMode searchWrap);

/* Default scope if selection exists when replace dialog pops up.
   "Smart" means "In Selection" if the selection spans more than
   one line; "In Window" otherwise. */
enum ReplaceAllDefaultScope {
    REPL_DEF_SCOPE_WINDOW,
    REPL_DEF_SCOPE_SELECTION,
    REPL_DEF_SCOPE_SMART
};

// NOTE(eteran): temporarily exposing these publically
int countWritableWindows();
int historyIndex(int nCycles);
bool isRegexType(SearchType searchType);
void saveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental);
int defaultRegexFlags(SearchType searchType);

std::unique_ptr<regexp> make_regex(const QString &re, int flags);

struct SearchReplaceHistoryEntry {
    QString    search;
    QString    replace;
    SearchType type;
};

extern int NHist;
extern SearchReplaceHistoryEntry SearchReplaceHistory[MAX_SEARCH_HISTORY];

#endif
