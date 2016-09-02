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

#include <QApplication>
#include <QMessageBox>
#include "ui/DialogFind.h"
#include "ui/DialogReplace.h"
#include "ui/DialogMultiReplace.h"
#include "util/memory.h"
#include "WrapStyle.h"
#include "TextDisplay.h"
#include "TextHelper.h"
#include "search.h"
#include "regularExp.h"
#include "TextBuffer.h"
#include "text.h"
#include "nedit.h"
#include "server.h"
#include "Document.h"
#include "window.h"
#include "userCmds.h"
#include "preferences.h"
#include "file.h"
#include "highlight.h"
#include "selection.h"
#include "util/MotifHelper.h"

#ifdef REPLACE_SCOPE
#include "TextDisplay.h"
#endif

#include "util/misc.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <sys/param.h>

#include <Xm/ToggleB.h>

int NHist = 0;

struct SearchSelectedCallData {
	SearchDirection direction;
	SearchType searchType;
	int searchWrap;
};

// History mechanism for search and replace strings 
char *SearchHistory[MAX_SEARCH_HISTORY];
char *ReplaceHistory[MAX_SEARCH_HISTORY];
SearchType SearchTypeHistory[MAX_SEARCH_HISTORY];
static int HistStart = 0;

static bool backwardRegexSearch(view::string_view string, view::string_view searchString, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool prefOrUserCancelsSubst(const Widget parent, const Display *display);
static bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, char *destStr, int maxDestLen, int prevChar, const char *delimiters, int defaultFlags);

static bool forwardRegexSearch(view::string_view string, view::string_view searchString, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool searchRegex(view::string_view string, view::string_view searchString, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static int countWindows(void);
static int defaultRegexFlags(SearchType searchType);
static int findMatchingChar(Document *window, char toMatch, void *toMatchStyle, int charPos, int startLimit, int endLimit, int *matchPos);
static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW);
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, const char *delimiters);
static bool searchMatchesSelection(Document *window, const char *searchString, SearchType searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW);
static void checkMultiReplaceListForDoomedW(Document *window, Document *doomedWindow);
static void eraseFlash(Document *window);
static void flashTimeoutProc(XtPointer clientData, XtIntervalId *id);
static void iSearchCaseToggleCB(Widget w, XtPointer clientData, XtPointer callData);
static void iSearchRecordLastBeginPos(Document *window, SearchDirection direction, int initPos);
static void iSearchRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData);
static void iSearchTextActivateCB(Widget w, XtPointer clientData, XtPointer call_data);
static void iSearchTextClearAndPasteAP(Widget w, XEvent *event, String *args, Cardinal *nArg);
static void iSearchTextClearCB(Widget w, XtPointer clientData, XtPointer call_data);
static void iSearchTextKeyEH(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch);
static void iSearchTextValueChangedCB(Widget w, XtPointer clientData, XtPointer call_data);
static void iSearchTryBeepOnWrap(Document *window, SearchDirection direction, int beginPos, int startPos);
static void removeDoomedWindowFromList(Document *window, int index);
static void selectedSearchCB(Widget w, XtPointer callData, Atom *selection, Atom *type, char *value, int *length, int *format);
static std::string upCaseStringEx(view::string_view inString);
static std::string downCaseStringEx(view::string_view inString);


struct charMatchTable {
	char c;
	char match;
	char direction;
};

#define N_MATCH_CHARS 13
#define N_FLASH_CHARS 6
static charMatchTable MatchingChars[N_MATCH_CHARS] = {
    {'{', '}', SEARCH_FORWARD},
    {'}', '{', SEARCH_BACKWARD},
    {'(', ')', SEARCH_FORWARD},
    {')', '(', SEARCH_BACKWARD},
    {'[', ']', SEARCH_FORWARD},
    {']', '[', SEARCH_BACKWARD},
    {'<', '>', SEARCH_FORWARD},
    {'>', '<', SEARCH_BACKWARD},
    {'/', '/', SEARCH_FORWARD},
    {'"', '"', SEARCH_FORWARD},
    {'\'', '\'', SEARCH_FORWARD},
    {'`', '`', SEARCH_FORWARD},
    {'\\', '\\', SEARCH_FORWARD},
};

/*
** Definitions for the search method strings, used as arguments for
** macro search subroutines and search action routines
*/
static const char *searchTypeStrings[] = {"literal",     // SEARCH_LITERAL         
                                          "case",        // SEARCH_CASE_SENSE      
                                          "regex",       // SEARCH_REGEX           
                                          "word",        // SEARCH_LITERAL_WORD    
                                          "caseWord",    // SEARCH_CASE_SENSE_WORD 
                                          "regexNoCase", // SEARCH_REGEX_NOCASE    
                                          nullptr};

/*
** Window for which a search dialog callback is currently active. That window
** cannot be safely closed because the callback would access the armed button
** after it got destroyed.
** Note that an attempt to close such a window can only happen if the search
** action triggers a modal dialog and the user tries to close the window via
** the window manager without dismissing the dialog first. It is up to the
** close callback of the window to intercept and reject the request by calling
** the WindowCanBeClosed() function.
*/
Document *windowNotToClose = nullptr;

