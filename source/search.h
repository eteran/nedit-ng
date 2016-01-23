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

#ifndef NEDIT_SEARCH_H_INCLUDED
#define NEDIT_SEARCH_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>
#include <X11/X.h>

enum SearchDirection { SEARCH_FORWARD, SEARCH_BACKWARD };

Boolean WindowCanBeClosed(WindowInfo *window);
bool ReplaceAll(WindowInfo *window, const char *searchString, const char *replaceString, int searchType);
bool ReplaceAndSearch(WindowInfo *window, int direction, const char *searchString, const char *replaceString, int searchType, int searchWrap);
bool ReplaceFindSame(WindowInfo *window, int direction, int searchWrap);
bool ReplaceSame(WindowInfo *window, int direction, int searchWrap);
bool SearchAndReplace(WindowInfo *window, int direction, const char *searchString, const char *replaceString, int searchType, int searchWrap);
bool SearchAndSelectIncremental(WindowInfo *window, int direction, const char *searchString, int searchType, int searchWrap, int continued);
bool SearchAndSelectSame(WindowInfo *window, int direction, int searchWrap);
bool SearchAndSelect(WindowInfo *window, int direction, const char *searchString, int searchType, int searchWrap);
bool SearchString(const char *string, const char *searchString, int direction, int searchType, int wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters);
bool SearchWindow(WindowInfo *window, int direction, const char *searchString, int searchType, int searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW);
char *ReplaceAllInString(const char *inString, const char *searchString, const char *replaceString, int searchType, int *copyStart, int *copyEnd, int *replacementLength, const char *delimiters);
void BeginISearch(WindowInfo *window, int direction);
void CreateFindDlog(Widget parent, WindowInfo *window);
void CreateReplaceDlog(Widget parent, WindowInfo *window);
void CreateReplaceMultiFileDlog(WindowInfo *window);
void DoFindDlog(WindowInfo *window, int direction, int keepDialogs, int searchType, Time time);
void DoFindReplaceDlog(WindowInfo *window, int direction, int keepDialogs, int searchType, Time time);
void DoReplaceMultiFileDlog(WindowInfo *window);
void EndISearch(WindowInfo *window);
void FlashMatching(WindowInfo *window, Widget textW);
void GotoMatchingCharacter(WindowInfo *window);
void RemoveFromMultiReplaceDialog(WindowInfo *window);
void ReplaceInSelection(const WindowInfo *window, const char *searchString, const char *replaceString, const int searchType);
void SearchForSelected(WindowInfo *window, int direction, int searchWrap, int searchType, Time time);
void SelectToMatchingCharacter(WindowInfo *window);
void SetISearchTextCallbacks(WindowInfo *window);
void UpdateReplaceActionButtons(WindowInfo *window);

/*
** Schwarzenberg: added SEARCH_LITERAL_WORD .. SEARCH_REGEX_NOCASE
**
** The order of the integers in this enumeration must be exactly
** the same as the order of the coressponding strings of the
** array  SearchMethodStrings defined in preferences.c (!!)
**
*/
enum SearchType {
	SEARCH_LITERAL,
	SEARCH_CASE_SENSE,
	SEARCH_REGEX,
	SEARCH_LITERAL_WORD,
	SEARCH_CASE_SENSE_WORD,
	SEARCH_REGEX_NOCASE,
	N_SEARCH_TYPES /* must be last in enum SearchType */
};

#ifdef REPLACE_SCOPE
/* Scope on which the replace operations apply */
enum ReplaceScope { REPL_SCOPE_WIN, REPL_SCOPE_SEL, REPL_SCOPE_MULTI };

/* Default scope if selection exists when replace dialog pops up.
   "Smart" means "In Selection" if the selection spans more than
   one line; "In Window" otherwise. */
enum ReplaceAllDefaultScope { REPL_DEF_SCOPE_WINDOW, REPL_DEF_SCOPE_SELECTION, REPL_DEF_SCOPE_SMART };
#endif

/*
** Returns a pointer to the string describing the search type for search
** action routine parameters (see menu.c for processing of action routines)
** If searchType is invalid defaultRV is returned.
*/
const char *SearchTypeArg(int searchType, const char *defaultRV);

/*
** Parses a search type description string. If the string contains a valid
** search type description, returns TRUE and writes the corresponding
** SearchType in searchType. Returns FALSE and leaves searchType untouched
** otherwise.
*/
int StringToSearchType(const char *string, int *searchType);

/*
** History of search actions.
*/
extern int NHist;

#endif /* NEDIT_SEARCH_H_INCLUDED */
