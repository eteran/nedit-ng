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

constexpr const int MAX_SEARCH_HISTORY = 100; /* Maximum length of search string history */

#include "SearchDirection.h"
#include "SearchType.h"
#include "util/string_view.h"

#include <X11/Intrinsic.h>
#include <X11/X.h>

class TextArea;
class DocumentWidget;
class MainWindow;
class Document;

Boolean WindowCanBeClosed(Document *window);

bool ReplaceAllEx(MainWindow *window, DocumentWidget *document, TextArea *area, const char *searchString, const char *replaceString, SearchType searchType);
bool ReplaceAndSearchEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, const char *searchString, const char *replaceString, SearchType searchType, int searchWrap);
bool ReplaceFindSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, int searchWrap);
bool ReplaceSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, int searchWrap);
bool SearchAndReplaceEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, const char *searchString, const char *replaceString, SearchType searchType, bool searchWrap);
bool SearchAndSelectIncrementalEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, const char *searchString, SearchType searchType, bool searchWrap, bool continued);
bool SearchAndSelectSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, bool searchWrap);
bool SearchAndSelectEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, const char *searchString, SearchType searchType, int searchWrap);
bool SearchString(view::string_view string, const char *searchString, SearchDirection direction, SearchType searchType, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters);
bool SearchWindowEx(MainWindow *window, DocumentWidget *document, SearchDirection direction, const char *searchString, SearchType searchType, int searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW);
char *ReplaceAllInString(view::string_view inString, const char *searchString, const char *replaceString, SearchType searchType, int *copyStart, int *copyEnd, int *replacementLength, const char *delimiters);
void BeginISearchEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction);
void DoFindDlogEx(MainWindow *window, DocumentWidget *document, SearchDirection direction, int keepDialogs, SearchType searchType);
void DoFindReplaceDlogEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, int keepDialogs, SearchType searchType);
void EndISearch(Document *window);
void FlashMatching(Document *window, Widget textW);
void FlashMatchingEx(DocumentWidget *document, TextArea *area);
void GotoMatchingCharacter(Document *window);
void RemoveFromMultiReplaceDialog(Document *window);
void ReplaceInSelection(const Document *window, const char *searchString, const char *replaceString, SearchType searchType);
void ReplaceInSelectionEx(MainWindow *window, DocumentWidget *document, TextArea *area, const char *searchString, const char *replaceString, SearchType searchType);
void SearchForSelectedEx(MainWindow *window, DocumentWidget *document, TextArea *area, SearchDirection direction, SearchType searchType, int searchWrap);
void SelectToMatchingCharacter(Document *window);
void SetISearchTextCallbacks(Document *window);
void eraseFlashEx(DocumentWidget *document);


/* Default scope if selection exists when replace dialog pops up.
   "Smart" means "In Selection" if the selection spans more than
   one line; "In Window" otherwise. */
enum ReplaceAllDefaultScope { REPL_DEF_SCOPE_WINDOW, REPL_DEF_SCOPE_SELECTION, REPL_DEF_SCOPE_SMART };

/*
** Returns a pointer to the string describing the search type for search
** action routine parameters (see menu.c for processing of action routines)
** If searchType is invalid defaultRV is returned.
*/
const char *SearchTypeArg(SearchType searchType, const char *defaultRV);

/*
** Parses a search type description string. If the string contains a valid
** search type description, returns TRUE and writes the corresponding
** SearchType in searchType. Returns FALSE and leaves searchType untouched
** otherwise.
*/
int StringToSearchType(const char *string, SearchType*searchType);

/*
** History of search actions.
*/
extern int NHist;

struct SelectionInfo {
	bool done;
	Document *window;
	char *selection;
};

// TODO(eteran): temporarily exposing these publically
const char *directionArg(SearchDirection direction);
const char *searchTypeArg(SearchType searchType);
const char *searchWrapArg(int searchWrap);
int countWritableWindows(void);
int historyIndex(int nCycles);
int isRegexType(SearchType searchType);
void unmanageReplaceDialogs(const Document *window);
void saveSearchHistory(const char *searchString, const char *replaceString, SearchType searchType, bool isIncremental);
int defaultRegexFlags(SearchType searchType);

extern char *SearchHistory[MAX_SEARCH_HISTORY];
extern char *ReplaceHistory[MAX_SEARCH_HISTORY];
extern SearchType SearchTypeHistory[MAX_SEARCH_HISTORY];
extern Document *windowNotToClose;

#endif