Boolean WindowCanBeClosed(Document *window) {
	if (windowNotToClose && Document::GetTopDocument(window->shell_) == Document::GetTopDocument(windowNotToClose->shell_)) {
		return False;
	}
	return True; // It's safe 
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
static void initToggleButtons(SearchType searchType, Widget regexToggle, Widget caseToggle, Widget *wordToggle, bool *lastLiteralCase, bool *lastRegexCase) {
	/* Set the initial search type and remember the corresponding case
	   sensitivity states in case sticky case sensitivity is required. */
	switch (searchType) {
	case SEARCH_LITERAL:
		*lastLiteralCase = False;
		*lastRegexCase = True;
		XmToggleButtonSetState(regexToggle, False, False);
		XmToggleButtonSetState(caseToggle, False, False);
		if (wordToggle) {
			XmToggleButtonSetState(*wordToggle, False, False);
			XtSetSensitive(*wordToggle, True);
		}
		break;
	case SEARCH_CASE_SENSE:
		*lastLiteralCase = True;
		*lastRegexCase = True;
		XmToggleButtonSetState(regexToggle, False, False);
		XmToggleButtonSetState(caseToggle, True, False);
		if (wordToggle) {
			XmToggleButtonSetState(*wordToggle, False, False);
			XtSetSensitive(*wordToggle, True);
		}
		break;
	case SEARCH_LITERAL_WORD:
		*lastLiteralCase = False;
		*lastRegexCase = True;
		XmToggleButtonSetState(regexToggle, False, False);
		XmToggleButtonSetState(caseToggle, False, False);
		if (wordToggle) {
			XmToggleButtonSetState(*wordToggle, True, False);
			XtSetSensitive(*wordToggle, True);
		}
		break;
	case SEARCH_CASE_SENSE_WORD:
		*lastLiteralCase = True;
		*lastRegexCase = True;
		XmToggleButtonSetState(regexToggle, False, False);
		XmToggleButtonSetState(caseToggle, True, False);
		if (wordToggle) {
			XmToggleButtonSetState(*wordToggle, True, False);
			XtSetSensitive(*wordToggle, True);
		}
		break;
	case SEARCH_REGEX:
		*lastLiteralCase = False;
		*lastRegexCase = True;
		XmToggleButtonSetState(regexToggle, True, False);
		XmToggleButtonSetState(caseToggle, True, False);
		if (wordToggle) {
			XmToggleButtonSetState(*wordToggle, False, False);
			XtSetSensitive(*wordToggle, False);
		}
		break;
	case SEARCH_REGEX_NOCASE:
		*lastLiteralCase = False;
		*lastRegexCase = False;
		XmToggleButtonSetState(regexToggle, True, False);
		XmToggleButtonSetState(caseToggle, False, False);
		if (wordToggle) {
			XmToggleButtonSetState(*wordToggle, False, False);
			XtSetSensitive(*wordToggle, False);
		}
		break;
	default:
		Q_ASSERT(0);
	}
}

#ifdef REPLACE_SCOPE
/*
** Checks whether a selection spans multiple lines. Used to decide on the
** default scope for replace dialogs.
** This routine introduces a dependency on TextDisplay.h, which is not so nice,
** but I currently don't have a cleaner solution.
*/
static bool selectionSpansMultipleLines(Document *window) {
	int selStart;
	int selEnd;
	int rectStart;
	int rectEnd;
	int lineStartStart;
	int lineStartEnd;
	bool isRect;
	int lineWidth;

	if (!window->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		return false;
	}

	/* This is kind of tricky. The perception of a line depends on the
	   line wrap mode being used. So in theory, we should take into
	   account the layout of the text on the screen. However, the
	   routine to calculate a line number for a given character position
	   (TextDPosToLineAndCol) only works for displayed lines, so we cannot
	   use it. Therefore, we use this simple heuristic:
	    - If a newline is found between the start and end of the selection,
	  we obviously have a multi-line selection.
	- If no newline is found, but the distance between the start and the
	      end of the selection is larger than the number of characters
	  displayed on a line, and we're in continuous wrap mode,
	  we also assume a multi-line selection.
	*/

	lineStartStart = window->buffer_->BufStartOfLine(selStart);
	lineStartEnd = window->buffer_->BufStartOfLine(selEnd);
	// If the line starts differ, we have a "\n" in between. 
	if (lineStartStart != lineStartEnd) {
		return true;
	}

	if (window->wrapMode_ != CONTINUOUS_WRAP) {
		return false; // Same line
	}

	// Estimate the number of characters on a line 
	TextDisplay *textD = textD_of(window->textArea_);
	if (textD->TextDGetFont()->max_bounds.width > 0) {
		lineWidth = textD->getRect().width / textD->TextDGetFont()->max_bounds.width;
	} else {
		lineWidth = 1;
	}

	if (lineWidth < 1) {
		lineWidth = 1; // Just in case 
	}

	/* Estimate the numbers of line breaks from the start of the line to
	   the start and ending positions of the selection and compare.*/
	if ((selStart - lineStartStart) / lineWidth != (selEnd - lineStartStart) / lineWidth) {
		return true; // Spans multiple lines
	}

	return false; // Small selection; probably doesn't span lines
}
#endif

void DoFindReplaceDlog(Document *window, SearchDirection direction, int keepDialogs, SearchType searchType, Time time) {

	// Create the dialog if it doesn't already exist 
	if (!window->dialogReplace_) {
		CreateReplaceDlog(window->shell_, window);
	}

	auto dialog = window->getDialogReplace();
	
	dialog->setTextField(window, time);
	
	// If the window is already up, just pop it to the top 
	if(dialog->isVisible()) {
		dialog->raise();
		dialog->activateWindow();
		return;
	}
	
	// Blank the Replace with field 
	dialog->ui.textReplace->setText(QString());
	
	// Set the initial search type 
	dialog->initToggleButtons(searchType);	
	
	// Set the initial direction based on the direction argument 
	dialog->ui.checkBackward->setChecked(direction == SEARCH_FORWARD ? false : true);	
	
	// Set the state of the Keep Dialog Up button 
	dialog->ui.checkKeep->setChecked(keepDialogs);	

#ifdef REPLACE_SCOPE
	/* Set the state of the scope radio buttons to "In Window".
	   Notify to make sure that callbacks are called.
	   NOTE: due to an apparent bug in OpenMotif, the radio buttons may
	   get stuck after resetting the scope to "In Window". Therefore we must
	   use RadioButtonChangeState(), which contains a workaround. */
	if (window->wasSelected_) {
		/* If a selection exists, the default scope depends on the preference
		       of the user. */
		switch (GetPrefReplaceDefScope()) {
		case REPL_DEF_SCOPE_SELECTION:
			/* The user prefers selection scope, no matter what the
			   size of the selection is. */
			dialog->ui.radioSelection->setChecked(true);
			break;
		case REPL_DEF_SCOPE_SMART:
			if (selectionSpansMultipleLines(window)) {
				/* If the selection spans multiple lines, the user most
				   likely wants to perform a replacement in the selection */
				dialog->ui.radioSelection->setChecked(true);
			} else {
				/* It's unlikely that the user wants a replacement in a
				   tiny selection only. */
				dialog->ui.radioWindow->setChecked(true);
			}
			break;
		default:
			// The user always wants window scope as default. 
			dialog->ui.radioWindow->setChecked(true);
			break;
		}
	} else {
		// No selection -> always choose "In Window" as default. 
		dialog->ui.radioWindow->setChecked(true);
	}
#endif

	dialog->UpdateReplaceActionButtons();

	// Start the search history mechanism at the current history item 
	window->rHistIndex_ = 0;

	// TODO(eteran): center it on the cursor if settings say so
	dialog->show();
}

void DoFindDlog(Document *window, SearchDirection direction, int keepDialogs, SearchType searchType, Time time) {

	if(!window->dialogFind_) {
		window->dialogFind_ = new DialogFind(window, nullptr /*parent*/);
	}
	
	auto dialog = qobject_cast<DialogFind *>(window->dialogFind_);
	
	dialog->setTextField(window, time);
	
	if(dialog->isVisible()) {
		dialog->raise();
		dialog->activateWindow();
		return;
	}
	
	// Set the initial search type 
	dialog->initToggleButtons(searchType);
	
	// Set the initial direction based on the direction argument 
	dialog->ui.checkBackward->setChecked(direction == SEARCH_FORWARD ? false : true);
	
	// Set the state of the Keep Dialog Up button 
	dialog->ui.checkKeep->setChecked(keepDialogs);
	
	// Set the state of the Find button 
	dialog->fUpdateActionButtons();

	// start the search history mechanism at the current history item 
	window->fHistIndex_ = 0;

	// TODO(eteran): center it on the cursor if settings say so
	dialog->show();
}

/*
** If a window is closed (possibly via the window manager) while it is on the
** multi-file replace dialog list of any other window (or even the same one),
** we must update those lists or we end up with dangling references.
** Normally, there can be only one of those dialogs at the same time
** (application modal), but Lesstif doesn't (always) honor application
** modalness, so there can be more than one dialog.
*/
void RemoveFromMultiReplaceDialog(Document *doomedWindow) {

	for(Document *w: WindowList) {
		if (w->writableWindows_) {
			// A multi-file replacement dialog is up for this window 
			checkMultiReplaceListForDoomedW(w, doomedWindow);
		}
	}
}

void CreateReplaceDlog(Widget parent, Document *window) {
	Q_UNUSED(parent);
	window->dialogReplace_ = new DialogReplace(window, nullptr /*parent*/);		
}

void CreateFindDlog(Widget parent, Document *window) {

	Q_UNUSED(parent);
	window->dialogFind_ = new DialogFind(window, nullptr /*parent*/);
}

/*
** Iterates through the list of writable windows of a window, and removes
** the doomed window if necessary.
*/
static void checkMultiReplaceListForDoomedW(Document *window, Document *doomedWindow) {

	/* If the window owning the list and the doomed window are one and the
	   same, we just close the multi-file replacement dialog. */
	if (window == doomedWindow) {
		if(auto dialog = window->getDialogReplace()) {
			dialog->dialogMultiReplace_->hide();
			return;
		}
	}
			
	// Check whether the doomed window is currently listed 
	for (int i = 0; i < window->nWritableWindows_; ++i) {
		Document *w = window->writableWindows_[i];
		if (w == doomedWindow) {
			removeDoomedWindowFromList(window, i);
			break;
		}
	}
}

/*
** Removes a window that is about to be closed from the list of files in
** which to replace. If the list becomes empty, the dialog is popped down.
*/
static void removeDoomedWindowFromList(Document *window, int index) {

	// If the list would become empty, we remove the dialog 
	if (window->nWritableWindows_ <= 1) {
		if(auto dialog = window->getDialogReplace()) {
			dialog->dialogMultiReplace_->hide();
			return;
		}
	}

	int entriesToMove = window->nWritableWindows_ - index - 1;
	memmove(&(window->writableWindows_[index]), &(window->writableWindows_[index + 1]), (size_t)(entriesToMove * sizeof(Document *)));
	window->nWritableWindows_ -= 1;

	if(auto dialogReplace = window->getDialogReplace()) {
		if(auto dialogReplaceMulti = dialogReplace->dialogMultiReplace_) {
			delete dialogReplaceMulti->ui.listFiles->item(index + 1);
		}		
	}
}

/*
** These callbacks fix a Motif 1.1 problem that the default button gets the
** keyboard focus when a dialog is created.  We want the first text field
** to get the focus, so we don't set the default button until the text field
** has the focus for sure.  I have tried many other ways and this is by far
** the least nasty.
*/


/*
** Count no. of windows
*/
static int countWindows() {
	return Document::WindowCount();
}

/*
** Count no. of writable windows, but first update the status of all files.
*/
int countWritableWindows() {
	int nAfter;

	int nBefore = countWindows();
	int nWritable = 0;
	
	for(auto it = WindowList.begin(); it != WindowList.end(); ++it) {
	
		Document *w = *it;
	
		/* We must be very careful! The status check may trigger a pop-up
		   dialog when the file has changed on disk, and the user may destroy
		   arbitrary windows in response. */
		CheckForChangesToFile(w);
		nAfter = countWindows();
		if (nAfter != nBefore) {
			// The user has destroyed a file; start counting all over again 
			nBefore = nAfter;
			it = WindowList.begin();
			nWritable = 0;
			continue;
		}
		
		if (!w->lockReasons_.isAnyLocked()) {
			++nWritable;
		}
	}

	return nWritable;
}

/*
** Unconditionally pops down the replace dialog and the
** replace-in-multiple-files dialog, if it exists.
*/
void unmanageReplaceDialogs(const Document *window) {
	/* If the replace dialog goes down, the multi-file replace dialog must
	   go down too */
	if (auto dialog = window->getDialogReplace()) {
		dialog->hide();
	}
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** which is assumed to be at least SEARCHMAX in length, return search type
** in "searchType", and return TRUE as the function value.  Otherwise,
** return FALSE.
*/
bool SearchAndSelectSame(Document *window, SearchDirection direction, int searchWrap) {
	if (NHist < 1) {
		QApplication::beep();
		return FALSE;
	}

	return SearchAndSelect(window, direction, SearchHistory[historyIndex(1)], SearchTypeHistory[historyIndex(1)], searchWrap);
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  Also
** adds the search string to the global search history.
*/
bool SearchAndSelect(Document *window, SearchDirection direction, const char *searchString, SearchType searchType, int searchWrap) {
	int startPos;
	int endPos;
	int beginPos;
	int cursorPos;
	int selStart;
	int selEnd;
	int movedFwd = 0;

	// Save a copy of searchString in the search history 
	saveSearchHistory(searchString, nullptr, searchType, FALSE);

	/* set the position to start the search so we don't find the same
	   string that was found on the last search	*/
	if (searchMatchesSelection(window, searchString, searchType, &selStart, &selEnd, nullptr, nullptr)) {
		// selection matches search string, start before or after sel.	
		if (direction == SEARCH_BACKWARD) {
			beginPos = selStart - 1;
		} else {
			beginPos = selStart + 1;
			movedFwd = 1;
		}
	} else {
		selStart = -1;
		selEnd = -1;
		// no selection, or no match, search relative cursor 
		
		auto textD = window->lastFocus();
		
		cursorPos = textD->TextGetCursorPos();
		if (direction == SEARCH_BACKWARD) {
			// use the insert position - 1 for backward searches 
			beginPos = cursorPos - 1;
		} else {
			// use the insert position for forward searches 
			beginPos = cursorPos;
		}
	}

	/* when the i-search bar is active and search is repeated there
	   (Return), the action "find" is called (not: "find_incremental").
	   "find" calls this function SearchAndSelect.
	   To keep track of the iSearchLastBeginPos correctly in the
	   repeated i-search case it is necessary to call the following
	   function here, otherwise there are no beeps on the repeated
	   incremental search wraps.  */
	iSearchRecordLastBeginPos(window, direction, beginPos);

	// do the search.  SearchWindow does appropriate dialogs and beeps 
	if (!SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr))
		return FALSE;

	/* if the search matched an empty string (possible with regular exps)
	   beginning at the start of the search, go to the next occurrence,
	   otherwise repeated finds will get "stuck" at zero-length matches */
	if (direction == SEARCH_FORWARD && beginPos == startPos && beginPos == endPos) {
		if (!movedFwd && !SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr))
			return FALSE;
	}

	// if matched text is already selected, just beep 
	if (selStart == startPos && selEnd == endPos) {
		QApplication::beep();
		return FALSE;
	}

	// select the text found string 
	window->buffer_->BufSelect(startPos, endPos);
	window->MakeSelectionVisible(window->lastFocus_);
	
	
	auto textD = window->lastFocus();
	textD->TextSetCursorPos(endPos);

	return TRUE;
}

void SearchForSelected(Document *window, SearchDirection direction, SearchType searchType, int searchWrap, Time time) {
	SearchSelectedCallData *callData = XtNew(SearchSelectedCallData);
	callData->direction = direction;
	callData->searchType = searchType;
	callData->searchWrap = searchWrap;
	XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)selectedSearchCB, callData, time);
}

static void selectedSearchCB(Widget w, XtPointer callData, Atom *selection, Atom *type, char *value, int *length, int *format) {

	(void)selection;

	Document *window = Document::WidgetToWindow(w);
	auto callDataItems = static_cast<SearchSelectedCallData *>(callData);
	SearchType searchType;
	char searchString[SEARCHMAX + 1];

	window = Document::WidgetToWindow(w);

	// skip if we can't get the selection data or it's too long 
	if (*type == XT_CONVERT_FAIL || value == nullptr) {
		if (GetPrefSearchDlogs())
			QMessageBox::warning(nullptr /*parent*/, QLatin1String("Wrong Selection"), QLatin1String("Selection not appropriate for searching"));
		else
			QApplication::beep();
		XtFree((char *)callData);
		return;
	}
	if (*length > SEARCHMAX) {
		if (GetPrefSearchDlogs())
			QMessageBox::warning(nullptr /*parent*/, QLatin1String("Selection too long"), QLatin1String("Selection too long"));
		else
			QApplication::beep();
		XtFree(value);
		XtFree((char *)callData);
		return;
	}
	if (*length == 0) {
		QApplication::beep();
		XtFree(value);
		XtFree((char *)callData);
		return;
	}
	// should be of type text??? 
	if (*format != 8) {
		fprintf(stderr, "NEdit: can't handle non 8-bit text\n");
		QApplication::beep();
		XtFree(value);
		XtFree((char *)callData);
		return;
	}
	// make the selection the current search string 
	strncpy(searchString, value, *length);
	searchString[*length] = '\0';
	XtFree(value);

	/* Use the passed method for searching, unless it is regex, since this
	   kind of search is by definition a literal search */
	searchType = callDataItems->searchType;
	if (searchType == SEARCH_REGEX)
		searchType = SEARCH_CASE_SENSE;
	else if (searchType == SEARCH_REGEX_NOCASE)
		searchType = SEARCH_LITERAL;

	// search for it in the window 
	SearchAndSelect(window, callDataItems->direction, searchString, searchType, callDataItems->searchWrap);
	XtFree((char *)callData);
}

/*
** Pop up and clear the incremental search line and prepare to search.
*/
void BeginISearch(Document *window, SearchDirection direction) {
	window->iSearchStartPos_ = -1;
	XmTextSetStringEx(window->iSearchText_, (String) "");
	XmToggleButtonSetState(window->iSearchRevToggle_, direction == SEARCH_BACKWARD, FALSE);
	/* Note: in contrast to the replace and find dialogs, the regex and
	   case toggles are not reset to their default state when the incremental
	   search bar is redisplayed. I'm not sure whether this is the best
	   choice. If not, an initToggleButtons() call should be inserted
	   here. But in that case, it might be appropriate to have different
	   default search modes for i-search and replace/find. */
	window->TempShowISearch(TRUE);
	XmProcessTraversal(window->iSearchText_, XmTRAVERSE_CURRENT);
}

/*
** Incremental searching is anchored at the position where the cursor
** was when the user began typing the search string.  Call this routine
** to forget about this original anchor, and if the search bar is not
** permanently up, pop it down.
*/
void EndISearch(Document *window) {
	/* Note: Please maintain this such that it can be freely peppered in
	   mainline code, without callers having to worry about performance
	   or visual glitches.  */

	// Forget the starting position used for the current run of searches 
	window->iSearchStartPos_ = -1;

	// Mark the end of incremental search history overwriting 
	saveSearchHistory("", nullptr, SEARCH_LITERAL, FALSE);

	// Pop down the search line (if it's not pegged up in Preferences) 
	window->TempShowISearch(FALSE);
}

/*
** Reset window->iSearchLastBeginPos_ to the resulting initial
** search begin position for incremental searches.
*/
static void iSearchRecordLastBeginPos(Document *window, SearchDirection direction, int initPos) {
	window->iSearchLastBeginPos_ = initPos;
	if (direction == SEARCH_BACKWARD)
		window->iSearchLastBeginPos_--;
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  If
** "continued" is TRUE and a prior incremental search starting position is
** recorded, search from that original position, otherwise, search from the
** current cursor position.
*/
bool SearchAndSelectIncremental(Document *window, SearchDirection direction, const char *searchString, SearchType searchType, int searchWrap, int continued) {
	int beginPos, startPos, endPos;

	/* If there's a search in progress, start the search from the original
	   starting position, otherwise search from the cursor position. */
	if (!continued || window->iSearchStartPos_ == -1) {
		
		auto textD = window->lastFocus();
		
		window->iSearchStartPos_ = textD->TextGetCursorPos();
		iSearchRecordLastBeginPos(window, direction, window->iSearchStartPos_);
	}
	beginPos = window->iSearchStartPos_;

	/* If the search string is empty, beep eventually if text wrapped
	   back to the initial position, re-init iSearchLastBeginPos,
	   clear the selection, set the cursor back to what would be the
	   beginning of the search, and return. */
	if (searchString[0] == 0) {
		int beepBeginPos = (direction == SEARCH_BACKWARD) ? beginPos - 1 : beginPos;
		iSearchTryBeepOnWrap(window, direction, beepBeginPos, beepBeginPos);
		iSearchRecordLastBeginPos(window, direction, window->iSearchStartPos_);
		window->buffer_->BufUnselect();
		
		auto textD = window->lastFocus();
		textD->TextSetCursorPos(beginPos);
		return true;
	}

	/* Save the string in the search history, unless we're cycling thru
	   the search history itself, which can be detected by matching the
	   search string with the search string of the current history index. */
	if (!(window->iSearchHistIndex_ > 1 && !strcmp(searchString, SearchHistory[historyIndex(window->iSearchHistIndex_)]))) {
		saveSearchHistory(searchString, nullptr, searchType, TRUE);
		// Reset the incremental search history pointer to the beginning 
		window->iSearchHistIndex_ = 1;
	}

	// begin at insert position - 1 for backward searches 
	if (direction == SEARCH_BACKWARD)
		beginPos--;

	// do the search.  SearchWindow does appropriate dialogs and beeps 
	if (!SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr))
		return false;

	window->iSearchLastBeginPos_ = startPos;

	/* if the search matched an empty string (possible with regular exps)
	   beginning at the start of the search, go to the next occurrence,
	   otherwise repeated finds will get "stuck" at zero-length matches */
	if (direction == SEARCH_FORWARD && beginPos == startPos && beginPos == endPos)
		if (!SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr))
			return false;

	window->iSearchLastBeginPos_ = startPos;

	// select the text found string 
	window->buffer_->BufSelect(startPos, endPos);
	window->MakeSelectionVisible(window->lastFocus_);
	
	auto textD = window->lastFocus();
	textD->TextSetCursorPos(endPos);

	return true;
}

/*
** Attach callbacks to the incremental search bar widgets.  This also fudges
** up the translations on the text widget so Shift+Return will call the
** activate callback (along with Return and Ctrl+Return).  It does this
** because incremental search uses the activate callback from the text
** widget to detect when the user has pressed Return to search for the next
** occurrence of the search string, and Shift+Return, which is the natural
** command for a reverse search does not naturally trigger this callback.
*/
void SetISearchTextCallbacks(Document *window) {
	static XtTranslations tableText = nullptr;
	static const char *translationsText = "Shift<KeyPress>Return: activate()\n";

	static XtTranslations tableClear = nullptr;
	static const char *translationsClear = "<Btn2Down>:Arm()\n<Btn2Up>: isearch_clear_and_paste() Disarm()\n";

	static XtActionsRec actions[] = {{(String) "isearch_clear_and_paste", iSearchTextClearAndPasteAP}};

	if(!tableText)
		tableText = XtParseTranslationTable(translationsText);
	XtOverrideTranslations(window->iSearchText_, tableText);

	if(!tableClear) {
		// make sure actions are loaded 
		XtAppAddActions(XtWidgetToApplicationContext(window->iSearchText_), actions, XtNumber(actions));
		tableClear = XtParseTranslationTable(translationsClear);
	}
	XtOverrideTranslations(window->iSearchClearButton_, tableClear);

	XtAddCallback(window->iSearchText_, XmNactivateCallback, iSearchTextActivateCB, window);
	XtAddCallback(window->iSearchText_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);
	XtAddEventHandler(window->iSearchText_, KeyPressMask, False, iSearchTextKeyEH, window);

	/* Attach callbacks to deal with the optional sticky case sensitivity
	   behaviour. Do this before installing the search callbacks to make
	   sure that the proper search parameters are taken into account. */
	XtAddCallback(window->iSearchCaseToggle_, XmNvalueChangedCallback, iSearchCaseToggleCB, window);
	XtAddCallback(window->iSearchRegexToggle_, XmNvalueChangedCallback, iSearchRegExpToggleCB, window);

	// When search parameters (direction or search type), redo the search 
	XtAddCallback(window->iSearchCaseToggle_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);
	XtAddCallback(window->iSearchRegexToggle_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);
	XtAddCallback(window->iSearchRevToggle_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);

	// find button: just like pressing return 
	XtAddCallback(window->iSearchFindButton_, XmNactivateCallback, iSearchTextActivateCB, window);
	// clear button: empty the search text widget 
	XtAddCallback(window->iSearchClearButton_, XmNactivateCallback, iSearchTextClearCB, window);
}

/*
** Remove callbacks before resetting the incremental search text to avoid any
** cursor movement and/or clearing of selections.
*/
static void iSearchTextSetString(Widget w, Document *window, const std::string &str) {

	(void)w;

	// remove callbacks which would be activated by emptying the text 
	XtRemoveAllCallbacks(window->iSearchText_, XmNvalueChangedCallback);
	XtRemoveAllCallbacks(window->iSearchText_, XmNactivateCallback);
	// empty the text 
	XmTextSetStringEx(window->iSearchText_, str);
	// put back the callbacks 
	XtAddCallback(window->iSearchText_, XmNactivateCallback, iSearchTextActivateCB, window);
	XtAddCallback(window->iSearchText_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);
}

/*
** Action routine for Mouse Button 2 on the iSearchClearButton: resets the
** string then calls the activate callback for the text directly.
*/
static void iSearchTextClearAndPasteAP(Widget w, XEvent *event, String *args, Cardinal *nArg) {

	(void)args;
	(void)nArg;

	XmAnyCallbackStruct cbdata;
	memset(&cbdata, 0, sizeof(cbdata));
	cbdata.event = event;

	Document *window = Document::WidgetToWindow(w);

	QString selText = GetAnySelectionEx(window);
	
	iSearchTextSetString(w, window, !selText.isNull() ? selText.toStdString() : "");
	
	if (!selText.isNull()) {
		XmTextSetInsertionPosition(window->iSearchText_, selText.size());
	}
	
	iSearchTextActivateCB(w, window, &cbdata);
}

/*
** User pressed the clear incremental search bar button. Remove callbacks
** before resetting the text to avoid any cursor movement and/or clearing
** of selections.
*/
static void iSearchTextClearCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	(void)callData;

	window = Document::WidgetToWindow(w);

	iSearchTextSetString(w, window, "");
}

/*
** User pressed return in the incremental search bar.  Do a new search with
** the search string displayed.  The direction of the search is toggled if
** the Ctrl key or the Shift key is pressed when the text field is activated.
*/
static void iSearchTextActivateCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	const char *params[4];
	SearchType searchType;
	SearchDirection direction;

	window = Document::WidgetToWindow(w);

	/* Fetch the string, search type and direction from the incremental
	   search bar widgets at the top of the window */
	QString searchString = XmTextGetStringEx(window->iSearchText_);
	if (XmToggleButtonGetState(window->iSearchCaseToggle_)) {
		if (XmToggleButtonGetState(window->iSearchRegexToggle_))
			searchType = SEARCH_REGEX;
		else
			searchType = SEARCH_CASE_SENSE;
	} else {
		if (XmToggleButtonGetState(window->iSearchRegexToggle_))
			searchType = SEARCH_REGEX_NOCASE;
		else
			searchType = SEARCH_LITERAL;
	}
	direction = XmToggleButtonGetState(window->iSearchRevToggle_) ? SEARCH_BACKWARD : SEARCH_FORWARD;

	// Reverse the search direction if the Ctrl or Shift key was pressed 
	if (callData->event->xbutton.state & (ShiftMask | ControlMask))
		direction = direction == SEARCH_FORWARD ? SEARCH_BACKWARD : SEARCH_FORWARD;

	QByteArray searchArray = searchString.toLatin1();

	// find the text and mark it 
	params[0] = searchArray.data();
	params[1] = directionArg(direction);
	params[2] = searchTypeArg(searchType);
	params[3] = searchWrapArg(GetPrefSearchWraps());
	
	XtCallActionProc(window->lastFocus_, "find", callData->event, const_cast<char **>(params), 4);
}

/*
** Called when user types in the incremental search line.  Redoes the
** search for the new search string.
*/
static void iSearchTextValueChangedCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);
	
	const char *params[5];
	SearchType searchType;
	SearchDirection direction;
	int nParams;

	window = Document::WidgetToWindow(w);

	/* Fetch the string, search type and direction from the incremental
	   search bar widgets at the top of the window */
	QString searchString = XmTextGetStringEx(window->iSearchText_);
	
	if (XmToggleButtonGetState(window->iSearchCaseToggle_)) {
		if (XmToggleButtonGetState(window->iSearchRegexToggle_))
			searchType = SEARCH_REGEX;
		else
			searchType = SEARCH_CASE_SENSE;
	} else {
		if (XmToggleButtonGetState(window->iSearchRegexToggle_))
			searchType = SEARCH_REGEX_NOCASE;
		else
			searchType = SEARCH_LITERAL;
	}
	direction = XmToggleButtonGetState(window->iSearchRevToggle_) ? SEARCH_BACKWARD : SEARCH_FORWARD;

	/* If the search type is a regular expression, test compile it.  If it
	   fails, silently skip it.  (This allows users to compose the expression
	   in peace when they have unfinished syntax, but still get beeps when
	   correct syntax doesn't match) */
	if (isRegexType(searchType)) {
		try {
			auto compiledRE = mem::make_unique<regexp>(searchString.toStdString(), defaultRegexFlags(searchType));
		} catch(const regex_error &e) {
			return;
		}
	}
	
	QByteArray searchArray = searchString.toLatin1();

	/* Call the incremental search action proc to do the searching and
	   selecting (this allows it to be recorded for learn/replay).  If
	   there's an incremental search already in progress, mark the operation
	   as "continued" so the search routine knows to re-start the search
	   from the original starting position */
	nParams = 0;
	params[nParams++] = searchArray.data(); // TODO(eteran): is this OK?
	params[nParams++] = directionArg(direction);
	params[nParams++] = searchTypeArg(searchType);
	params[nParams++] = searchWrapArg(GetPrefSearchWraps());
	if (window->iSearchStartPos_ != -1) {
		params[nParams++] = "continued";
	}
	
	XtCallActionProc(window->lastFocus_, "find_incremental", callData->event, const_cast<char **>(params), nParams);
}

/*
** Process arrow keys for history recall, and escape key for leaving
** incremental search bar.
*/
static void iSearchTextKeyEH(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch) {


	auto event = reinterpret_cast<XKeyEvent *>(Event);
	auto window = static_cast<Document *>(clientData);

	KeySym keysym = XLookupKeysym(event, 0);
	int index;
	const char *searchStr;
	SearchType searchType;

	// only process up and down arrow keys 
	if (keysym != XK_Up && keysym != XK_Down && keysym != XK_Escape) {
		*continueDispatch = TRUE;
		return;
	}

	window = Document::WidgetToWindow(w);
	index = window->iSearchHistIndex_;
	*continueDispatch = FALSE;

	// allow escape key to cancel search 
	if (keysym == XK_Escape) {
		XmProcessTraversal(window->lastFocus_, XmTRAVERSE_CURRENT);
		EndISearch(window);
		return;
	}

	// increment or decrement the index depending on which arrow was pressed 
	index += (keysym == XK_Up) ? 1 : -1;

	// if the index is out of range, beep and return 
	if (index != 0 && historyIndex(index) == -1) {
		QApplication::beep();
		return;
	}

	// determine the strings and button settings to use 
	if (index == 0) {
		searchStr = "";
		searchType = GetPrefSearch();
	} else {
		searchStr = SearchHistory[historyIndex(index)];
		searchType = SearchTypeHistory[historyIndex(index)];
	}

	/* Set the info used in the value changed callback before calling
	  XmTextSetStringEx(). */
	window->iSearchHistIndex_ = index;
	initToggleButtons(searchType, window->iSearchRegexToggle_, window->iSearchCaseToggle_, nullptr, &window->iSearchLastLiteralCase_, &window->iSearchLastRegexCase_);

	// Beware the value changed callback is processed as part of this call 
	XmTextSetStringEx(window->iSearchText_, searchStr);
	XmTextSetInsertionPosition(window->iSearchText_, XmTextGetLastPosition(window->iSearchText_));
}

/*
** Check the character before the insertion cursor of textW and flash
** matching parenthesis, brackets, or braces, by temporarily highlighting
** the matching character (a timer procedure is scheduled for removing the
** highlights)
*/
void FlashMatching(Document *window, Widget textW) {
	char c;
	void *style;
	int matchIndex;
	int startPos;
	int endPos;
	int searchPos;
	int matchPos;
	int constrain;

	// if a marker is already drawn, erase it and cancel the timeout 
	if (window->flashTimeoutID_ != 0) {
		eraseFlash(window);
		XtRemoveTimeOut(window->flashTimeoutID_);
		window->flashTimeoutID_ = 0;
	}

	// no flashing required 
	if (window->showMatchingStyle_ == NO_FLASH) {
		return;
	}

	// don't flash matching characters if there's a selection 
	if (window->buffer_->primary_.selected)
		return;

	// get the character to match and the position to start from 
	auto textD = textD_of(textW);
	
	int pos = textD->TextGetCursorPos() - 1;
	if (pos < 0)
		return;
	c = window->buffer_->BufGetCharacter(pos);
	style = GetHighlightInfo(window, pos);

	// is the character one we want to flash? 
	for (matchIndex = 0; matchIndex < N_FLASH_CHARS; matchIndex++) {
		if (MatchingChars[matchIndex].c == c)
			break;
	}
	if (matchIndex == N_FLASH_CHARS)
		return;

	/* constrain the search to visible text only when in single-pane mode
	   AND using delimiter flashing (otherwise search the whole buffer) */
	constrain = ((window->textPanes_.size() == 0) && (window->showMatchingStyle_ == FLASH_DELIMIT));

	if (MatchingChars[matchIndex].direction == SEARCH_BACKWARD) {
		startPos = constrain ? textD->TextFirstVisiblePos() : 0;
		endPos = pos;
		searchPos = endPos;
	} else {
		startPos = pos;
		endPos = constrain ? textD->TextLastVisiblePos() : window->buffer_->BufGetLength();
		searchPos = startPos;
	}

	// do the search 
	if (!findMatchingChar(window, c, style, searchPos, startPos, endPos, &matchPos))
		return;

	if (window->showMatchingStyle_ == FLASH_DELIMIT) {
		// Highlight either the matching character ... 
		window->buffer_->BufHighlight(matchPos, matchPos + 1);
	} else {
		// ... or the whole range. 
		if (MatchingChars[matchIndex].direction == SEARCH_BACKWARD) {
			window->buffer_->BufHighlight(matchPos, pos + 1);
		} else {
			window->buffer_->BufHighlight(matchPos + 1, pos);
		}
	}

	// Set up a timer to erase the box after 1.5 seconds 
	window->flashTimeoutID_ = XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell_), 1500, flashTimeoutProc, window);
	window->flashPos_ = matchPos;
}

void SelectToMatchingCharacter(Document *window) {
	int selStart;
	int selEnd;
	int startPos;
	int endPos;
	int matchPos;
	TextBuffer *buf = window->buffer_;

	/* get the character to match and its position from the selection, or
	   the character before the insert point if nothing is selected.
	   Give up if too many characters are selected */
	if (!buf->GetSimpleSelection(&selStart, &selEnd)) {
		
		auto textD = window->lastFocus();
		
		selEnd = textD->TextGetCursorPos();
		if (window->overstrike_)
			selEnd += 1;
		selStart = selEnd - 1;
		if (selStart < 0) {
			QApplication::beep();
			return;
		}
	}
	if ((selEnd - selStart) != 1) {
		QApplication::beep();
		return;
	}

	// Search for it in the buffer 
	if (!findMatchingChar(window, buf->BufGetCharacter(selStart), GetHighlightInfo(window, selStart), selStart, 0, buf->BufGetLength(), &matchPos)) {
		QApplication::beep();
		return;
	}
	startPos = (matchPos > selStart) ? selStart : matchPos;
	endPos = (matchPos > selStart) ? matchPos : selStart;

	/* temporarily shut off autoShowInsertPos before setting the cursor
	   position so MakeSelectionVisible gets a chance to place the cursor
	   string at a pleasing position on the screen (otherwise, the cursor would
	   be automatically scrolled on screen and MakeSelectionVisible would do
	   nothing) */
	textD_of(window->lastFocus_)->setAutoShowInsertPos(false);
	// select the text between the matching characters 
	buf->BufSelect(startPos, endPos + 1);
	window->MakeSelectionVisible(window->lastFocus_);
	textD_of(window->lastFocus_)->setAutoShowInsertPos(true);
}

void GotoMatchingCharacter(Document *window) {
	int selStart, selEnd;
	int matchPos;
	TextBuffer *buf = window->buffer_;

	/* get the character to match and its position from the selection, or
	   the character before the insert point if nothing is selected.
	   Give up if too many characters are selected */
	if (!buf->GetSimpleSelection(&selStart, &selEnd)) {
		
		auto textD = window->lastFocus();
		
		selEnd = textD->TextGetCursorPos();
		
		if (window->overstrike_) {
			selEnd += 1;
		}
		
		selStart = selEnd - 1;
		if (selStart < 0) {
			QApplication::beep();
			return;
		}
	}
	if ((selEnd - selStart) != 1) {
		QApplication::beep();
		return;
	}

	// Search for it in the buffer 
	if (!findMatchingChar(window, buf->BufGetCharacter(selStart), GetHighlightInfo(window, selStart), selStart, 0, buf->BufGetLength(), &matchPos)) {
		QApplication::beep();
		return;
	}

	/* temporarily shut off autoShowInsertPos before setting the cursor
	   position so MakeSelectionVisible gets a chance to place the cursor
	   string at a pleasing position on the screen (otherwise, the cursor would
	   be automatically scrolled on screen and MakeSelectionVisible would do
	   nothing) */
	textD_of(window->lastFocus_)->setAutoShowInsertPos(false);

	auto textD = window->lastFocus();
	
	textD->TextSetCursorPos(matchPos + 1);
	window->MakeSelectionVisible(window->lastFocus_);
	textD_of(window->lastFocus_)->setAutoShowInsertPos(true);
}

static int findMatchingChar(Document *window, char toMatch, void *styleToMatch, int charPos, int startLimit, int endLimit, int *matchPos) {
	int nestDepth, matchIndex, direction, beginPos, pos;
	char matchChar, c;
	void *style = nullptr;
	TextBuffer *buf = window->buffer_;
	int matchSyntaxBased = window->matchSyntaxBased_;

	// If we don't match syntax based, fake a matching style. 
	if (!matchSyntaxBased)
		style = styleToMatch;

	// Look up the matching character and match direction 
	for (matchIndex = 0; matchIndex < N_MATCH_CHARS; matchIndex++) {
		if (MatchingChars[matchIndex].c == toMatch)
			break;
	}
	if (matchIndex == N_MATCH_CHARS)
		return FALSE;
	matchChar = MatchingChars[matchIndex].match;
	direction = MatchingChars[matchIndex].direction;

	// find it in the buffer 
	beginPos = (direction == SEARCH_FORWARD) ? charPos + 1 : charPos - 1;
	nestDepth = 1;
	if (direction == SEARCH_FORWARD) {
		for (pos = beginPos; pos < endLimit; pos++) {
			c = buf->BufGetCharacter(pos);
			if (c == matchChar) {
				if (matchSyntaxBased)
					style = GetHighlightInfo(window, pos);
				if (style == styleToMatch) {
					nestDepth--;
					if (nestDepth == 0) {
						*matchPos = pos;
						return TRUE;
					}
				}
			} else if (c == toMatch) {
				if (matchSyntaxBased)
					style = GetHighlightInfo(window, pos);
				if (style == styleToMatch)
					nestDepth++;
			}
		}
	} else { // SEARCH_BACKWARD 
		for (pos = beginPos; pos >= startLimit; pos--) {
			c = buf->BufGetCharacter(pos);
			if (c == matchChar) {
				if (matchSyntaxBased)
					style = GetHighlightInfo(window, pos);
				if (style == styleToMatch) {
					nestDepth--;
					if (nestDepth == 0) {
						*matchPos = pos;
						return TRUE;
					}
				}
			} else if (c == toMatch) {
				if (matchSyntaxBased)
					style = GetHighlightInfo(window, pos);
				if (style == styleToMatch)
					nestDepth++;
			}
		}
	}
	return FALSE;
}

/*
** Xt timer procedure for erasing the matching parenthesis marker.
*/
static void flashTimeoutProc(XtPointer clientData, XtIntervalId *id) {

	(void)id;

	eraseFlash(static_cast<Document *>(clientData));
	static_cast<Document *>(clientData)->flashTimeoutID_ = 0;
}

/*
** Erase the marker drawn on a matching parenthesis bracket or brace
** character.
*/
static void eraseFlash(Document *window) {
	window->buffer_->BufUnhighlight();
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool ReplaceSame(Document *window, SearchDirection direction, int searchWrap) {
	if (NHist < 1) {
		QApplication::beep();
		return FALSE;
	}

	return SearchAndReplace(window, direction, SearchHistory[historyIndex(1)], ReplaceHistory[historyIndex(1)], SearchTypeHistory[historyIndex(1)], searchWrap);
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool ReplaceFindSame(Document *window, SearchDirection direction, int searchWrap) {
	if (NHist < 1) {
		QApplication::beep();
		return FALSE;
	}

	return ReplaceAndSearch(window, direction, SearchHistory[historyIndex(1)], ReplaceHistory[historyIndex(1)], SearchTypeHistory[historyIndex(1)], searchWrap);
}

/*
** Replace selection with "replaceString" and search for string "searchString" in window "window",
** using algorithm "searchType" and direction "direction"
*/
bool ReplaceAndSearch(Document *window, SearchDirection direction, const char *searchString, const char *replaceString, SearchType searchType, int searchWrap) {
	int startPos = 0;
	int endPos = 0;
	int replaceLen = 0;
	int searchExtentBW;
	int searchExtentFW;

	// Save a copy of search and replace strings in the search history 
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	bool replaced = false;

	// Replace the selected text only if it matches the search string 
	if (searchMatchesSelection(window, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {
		// replace the text 
		if (isRegexType(searchType)) {
			char replaceResult[SEARCHMAX + 1];
			const std::string foundString = window->buffer_->BufGetRangeEx(searchExtentBW, searchExtentFW + 1);

			replaceUsingREEx(
				searchString,
				replaceString,
				foundString,
				startPos - searchExtentBW,
				replaceResult,
				SEARCHMAX,
				startPos == 0 ? '\0' : window->buffer_->BufGetCharacter(startPos - 1),
				GetWindowDelimiters(window).toLatin1().data(),
				defaultRegexFlags(searchType));

			window->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
			replaceLen = strlen(replaceResult);
		} else {
			window->buffer_->BufReplaceEx(startPos, endPos, replaceString);
			replaceLen = strlen(replaceString);
		}

		// Position the cursor so the next search will work correctly based 
		// on the direction of the search 
		auto textD = window->lastFocus();
		textD->TextSetCursorPos(startPos + ((direction == SEARCH_FORWARD) ? replaceLen : 0));
		replaced = true;
	}

	// do the search; beeps/dialogs are taken care of 
	SearchAndSelect(window, direction, searchString, searchType, searchWrap);

	return replaced;
}

/*
** Search for string "searchString" in window "window", using algorithm
** "searchType" and direction "direction", and replace it with "replaceString"
** Also adds the search and replace strings to the global search history.
*/
bool SearchAndReplace(Document *window, SearchDirection direction, const char *searchString, const char *replaceString, SearchType searchType, int searchWrap) {
	int startPos;
	int endPos;
	int replaceLen;
	int searchExtentBW;
	int searchExtentFW;
	int found;
	int beginPos;
	int cursorPos;

	// Save a copy of search and replace strings in the search history 
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	// If the text selected in the window matches the search string, 	
	// the user is probably using search then replace method, so	
	// replace the selected text regardless of where the cursor is.	
	// Otherwise, search for the string.				
	if (!searchMatchesSelection(window, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {
		// get the position to start the search 
		
		auto textD = window->lastFocus();
		
		cursorPos = textD->TextGetCursorPos();
		if (direction == SEARCH_BACKWARD) {
			// use the insert position - 1 for backward searches 
			beginPos = cursorPos - 1;
		} else {
			// use the insert position for forward searches 
			beginPos = cursorPos;
		}
		// do the search 
		found = SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW);
		if (!found)
			return false;
	}

	// replace the text 
	if (isRegexType(searchType)) {
		char replaceResult[SEARCHMAX];
		const std::string foundString = window->buffer_->BufGetRangeEx(searchExtentBW, searchExtentFW + 1);
		replaceUsingREEx(
			searchString,
			replaceString,
			foundString,
			startPos - searchExtentBW,
			replaceResult,
			SEARCHMAX,
			startPos == 0 ? '\0' : window->buffer_->BufGetCharacter(startPos - 1),
			GetWindowDelimiters(window).toLatin1().data(),
			defaultRegexFlags(searchType));

		window->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
		replaceLen = strlen(replaceResult);
	} else {
		window->buffer_->BufReplaceEx(startPos, endPos, replaceString);
		replaceLen = strlen(replaceString);
	}

	/* after successfully completing a replace, selected text attracts
	   attention away from the area of the replacement, particularly
	   when the selection represents a previous search. so deselect */
	window->buffer_->BufUnselect();

	/* temporarily shut off autoShowInsertPos before setting the cursor
	   position so MakeSelectionVisible gets a chance to place the replaced
	   string at a pleasing position on the screen (otherwise, the cursor would
	   be automatically scrolled on screen and MakeSelectionVisible would do
	   nothing) */
	auto textD = window->lastFocus();
	textD->setAutoShowInsertPos(false);

	
	textD->TextSetCursorPos(startPos + ((direction == SEARCH_FORWARD) ? replaceLen : 0));
	window->MakeSelectionVisible(window->lastFocus_);
	textD->setAutoShowInsertPos(true);

	return true;
}

/*
**  Uses the resource nedit.truncSubstitution to determine how to deal with
**  regex failures. This function only knows about the resource (via the usual
**  setting getter) and asks or warns the user depending on that.
**
**  One could argue that the dialoging should be determined by the setting
**  'searchDlogs'. However, the incomplete substitution is not just a question
**  of verbosity, but of data loss. The search is successful, only the
**  replacement fails due to an internal limitation of NEdit.
**
**  The parameters 'parent' and 'display' are only used to put dialogs and
**  beeps at the right place.
**
**  The result is either predetermined by the resource or the user's choice.
*/
static bool prefOrUserCancelsSubst(const Widget parent, const Display *display) {

	Q_UNUSED(parent);
	
	bool cancel = true;

	switch (GetPrefTruncSubstitution()) {
	case TRUNCSUBST_SILENT:
		//  silently fail the operation  
		cancel = true;
		break;

	case TRUNCSUBST_FAIL:
		//  fail the operation and pop up a dialog informing the user  
		XBell((Display *)display, 0);
		
		QMessageBox::information(nullptr /* parent */, QLatin1String("Substitution Failed"), QLatin1String("The result length of the substitution exceeded an internal limit.\n"
		                                                                                                   "The substitution is canceled."));

		cancel = true;
		break;

	case TRUNCSUBST_WARN:
		//  pop up dialog and ask for confirmation  
		XBell((Display *)display, 0);
				
		{		
			QMessageBox messageBox(nullptr /*parent*/);
			messageBox.setWindowTitle(QLatin1String("Substitution Failed"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(QLatin1String("The result length of the substitution exceeded an internal limit.\nExecuting the substitution will result in loss of data."));
			QPushButton *buttonLose   = messageBox.addButton(QLatin1String("Lose Data"), QMessageBox::AcceptRole);
			QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
			Q_UNUSED(buttonLose);

			messageBox.exec();
			cancel = (messageBox.clickedButton() == buttonCancel);		
		}				
		break;

	case TRUNCSUBST_IGNORE:
		//  silently conclude the operation; THIS WILL DESTROY DATA.  
		cancel = false;
		break;
	}

	return cancel;
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString"
** within the current primary selection in "window". Also adds the search and
** replace strings to the global search history.
*/
void ReplaceInSelection(const Document *window, const char *searchString, const char *replaceString, SearchType searchType) {
	int selStart;
	int selEnd;
	int beginPos;
	int startPos;
	int endPos;
	int realOffset;
	int replaceLen;
	bool found;
	bool isRect;
	int rectStart;
	int rectEnd;
	int lineStart;
	int cursorPos;
	int extentBW;
	int extentFW;
	std::string fileString;
	bool substSuccess = false;
	bool anyFound     = false;
	bool cancelSubst  = true;

	// save a copy of search and replace strings in the search history 
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	// find out where the selection is 
	if (!window->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		return;
	}

	// get the selected text 
	if (isRect) {
		selStart = window->buffer_->BufStartOfLine(selStart);
		selEnd = window->buffer_->BufEndOfLine( selEnd);
		fileString = window->buffer_->BufGetRangeEx(selStart, selEnd);
	} else {
		fileString = window->buffer_->BufGetSelectionTextEx();
	}

	/* create a temporary buffer in which to do the replacements to hide the
	   intermediate steps from the display routines, and so everything can
	   be undone in a single operation */
	auto tempBuf = mem::make_unique<TextBuffer>();
	tempBuf->BufSetAllEx(fileString);

	// search the string and do the replacements in the temporary buffer 
	replaceLen = strlen(replaceString);
	found = true;
	beginPos = 0;
	cursorPos = 0;
	realOffset = 0;
	while (found) {
		found = SearchString(fileString, searchString, SEARCH_FORWARD, searchType, FALSE, beginPos, &startPos, &endPos, &extentBW, &extentFW, GetWindowDelimiters(window).toLatin1().data());
		if (!found)
			break;

		anyFound = True;
		/* if the selection is rectangular, verify that the found
		   string is in the rectangle */
		if (isRect) {
			lineStart = window->buffer_->BufStartOfLine(selStart + startPos);
			if (window->buffer_->BufCountDispChars(lineStart, selStart + startPos) < rectStart || window->buffer_->BufCountDispChars(lineStart, selStart + endPos) > rectEnd) {
				if (fileString[endPos] == '\0')
					break;
				/* If the match starts before the left boundary of the
				   selection, and extends past it, we should not continue
				   search after the end of the (false) match, because we
				   could miss a valid match starting between the left boundary
				   and the end of the false match. */
				if (window->buffer_->BufCountDispChars(lineStart, selStart + startPos) < rectStart && window->buffer_->BufCountDispChars(lineStart, selStart + endPos) > rectStart)
					beginPos += 1;
				else
					beginPos = (startPos == endPos) ? endPos + 1 : endPos;
				continue;
			}
		}

		/* Make sure the match did not start past the end (regular expressions
		   can consider the artificial end of the range as the end of a line,
		   and match a fictional whole line beginning there) */
		if (startPos == (selEnd - selStart)) {
			found = false;
			break;
		}

		// replace the string and compensate for length change 
		if (isRegexType(searchType)) {
			char replaceResult[SEARCHMAX];
			const std::string foundString = tempBuf->BufGetRangeEx(extentBW + realOffset, extentFW + realOffset + 1);
			substSuccess = replaceUsingREEx(
							searchString,
							replaceString,
							foundString,
							startPos - extentBW,
							replaceResult,
							SEARCHMAX,
							(startPos + realOffset) == 0 ? '\0' : tempBuf->BufGetCharacter(startPos + realOffset - 1),
							GetWindowDelimiters(window).toLatin1().data(),
							defaultRegexFlags(searchType));

			if (!substSuccess) {
				/*  The substitution failed. Primary reason for this would be
				    a result string exceeding SEARCHMAX. */

				cancelSubst = prefOrUserCancelsSubst(window->shell_, TheDisplay);

				if (cancelSubst) {
					//  No point in trying other substitutions.  
					break;
				}
			}

			tempBuf->BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceResult);
			replaceLen = strlen(replaceResult);
		} else {
			// at this point plain substitutions (should) always work 
			tempBuf->BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceString);
			substSuccess = True;
		}

		realOffset += replaceLen - (endPos - startPos);
		// start again after match unless match was empty, then endPos+1 
		beginPos = (startPos == endPos) ? endPos + 1 : endPos;
		cursorPos = endPos;
		if (fileString[endPos] == '\0')
			break;
	}

	if (anyFound) {
		if (substSuccess || !cancelSubst) {
			/*  Either the substitution was successful (the common case) or the
			    user does not care and wants to have a faulty replacement.  */

			// replace the selected range in the real buffer 
			window->buffer_->BufReplaceEx(selStart, selEnd, tempBuf->BufAsStringEx());

			// set the insert point at the end of the last replacement 
			auto textD = window->lastFocus();
			
			textD->TextSetCursorPos(selStart + cursorPos + realOffset);

			/* leave non-rectangular selections selected (rect. ones after replacement
			   are less useful since left/right positions are randomly adjusted) */
			if (!isRect) {
				window->buffer_->BufSelect(selStart, selEnd + realOffset);
			}
		}
	} else {
		//  Nothing found, tell the user about it  
		if (GetPrefSearchDlogs()) {

			// Avoid bug in Motif 1.1 by putting away search dialog before Dialogs
			if (auto dialog = qobject_cast<DialogFind *>(window->dialogFind_)) {
				if(!dialog->keepDialog()) {
					dialog->hide();
				}
			}
			
			auto dialog = window->getDialogReplace();
			if (dialog && !dialog->keepDialog()) {			
				unmanageReplaceDialogs(window);
			}
			
			QMessageBox::information(nullptr /*parent*/, QLatin1String("String not found"), QLatin1String("String was not found"));
		} else {
			QApplication::beep();
		}
	}
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString".
** Also adds the search and replace strings to the global search history.
*/
bool ReplaceAll(Document *window, const char *searchString, const char *replaceString, SearchType searchType) {
	char *newFileString;
	int copyStart, copyEnd, replacementLen;

	// reject empty string 
	if (*searchString == '\0')
		return false;

	// save a copy of search and replace strings in the search history 
	saveSearchHistory(searchString, replaceString, searchType, false);

	// view the entire text buffer from the text area widget as a string 
	view::string_view fileString = window->buffer_->BufAsStringEx();

	newFileString = ReplaceAllInString(fileString, searchString, replaceString, searchType, &copyStart, &copyEnd, &replacementLen, GetWindowDelimiters(window).toLatin1().data());

	if(!newFileString) {
		if (window->multiFileBusy_) {
			window->replaceFailed_ = TRUE; /* only needed during multi-file
			                                 replacements */
		} else if (GetPrefSearchDlogs()) {
		
			if (auto dialog = qobject_cast<DialogFind *>(window->dialogFind_)) {
				if(!dialog->keepDialog()) {					
					dialog->hide();
				}
			}
			
			auto dialog = window->getDialogReplace();
			if (dialog && !dialog->keepDialog()) {
				unmanageReplaceDialogs(window);
			}
			
			QMessageBox::information(nullptr /*parent*/, QLatin1String("String not found"), QLatin1String("String was not found"));
		} else
			QApplication::beep();
		return false;
	}

	// replace the contents of the text widget with the substituted text 
	window->buffer_->BufReplaceEx(copyStart, copyEnd, newFileString);

	// Move the cursor to the end of the last replacement 
	auto textD = window->lastFocus();
	textD->TextSetCursorPos(copyStart + replacementLen);

	XtFree(newFileString);
	return true;
}

/*
** Replace all occurences of "searchString" in "inString" with "replaceString"
** and return an allocated string covering the range between the start of the
** first replacement (returned in "copyStart", and the end of the last
** replacement (returned in "copyEnd")
*/
char *ReplaceAllInString(view::string_view inString, const char *searchString, const char *replaceString, SearchType searchType, int *copyStart, int *copyEnd, int *replacementLength, const char *delimiters) {
	int beginPos, startPos, endPos, lastEndPos;
	int found, nFound, removeLen, replaceLen, copyLen, addLen;
	char *outString, *fillPtr;
	int searchExtentBW, searchExtentFW;

	// reject empty string 
	if (*searchString == '\0')
		return nullptr;

	/* rehearse the search first to determine the size of the buffer needed
	   to hold the substituted text.  No substitution done here yet */
	replaceLen = strlen(replaceString);
	found = TRUE;
	nFound = 0;
	removeLen = 0;
	addLen = 0;
	beginPos = 0;
	*copyStart = -1;
	while (found) {
		found = SearchString(inString, searchString, SEARCH_FORWARD, searchType, FALSE, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW, delimiters);
		if (found) {
			if (*copyStart < 0)
				*copyStart = startPos;
			*copyEnd = endPos;
			// start next after match unless match was empty, then endPos+1 
			beginPos = (startPos == endPos) ? endPos + 1 : endPos;
			nFound++;
			removeLen += endPos - startPos;
			if (isRegexType(searchType)) {
				char replaceResult[SEARCHMAX];
				replaceUsingREEx(
					searchString,
					replaceString,
					&inString[searchExtentBW],
					startPos - searchExtentBW,
					replaceResult,
					SEARCHMAX,
					startPos == 0 ? '\0' : inString[startPos - 1],
					delimiters,
					defaultRegexFlags(searchType));

				addLen += strlen(replaceResult);
			} else
				addLen += replaceLen;
			if (inString[endPos] == '\0')
				break;
		}
	}
	if (nFound == 0)
		return nullptr;

	/* Allocate a new buffer to hold all of the new text between the first
	   and last substitutions */
	copyLen = *copyEnd - *copyStart;
	outString = XtMalloc(copyLen - removeLen + addLen + 1);

	/* Scan through the text buffer again, substituting the replace string
	   and copying the part between replaced text to the new buffer  */
	found = TRUE;
	beginPos = 0;
	lastEndPos = 0;
	fillPtr = outString;
	while (found) {
		found = SearchString(inString, searchString, SEARCH_FORWARD, searchType, FALSE, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW, delimiters);
		if (found) {
			if (beginPos != 0) {
				memcpy(fillPtr, &inString[lastEndPos], startPos - lastEndPos);
				fillPtr += startPos - lastEndPos;
			}
			if (isRegexType(searchType)) {
				char replaceResult[SEARCHMAX];
				replaceUsingREEx(
					searchString,
					replaceString,
					&inString[searchExtentBW],
					startPos - searchExtentBW,
					replaceResult,
					SEARCHMAX,
					startPos == 0 ? '\0' : inString[startPos - 1],
					delimiters,
					defaultRegexFlags(searchType));

				replaceLen = strlen(replaceResult);
				memcpy(fillPtr, replaceResult, replaceLen);
			} else {
				memcpy(fillPtr, replaceString, replaceLen);
			}
			fillPtr += replaceLen;
			lastEndPos = endPos;
			// start next after match unless match was empty, then endPos+1 
			beginPos = (startPos == endPos) ? endPos + 1 : endPos;
			if (inString[endPos] == '\0')
				break;
		}
	}
	*fillPtr = '\0';
	*replacementLength = fillPtr - outString;
	return outString;
}

/*
** If this is an incremental search and BeepOnSearchWrap is on:
** Emit a beep if the search wrapped over BOF/EOF compared to
** the last startPos of the current incremental search.
*/
static void iSearchTryBeepOnWrap(Document *window, SearchDirection direction, int beginPos, int startPos) {
	if (GetPrefBeepOnSearchWrap()) {
		if (direction == SEARCH_FORWARD) {
			if ((startPos >= beginPos && window->iSearchLastBeginPos_ < beginPos) || (startPos < beginPos && window->iSearchLastBeginPos_ >= beginPos)) {
				QApplication::beep();
			}
		} else {
			if ((startPos <= beginPos && window->iSearchLastBeginPos_ > beginPos) || (startPos > beginPos && window->iSearchLastBeginPos_ <= beginPos)) {
				QApplication::beep();
			}
		}
	}
}

/*
** Search the text in "window", attempting to match "searchString"
*/
bool SearchWindow(Document *window, SearchDirection direction, const char *searchString, SearchType searchType, int searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW) {
	bool found;
	int fileEnd = window->buffer_->BufGetLength() - 1;
	bool outsideBounds;

	// reject empty string 
	if (*searchString == '\0')
		return false;

	// get the entire text buffer from the text area widget 
	view::string_view fileString = window->buffer_->BufAsStringEx();

	/* If we're already outside the boundaries, we must consider wrapping
	   immediately (Note: fileEnd+1 is a valid starting position. Consider
	   searching for $ at the end of a file ending with \n.) */
	if ((direction == SEARCH_FORWARD && beginPos > fileEnd + 1) || (direction == SEARCH_BACKWARD && beginPos < 0)) {
		outsideBounds = true;
	} else {
		outsideBounds = false;
	}

	/* search the string copied from the text area widget, and present
	   dialogs, or just beep.  iSearchStartPos is not a perfect indicator that
	   an incremental search is in progress.  A parameter would be better. */
	if (window->iSearchStartPos_ == -1) { // normal search 
		found = !outsideBounds && SearchString(fileString, searchString, direction, searchType, FALSE, beginPos, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window).toLatin1().data());
		
		// Avoid Motif 1.1 bug by putting away search dialog before Dialogs
		if (auto dialog = qobject_cast<DialogFind *>(window->dialogFind_)) {
			if(!dialog->keepDialog()) {		
				dialog->hide();
			}
		}
		
		auto dialog = window->getDialogReplace();
		if (dialog && !dialog->keepDialog()) {
			dialog->hide();
		}
		
		if (!found) {
			if (searchWrap) {
				if (direction == SEARCH_FORWARD && beginPos != 0) {
					if (GetPrefBeepOnSearchWrap()) {
						QApplication::beep();
					} else if (GetPrefSearchDlogs()) {	
													
						QMessageBox messageBox(nullptr /*window->shell_*/);
						messageBox.setWindowTitle(QLatin1String("Wrap Search"));
						messageBox.setIcon(QMessageBox::Question);
						messageBox.setText(QLatin1String("Continue search from\nbeginning of file?"));
						QPushButton *buttonContinue = messageBox.addButton(QLatin1String("Continue"), QMessageBox::AcceptRole);
						QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
						Q_UNUSED(buttonContinue);

						messageBox.exec();
						if(messageBox.clickedButton() == buttonCancel) {
							return false;
						}
					}
					found = SearchString(fileString, searchString, direction, searchType, FALSE, 0, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window).toLatin1().data());
				} else if (direction == SEARCH_BACKWARD && beginPos != fileEnd) {
					if (GetPrefBeepOnSearchWrap()) {
						QApplication::beep();
					} else if (GetPrefSearchDlogs()) {
						
						QMessageBox messageBox(nullptr /*window->shell_*/);
						messageBox.setWindowTitle(QLatin1String("Wrap Search"));
						messageBox.setIcon(QMessageBox::Question);
						messageBox.setText(QLatin1String("Continue search\nfrom end of file?"));
						QPushButton *buttonContinue = messageBox.addButton(QLatin1String("Continue"), QMessageBox::AcceptRole);
						QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
						Q_UNUSED(buttonContinue);

						messageBox.exec();
						if(messageBox.clickedButton() == buttonCancel) {
							return false;
						}
					}
					found = SearchString(fileString, searchString, direction, searchType, FALSE, fileEnd + 1, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window).toLatin1().data());
				}
			}
			if (!found) {
				if (GetPrefSearchDlogs()) {
					QMessageBox::information(nullptr /*parent*/, QLatin1String("String not found"), QLatin1String("String was not found"));
				} else {
					QApplication::beep();
				}
			}
		}
	} else { // incremental search 
		if (outsideBounds && searchWrap) {
			if (direction == SEARCH_FORWARD)
				beginPos = 0;
			else
				beginPos = fileEnd + 1;
			outsideBounds = FALSE;
		}
		found = !outsideBounds && SearchString(fileString, searchString, direction, searchType, searchWrap, beginPos, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window).toLatin1().data());
		if (found) {
			iSearchTryBeepOnWrap(window, direction, beginPos, *startPos);
		} else
			QApplication::beep();
	}

	return found;
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
bool SearchString(view::string_view string, const char *searchString, SearchDirection direction, SearchType searchType, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters) {
	switch (searchType) {
	case SEARCH_CASE_SENSE_WORD:
		return searchLiteralWord(string, searchString, true, direction, wrap, beginPos, startPos, endPos, delimiters);
	case SEARCH_LITERAL_WORD:
		return searchLiteralWord(string, searchString, false, direction, wrap, beginPos, startPos, endPos, delimiters);
	case SEARCH_CASE_SENSE:
		return searchLiteral(string, searchString, true, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW);
	case SEARCH_LITERAL:
		return searchLiteral(string, searchString, false, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW);
	case SEARCH_REGEX:
		return searchRegex(string, searchString, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, REDFLT_STANDARD);
	case SEARCH_REGEX_NOCASE:
		return searchRegex(string, searchString, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, REDFLT_CASE_INSENSITIVE);
	default:
		Q_ASSERT(0);
	}
	return false; // never reached, just makes compilers happy 
}

/*
** Parses a search type description string. If the string contains a valid
** search type description, returns TRUE and writes the corresponding
** SearchType in searchType. Returns FALSE and leaves searchType untouched
** otherwise. (Originally written by Markus Schwarzenberg; slightly adapted).
*/
int StringToSearchType(const char *string, SearchType *searchType) {
	int i;
	for (i = 0; searchTypeStrings[i]; i++) {
		if (!strcmp(string, searchTypeStrings[i])) {
			break;
		}
	}
	if (!searchTypeStrings[i]) {
		return false;
	}
	*searchType = static_cast<SearchType>(i);
	return true;
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
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, const char *delimiters) {


	// TODO(eteran): rework this code in terms of iterators, it will be more clean
	//               also, I'd love to have a nice clean way of replacing this macro
	//               with a lambda or similar

	std::string lcString;
	std::string ucString;
	bool cignore_L = false;
	bool cignore_R = false;

	auto DOSEARCHWORD2 = [&](const char *filePtr) {
		if (*filePtr == ucString[0] || *filePtr == lcString[0]) {
			// matched first character 
			auto ucPtr = ucString.begin();
			auto lcPtr = lcString.begin();
			const char *tempPtr = filePtr;
			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;
				if (ucPtr == ucString.end()                                                            // matched whole string 
			    	&& (cignore_R || isspace((uint8_t)*tempPtr) || strchr(delimiters, *tempPtr)) // next char right delimits word ? 
			    	&& (cignore_L || filePtr == &string[0] ||                                          // border case 
			        	isspace((uint8_t)filePtr[-1]) || strchr(delimiters, filePtr[-1])))       /* next char left delimits word ? */ {
					*startPos = filePtr - &string[0];
					*endPos = tempPtr - &string[0];
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

	if (isspace((uint8_t)searchString[0]) || strchr(delimiters, searchString[0])) {
		cignore_L = true;
	}

	if (isspace((uint8_t)searchString[searchString.size() - 1]) || strchr(delimiters, searchString[searchString.size() - 1])) {
		cignore_R = true;
	}

	if (caseSense) {
		ucString = searchString.to_string();
		lcString = searchString.to_string();
	} else {
		ucString = upCaseStringEx(searchString);
		lcString = downCaseStringEx(searchString);
	}

	if (direction == SEARCH_FORWARD) {
		// search from beginPos to end of string 
		for (auto filePtr = string.begin() + beginPos; filePtr != string.end(); filePtr++) {
			if(DOSEARCHWORD2(filePtr)) {
				return true;
			}
		}
		if (!wrap)
			return FALSE;

		// search from start of file to beginPos 
		for (auto filePtr = string.begin(); filePtr <= string.begin() + beginPos; filePtr++) {
			if(DOSEARCHWORD2(filePtr)) {
				return true;
			}
		}
		return FALSE;
	} else {
		// SEARCH_BACKWARD 
		// search from beginPos to start of file. A negative begin pos 
		// says begin searching from the far end of the file 
		if (beginPos >= 0) {
			for (auto filePtr = string.begin() + beginPos; filePtr >= string.begin(); filePtr--) {
				if(DOSEARCHWORD2(filePtr)) {
					return true;
				}
			}
		}
		if (!wrap)
			return FALSE;
		// search from end of file to beginPos 
		/*... this strlen call is extreme inefficiency, but it's not obvious */
		// how to get the text string length from the text widget (under 1.1)
		for (auto filePtr = string.begin() + string.size(); filePtr >= string.begin() + beginPos; filePtr--) {
			if(DOSEARCHWORD2(filePtr)) {
				return true;
			}
		}
		return FALSE;
	}
}

static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW) {


	std::string lcString;
	std::string ucString;

	auto DOSEARCH2 = [&](const char *filePtr) {
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
					*startPos = filePtr - &string[0];
					*endPos   = tempPtr - &string[0];
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

	if (caseSense) {
		lcString = searchString.to_string();
		ucString = searchString.to_string();
	} else {
		ucString = upCaseStringEx(searchString);
		lcString = downCaseStringEx(searchString);
	}

	if (direction == SEARCH_FORWARD) {

		auto first = string.begin();
		auto mid   = first + beginPos;
		auto last  = string.end();

		// search from beginPos to end of string 
		for (auto filePtr = mid; filePtr != last; ++filePtr) {
			if(DOSEARCH2(filePtr)) {
				return true;
			}
		}

		if (!wrap) {
			return false;
		}

		// search from start of file to beginPos 
		// TODO(eteran): this used to include "mid", but that seems redundant given that we already looked there
		//               in the first loop
		for (auto filePtr = first; filePtr != mid; ++filePtr) {
			if(DOSEARCH2(filePtr)) {
				return true;
			}
		}

		return false;
	} else {
		// SEARCH_BACKWARD 
		// search from beginPos to start of file.  A negative begin pos	
		// says begin searching from the far end of the file 

		auto first = string.begin();
		auto mid   = first + beginPos;
		auto last  = string.end();

		if (beginPos >= 0) {
			for (auto filePtr = mid; filePtr >= first; --filePtr) {
				if(DOSEARCH2(filePtr)) {
					return true;
				}
			}
		}

		if (!wrap) {
			return false;
		}

		// search from end of file to beginPos 
		// how to get the text string length from the text widget (under 1.1)
		for (auto filePtr = last; filePtr >= mid; --filePtr) {
			if(DOSEARCH2(filePtr)) {
				return true;
			}
		}

		return false;
	}
}

static bool searchRegex(view::string_view string, view::string_view searchString, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {
	if (direction == SEARCH_FORWARD)
		return forwardRegexSearch(string, searchString, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, defaultFlags);
	else
		return backwardRegexSearch(string, searchString, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, defaultFlags);
}

static bool forwardRegexSearch(view::string_view string, view::string_view searchString, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {

	try {
		regexp compiledRE(searchString, defaultFlags);

		// search from beginPos to end of string 
		if (compiledRE.execute(string, beginPos, delimiters, false)) {

			*startPos = compiledRE.startp[0] - &string[0];
			*endPos   = compiledRE.endp[0]   - &string[0];

			if(searchExtentFW) {
				*searchExtentFW = compiledRE.extentpFW - &string[0];
			}

			if(searchExtentBW) {
				*searchExtentBW = compiledRE.extentpBW - &string[0];
			}

			return true;
		}

		// if wrap turned off, we're done 
		if (!wrap) {
			return false;
		}

		// search from the beginning of the string to beginPos 
		if (compiledRE.execute(string, 0, beginPos, delimiters, false)) {

			*startPos = compiledRE.startp[0] - &string[0];
			*endPos = compiledRE.endp[0]     - &string[0];

			if(searchExtentFW) {
				*searchExtentFW = compiledRE.extentpFW - &string[0];
			}

			if(searchExtentBW) {
				*searchExtentBW = compiledRE.extentpBW - &string[0];
			}
			return true;
		}

		return false;
	} catch(const regex_error &e) {
		/* Note that this does not process errors from compiling the expression.
		 * It assumes that the expression was checked earlier.
		 */
		return false;
	}
}

static bool backwardRegexSearch(view::string_view string, view::string_view searchString, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {

	try {
		regexp compiledRE(searchString, defaultFlags);

		// search from beginPos to start of file.  A negative begin pos	
		// says begin searching from the far end of the file.		
		if (beginPos >= 0) {

			// TODO(eteran): why do we use '\0' as the previous char, and not string[beginPos - 1] (assuming that beginPos > 0)?
			if (compiledRE.execute(string, 0, beginPos, '\0', '\0', delimiters, true)) {

				*startPos = compiledRE.startp[0] - &string[0];
				*endPos   = compiledRE.endp[0]   - &string[0];

				if(searchExtentFW) {
					*searchExtentFW = compiledRE.extentpFW - &string[0];
				}

				if(searchExtentBW) {
					*searchExtentBW = compiledRE.extentpBW - &string[0];
				}

				return true;
			}
		}

		// if wrap turned off, we're done 
		if (!wrap) {
			return false;
		}

		// search from the end of the string to beginPos 
		if (beginPos < 0) {
			beginPos = 0;
		}

		if (compiledRE.execute(string, beginPos, delimiters, true)) {

			*startPos = compiledRE.startp[0] - &string[0];
			*endPos   = compiledRE.endp[0]   - &string[0];

			if(searchExtentFW) {
				*searchExtentFW = compiledRE.extentpFW - &string[0];
			}

			if(searchExtentBW) {
				*searchExtentBW = compiledRE.extentpBW - &string[0];
			}

			return true;
		}

		return false;
	} catch(const regex_error &e) {
		// NOTE(eteran): ignoring error!
		return false;
	}
}

static std::string upCaseStringEx(view::string_view inString) {


	std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
		return toupper((uint8_t)ch);
	});
	return str;
}

static std::string downCaseStringEx(view::string_view inString) {
	std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
		return tolower((uint8_t)ch);
	});
	return str;
}

/*
** Return TRUE if "searchString" exactly matches the text in the window's
** current primary selection using search algorithm "searchType".  If true,
** also return the position of the selection in "left" and "right".
*/
static bool searchMatchesSelection(Document *window, const char *searchString, SearchType searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW) {
	int selLen, selStart, selEnd, startPos, endPos, extentBW, extentFW, beginPos;
	int regexLookContext = isRegexType(searchType) ? 1000 : 0;
	std::string string;
	int rectStart, rectEnd, lineStart = 0;
	bool isRect;

	// find length of selection, give up on no selection or too long 
	if (!window->buffer_->BufGetEmptySelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		return false;
	}

	if (selEnd - selStart > SEARCHMAX) {
		return false;
	}

	// if the selection is rectangular, don't match if it spans lines 
	if (isRect) {
		lineStart = window->buffer_->BufStartOfLine(selStart);
		if (lineStart != window->buffer_->BufStartOfLine(selEnd)) {
			return false;
		}
	}

	/* get the selected text plus some additional context for regular
	   expression lookahead */
	if (isRect) {
		int stringStart = lineStart + rectStart - regexLookContext;
		if (stringStart < 0) {
			stringStart = 0;
		}

		string = window->buffer_->BufGetRangeEx(stringStart, lineStart + rectEnd + regexLookContext);
		selLen = rectEnd - rectStart;
		beginPos = lineStart + rectStart - stringStart;
	} else {
		int stringStart = selStart - regexLookContext;
		if (stringStart < 0) {
			stringStart = 0;
		}

		string = window->buffer_->BufGetRangeEx(stringStart, selEnd + regexLookContext);
		selLen = selEnd - selStart;
		beginPos = selStart - stringStart;
	}
	if (string.empty()) {
		return false;
	}

	// search for the string in the selection (we are only interested 	
	// in an exact match, but the procedure SearchString does important 
	// stuff like applying the correct matching algorithm)		
	bool found = SearchString(string, searchString, SEARCH_FORWARD, searchType, FALSE, beginPos, &startPos, &endPos, &extentBW, &extentFW, GetWindowDelimiters(window).toLatin1().data());

	// decide if it is an exact match 
	if (!found) {
		return false;
	}

	if (startPos != beginPos || endPos - beginPos != selLen) {
		return false;
	}

	// return the start and end of the selection 
	if (isRect) {
		window->buffer_->GetSimpleSelection(left, right);
	} else {
		*left  = selStart;
		*right = selEnd;
	}

	if(searchExtentBW) {
		*searchExtentBW = *left - (startPos - extentBW);
	}

	if(searchExtentFW) {
		*searchExtentFW = *right + extentFW - endPos;
	}

	return true;
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
static bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, char *destStr, int maxDestLen, int prevChar, const char *delimiters, int defaultFlags) {
	try {
		regexp compiledRE(searchStr, defaultFlags);
		compiledRE.execute(sourceStr, beginPos, sourceStr.size(), prevChar, '\0', delimiters, false);
		return compiledRE.SubstituteRE(replaceStr, destStr, maxDestLen);
	} catch(const regex_error &e) {
		// NOTE(eteran): ignoring error!
		return false;
	}
}

/*
** Store the search and replace strings, and search type for later recall.
** If replaceString is nullptr, duplicate the last replaceString used.
** Contiguous incremental searches share the same history entry (each new
** search modifies the current search string, until a non-incremental search
** is made.  To mark the end of an incremental search, call saveSearchHistory
** again with an empty search string and isIncremental==False.
*/
void saveSearchHistory(const char *searchString, const char *replaceString, SearchType searchType, int isIncremental) {
	char *sStr, *rStr;
	static int currentItemIsIncremental = FALSE;

	/* Cancel accumulation of contiguous incremental searches (even if the
	   information is not worthy of saving) if search is not incremental */
	if (!isIncremental)
		currentItemIsIncremental = FALSE;

	// Don't save empty search strings 
	if (searchString[0] == '\0')
		return;

	// If replaceString is nullptr, duplicate the last one (if any) 
	if(!replaceString)
		replaceString = NHist >= 1 ? ReplaceHistory[historyIndex(1)] : "";

	/* Compare the current search and replace strings against the saved ones.
	   If they are identical, don't bother saving */
	if (NHist >= 1 && searchType == SearchTypeHistory[historyIndex(1)] && !strcmp(SearchHistory[historyIndex(1)], searchString) && !strcmp(ReplaceHistory[historyIndex(1)], replaceString)) {
		return;
	}

	/* If the current history item came from an incremental search, and the
	   new one is also incremental, just update the entry */
	if (currentItemIsIncremental && isIncremental) {
		XtFree(SearchHistory[historyIndex(1)]);
		SearchHistory[historyIndex(1)] = XtNewStringEx(searchString);
		SearchTypeHistory[historyIndex(1)] = searchType;
		return;
	}
	currentItemIsIncremental = isIncremental;

	if (NHist == 0) {
		for(Document *w: WindowList) {
			if (w->IsTopDocument()) {
				XtSetSensitive(w->findAgainItem_, True);
				XtSetSensitive(w->replaceFindAgainItem_, True);
				XtSetSensitive(w->replaceAgainItem_, True);
			}
		}
	}

	/* If there are more than MAX_SEARCH_HISTORY strings saved, recycle
	   some space, free the entry that's about to be overwritten */
	if (NHist == MAX_SEARCH_HISTORY) {
		XtFree(SearchHistory[HistStart]);
		XtFree(ReplaceHistory[HistStart]);
	} else
		NHist++;

	/* Allocate and copy the search and replace strings and add them to the
	   circular buffers at HistStart, bump the buffer pointer to next pos. */
	sStr = XtStringDup(searchString);
	rStr = XtStringDup(replaceString);

	SearchHistory[HistStart] = sStr;
	ReplaceHistory[HistStart] = rStr;
	SearchTypeHistory[HistStart] = searchType;
	HistStart++;
	if (HistStart >= MAX_SEARCH_HISTORY)
		HistStart = 0;
}

/*
** return an index into the circular buffer arrays of history information
** for search strings, given the number of saveSearchHistory cycles back from
** the current time.
*/
int historyIndex(int nCycles) {
	int index;

	if (nCycles > NHist || nCycles <= 0)
		return -1;
	index = HistStart - nCycles;
	if (index < 0)
		index = MAX_SEARCH_HISTORY + index;
	return index;
}

/*
** Return a pointer to the string describing search type for search action
** routine parameters (see menu.c for processing of action routines)
*/
const char *searchTypeArg(SearchType searchType) {
	if (0 <= searchType && searchType < N_SEARCH_TYPES) {
		return searchTypeStrings[searchType];
	}
	return searchTypeStrings[SEARCH_LITERAL];
}

/*
** Return a pointer to the string describing search wrap for search action
** routine parameters (see menu.c for processing of action routines)
*/
const char *searchWrapArg(int searchWrap) {
	if (searchWrap) {
		return "wrap";
	}
	return "nowrap";
}

/*
** Return a pointer to the string describing search direction for search action
** routine parameters (see menu.c for processing of action routines)
*/
const char *directionArg(SearchDirection direction) {
	if (direction == SEARCH_BACKWARD) {
		return "backward";
	}
	
	return "forward";
}

/*
** Checks whether a search mode in one of the regular expression modes.
*/
int isRegexType(SearchType searchType) {
	return searchType == SEARCH_REGEX || searchType == SEARCH_REGEX_NOCASE;
}

/*
** Returns the default flags for regular expression matching, given a
** regular expression search mode.
*/
static int defaultRegexFlags(SearchType searchType) {
	switch (searchType) {
	case SEARCH_REGEX:
		return REDFLT_STANDARD;
	case SEARCH_REGEX_NOCASE:
		return REDFLT_CASE_INSENSITIVE;
	default:
		// We should never get here, but just in case ... 
		return REDFLT_STANDARD;
	}
}

/*
** The next 4 callbacks handle the states of find/replace toggle
** buttons, which depend on the state of the "Regex" button, and the
** sensitivity of the Whole Word buttons.
** Callbacks are necessary for both "Regex" and "Case Sensitive"
** buttons to make sure the states are saved even after a cancel operation.
**
** If sticky case sensitivity is requested, the behaviour is as follows:
**   The first time "Regular expression" is checked, "Match case" gets
**   checked too. Thereafter, checking or unchecking "Regular expression"
**   restores the "Match case" button to the setting it had the last
**   time when literals or REs where used.
** Without sticky behaviour, the state of the Regex button doesn't influence
** the state of the Case Sensitive button.
**
** Independently, the state of the buttons is always restored to the
** default state when a dialog is popped up, and when the user returns
** from stepping through the search history.
**
** NOTE: similar call-backs exist for the incremental search bar; see window.c.
*/
static void iSearchRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchRegex = XmToggleButtonGetState(w);
	int searchCaseSense = XmToggleButtonGetState(window->iSearchCaseToggle_);

	// In sticky mode, restore the state of the Case Sensitive button 
	if (GetPrefStickyCaseSenseBtn()) {
		if (searchRegex) {
			window->iSearchLastLiteralCase_ = searchCaseSense;
			XmToggleButtonSetState(window->iSearchCaseToggle_, window->iSearchLastRegexCase_, False);
		} else {
			window->iSearchLastRegexCase_ = searchCaseSense;
			XmToggleButtonSetState(window->iSearchCaseToggle_, window->iSearchLastLiteralCase_, False);
		}
	}
	// The iSearch bar has no Whole Word button to enable/disable. 
}

static void iSearchCaseToggleCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchCaseSense = XmToggleButtonGetState(w);

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (XmToggleButtonGetState(window->iSearchRegexToggle_))
		window->iSearchLastRegexCase_ = searchCaseSense;
	else
		window->iSearchLastLiteralCase_ = searchCaseSense;
}
