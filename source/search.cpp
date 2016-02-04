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
#include "MotifHelper.h"

#ifdef REPLACE_SCOPE
#include "textDisp.h"
#include "textP.h"
#endif

#include "../util/DialogF.h"
#include "../util/misc.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <sys/param.h>

#include <Xm/Xm.h>
#include <X11/Shell.h>
#include <Xm/XmP.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#ifdef REPLACE_SCOPE
#if XmVersion >= 1002
#include <Xm/PrimitiveP.h>
#endif
#endif

#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/List.h>
#include <X11/Xatom.h> /* for getting selection */
#include <X11/keysym.h>
#include <X11/X.h> /* " " */

int NHist = 0;

struct SelectionInfo {
	int done;
	Document *window;
	char *selection;
};

struct SearchSelectedCallData {
	SearchDirection direction;
	int searchType;
	int searchWrap;
};

/* History mechanism for search and replace strings */
static char *SearchHistory[MAX_SEARCH_HISTORY];
static char *ReplaceHistory[MAX_SEARCH_HISTORY];
static int SearchTypeHistory[MAX_SEARCH_HISTORY];
static int HistStart = 0;

static int textFieldNonEmpty(Widget w);
static void setTextField(Document *window, Time time, Widget textField);
static void getSelectionCB(Widget w, SelectionInfo *selectionInfo, Atom *selection, Atom *type, char *value, int *length, int *format);
static void fFocusCB(Widget w, XtPointer clientData, XtPointer callData);
static void rFocusCB(Widget w, XtPointer clientData, XtPointer callData);
static void rKeepCB(Widget w, XtPointer clientData, XtPointer callData);
static void fKeepCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceCB(Widget w, XtPointer clientData, XtPointer call_data);
static void replaceAllCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rInSelCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void fCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void rFindCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rFindTextValueChangedCB(Widget w, XtPointer clientData, XtPointer callData);
static void rFindArrowKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch);

static void rSetActionButtons(Document *window, int replaceBtn, int replaceFindBtn, int replaceAndFindBtn,
#ifndef REPLACE_SCOPE
                              int replaceInWinBtn, int replaceInSelBtn,
#endif
                              int replaceAllBtn);
#ifdef REPLACE_SCOPE
static void rScopeWinCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rScopeSelCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rScopeMultiCB(Widget w, XtPointer clientData, XtPointer call_data);
static void replaceAllScopeCB(Widget w, XtPointer clientData, XtPointer call_data);
#endif

static bool backwardRegexSearch(view::string_view string, view::string_view searchString, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static Boolean prefOrUserCancelsSubst(const Widget parent, const Display *display);
static bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, char *destStr, int maxDestLen, int prevChar, const char *delimiters, int defaultFlags);

static bool forwardRegexSearch(view::string_view string, view::string_view searchString, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool searchRegex(view::string_view string, view::string_view searchString, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static const char *directionArg(SearchDirection direction);
static const char *searchTypeArg(int searchType);
static const char *searchWrapArg(int searchWrap);
static int countWindows(void);
static int countWritableWindows(void);
static int defaultRegexFlags(int searchType);
static int findMatchingChar(Document *window, char toMatch, void *toMatchStyle, int charPos, int startLimit, int endLimit, int *matchPos);
static int getFindDlogInfoEx(Document *window, SearchDirection *direction, std::string *searchString, int *searchType);
static int getReplaceDlogInfo(Document *window, SearchDirection *direction, char *searchString, char *replaceString, int *searchType);
static int historyIndex(int nCycles);
static int isRegexType(int searchType);
static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW);
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, SearchDirection direction, bool wrap, int beginPos, int *startPos, int *endPos, const char *delimiters);
static bool searchMatchesSelection(Document *window, const char *searchString, int searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW);
static void checkMultiReplaceListForDoomedW(Document *window, Document *doomedWindow);
static void collectWritableWindows(Document *window);
static void eraseFlash(Document *window);
static void findArrowKeyCB(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch);
static void findCaseToggleCB(Widget w, XtPointer clientData, XtPointer callData);
static void findCB(Widget w, XtPointer clientData, XtPointer call_data);
static void findRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData);
static void findTextValueChangedCB(Widget w, XtPointer clientData, XtPointer callData);
static void flashTimeoutProc(XtPointer clientData, XtIntervalId *id);
static void freeWritableWindowsCB(Widget w, XtPointer clientData, XtPointer call_data);
static void fUpdateActionButtons(Document *window);
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
static void replaceArrowKeyCB(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch);
static void replaceCaseToggleCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceFindCB(Widget w, XtPointer clientData, XtPointer call_data);
static void replaceMultiFileCB(Widget w, XtPointer clientData, XtPointer call_data);
static void replaceRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData);
static void resetFindTabGroup(Document *window);
static void resetReplaceTabGroup(Document *window);
static void rMultiFileCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void rMultiFileDeselectAllCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rMultiFilePathCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rMultiFileReplaceCB(Widget w, XtPointer clientData, XtPointer call_data);
static void rMultiFileSelectAllCB(Widget w, XtPointer clientData, XtPointer call_data);
static void saveSearchHistory(const char *searchString, const char *replaceString, int searchType, int isIncremental);
static void selectedSearchCB(Widget w, XtPointer callData, Atom *selection, Atom *type, char *value, int *length, int *format);
static void unmanageReplaceDialogs(const Document *window);
static void uploadFileListItems(Document *window, Bool replace);
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
static const char *searchTypeStrings[] = {"literal",     /* SEARCH_LITERAL         */
                                          "case",        /* SEARCH_CASE_SENSE      */
                                          "regex",       /* SEARCH_REGEX           */
                                          "word",        /* SEARCH_LITERAL_WORD    */
                                          "caseWord",    /* SEARCH_CASE_SENSE_WORD */
                                          "regexNoCase", /* SEARCH_REGEX_NOCASE    */
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
static Document *windowNotToClose = nullptr;

Boolean WindowCanBeClosed(Document *window) {
	if (windowNotToClose && Document::GetTopDocument(window->shell_) == Document::GetTopDocument(windowNotToClose->shell_)) {
		return False;
	}
	return True; /* It's safe */
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
static void initToggleButtons(int searchType, Widget regexToggle, Widget caseToggle, Widget *wordToggle, Bool *lastLiteralCase, Bool *lastRegexCase) {
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
	}
}

#ifdef REPLACE_SCOPE
/*
** Checks whether a selection spans multiple lines. Used to decide on the
** default scope for replace dialogs.
** This routine introduces a dependency on textDisp.h, which is not so nice,
** but I currently don't have a cleaner solution.
*/
static int selectionSpansMultipleLines(Document *window) {
	int selStart, selEnd, isRect, rectStart, rectEnd, lineStartStart, lineStartEnd;
	int lineWidth;
	textDisp *textD;

	if (!window->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd))
		return FALSE;

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
	/* If the line starts differ, we have a "\n" in between. */
	if (lineStartStart != lineStartEnd)
		return TRUE;

	if (window->wrapMode_ != CONTINUOUS_WRAP)
		return FALSE; /* Same line */

	/* Estimate the number of characters on a line */
	textD = ((TextWidget)window->textArea_)->text.textD;
	if (textD->fontStruct->max_bounds.width > 0)
		lineWidth = textD->width / textD->fontStruct->max_bounds.width;
	else
		lineWidth = 1;
	if (lineWidth < 1)
		lineWidth = 1; /* Just in case */

	/* Estimate the numbers of line breaks from the start of the line to
	   the start and ending positions of the selection and compare.*/
	if ((selStart - lineStartStart) / lineWidth != (selEnd - lineStartStart) / lineWidth)
		return TRUE; /* Spans multiple lines */

	return FALSE; /* Small selection; probably doesn't span lines */
}
#endif

void DoFindReplaceDlog(Document *window, SearchDirection direction, int keepDialogs, int searchType, Time time) {

	/* Create the dialog if it doesn't already exist */
	if (window->replaceDlog_ == nullptr)
		CreateReplaceDlog(window->shell_, window);

	setTextField(window, time, window->replaceText_);

	/* If the window is already up, just pop it to the top */
	if (XtIsManaged(window->replaceDlog_)) {
		RaiseDialogWindow(XtParent(window->replaceDlog_));
		return;
	}

	/* Blank the Replace with field */
	XmTextSetStringEx(window->replaceWithText_, "");

	/* Set the initial search type */
	initToggleButtons(searchType, window->replaceRegexToggle_, window->replaceCaseToggle_, &window->replaceWordToggle_, &window->replaceLastLiteralCase_, &window->replaceLastRegexCase_);

	/* Set the initial direction based on the direction argument */
	XmToggleButtonSetState(window->replaceRevToggle_, direction == SEARCH_FORWARD ? False : True, True);

	/* Set the state of the Keep Dialog Up button */
	XmToggleButtonSetState(window->replaceKeepBtn_, keepDialogs, True);

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
			RadioButtonChangeState(window->replaceScopeSelToggle_, True, True);
			break;
		case REPL_DEF_SCOPE_SMART:
			if (selectionSpansMultipleLines(window)) {
				/* If the selection spans multiple lines, the user most
				   likely wants to perform a replacement in the selection */
				RadioButtonChangeState(window->replaceScopeSelToggle_, True, True);
			} else {
				/* It's unlikely that the user wants a replacement in a
				   tiny selection only. */
				RadioButtonChangeState(window->replaceScopeWinToggle_, True, True);
			}
			break;
		default:
			/* The user always wants window scope as default. */
			RadioButtonChangeState(window->replaceScopeWinToggle_, True, True);
			break;
		}
	} else {
		/* No selection -> always choose "In Window" as default. */
		RadioButtonChangeState(window->replaceScopeWinToggle_, True, True);
	}
#endif

	UpdateReplaceActionButtons(window);

	/* Start the search history mechanism at the current history item */
	window->rHistIndex_ = 0;

	/* Display the dialog */
	ManageDialogCenteredOnPointer(window->replaceDlog_);

	/* Workaround: LessTif (as of version 0.89) needs reminding of who had
	   the focus when the dialog was unmanaged.  When re-managed, focus is
	   lost and events fall through to the window below. */
	XmProcessTraversal(window->replaceText_, XmTRAVERSE_CURRENT);
}

static void setTextField(Document *window, Time time, Widget textField) {
	XEvent nextEvent;
	char *primary_selection = nullptr;
	auto selectionInfo = new SelectionInfo;

	if (GetPrefFindReplaceUsesSelection()) {
		selectionInfo->done      = 0;
		selectionInfo->window    = window;
		selectionInfo->selection = nullptr;
		XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)getSelectionCB, selectionInfo, time);

		while (selectionInfo->done == 0) {
			XtAppNextEvent(XtWidgetToApplicationContext(window->textArea_), &nextEvent);
			ServerDispatchEvent(&nextEvent);
		}

		primary_selection = selectionInfo->selection;
	}

	if (primary_selection == nullptr) {
		primary_selection = XtNewStringEx("");
	}

	/* Update the field */
	XmTextSetStringEx(textField, primary_selection);

	XtFree(primary_selection);

	delete selectionInfo;
}

static void getSelectionCB(Widget w, SelectionInfo *selectionInfo, Atom *selection, Atom *type, char *value, int *length, int *format) {

	(void)w;
	(void)selection;

	Document *window = selectionInfo->window;

	/* return an empty string if we can't get the selection data */
	if (*type == XT_CONVERT_FAIL || *type != XA_STRING || value == nullptr || *length == 0) {
		XtFree(value);
		selectionInfo->selection = nullptr;
		selectionInfo->done = 1;
		return;
	}
	/* return an empty string if the data is not of the correct format. */
	if (*format != 8) {
		DialogF(DF_WARN, window->shell_, 1, "Invalid Format", "NEdit can't handle non 8-bit text", "OK");
		XtFree(value);
		selectionInfo->selection = nullptr;
		selectionInfo->done = 1;
		return;
	}
	selectionInfo->selection = XtMalloc(*length + 1);
	memcpy(selectionInfo->selection, value, *length);
	selectionInfo->selection[*length] = 0;
	XtFree(value);
	selectionInfo->done = 1;
}

void DoFindDlog(Document *window, SearchDirection direction, int keepDialogs, int searchType, Time time) {

	/* Create the dialog if it doesn't already exist */
	if (window->findDlog_ == nullptr)
		CreateFindDlog(window->shell_, window);

	setTextField(window, time, window->findText_);

	/* If the window is already up, just pop it to the top */
	if (XtIsManaged(window->findDlog_)) {
		RaiseDialogWindow(XtParent(window->findDlog_));
		return;
	}

	/* Set the initial search type */
	initToggleButtons(searchType, window->findRegexToggle_, window->findCaseToggle_, &window->findWordToggle_, &window->findLastLiteralCase_, &window->findLastRegexCase_);

	/* Set the initial direction based on the direction argument */
	XmToggleButtonSetState(window->findRevToggle_, direction == SEARCH_FORWARD ? False : True, True);

	/* Set the state of the Keep Dialog Up button */
	XmToggleButtonSetState(window->findKeepBtn_, keepDialogs, True);

	/* Set the state of the Find button */
	fUpdateActionButtons(window);

	/* start the search history mechanism at the current history item */
	window->fHistIndex_ = 0;

	/* Display the dialog */
	ManageDialogCenteredOnPointer(window->findDlog_);

	/* Workaround: LessTif (as of version 0.89) needs reminding of who had
	   the focus when the dialog was unmanaged.  When re-managed, focus is
	   lost and events fall through to the window below. */
	XmProcessTraversal(window->findText_, XmTRAVERSE_CURRENT);
}

void DoReplaceMultiFileDlog(Document *window) {
	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;

	/* Validate and fetch the find and replace strings from the dialog */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Don't let the user select files when no replacement can be made */
	if (*searchString == '\0') {
		/* Set the initial focus of the dialog back to the search string */
		resetReplaceTabGroup(window);
		/* pop down the replace dialog */
		if (!XmToggleButtonGetState(window->replaceKeepBtn_))
			unmanageReplaceDialogs(window);
		return;
	}

	/* Create the dialog if it doesn't already exist */
	if (window->replaceMultiFileDlog_ == nullptr)
		CreateReplaceMultiFileDlog(window);

	/* Raising the window doesn't make sense. It is modal, so we
	   can't get here unless it is unmanaged */
	/* Prepare a list of writable windows */
	collectWritableWindows(window);

	/* Initialize/update the list of files. */
	uploadFileListItems(window, False);

	/* Display the dialog */
	ManageDialogCenteredOnPointer(window->replaceMultiFileDlog_);
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

	Document::for_each([doomedWindow](Document *w) {
		if (w->writableWindows_)
			/* A multi-file replacement dialog is up for this window */
			checkMultiReplaceListForDoomedW(w, doomedWindow);
	});
}

void CreateReplaceDlog(Widget parent, Document *window) {
	Arg args[50];
	int argcnt, defaultBtnOffset;
	XmString st1;
	Widget form, btnForm;
#ifdef REPLACE_SCOPE
	Widget scopeForm, replaceAllBtn;
#else
	Widget label3, allForm;
#endif
	Widget inWinBtn, inSelBtn, inMultiBtn;
	Widget searchTypeBox;
	Widget label2, label1, label, replaceText, findText;
	Widget findBtn, cancelBtn, replaceBtn;
	Widget replaceFindBtn;
	Widget searchDirBox, reverseBtn, keepBtn;
	char title[MAXPATHLEN + 19];
	Dimension shadowThickness;

	argcnt = 0;
	XtSetArg(args[argcnt], XmNautoUnmanage, False);
	argcnt++;
	form = CreateFormDialog(parent, "replaceDialog", args, argcnt);
	XtVaSetValues(form, XmNshadowThickness, 0, nullptr);
	if (GetPrefKeepSearchDlogs()) {
		sprintf(title, "Replace/Find (in %s)", window->filename_);
		XtVaSetValues(XtParent(form), XmNtitle, title, nullptr);
	} else
		XtVaSetValues(XtParent(form), XmNtitle, "Replace/Find", nullptr);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 4);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_BEGINNING);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("String to Find:"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 't');
	argcnt++;
	label1 = XmCreateLabel(form, (String) "label1", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label1);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_END);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("(use up arrow key to recall previous)"));
	argcnt++;
	label2 = XmCreateLabel(form, (String) "label2", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label2);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, label1);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNmaxLength, SEARCHMAX);
	argcnt++;
	findText = XmCreateText(form, (String) "replaceString", args, argcnt);
	XtAddCallback(findText, XmNfocusCallback, rFocusCB, window);
	XtAddCallback(findText, XmNvalueChangedCallback, rFindTextValueChangedCB, window);
	XtAddEventHandler(findText, KeyPressMask, False, rFindArrowKeyCB, window);
	RemapDeleteKey(findText);
	XtManageChild(findText);
	XmAddTabGroup(findText);
	XtVaSetValues(label1, XmNuserData, findText, nullptr); /* mnemonic processing */

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, findText);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_BEGINNING);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Replace With:"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'W');
	argcnt++;
	label = XmCreateLabel(form, (String) "label", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, label);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNmaxLength, SEARCHMAX);
	argcnt++;
	replaceText = XmCreateText(form, (String) "replaceWithString", args, argcnt);
	XtAddEventHandler(replaceText, KeyPressMask, False, replaceArrowKeyCB, window);
	RemapDeleteKey(replaceText);
	XtManageChild(replaceText);
	XmAddTabGroup(replaceText);
	XtVaSetValues(label, XmNuserData, replaceText, nullptr); /* mnemonic processing */

	argcnt = 0;
	XtSetArg(args[argcnt], XmNorientation, XmHORIZONTAL);
	argcnt++;
	XtSetArg(args[argcnt], XmNpacking, XmPACK_TIGHT);
	argcnt++;
	XtSetArg(args[argcnt], XmNmarginHeight, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, replaceText);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 4);
	argcnt++;
	searchTypeBox = XmCreateRowColumn(form, (String) "searchTypeBox", args, argcnt);
	XtManageChild(searchTypeBox);
	XmAddTabGroup(searchTypeBox);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Regular Expression"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'R');
	argcnt++;
	window->replaceRegexToggle_ = XmCreateToggleButton(searchTypeBox, (String) "regExp", args, argcnt);
	XmStringFree(st1);
	XtManageChild(window->replaceRegexToggle_);
	XtAddCallback(window->replaceRegexToggle_, XmNvalueChangedCallback, replaceRegExpToggleCB, window);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Case Sensitive"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'C');
	argcnt++;
	window->replaceCaseToggle_ = XmCreateToggleButton(searchTypeBox, (String) "caseSensitive", args, argcnt);
	XmStringFree(st1);
	XtManageChild(window->replaceCaseToggle_);
	XtAddCallback(window->replaceCaseToggle_, XmNvalueChangedCallback, replaceCaseToggleCB, window);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Whole Word"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'h');
	argcnt++;
	window->replaceWordToggle_ = XmCreateToggleButton(searchTypeBox, (String) "wholeWord", args, argcnt);
	XmStringFree(st1);
	XtManageChild(window->replaceWordToggle_);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNorientation, XmHORIZONTAL);
	argcnt++;
	XtSetArg(args[argcnt], XmNpacking, XmPACK_TIGHT);
	argcnt++;
	XtSetArg(args[argcnt], XmNmarginHeight, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchTypeBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNradioBehavior, False);
	argcnt++;
	searchDirBox = XmCreateRowColumn(form, (String) "searchDirBox", args, argcnt);
	XtManageChild(searchDirBox);
	XmAddTabGroup(searchDirBox);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Search Backward"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'B');
	argcnt++;
	reverseBtn = XmCreateToggleButton(searchDirBox, (String) "reverse", args, argcnt);
	XmStringFree(st1);
	XtManageChild(reverseBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Keep Dialog"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'K');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchTypeBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 4);
	argcnt++;
	keepBtn = XmCreateToggleButton(form, (String) "keep", args, argcnt);
	XtAddCallback(keepBtn, XmNvalueChangedCallback, rKeepCB, window);
	XmStringFree(st1);
	XtManageChild(keepBtn);
	XmAddTabGroup(keepBtn);

#ifdef REPLACE_SCOPE
	argcnt = 0;
	XtSetArg(args[argcnt], XmNorientation, XmHORIZONTAL);
	argcnt++;
	XtSetArg(args[argcnt], XmNpacking, XmPACK_TIGHT);
	argcnt++;
	XtSetArg(args[argcnt], XmNmarginHeight, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchDirBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNradioBehavior, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNradioAlwaysOne, True);
	argcnt++;
	scopeForm = XmCreateRowColumn(form, "scope", args, argcnt);
	XtManageChild(scopeForm);
	XmAddTabGroup(scopeForm);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("In Window"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'i');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	inWinBtn = XmCreateToggleButton(scopeForm, "inWindow", args, argcnt);
	XtAddCallback(inWinBtn, XmNvalueChangedCallback, rScopeWinCB, window);
	XmStringFree(st1);
	XtManageChild(inWinBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("In Selection"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'S');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, inWinBtn);
	argcnt++;
	inSelBtn = XmCreateToggleButton(scopeForm, "inSel", args, argcnt);
	XtAddCallback(inSelBtn, XmNvalueChangedCallback, rScopeSelCB, window);
	XmStringFree(st1);
	XtManageChild(inSelBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("In Multiple Documents"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'M');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, inSelBtn);
	argcnt++;
	inMultiBtn = XmCreateToggleButton(scopeForm, "multiFile", args, argcnt);
	XtAddCallback(inMultiBtn, XmNvalueChangedCallback, rScopeMultiCB, window);
	XmStringFree(st1);
	XtManageChild(inMultiBtn);
#else
	argcnt = 0;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchDirBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	allForm = XmCreateForm(form, (String) "all", args, argcnt);
	XtManageChild(allForm);
	XmAddTabGroup(allForm);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 4);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_BEGINNING);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Replace all in:"));
	argcnt++;
	label3 = XmCreateLabel(allForm, (String) "label3", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label3);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Window"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'i');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, label3);
	argcnt++;
	inWinBtn = XmCreatePushButton(allForm, (String) "inWindow", args, argcnt);
	XtAddCallback(inWinBtn, XmNactivateCallback, replaceAllCB, window);
	XmStringFree(st1);
	XtManageChild(inWinBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Selection"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'S');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, inWinBtn);
	argcnt++;
	inSelBtn = XmCreatePushButton(allForm, (String) "inSel", args, argcnt);
	XtAddCallback(inSelBtn, XmNactivateCallback, rInSelCB, window);
	XmStringFree(st1);
	XtManageChild(inSelBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Multiple Documents..."));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'M');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, inSelBtn);
	argcnt++;
	inMultiBtn = XmCreatePushButton(allForm, (String) "multiFile", args, argcnt);
	XtAddCallback(inMultiBtn, XmNactivateCallback, replaceMultiFileCB, window);
	XmStringFree(st1);
	XtManageChild(inMultiBtn);

#endif

	argcnt = 0;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
#ifdef REPLACE_SCOPE
	XtSetArg(args[argcnt], XmNtopWidget, scopeForm);
	argcnt++;
#else
	XtSetArg(args[argcnt], XmNtopWidget, allForm);
	argcnt++;
#endif
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	btnForm = XmCreateForm(form, (String) "buttons", args, argcnt);
	XtManageChild(btnForm);
	XmAddTabGroup(btnForm);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Replace"));
	argcnt++;
	XtSetArg(args[argcnt], XmNshowAsDefault, (short)1);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_FORM);
	argcnt++;
#ifdef REPLACE_SCOPE
	XtSetArg(args[argcnt], XmNleftPosition, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 21);
	argcnt++;
#else
	XtSetArg(args[argcnt], XmNleftPosition, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 25);
	argcnt++;
#endif
	replaceBtn = XmCreatePushButton(btnForm, (String) "replace", args, argcnt);
	XtAddCallback(replaceBtn, XmNactivateCallback, replaceCB, window);
	XmStringFree(st1);
	XtManageChild(replaceBtn);
	XtVaGetValues(replaceBtn, XmNshadowThickness, &shadowThickness, nullptr);
	defaultBtnOffset = shadowThickness + 4;

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Find"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'F');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
#ifdef REPLACE_SCOPE
	XtSetArg(args[argcnt], XmNleftPosition, 21);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 33);
	argcnt++;
#else
	XtSetArg(args[argcnt], XmNleftPosition, 25);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 42);
	argcnt++;
#endif
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomOffset, defaultBtnOffset);
	argcnt++;
	findBtn = XmCreatePushButton(btnForm, (String) "find", args, argcnt);
	XtAddCallback(findBtn, XmNactivateCallback, rFindCB, window);
	XmStringFree(st1);
	XtManageChild(findBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Replace & Find"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'n');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
#ifdef REPLACE_SCOPE
	XtSetArg(args[argcnt], XmNleftPosition, 33);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 62);
	argcnt++;
#else
	XtSetArg(args[argcnt], XmNleftPosition, 42);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 79);
	argcnt++;
#endif
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomOffset, defaultBtnOffset);
	argcnt++;
	replaceFindBtn = XmCreatePushButton(btnForm, (String) "replacefind", args, argcnt);
	XtAddCallback(replaceFindBtn, XmNactivateCallback, replaceFindCB, window);
	XmStringFree(st1);
	XtManageChild(replaceFindBtn);

#ifdef REPLACE_SCOPE
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Replace All"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'A');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftPosition, 62);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 85);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	replaceAllBtn = XmCreatePushButton(btnForm, "all", args, argcnt);
	XtAddCallback(replaceAllBtn, XmNactivateCallback, replaceAllScopeCB, window);
	XmStringFree(st1);
	XtManageChild(replaceAllBtn);
#endif

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Cancel"));
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
#ifdef REPLACE_SCOPE
	XtSetArg(args[argcnt], XmNleftPosition, 85);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 100);
	argcnt++;
#else
	XtSetArg(args[argcnt], XmNleftPosition, 79);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 100);
	argcnt++;
#endif
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomOffset, defaultBtnOffset);
	argcnt++;
	cancelBtn = XmCreatePushButton(btnForm, (String) "cancel", args, argcnt);
	XmStringFree(st1);
	XtAddCallback(cancelBtn, XmNactivateCallback, rCancelCB, window);
	XtManageChild(cancelBtn);

	XtVaSetValues(form, XmNcancelButton, cancelBtn, nullptr);
	AddDialogMnemonicHandler(form, FALSE);

	window->replaceDlog_ = form;
	window->replaceText_ = findText;
	window->replaceWithText_ = replaceText;
	window->replaceRevToggle_ = reverseBtn;
	window->replaceKeepBtn_ = keepBtn;
	window->replaceBtns_ = btnForm;
	window->replaceBtn_ = replaceBtn;
	window->replaceAndFindBtn_ = replaceFindBtn;
	window->replaceFindBtn_ = findBtn;
	window->replaceSearchTypeBox_ = searchTypeBox;
#ifdef REPLACE_SCOPE
	window->replaceAllBtn_ = replaceAllBtn;
	window->replaceScopeWinToggle_ = inWinBtn;
	window->replaceScopeSelToggle_ = inSelBtn;
	window->replaceScopeMultiToggle_ = inMultiBtn;
#else
	window->replaceInWinBtn_ = inWinBtn;
	window->replaceAllBtn_ = inMultiBtn;
	window->replaceInSelBtn_ = inSelBtn;
#endif
}

void CreateFindDlog(Widget parent, Document *window) {
	Arg args[50];
	int argcnt, defaultBtnOffset;
	XmString st1;
	Widget form, btnForm, searchTypeBox;
	Widget findText, label1, label2, cancelBtn, findBtn;
	Widget searchDirBox, reverseBtn, keepBtn;
	char title[MAXPATHLEN + 11];
	Dimension shadowThickness;

	argcnt = 0;
	XtSetArg(args[argcnt], XmNautoUnmanage, False);
	argcnt++;
	form = CreateFormDialog(parent, "findDialog", args, argcnt);
	XtVaSetValues(form, XmNshadowThickness, 0, nullptr);
	if (GetPrefKeepSearchDlogs()) {
		sprintf(title, "Find (in %s)", window->filename_);
		XtVaSetValues(XtParent(form), XmNtitle, title, nullptr);
	} else
		XtVaSetValues(XtParent(form), XmNtitle, "Find", nullptr);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_BEGINNING);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("String to Find:"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'S');
	argcnt++;
	label1 = XmCreateLabel(form, (String) "label1", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label1);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_END);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("(use up arrow key to recall previous)"));
	argcnt++;
	label2 = XmCreateLabel(form, (String) "label2", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label2);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, label1);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNmaxLength, SEARCHMAX);
	argcnt++;
	findText = XmCreateText(form, (String) "searchString", args, argcnt);
	XtAddCallback(findText, XmNfocusCallback, fFocusCB, window);
	XtAddCallback(findText, XmNvalueChangedCallback, findTextValueChangedCB, window);
	XtAddEventHandler(findText, KeyPressMask, False, findArrowKeyCB, window);
	RemapDeleteKey(findText);
	XtManageChild(findText);
	XmAddTabGroup(findText);
	XtVaSetValues(label1, XmNuserData, findText, nullptr); /* mnemonic processing */

	argcnt = 0;
	XtSetArg(args[argcnt], XmNorientation, XmHORIZONTAL);
	argcnt++;
	XtSetArg(args[argcnt], XmNpacking, XmPACK_TIGHT);
	argcnt++;
	XtSetArg(args[argcnt], XmNmarginHeight, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, findText);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 4);
	argcnt++;

	searchTypeBox = XmCreateRowColumn(form, (String) "searchTypeBox", args, argcnt);
	XtManageChild(searchTypeBox);
	XmAddTabGroup(searchTypeBox);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Regular Expression"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'R');
	argcnt++;
	window->findRegexToggle_ = XmCreateToggleButton(searchTypeBox, (String) "regExp", args, argcnt);
	XmStringFree(st1);
	XtManageChild(window->findRegexToggle_);
	XtAddCallback(window->findRegexToggle_, XmNvalueChangedCallback, findRegExpToggleCB, window);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Case Sensitive"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'C');
	argcnt++;
	window->findCaseToggle_ = XmCreateToggleButton(searchTypeBox, (String) "caseSensitive", args, argcnt);
	XmStringFree(st1);
	XtManageChild(window->findCaseToggle_);
	XtAddCallback(window->findCaseToggle_, XmNvalueChangedCallback, findCaseToggleCB, window);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Whole Word"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'h');
	argcnt++;
	window->findWordToggle_ = XmCreateToggleButton(searchTypeBox, (String) "wholeWord", args, argcnt);
	XmStringFree(st1);
	XtManageChild(window->findWordToggle_);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNorientation, XmHORIZONTAL);
	argcnt++;
	XtSetArg(args[argcnt], XmNpacking, XmPACK_TIGHT);
	argcnt++;
	XtSetArg(args[argcnt], XmNmarginHeight, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchTypeBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNradioBehavior, False);
	argcnt++;
	searchDirBox = XmCreateRowColumn(form, (String) "searchDirBox", args, argcnt);
	XtManageChild(searchDirBox);
	XmAddTabGroup(searchDirBox);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Search Backward"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'B');
	argcnt++;
	reverseBtn = XmCreateToggleButton(searchDirBox, (String) "reverse", args, argcnt);
	XmStringFree(st1);
	XtManageChild(reverseBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Keep Dialog"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'K');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchTypeBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 4);
	argcnt++;
	keepBtn = XmCreateToggleButton(form, (String) "keep", args, argcnt);
	XtAddCallback(keepBtn, XmNvalueChangedCallback, fKeepCB, window);
	XmStringFree(st1);
	XtManageChild(keepBtn);
	XmAddTabGroup(keepBtn);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, searchDirBox);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 4);
	argcnt++;
	btnForm = XmCreateForm(form, (String) "buttons", args, argcnt);
	XtManageChild(btnForm);
	XmAddTabGroup(btnForm);

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Find"));
	argcnt++;
	XtSetArg(args[argcnt], XmNshowAsDefault, (short)1);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftPosition, 20);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomOffset, 6);
	argcnt++;
	findBtn = XmCreatePushButton(btnForm, (String) "find", args, argcnt);
	XtAddCallback(findBtn, XmNactivateCallback, findCB, window);
	XmStringFree(st1);
	XtManageChild(findBtn);
	XtVaGetValues(findBtn, XmNshadowThickness, &shadowThickness, nullptr);
	defaultBtnOffset = shadowThickness + 4;

	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Cancel"));
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 80);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	cancelBtn = XmCreatePushButton(btnForm, (String) "cancel", args, argcnt);
	XtAddCallback(cancelBtn, XmNactivateCallback, fCancelCB, window);
	XmStringFree(st1);
	XtManageChild(cancelBtn);
	XtVaSetValues(form, XmNcancelButton, cancelBtn, nullptr);
	AddDialogMnemonicHandler(form, FALSE);

	window->findDlog_ = form;
	window->findText_ = findText;
	window->findRevToggle_ = reverseBtn;
	window->findKeepBtn_ = keepBtn;
	window->findBtns_ = btnForm;
	window->findBtn_ = findBtn;
	window->findSearchTypeBox_ = searchTypeBox;
}

void CreateReplaceMultiFileDlog(Document *window) {
	Arg args[50];
	int argcnt, defaultBtnOffset;
	XmString st1;
	Widget list, label1, form, pathBtn;
	Widget btnForm, replaceBtn, selectBtn, deselectBtn, cancelBtn;
	Dimension shadowThickness;

	argcnt = 0;
	XtSetArg(args[argcnt], XmNautoUnmanage, False);
	argcnt++;
	XtSetArg(args[argcnt], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
	argcnt++;

	/* Ideally, we should create the multi-file dialog as a child widget
	   of the replace dialog. However, if we do this, the main window
	   can hide the multi-file dialog when raised (I'm not sure why, but
	   it's something that I observed with fvwm). By using the main window
	   as the parent, it is possible that the replace dialog _partially_
	   covers the multi-file dialog, but this much better than the multi-file
	   dialog being covered completely by the main window */
	form = CreateFormDialog(window->shell_, (String) "replaceMultiFileDialog", args, argcnt);
	XtVaSetValues(form, XmNshadowThickness, 0, nullptr);
	XtVaSetValues(XtParent(form), XmNtitle, (String) "Replace All in Multiple Documents", nullptr);

	/* Label at top left. */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_NONE);
	argcnt++;
	/* Offset = 6 + (highlightThickness + detailShadowThickness) of the
	   toggle button (see below). Unfortunately, detailShadowThickness is
	   a Motif 2.x property, so we can't measure it. The default is 2 pixels.
	   To make things even more complicated, the SunOS 5.6 / Solaris 2.6
	   version of Motif 1.2 seems to use a detailShadowThickness of 0 ...
	   So we'll have to live with a slight misalignment on that platform
	   (those Motif libs are known to have many other problems). */
	XtSetArg(args[argcnt], XmNtopOffset, 10);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_BEGINNING);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Files in which to Replace All:"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'F');
	argcnt++;
	label1 = XmCreateLabel(form, (String) "label1", args, argcnt);
	XmStringFree(st1);
	XtManageChild(label1);

	/* Pathname toggle button at top right (always unset by default) */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNset, False);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNalignment, XmALIGNMENT_END);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Show Path Names"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'P');
	argcnt++;
	pathBtn = XmCreateToggleButton(form, (String) "path", args, argcnt);
	XmStringFree(st1);
	XtAddCallback(pathBtn, XmNvalueChangedCallback, rMultiFilePathCB, window);
	XtManageChild(pathBtn);

	/*
	 * Buttons at bottom. Place them before the list, such that we can
	 * attach the list to the label and the button box. In that way only
	 * the lists resizes vertically when the dialog is resized; users expect
	 * the list to resize, not the buttons.
	 */

	argcnt = 0;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNresizable, (short)0);
	argcnt++;
	btnForm = XmCreateForm(form, (String) "buttons", args, argcnt);
	XtManageChild(btnForm);

	/* Replace */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Replace"));
	argcnt++;
	XtSetArg(args[argcnt], XmNshowAsDefault, (short)1);
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'R');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftPosition, 0);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 25);
	argcnt++;
	replaceBtn = XmCreatePushButton(btnForm, (String) "replace", args, argcnt);
	XmStringFree(st1);
	XtAddCallback(replaceBtn, XmNactivateCallback, rMultiFileReplaceCB, window);
	/*
	 * _DON'T_ set the replace button as default (as in other dialogs).
	 * Multi-selection lists have the nasty property of selecting the
	 * current item when <enter> is pressed.
	 * In that way, the user could inadvertently select an additional file
	 * (most likely the last one that was deselected).
	 * The user has to activate the replace button explictly (either with
	 * a mouse click or with the shortcut key).
	 *
	 * XtVaSetValues(form, XmNdefaultButton, replaceBtn, nullptr); */

	XtManageChild(replaceBtn);
	XtVaGetValues(replaceBtn, XmNshadowThickness, &shadowThickness, nullptr);
	defaultBtnOffset = shadowThickness + 4;

	/* Select All */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Select All"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'S');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftPosition, 25);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 50);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	selectBtn = XmCreatePushButton(btnForm, (String) "select", args, argcnt);
	XmStringFree(st1);
	XtAddCallback(selectBtn, XmNactivateCallback, rMultiFileSelectAllCB, window);
	XtManageChild(selectBtn);

	/* Deselect All */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Deselect All"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'D');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftPosition, 50);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 75);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	deselectBtn = XmCreatePushButton(btnForm, (String) "deselect", args, argcnt);
	XmStringFree(st1);
	XtAddCallback(deselectBtn, XmNactivateCallback, rMultiFileDeselectAllCB, window);
	XtManageChild(deselectBtn);

	/* Cancel */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNhighlightThickness, 2);
	argcnt++;
	XtSetArg(args[argcnt], XmNlabelString, st1 = XmStringCreateLtoREx("Cancel"));
	argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'C');
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_NONE);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftPosition, 75);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightPosition, 100);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, defaultBtnOffset);
	argcnt++;
	cancelBtn = XmCreatePushButton(btnForm, (String) "cancel", args, argcnt);
	XmStringFree(st1);
	XtAddCallback(cancelBtn, XmNactivateCallback, rMultiFileCancelCB, window);
	XtManageChild(cancelBtn);

	/* The list of files */
	argcnt = 0;
	XtSetArg(args[argcnt], XmNtraversalOn, True);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomAttachment, XmATTACH_WIDGET);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomWidget, btnForm);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, label1);
	argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 10);
	argcnt++;
	XtSetArg(args[argcnt], XmNvisibleItemCount, 10);
	argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNbottomOffset, 6);
	argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 10);
	argcnt++;
	/* An alternative is to use the EXTENDED_SELECT, but that one
	   is less suited for keyboard manipulation (moving the selection cursor
	   with the keyboard deselects everything). */
	XtSetArg(args[argcnt], XmNselectionPolicy, XmMULTIPLE_SELECT);
	argcnt++;
	list = XmCreateScrolledList(form, (String) "list_of_files", args, argcnt);
	AddMouseWheelSupport(list);
	XtManageChild(list);

	/* Traverse: list -> buttons -> path name toggle button */
	XmAddTabGroup(list);
	XmAddTabGroup(btnForm);
	XmAddTabGroup(pathBtn);

	XtVaSetValues(label1, XmNuserData, list, nullptr); /* mnemonic processing */

	/* Cancel/Mnemonic stuff. */
	XtVaSetValues(form, XmNcancelButton, cancelBtn, nullptr);
	AddDialogMnemonicHandler(form, FALSE);

	window->replaceMultiFileDlog_ = form;
	window->replaceMultiFileList_ = list;
	window->replaceMultiFilePathBtn_ = pathBtn;

	/* Install a handler that frees the list of writable windows when
	   the dialog is unmapped. */
	XtAddCallback(form, XmNunmapCallback, freeWritableWindowsCB, window);
}

/*
** Iterates through the list of writable windows of a window, and removes
** the doomed window if necessary.
*/
static void checkMultiReplaceListForDoomedW(Document *window, Document *doomedWindow) {
	Document *w;
	int i;

	/* If the window owning the list and the doomed window are one and the
	   same, we just close the multi-file replacement dialog. */
	if (window == doomedWindow) {
		XtUnmanageChild(window->replaceMultiFileDlog_);
		return;
	}

	/* Check whether the doomed window is currently listed */
	for (i = 0; i < window->nWritableWindows_; ++i) {
		w = window->writableWindows_[i];
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
	int entriesToMove;

	/* If the list would become empty, we remove the dialog */
	if (window->nWritableWindows_ <= 1) {
		XtUnmanageChild(window->replaceMultiFileDlog_);
		return;
	}

	entriesToMove = window->nWritableWindows_ - index - 1;
	memmove(&(window->writableWindows_[index]), &(window->writableWindows_[index + 1]), (size_t)(entriesToMove * sizeof(Document *)));
	window->nWritableWindows_ -= 1;

	XmListDeletePos(window->replaceMultiFileList_, index + 1);
}

/*
** These callbacks fix a Motif 1.1 problem that the default button gets the
** keyboard focus when a dialog is created.  We want the first text field
** to get the focus, so we don't set the default button until the text field
** has the focus for sure.  I have tried many other ways and this is by far
** the least nasty.
*/
static void fFocusCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	window = Document::WidgetToWindow(w);
	SET_ONE_RSRC(window->findDlog_, XmNdefaultButton, window->findBtn_);
}
static void rFocusCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	window = Document::WidgetToWindow(w);
	SET_ONE_RSRC(window->replaceDlog_, XmNdefaultButton, window->replaceBtn_);
}

/* when keeping a window up, clue the user what window it's associated with */
static void rKeepCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	char title[MAXPATHLEN + 19];

	window = Document::WidgetToWindow(w);

	if (XmToggleButtonGetState(w)) {
		sprintf(title, "Replace/Find (in %s)", window->filename_);
		XtVaSetValues(XtParent(window->replaceDlog_), XmNtitle, title, nullptr);
	} else
		XtVaSetValues(XtParent(window->replaceDlog_), XmNtitle, "Replace/Find", nullptr);
}
static void fKeepCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	char title[MAXPATHLEN + 11];

	window = Document::WidgetToWindow(w);

	if (XmToggleButtonGetState(w)) {
		sprintf(title, "Find (in %s)", window->filename_);
		XtVaSetValues(XtParent(window->findDlog_), XmNtitle, title, nullptr);
	} else
		XtVaSetValues(XtParent(window->findDlog_), XmNtitle, "Find", nullptr);
}

static void replaceCB(Widget w, XtPointer clientData, XtPointer call_data) {
	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	const char *params[5];

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	window = Document::WidgetToWindow(w);

	/* Validate and fetch the find and replace strings from the dialog */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string */
	resetReplaceTabGroup(window);

	/* Find the text and replace it */
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = directionArg(direction);
	params[3] = searchTypeArg(searchType);
	params[4] = searchWrapArg(GetPrefSearchWraps());
	windowNotToClose = window;
	XtCallActionProc(window->lastFocus_, (String) "replace", callData->event, (char **)params, 5);
	windowNotToClose = nullptr;

	/* Pop down the dialog */
	if (!XmToggleButtonGetState(window->replaceKeepBtn_))
		unmanageReplaceDialogs(window);
}

static void replaceAllCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	const char *params[3];

	window = Document::WidgetToWindow(w);

	/* Validate and fetch the find and replace strings from the dialog */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string	*/
	resetReplaceTabGroup(window);

	/* do replacement */
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = searchTypeArg(searchType);
	windowNotToClose = window;
	XtCallActionProc(window->lastFocus_, (String) "replace_all", callData->event, (char **)params, 3);
	windowNotToClose = nullptr;

	/* pop down the dialog */
	if (!XmToggleButtonGetState(window->replaceKeepBtn_))
		unmanageReplaceDialogs(window);
}

static void replaceMultiFileCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);
	(void)callData;

	window = Document::WidgetToWindow(w);
	DoReplaceMultiFileDlog(window);
}

/*
** Callback that frees the list of windows the multi-file replace
** dialog is unmapped.
**/
static void freeWritableWindowsCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);
	(void)callData;

	window = Document::WidgetToWindow(w);
	delete [] window->writableWindows_;
	window->writableWindows_ = nullptr;
	window->nWritableWindows_ = 0;
}

/*
** Count no. of windows
*/
static int countWindows(void) {
	return Document::WindowCount();
}

/*
** Count no. of writable windows, but first update the status of all files.
*/
static int countWritableWindows(void) {
	int nAfter;

	int nBefore = countWindows();
	int nWritable = 0;
	
	for (Document *w = WindowList; w != nullptr; w = w->next_) {
	
		/* We must be very careful! The status check may trigger a pop-up
		   dialog when the file has changed on disk, and the user may destroy
		   arbitrary windows in response. */
		CheckForChangesToFile(w);
		nAfter = countWindows();
		if (nAfter != nBefore) {
			/* The user has destroyed a file; start counting all over again */
			nBefore = nAfter;
			w = WindowList;
			nWritable = 0;
			continue;
		}
		if (!IS_ANY_LOCKED(w->lockReasons_))
			++nWritable;
	}
	return nWritable;
}

/*
** Collects a list of writable windows (sorted by file name).
** The previous list, if any is freed first.
**/
static void collectWritableWindows(Document *window) {
	int nWritable = countWritableWindows();
	int i;
	Document **windows;

	delete [] window->writableWindows_;

	/* Make a sorted list of writable windows */
	windows = new Document*[nWritable];
	
	
	Document::for_each([&windows, &i](Document *w) {
		if (!IS_ANY_LOCKED(w->lockReasons_)) {
			windows[i++] = w;
		}
	});

	std::sort(windows, windows + nWritable, [](const Document *lhs, const Document *rhs) {
		return strcmp(lhs->filename_, rhs->filename_) < 0;
	});

	window->writableWindows_  = windows;
	window->nWritableWindows_ = nWritable;
}

static void rMultiFileReplaceCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	const char *params[4];
	int nSelected, i;
	Document *writableWin;
	Bool replaceFailed, noWritableLeft;

	window = Document::WidgetToWindow(w);
	nSelected = 0;
	for (i = 0; i < window->nWritableWindows_; ++i)
		if (XmListPosSelected(window->replaceMultiFileList_, i + 1))
			++nSelected;

	if (!nSelected) {
		DialogF(DF_INF, XtParent(window->replaceMultiFileDlog_), 1, "No Files", "No files selected!", "OK");
		return; /* Give the user another chance */
	}

	/* Set the initial focus of the dialog back to the search string */
	resetReplaceTabGroup(window);

	/*
	 * Protect the user against him/herself; Maybe this is a bit too much?
	 */
	if (DialogF(DF_QUES, window->shell_, 2, "Multi-File Replacement", "Multi-file replacements are difficult to undo.\n"
	                                                                 "Proceed with the replacement ?",
	            "Yes", "Cancel") != 1) {
		/* pop down the multi-file dialog only */
		XtUnmanageChild(window->replaceMultiFileDlog_);

		return;
	}

	/* Fetch the find and replace strings from the dialog;
	   they should have been validated already, but since Lesstif may not
	   honor modal dialogs, it is possible that the user modified the
	   strings again, so we should verify them again too. */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string */
	resetReplaceTabGroup(window);

	params[0] = searchString;
	params[1] = replaceString;
	params[2] = searchTypeArg(searchType);

	replaceFailed = True;
	noWritableLeft = True;
	/* Perform the replacements and mark the selected files (history) */
	for (i = 0; i < window->nWritableWindows_; ++i) {
		writableWin = window->writableWindows_[i];
		if (XmListPosSelected(window->replaceMultiFileList_, i + 1)) {
			/* First check again whether the file is still writable. If the
			   file status has changed or the file was locked in the mean time
			   (possible due to Lesstif modal dialog bug), we just skip the
			   window. */
			if (!IS_ANY_LOCKED(writableWin->lockReasons_)) {
				noWritableLeft = False;
				writableWin->multiFileReplSelected_ = True;
				writableWin->multiFileBusy_ = True; /* Avoid multi-beep/dialog */
				writableWin->replaceFailed_ = False;
				XtCallActionProc(writableWin->lastFocus_, "replace_all", callData->event, (char **)params, 3);
				writableWin->multiFileBusy_ = False;
				if (!writableWin->replaceFailed_)
					replaceFailed = False;
			}
		} else {
			writableWin->multiFileReplSelected_ = False;
		}
	}

	if (!XmToggleButtonGetState(window->replaceKeepBtn_)) {
		/* Pop down both replace dialogs. */
		unmanageReplaceDialogs(window);
	} else {
		/* pow down only the file selection dialog */
		XtUnmanageChild(window->replaceMultiFileDlog_);
	}

	/* We suppressed multiple beeps/dialogs. If there wasn't any file in
	   which the replacement succeeded, we should still warn the user */
	if (replaceFailed) {
		if (GetPrefSearchDlogs()) {
			if (noWritableLeft) {
				DialogF(DF_INF, window->shell_, 1, "Read-only Files", "All selected files have become read-only.", "OK");
			} else {
				DialogF(DF_INF, window->shell_, 1, "String not found", "String was not found", "OK");
			}
		} else {
			XBell(TheDisplay, 0);
		}
	}
}

static void rMultiFileCancelCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	window = Document::WidgetToWindow(w);

	/* Set the initial focus of the dialog back to the search string	*/
	resetReplaceTabGroup(window);

	/* pop down the multi-window replace dialog */
	XtUnmanageChild(window->replaceMultiFileDlog_);
}

static void rMultiFileSelectAllCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	(void)callData;

	int i;
	char policy;
	Widget list;

	window = Document::WidgetToWindow(w);
	list = window->replaceMultiFileList_;

	/*
	 * If the list is in extended selection mode, we can't select more
	 * than one item (probably because XmListSelectPos is equivalent
	 * to a button1 click; I don't think that there is an equivalent
	 * for CTRL-button1). Therefore, we temporarily put the list into
	 * multiple selection mode.
	 * Note: this is not really necessary if the list is in multiple select
	 *       mode all the time (as it currently is).
	 */
	XtVaGetValues(list, XmNselectionPolicy, &policy, nullptr);
	XtVaSetValues(list, XmNselectionPolicy, XmMULTIPLE_SELECT, nullptr);

	/* Is there no other way (like "select all") ? */
	XmListDeselectAllItems(window->replaceMultiFileList_); /* select toggles */

	for (i = 0; i < window->nWritableWindows_; ++i) {
		XmListSelectPos(list, i + 1, FALSE);
	}

	/* Restore the original policy. */
	XtVaSetValues(list, XmNselectionPolicy, policy, nullptr);
}

static void rMultiFileDeselectAllCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	(void)callData;

	window = Document::WidgetToWindow(w);
	XmListDeselectAllItems(window->replaceMultiFileList_);
}

static void rMultiFilePathCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	(void)callData;

	window = Document::WidgetToWindow(w);
	uploadFileListItems(window, True); /* Replace */
}

/*
 * Uploads the file items to the multi-file replament dialog list.
 * A boolean argument indicates whether the elements currently in the
 * list have to be replaced or not.
 * Depending on the state of the "Show path names" toggle button, either
 * the file names or the path names are listed.
 */
static void uploadFileListItems(Document *window, Bool replace) {

	int nWritable, i, *selected, selectedCount;
	char buf[MAXPATHLEN + 1], policy;
	Bool usePathNames;
	Document *w;
	Widget list;

	nWritable = window->nWritableWindows_;
	list = window->replaceMultiFileList_;

	auto names = new XmString[nWritable];

	usePathNames = XmToggleButtonGetState(window->replaceMultiFilePathBtn_);

	/* Note: the windows are sorted alphabetically by _file_ name. This
	         order is _not_ changed when we switch to path names. That
	         would be confusing for the user */

	for (i = 0; i < nWritable; ++i) {
		w = window->writableWindows_[i];
		if (usePathNames && window->filenameSet_) {
			sprintf(buf, "%s%s", w->path_, w->filename_);
		} else {
			sprintf(buf, "%s", w->filename_);
		}
		names[i] = XmStringCreateSimpleEx(buf);
	}

	/*
	 * If the list is in extended selection mode, we can't pre-select
	 * more than one item in (probably because XmListSelectPos is
	 * equivalent to a button1 click; I don't think that there is an
	 * equivalent for CTRL-button1). Therefore, we temporarily put the
	 * list into multiple selection mode.
	 */
	XtVaGetValues(list, XmNselectionPolicy, &policy, nullptr);
	XtVaSetValues(list, XmNselectionPolicy, XmMULTIPLE_SELECT, nullptr);
	if (replace) {
		/* Note: this function is obsolete in Motif 2.x, but it is available
		         for compatibility reasons */
		XmListGetSelectedPos(list, &selected, &selectedCount);

		XmListReplaceItemsPos(list, names, nWritable, 1);

		/* Maintain the selections */
		XmListDeselectAllItems(list);
		for (i = 0; i < selectedCount; ++i) {
			XmListSelectPos(list, selected[i], False);
		}

		XtFree((char *)selected);
	} else {
		Arg args[1];
		int nVisible;
		int firstSelected = 0;

		/* Remove the old list, if any */
		XmListDeleteAllItems(list);

		/* Initial settings */
		XmListAddItems(list, names, nWritable, 1);

		/* Pre-select the files from the last run. */
		selectedCount = 0;
		for (i = 0; i < nWritable; ++i) {
			if (window->writableWindows_[i]->multiFileReplSelected_) {
				XmListSelectPos(list, i + 1, False);
				++selectedCount;
				/* Remember the first selected item */
				if (firstSelected == 0)
					firstSelected = i + 1;
			}
		}
		/* If no files are selected, we select them all. Normally this only
		   happens the first time the dialog is used, but it looks "silly"
		   if the dialog pops up with nothing selected. */
		if (selectedCount == 0) {
			for (i = 0; i < nWritable; ++i) {
				XmListSelectPos(list, i + 1, False);
			}
			firstSelected = 1;
		}

		/* Make sure that the first selected item is visible; otherwise, the
		   user could get the impression that nothing is selected. By
		   visualizing at least the first selected item, the user will more
		   easily be confident that the previous selection is still active. */
		XtSetArg(args[0], XmNvisibleItemCount, &nVisible);
		XtGetValues(list, args, 1);
		/* Make sure that we don't create blank lines at the bottom by
		   positioning too far. */
		if (nWritable <= nVisible) {
			/* No need to shift the visible position */
			firstSelected = 1;
		} else {
			int maxFirst = nWritable - nVisible + 1;
			if (firstSelected > maxFirst)
				firstSelected = maxFirst;
		}
		XmListSetPos(list, firstSelected);
	}

	/* Put the list back into its original selection policy. */
	XtVaSetValues(list, XmNselectionPolicy, policy, nullptr);

	for (int i = 0; i < nWritable; ++i) {
		XmStringFree(names[i]);
	}
	
	delete [] names;
}

/*
** Unconditionally pops down the replace dialog and the
** replace-in-multiple-files dialog, if it exists.
*/
static void unmanageReplaceDialogs(const Document *window) {
	/* If the replace dialog goes down, the multi-file replace dialog must
	   go down too */
	if (window->replaceMultiFileDlog_ && XtIsManaged(window->replaceMultiFileDlog_)) {
		XtUnmanageChild(window->replaceMultiFileDlog_);
	}

	if (window->replaceDlog_ && XtIsManaged(window->replaceDlog_)) {
		XtUnmanageChild(window->replaceDlog_);
	}
}

static void rInSelCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	const char *params[3];

	window = Document::WidgetToWindow(w);

	/* Validate and fetch the find and replace strings from the dialog */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string */
	resetReplaceTabGroup(window);

	/* do replacement */
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = searchTypeArg(searchType);
	windowNotToClose = window;
	XtCallActionProc(window->lastFocus_, "replace_in_selection", callData->event, (char **)params, 3);
	windowNotToClose = nullptr;

	/* pop down the dialog */
	if (!XmToggleButtonGetState(window->replaceKeepBtn_))
		unmanageReplaceDialogs(window);
}

static void rCancelCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	window = Document::WidgetToWindow(w);

	/* Set the initial focus of the dialog back to the search string	*/
	resetReplaceTabGroup(window);

	/* pop down the dialog */
	unmanageReplaceDialogs(window);
}

static void fCancelCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	window = Document::WidgetToWindow(w);

	/* Set the initial focus of the dialog back to the search string	*/
	resetFindTabGroup(window);

	/* pop down the dialog */
	XtUnmanageChild(window->findDlog_);
}

static void rFindCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	const char *params[4];

	window = Document::WidgetToWindow(w);

	/* Validate and fetch the find and replace strings from the dialog */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string	*/
	resetReplaceTabGroup(window);

	/* Find the text and mark it */
	params[0] = searchString;
	params[1] = directionArg(direction);
	params[2] = searchTypeArg(searchType);
	params[3] = searchWrapArg(GetPrefSearchWraps());
	windowNotToClose = window;
	XtCallActionProc(window->lastFocus_, (String) "find", callData->event, (char **)params, 4);
	windowNotToClose = nullptr;

	/* Doctor the search history generated by the action to include the
	   replace string (if any), so the replace string can be used on
	   subsequent replaces, even though no actual replacement was done. */
	if (historyIndex(1) != -1 && !strcmp(SearchHistory[historyIndex(1)], searchString)) {
		XtFree(ReplaceHistory[historyIndex(1)]);
		ReplaceHistory[historyIndex(1)] = XtNewStringEx(replaceString);
	}

	/* Pop down the dialog */
	if (!XmToggleButtonGetState(window->replaceKeepBtn_))
		unmanageReplaceDialogs(window);
}

static void replaceFindCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	char searchString[SEARCHMAX + 1], replaceString[SEARCHMAX + 1];
	SearchDirection direction;
	int searchType;
	const char *params[4];

	window = Document::WidgetToWindow(w);

	/* Validate and fetch the find and replace strings from the dialog */
	if (!getReplaceDlogInfo(window, &direction, searchString, replaceString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string */
	resetReplaceTabGroup(window);

	/* Find the text and replace it */
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = directionArg(direction);
	params[3] = searchTypeArg(searchType);
	windowNotToClose = window;
	XtCallActionProc(window->lastFocus_, (String) "replace_find", callData->event, (char **)params, 4);
	windowNotToClose = nullptr;

	/* Pop down the dialog */
	if (!XmToggleButtonGetState(window->replaceKeepBtn_))
		unmanageReplaceDialogs(window);
}

static void rSetActionButtons(Document *window, int replaceBtn, int replaceFindBtn, int replaceAndFindBtn,
#ifndef REPLACE_SCOPE
                              int replaceInWinBtn, int replaceInSelBtn,
#endif
                              int replaceAllBtn) {
	XtSetSensitive(window->replaceBtn_, replaceBtn);
	XtSetSensitive(window->replaceFindBtn_, replaceFindBtn);
	XtSetSensitive(window->replaceAndFindBtn_, replaceAndFindBtn);
#ifndef REPLACE_SCOPE
	XtSetSensitive(window->replaceInWinBtn_, replaceInWinBtn);
	XtSetSensitive(window->replaceInSelBtn_, replaceInSelBtn);
#endif
	XtSetSensitive(window->replaceAllBtn_, replaceAllBtn);
}

void UpdateReplaceActionButtons(Document *window) {
	/* Is there any text in the search for field */
	int searchText = textFieldNonEmpty(window->replaceText_);
#ifdef REPLACE_SCOPE
	switch (window->replaceScope_) {
	case REPL_SCOPE_WIN:
		/* Enable all buttons, if there is any text in the search field. */
		rSetActionButtons(window, searchText, searchText, searchText, searchText);
		break;

	case REPL_SCOPE_SEL:
		/* Only enable Replace All, if a selection exists and text in search field. */
		rSetActionButtons(window, False, False, False, searchText && window->wasSelected_);
		break;

	case REPL_SCOPE_MULTI:
		/* Only enable Replace All, if text in search field. */
		rSetActionButtons(window, False, False, False, searchText);
		break;
	}
#else
	rSetActionButtons(window, searchText, searchText, searchText, searchText, searchText && window->wasSelected_, searchText && (countWritableWindows() > 1));
#endif
}

#ifdef REPLACE_SCOPE
/*
** The next 3 callback adapt the sensitivity of the replace dialog push
** buttons to the state of the scope radio buttons.
*/
static void rScopeWinCB(Widget w, XtPointer clientData, XtPointer call_data) {
	window = Document::WidgetToWindow(w);
	if (XmToggleButtonGetState(window->replaceScopeWinToggle_)) {
		window->replaceScope_ = REPL_SCOPE_WIN;
		UpdateReplaceActionButtons(window);
	}
}

static void rScopeSelCB(Widget w, XtPointer clientData, XtPointer call_data) {
	window = Document::WidgetToWindow(w);
	if (XmToggleButtonGetState(window->replaceScopeSelToggle_)) {
		window->replaceScope_ = REPL_SCOPE_SEL;
		UpdateReplaceActionButtons(window);
	}
}

static void rScopeMultiCB(Widget w, XtPointer clientData, XtPointer call_data) {
	window = Document::WidgetToWindow(w);
	if (XmToggleButtonGetState(window->replaceScopeMultiToggle_)) {
		window->replaceScope_ = REPL_SCOPE_MULTI;
		UpdateReplaceActionButtons(window);
	}
}

/*
** This routine dispatches a push on the replace-all button to the appropriate
** callback, depending on the state of the scope radio buttons.
*/
static void replaceAllScopeCB(Widget w, XtPointer clientData, XtPointer call_data) {
	window = Document::WidgetToWindow(w);
	switch (window->replaceScope_) {
	case REPL_SCOPE_WIN:
		replaceAllCB(w, window, callData);
		break;
	case REPL_SCOPE_SEL:
		rInSelCB(w, window, callData);
		break;
	case REPL_SCOPE_MULTI:
		replaceMultiFileCB(w, window, callData);
		break;
	}
}
#endif

static int textFieldNonEmpty(Widget w) {
	char *str = XmTextGetString(w);
	int nonEmpty = (str[0] != '\0');
	XtFree(str);
	return (nonEmpty);
}

static void rFindTextValueChangedCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto event = (XKeyEvent *)callData;
	auto window = static_cast<Document *>(clientData);

	(void)event;

	window = Document::WidgetToWindow(w);
	UpdateReplaceActionButtons(window);
}

static void rFindArrowKeyCB(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch) {

	(void)continueDispatch;

	auto event = (XKeyEvent *)Event;
	auto window = static_cast<Document *>(clientData);

	KeySym keysym = XLookupKeysym(event, 0);
	int index;
	const char *searchStr;
	const char *replaceStr;
	int searchType;

	window = Document::WidgetToWindow(w);
	index = window->rHistIndex_;

	/* only process up and down arrow keys */
	if (keysym != XK_Up && keysym != XK_Down)
		return;

	/* increment or decrement the index depending on which arrow was pressed */
	index += (keysym == XK_Up) ? 1 : -1;

	/* if the index is out of range, beep and return */
	if (index != 0 && historyIndex(index) == -1) {
		XBell(TheDisplay, 0);
		return;
	}

	window = Document::WidgetToWindow(w);

	/* determine the strings and button settings to use */
	if (index == 0) {
		searchStr = "";
		replaceStr = "";
		searchType = GetPrefSearch();
	} else {
		searchStr = SearchHistory[historyIndex(index)];
		replaceStr = ReplaceHistory[historyIndex(index)];
		searchType = SearchTypeHistory[historyIndex(index)];
	}

	/* Set the buttons and fields with the selected search type */
	initToggleButtons(searchType, window->replaceRegexToggle_, window->replaceCaseToggle_, &window->replaceWordToggle_, &window->replaceLastLiteralCase_, &window->replaceLastRegexCase_);

	XmTextSetStringEx(window->replaceText_, (String)searchStr);
	XmTextSetStringEx(window->replaceWithText_, (String)replaceStr);

	/* Set the state of the Replace, Find ... buttons */
	UpdateReplaceActionButtons(window);

	window->rHistIndex_ = index;
}

static void replaceArrowKeyCB(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch) {

	(void)continueDispatch;

	auto event = (XKeyEvent *)Event;
	auto window = static_cast<Document *>(clientData);

	KeySym keysym = XLookupKeysym(event, 0);
	int index;

	window = Document::WidgetToWindow(w);
	index = window->rHistIndex_;

	/* only process up and down arrow keys */
	if (keysym != XK_Up && keysym != XK_Down)
		return;

	/* increment or decrement the index depending on which arrow was pressed */
	index += (keysym == XK_Up) ? 1 : -1;

	/* if the index is out of range, beep and return */
	if (index != 0 && historyIndex(index) == -1) {
		XBell(TheDisplay, 0);
		return;
	}

	window = Document::WidgetToWindow(w);

	/* change only the replace field information */
	if (index == 0)
		XmTextSetStringEx(window->replaceWithText_, (String) "");
	else
		XmTextSetStringEx(window->replaceWithText_, ReplaceHistory[historyIndex(index)]);
	window->rHistIndex_ = index;
}

static void fUpdateActionButtons(Document *window) {
	int buttonState = textFieldNonEmpty(window->findText_);
	XtSetSensitive(window->findBtn_, buttonState);
}

static void findTextValueChangedCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto event = (XKeyEvent *)callData;
	auto window = static_cast<Document *>(clientData);

	(void)event;

	window = Document::WidgetToWindow(w);
	fUpdateActionButtons(window);
}

static void findArrowKeyCB(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch) {

	(void)continueDispatch;

	auto event = (XKeyEvent *)Event;
	auto window = static_cast<Document *>(clientData);

	KeySym keysym = XLookupKeysym(event, 0);
	int index;
	const char *searchStr;
	int searchType;

	window = Document::WidgetToWindow(w);
	index = window->fHistIndex_;

	/* only process up and down arrow keys */
	if (keysym != XK_Up && keysym != XK_Down)
		return;

	/* increment or decrement the index depending on which arrow was pressed */
	index += (keysym == XK_Up) ? 1 : -1;

	/* if the index is out of range, beep and return */
	if (index != 0 && historyIndex(index) == -1) {
		XBell(TheDisplay, 0);
		return;
	}

	/* determine the strings and button settings to use */
	if (index == 0) {
		searchStr = "";
		searchType = GetPrefSearch();
	} else {
		searchStr = SearchHistory[historyIndex(index)];
		searchType = SearchTypeHistory[historyIndex(index)];
	}

	/* Set the buttons and fields with the selected search type */
	initToggleButtons(searchType, window->findRegexToggle_, window->findCaseToggle_, &window->findWordToggle_, &window->findLastLiteralCase_, &window->findLastRegexCase_);
	XmTextSetStringEx(window->findText_, (String)searchStr);

	/* Set the state of the Find ... button */
	fUpdateActionButtons(window);

	window->fHistIndex_ = index;
}

static void findCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);

	SearchDirection direction;
	int searchType;
	const char *params[4];

	window = Document::WidgetToWindow(w);

	/* fetch find string, direction and type from the dialog */
	std::string searchString;
	if (!getFindDlogInfoEx(window, &direction, &searchString, &searchType))
		return;

	/* Set the initial focus of the dialog back to the search string	*/
	resetFindTabGroup(window);

	/* find the text and mark it */
	params[0] = searchString.c_str(); // TODO(eteran): is this OK?
	params[1] = directionArg(direction);
	params[2] = searchTypeArg(searchType);
	params[3] = searchWrapArg(GetPrefSearchWraps());
	
	windowNotToClose = window;
	XtCallActionProc(window->lastFocus_, (String) "find", callData->event, (char **)params, 4);
	windowNotToClose = nullptr;

	/* pop down the dialog */
	if (!XmToggleButtonGetState(window->findKeepBtn_))
		XtUnmanageChild(window->findDlog_);
}

/*
** Fetch and verify (particularly regular expression) search and replace
** strings and search type from the Replace dialog.  If the strings are ok,
** save a copy in the search history, copy them in to "searchString",
** "replaceString', which are assumed to be at least SEARCHMAX in length,
** return search type in "searchType", and return TRUE as the function
** value.  Otherwise, return FALSE.
*/
static int getReplaceDlogInfo(Document *window, SearchDirection *direction, char *searchString, char *replaceString, int *searchType) {
	regexp *compiledRE = nullptr;

	/* Get the search and replace strings, search type, and direction
	   from the dialog */
	char *replaceText     = XmTextGetString(window->replaceText_);
	char *replaceWithText = XmTextGetString(window->replaceWithText_);

	if (XmToggleButtonGetState(window->replaceRegexToggle_)) {
		int regexDefault;
		if (XmToggleButtonGetState(window->replaceCaseToggle_)) {
			*searchType = SEARCH_REGEX;
			regexDefault = REDFLT_STANDARD;
		} else {
			*searchType = SEARCH_REGEX_NOCASE;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			compiledRE = new regexp(replaceText, regexDefault);
		} catch(const regex_error &e) {
			DialogF(DF_WARN, XtParent(window->replaceDlog_), 1, "Search String", "Please respecify the search string:\n%s", "OK", e.what());
			XtFree(replaceText);
			XtFree(replaceWithText);
			return FALSE;
		}
		delete compiledRE;
	} else {
		if (XmToggleButtonGetState(window->replaceCaseToggle_)) {
			if (XmToggleButtonGetState(window->replaceWordToggle_))
				*searchType = SEARCH_CASE_SENSE_WORD;
			else
				*searchType = SEARCH_CASE_SENSE;
		} else {
			if (XmToggleButtonGetState(window->replaceWordToggle_))
				*searchType = SEARCH_LITERAL_WORD;
			else
				*searchType = SEARCH_LITERAL;
		}
	}

	*direction = XmToggleButtonGetState(window->replaceRevToggle_) ? SEARCH_BACKWARD : SEARCH_FORWARD;

	/* Return strings */
	if (strlen(replaceText) >= SEARCHMAX) {
		DialogF(DF_WARN, XtParent(window->replaceDlog_), 1, "String too long", "Search string too long.", "OK");
		XtFree(replaceText);
		XtFree(replaceWithText);
		return FALSE;
	}
	if (strlen(replaceWithText) >= SEARCHMAX) {
		DialogF(DF_WARN, XtParent(window->replaceDlog_), 1, "String too long", "Replace string too long.", "OK");
		XtFree(replaceText);
		XtFree(replaceWithText);
		return FALSE;
	}
	strcpy(searchString, replaceText);
	strcpy(replaceString, replaceWithText);
	XtFree(replaceText);
	XtFree(replaceWithText);
	return TRUE;
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** which is assumed to be at least SEARCHMAX in length, return search type
** in "searchType", and return TRUE as the function value.  Otherwise,
** return FALSE.
*/
static int getFindDlogInfoEx(Document *window, SearchDirection *direction, std::string *searchString, int *searchType) {

	regexp *compiledRE = nullptr;

	/* Get the search string, search type, and direction from the dialog */
	std::string findText = *XmTextGetStringEx(window->findText_);

	if (XmToggleButtonGetState(window->findRegexToggle_)) {
		int regexDefault;
		if (XmToggleButtonGetState(window->findCaseToggle_)) {
			*searchType = SEARCH_REGEX;
			regexDefault = REDFLT_STANDARD;
		} else {
			*searchType = SEARCH_REGEX_NOCASE;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			compiledRE = new regexp(findText, regexDefault);
		} catch(const regex_error &e) {
			DialogF(DF_WARN, XtParent(window->findDlog_), 1, "Regex Error", "Please respecify the search string:\n%s", "OK", e.what());
			return FALSE;
		}
		delete compiledRE;
	} else {
		if (XmToggleButtonGetState(window->findCaseToggle_)) {
			if (XmToggleButtonGetState(window->findWordToggle_))
				*searchType = SEARCH_CASE_SENSE_WORD;
			else
				*searchType = SEARCH_CASE_SENSE;
		} else {
			if (XmToggleButtonGetState(window->findWordToggle_))
				*searchType = SEARCH_LITERAL_WORD;
			else
				*searchType = SEARCH_LITERAL;
		}
	}

	*direction = XmToggleButtonGetState(window->findRevToggle_) ? SEARCH_BACKWARD : SEARCH_FORWARD;

	if (isRegexType(*searchType)) {
	}

	/* Return the search string */
	if (findText.size() >= SEARCHMAX) {
		DialogF(DF_WARN, XtParent(window->findDlog_), 1, "String too long", "Search string too long.", "OK");
		return FALSE;
	}
	
	*searchString = findText;
	return TRUE;
}

bool SearchAndSelectSame(Document *window, SearchDirection direction, int searchWrap) {
	if (NHist < 1) {
		XBell(TheDisplay, 0);
		return FALSE;
	}

	return SearchAndSelect(window, direction, SearchHistory[historyIndex(1)], SearchTypeHistory[historyIndex(1)], searchWrap);
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  Also
** adds the search string to the global search history.
*/
bool SearchAndSelect(Document *window, SearchDirection direction, const char *searchString, int searchType, int searchWrap) {
	int startPos, endPos;
	int beginPos, cursorPos, selStart, selEnd;
	int movedFwd = 0;

	/* Save a copy of searchString in the search history */
	saveSearchHistory(searchString, nullptr, searchType, FALSE);

	/* set the position to start the search so we don't find the same
	   string that was found on the last search	*/
	if (searchMatchesSelection(window, searchString, searchType, &selStart, &selEnd, nullptr, nullptr)) {
		/* selection matches search string, start before or after sel.	*/
		if (direction == SEARCH_BACKWARD) {
			beginPos = selStart - 1;
		} else {
			beginPos = selStart + 1;
			movedFwd = 1;
		}
	} else {
		selStart = -1;
		selEnd = -1;
		/* no selection, or no match, search relative cursor */
		cursorPos = TextGetCursorPos(window->lastFocus_);
		if (direction == SEARCH_BACKWARD) {
			/* use the insert position - 1 for backward searches */
			beginPos = cursorPos - 1;
		} else {
			/* use the insert position for forward searches */
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

	/* do the search.  SearchWindow does appropriate dialogs and beeps */
	if (!SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr))
		return FALSE;

	/* if the search matched an empty string (possible with regular exps)
	   beginning at the start of the search, go to the next occurrence,
	   otherwise repeated finds will get "stuck" at zero-length matches */
	if (direction == SEARCH_FORWARD && beginPos == startPos && beginPos == endPos) {
		if (!movedFwd && !SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr))
			return FALSE;
	}

	/* if matched text is already selected, just beep */
	if (selStart == startPos && selEnd == endPos) {
		XBell(TheDisplay, 0);
		return FALSE;
	}

	/* select the text found string */
	window->buffer_->BufSelect(startPos, endPos);
	window->MakeSelectionVisible(window->lastFocus_);
	TextSetCursorPos(window->lastFocus_, endPos);

	return TRUE;
}

void SearchForSelected(Document *window, SearchDirection direction, int searchType, int searchWrap, Time time) {
	SearchSelectedCallData *callData = XtNew(SearchSelectedCallData);
	callData->direction = direction;
	callData->searchType = searchType;
	callData->searchWrap = searchWrap;
	XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)selectedSearchCB, callData, time);
}

static void selectedSearchCB(Widget w, XtPointer callData, Atom *selection, Atom *type, char *value, int *length, int *format) {

	(void)selection;

	Document *window = Document::WidgetToWindow(w);
	SearchSelectedCallData *callDataItems = (SearchSelectedCallData *)callData;
	int searchType;
	char searchString[SEARCHMAX + 1];

	window = Document::WidgetToWindow(w);

	/* skip if we can't get the selection data or it's too long */
	if (*type == XT_CONVERT_FAIL || value == nullptr) {
		if (GetPrefSearchDlogs())
			DialogF(DF_WARN, window->shell_, 1, "Wrong Selection", "Selection not appropriate for searching", "OK");
		else
			XBell(TheDisplay, 0);
		XtFree((char *)callData);
		return;
	}
	if (*length > SEARCHMAX) {
		if (GetPrefSearchDlogs())
			DialogF(DF_WARN, window->shell_, 1, "Selection too long", "Selection too long", "OK");
		else
			XBell(TheDisplay, 0);
		XtFree(value);
		XtFree((char *)callData);
		return;
	}
	if (*length == 0) {
		XBell(TheDisplay, 0);
		XtFree(value);
		XtFree((char *)callData);
		return;
	}
	/* should be of type text??? */
	if (*format != 8) {
		fprintf(stderr, "NEdit: can't handle non 8-bit text\n");
		XBell(TheDisplay, 0);
		XtFree(value);
		XtFree((char *)callData);
		return;
	}
	/* make the selection the current search string */
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

	/* search for it in the window */
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

	/* Forget the starting position used for the current run of searches */
	window->iSearchStartPos_ = -1;

	/* Mark the end of incremental search history overwriting */
	saveSearchHistory("", nullptr, 0, FALSE);

	/* Pop down the search line (if it's not pegged up in Preferences) */
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
bool SearchAndSelectIncremental(Document *window, SearchDirection direction, const char *searchString, int searchType, int searchWrap, int continued) {
	int beginPos, startPos, endPos;

	/* If there's a search in progress, start the search from the original
	   starting position, otherwise search from the cursor position. */
	if (!continued || window->iSearchStartPos_ == -1) {
		window->iSearchStartPos_ = TextGetCursorPos(window->lastFocus_);
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
		TextSetCursorPos(window->lastFocus_, beginPos);
		return true;
	}

	/* Save the string in the search history, unless we're cycling thru
	   the search history itself, which can be detected by matching the
	   search string with the search string of the current history index. */
	if (!(window->iSearchHistIndex_ > 1 && !strcmp(searchString, SearchHistory[historyIndex(window->iSearchHistIndex_)]))) {
		saveSearchHistory(searchString, nullptr, searchType, TRUE);
		/* Reset the incremental search history pointer to the beginning */
		window->iSearchHistIndex_ = 1;
	}

	/* begin at insert position - 1 for backward searches */
	if (direction == SEARCH_BACKWARD)
		beginPos--;

	/* do the search.  SearchWindow does appropriate dialogs and beeps */
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

	/* select the text found string */
	window->buffer_->BufSelect(startPos, endPos);
	window->MakeSelectionVisible(window->lastFocus_);
	TextSetCursorPos(window->lastFocus_, endPos);

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
		/* make sure actions are loaded */
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

	/* When search parameters (direction or search type), redo the search */
	XtAddCallback(window->iSearchCaseToggle_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);
	XtAddCallback(window->iSearchRegexToggle_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);
	XtAddCallback(window->iSearchRevToggle_, XmNvalueChangedCallback, iSearchTextValueChangedCB, window);

	/* find button: just like pressing return */
	XtAddCallback(window->iSearchFindButton_, XmNactivateCallback, iSearchTextActivateCB, window);
	/* clear button: empty the search text widget */
	XtAddCallback(window->iSearchClearButton_, XmNactivateCallback, iSearchTextClearCB, window);
}

/*
** Remove callbacks before resetting the incremental search text to avoid any
** cursor movement and/or clearing of selections.
*/
static void iSearchTextSetString(Widget w, Document *window, const std::string &str) {

	(void)w;

	/* remove callbacks which would be activated by emptying the text */
	XtRemoveAllCallbacks(window->iSearchText_, XmNvalueChangedCallback);
	XtRemoveAllCallbacks(window->iSearchText_, XmNactivateCallback);
	/* empty the text */
	XmTextSetStringEx(window->iSearchText_, str);
	/* put back the callbacks */
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

	nullable_string selText = GetAnySelectionEx(window);
	
	iSearchTextSetString(w, window, selText ? *selText : "");
	
	if (selText) {
		XmTextSetInsertionPosition(window->iSearchText_, selText->size());
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
	char *searchString;
	int searchType;
	SearchDirection direction;

	window = Document::WidgetToWindow(w);

	/* Fetch the string, search type and direction from the incremental
	   search bar widgets at the top of the window */
	searchString = XmTextGetString(window->iSearchText_);
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

	/* Reverse the search direction if the Ctrl or Shift key was pressed */
	if (callData->event->xbutton.state & (ShiftMask | ControlMask))
		direction = direction == SEARCH_FORWARD ? SEARCH_BACKWARD : SEARCH_FORWARD;

	/* find the text and mark it */
	params[0] = searchString;
	params[1] = directionArg(direction);
	params[2] = searchTypeArg(searchType);
	params[3] = searchWrapArg(GetPrefSearchWraps());
	XtCallActionProc(window->lastFocus_, (String) "find", callData->event, (char **)params, 4);
	XtFree(searchString);
}

/*
** Called when user types in the incremental search line.  Redoes the
** search for the new search string.
*/
static void iSearchTextValueChangedCB(Widget w, XtPointer clientData, XtPointer call_data) {

	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<XmAnyCallbackStruct *>(call_data);
	
	const char *params[5];
	int searchType;
	SearchDirection direction;
	int nParams;

	window = Document::WidgetToWindow(w);

	/* Fetch the string, search type and direction from the incremental
	   search bar widgets at the top of the window */
	std::string searchString = *XmTextGetStringEx(window->iSearchText_);
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
		regexp *compiledRE = nullptr;
		try {
			compiledRE = new regexp(searchString, defaultRegexFlags(searchType));
		} catch(const regex_error &e) {
			return;
		}
		delete compiledRE;
	}

	/* Call the incremental search action proc to do the searching and
	   selecting (this allows it to be recorded for learn/replay).  If
	   there's an incremental search already in progress, mark the operation
	   as "continued" so the search routine knows to re-start the search
	   from the original starting position */
	nParams = 0;
	params[nParams++] = searchString.c_str(); // TODO(eteran): is this OK?
	params[nParams++] = directionArg(direction);
	params[nParams++] = searchTypeArg(searchType);
	params[nParams++] = searchWrapArg(GetPrefSearchWraps());
	if (window->iSearchStartPos_ != -1) {
		params[nParams++] = "continued";
	}
	
	XtCallActionProc(window->lastFocus_, (String) "find_incremental", callData->event, (char **)params, nParams);
}

/*
** Process arrow keys for history recall, and escape key for leaving
** incremental search bar.
*/
static void iSearchTextKeyEH(Widget w, XtPointer clientData, XEvent *Event, Boolean *continueDispatch) {


	auto event = (XKeyEvent *)Event;
	auto window = static_cast<Document *>(clientData);

	KeySym keysym = XLookupKeysym(event, 0);
	int index;
	const char *searchStr;
	int searchType;

	/* only process up and down arrow keys */
	if (keysym != XK_Up && keysym != XK_Down && keysym != XK_Escape) {
		*continueDispatch = TRUE;
		return;
	}

	window = Document::WidgetToWindow(w);
	index = window->iSearchHistIndex_;
	*continueDispatch = FALSE;

	/* allow escape key to cancel search */
	if (keysym == XK_Escape) {
		XmProcessTraversal(window->lastFocus_, XmTRAVERSE_CURRENT);
		EndISearch(window);
		return;
	}

	/* increment or decrement the index depending on which arrow was pressed */
	index += (keysym == XK_Up) ? 1 : -1;

	/* if the index is out of range, beep and return */
	if (index != 0 && historyIndex(index) == -1) {
		XBell(TheDisplay, 0);
		return;
	}

	/* determine the strings and button settings to use */
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

	/* Beware the value changed callback is processed as part of this call */
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
	int pos, matchIndex;
	int startPos, endPos, searchPos, matchPos;
	int constrain;

	/* if a marker is already drawn, erase it and cancel the timeout */
	if (window->flashTimeoutID_ != 0) {
		eraseFlash(window);
		XtRemoveTimeOut(window->flashTimeoutID_);
		window->flashTimeoutID_ = 0;
	}

	/* no flashing required */
	if (window->showMatchingStyle_ == NO_FLASH) {
		return;
	}

	/* don't flash matching characters if there's a selection */
	if (window->buffer_->primary_.selected)
		return;

	/* get the character to match and the position to start from */
	pos = TextGetCursorPos(textW) - 1;
	if (pos < 0)
		return;
	c = window->buffer_->BufGetCharacter(pos);
	style = GetHighlightInfo(window, pos);

	/* is the character one we want to flash? */
	for (matchIndex = 0; matchIndex < N_FLASH_CHARS; matchIndex++) {
		if (MatchingChars[matchIndex].c == c)
			break;
	}
	if (matchIndex == N_FLASH_CHARS)
		return;

	/* constrain the search to visible text only when in single-pane mode
	   AND using delimiter flashing (otherwise search the whole buffer) */
	constrain = ((window->nPanes_ == 0) && (window->showMatchingStyle_ == FLASH_DELIMIT));

	if (MatchingChars[matchIndex].direction == SEARCH_BACKWARD) {
		startPos = constrain ? TextFirstVisiblePos(textW) : 0;
		endPos = pos;
		searchPos = endPos;
	} else {
		startPos = pos;
		endPos = constrain ? TextLastVisiblePos(textW) : window->buffer_->BufGetLength();
		searchPos = startPos;
	}

	/* do the search */
	if (!findMatchingChar(window, c, style, searchPos, startPos, endPos, &matchPos))
		return;

	if (window->showMatchingStyle_ == FLASH_DELIMIT) {
		/* Highlight either the matching character ... */
		window->buffer_->BufHighlight(matchPos, matchPos + 1);
	} else {
		/* ... or the whole range. */
		if (MatchingChars[matchIndex].direction == SEARCH_BACKWARD) {
			window->buffer_->BufHighlight(matchPos, pos + 1);
		} else {
			window->buffer_->BufHighlight(matchPos + 1, pos);
		}
	}

	/* Set up a timer to erase the box after 1.5 seconds */
	window->flashTimeoutID_ = XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell_), 1500, flashTimeoutProc, window);
	window->flashPos_ = matchPos;
}

void SelectToMatchingCharacter(Document *window) {
	int selStart, selEnd;
	int startPos, endPos, matchPos;
	TextBuffer *buf = window->buffer_;

	/* get the character to match and its position from the selection, or
	   the character before the insert point if nothing is selected.
	   Give up if too many characters are selected */
	if (!GetSimpleSelection(buf, &selStart, &selEnd)) {
		selEnd = TextGetCursorPos(window->lastFocus_);
		if (window->overstrike_)
			selEnd += 1;
		selStart = selEnd - 1;
		if (selStart < 0) {
			XBell(TheDisplay, 0);
			return;
		}
	}
	if ((selEnd - selStart) != 1) {
		XBell(TheDisplay, 0);
		return;
	}

	/* Search for it in the buffer */
	if (!findMatchingChar(window, buf->BufGetCharacter(selStart), GetHighlightInfo(window, selStart), selStart, 0, buf->BufGetLength(), &matchPos)) {
		XBell(TheDisplay, 0);
		return;
	}
	startPos = (matchPos > selStart) ? selStart : matchPos;
	endPos = (matchPos > selStart) ? matchPos : selStart;

	/* temporarily shut off autoShowInsertPos before setting the cursor
	   position so MakeSelectionVisible gets a chance to place the cursor
	   string at a pleasing position on the screen (otherwise, the cursor would
	   be automatically scrolled on screen and MakeSelectionVisible would do
	   nothing) */
	XtVaSetValues(window->lastFocus_, textNautoShowInsertPos, False, nullptr);
	/* select the text between the matching characters */
	buf->BufSelect(startPos, endPos + 1);
	window->MakeSelectionVisible(window->lastFocus_);
	XtVaSetValues(window->lastFocus_, textNautoShowInsertPos, True, nullptr);
}

void GotoMatchingCharacter(Document *window) {
	int selStart, selEnd;
	int matchPos;
	TextBuffer *buf = window->buffer_;

	/* get the character to match and its position from the selection, or
	   the character before the insert point if nothing is selected.
	   Give up if too many characters are selected */
	if (!GetSimpleSelection(buf, &selStart, &selEnd)) {
		selEnd = TextGetCursorPos(window->lastFocus_);
		if (window->overstrike_)
			selEnd += 1;
		selStart = selEnd - 1;
		if (selStart < 0) {
			XBell(TheDisplay, 0);
			return;
		}
	}
	if ((selEnd - selStart) != 1) {
		XBell(TheDisplay, 0);
		return;
	}

	/* Search for it in the buffer */
	if (!findMatchingChar(window, buf->BufGetCharacter(selStart), GetHighlightInfo(window, selStart), selStart, 0, buf->BufGetLength(), &matchPos)) {
		XBell(TheDisplay, 0);
		return;
	}

	/* temporarily shut off autoShowInsertPos before setting the cursor
	   position so MakeSelectionVisible gets a chance to place the cursor
	   string at a pleasing position on the screen (otherwise, the cursor would
	   be automatically scrolled on screen and MakeSelectionVisible would do
	   nothing) */
	XtVaSetValues(window->lastFocus_, textNautoShowInsertPos, False, nullptr);
	TextSetCursorPos(window->lastFocus_, matchPos + 1);
	window->MakeSelectionVisible(window->lastFocus_);
	XtVaSetValues(window->lastFocus_, textNautoShowInsertPos, True, nullptr);
}

static int findMatchingChar(Document *window, char toMatch, void *styleToMatch, int charPos, int startLimit, int endLimit, int *matchPos) {
	int nestDepth, matchIndex, direction, beginPos, pos;
	char matchChar, c;
	void *style = nullptr;
	TextBuffer *buf = window->buffer_;
	int matchSyntaxBased = window->matchSyntaxBased_;

	/* If we don't match syntax based, fake a matching style. */
	if (!matchSyntaxBased)
		style = styleToMatch;

	/* Look up the matching character and match direction */
	for (matchIndex = 0; matchIndex < N_MATCH_CHARS; matchIndex++) {
		if (MatchingChars[matchIndex].c == toMatch)
			break;
	}
	if (matchIndex == N_MATCH_CHARS)
		return FALSE;
	matchChar = MatchingChars[matchIndex].match;
	direction = MatchingChars[matchIndex].direction;

	/* find it in the buffer */
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
	} else { /* SEARCH_BACKWARD */
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
		XBell(TheDisplay, 0);
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
		XBell(TheDisplay, 0);
		return FALSE;
	}

	return ReplaceAndSearch(window, direction, SearchHistory[historyIndex(1)], ReplaceHistory[historyIndex(1)], SearchTypeHistory[historyIndex(1)], searchWrap);
}

/*
** Replace selection with "replaceString" and search for string "searchString" in window "window",
** using algorithm "searchType" and direction "direction"
*/
bool ReplaceAndSearch(Document *window, SearchDirection direction, const char *searchString, const char *replaceString, int searchType, int searchWrap) {
	int startPos = 0;
	int endPos = 0;
	int replaceLen = 0;
	int searchExtentBW;
	int searchExtentFW;

	/* Save a copy of search and replace strings in the search history */
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	bool replaced = false;

	/* Replace the selected text only if it matches the search string */
	if (searchMatchesSelection(window, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {
		/* replace the text */
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
				GetWindowDelimiters(window),
				defaultRegexFlags(searchType));

			window->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
			replaceLen = strlen(replaceResult);
		} else {
			window->buffer_->BufReplaceEx(startPos, endPos, replaceString);
			replaceLen = strlen(replaceString);
		}

		/* Position the cursor so the next search will work correctly based */
		/* on the direction of the search */
		TextSetCursorPos(window->lastFocus_, startPos + ((direction == SEARCH_FORWARD) ? replaceLen : 0));
		replaced = true;
	}

	/* do the search; beeps/dialogs are taken care of */
	SearchAndSelect(window, direction, searchString, searchType, searchWrap);

	return replaced;
}

/*
** Search for string "searchString" in window "window", using algorithm
** "searchType" and direction "direction", and replace it with "replaceString"
** Also adds the search and replace strings to the global search history.
*/
bool SearchAndReplace(Document *window, SearchDirection direction, const char *searchString, const char *replaceString, int searchType, int searchWrap) {
	int startPos, endPos, replaceLen, searchExtentBW, searchExtentFW;
	int found;
	int beginPos, cursorPos;

	/* Save a copy of search and replace strings in the search history */
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	/* If the text selected in the window matches the search string, 	*/
	/* the user is probably using search then replace method, so	*/
	/* replace the selected text regardless of where the cursor is.	*/
	/* Otherwise, search for the string.				*/
	if (!searchMatchesSelection(window, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {
		/* get the position to start the search */
		cursorPos = TextGetCursorPos(window->lastFocus_);
		if (direction == SEARCH_BACKWARD) {
			/* use the insert position - 1 for backward searches */
			beginPos = cursorPos - 1;
		} else {
			/* use the insert position for forward searches */
			beginPos = cursorPos;
		}
		/* do the search */
		found = SearchWindow(window, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW);
		if (!found)
			return false;
	}

	/* replace the text */
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
			GetWindowDelimiters(window),
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
	XtVaSetValues(window->lastFocus_, textNautoShowInsertPos, False, nullptr);
	TextSetCursorPos(window->lastFocus_, startPos + ((direction == SEARCH_FORWARD) ? replaceLen : 0));
	window->MakeSelectionVisible(window->lastFocus_);
	XtVaSetValues(window->lastFocus_, textNautoShowInsertPos, True, nullptr);

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
static Boolean prefOrUserCancelsSubst(const Widget parent, const Display *display) {
	Boolean cancel = True;
	unsigned confirmResult = 0;

	switch (GetPrefTruncSubstitution()) {
	case TRUNCSUBST_SILENT:
		/*  silently fail the operation  */
		cancel = True;
		break;

	case TRUNCSUBST_FAIL:
		/*  fail the operation and pop up a dialog informing the user  */
		XBell((Display *)display, 0);
		DialogF(DF_INF, parent, 1, "Substitution Failed", "The result length of the substitution exceeded an internal limit.\n"
		                                                  "The substitution is canceled.",
		        "OK");
		cancel = True;
		break;

	case TRUNCSUBST_WARN:
		/*  pop up dialog and ask for confirmation  */
		XBell((Display *)display, 0);
		confirmResult = DialogF(DF_WARN, parent, 2, "Substitution Failed", "The result length of the substitution exceeded an internal limit.\n"
		                                                                   "Executing the substitution will result in loss of data.",
		                        "Lose Data", "Cancel");
		cancel = (confirmResult != 1);
		break;

	case TRUNCSUBST_IGNORE:
		/*  silently conclude the operation; THIS WILL DESTROY DATA.  */
		cancel = False;
		break;
	}

	return cancel;
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString"
** within the current primary selection in "window". Also adds the search and
** replace strings to the global search history.
*/
void ReplaceInSelection(const Document *window, const char *searchString, const char *replaceString, const int searchType) {
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

	/* save a copy of search and replace strings in the search history */
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	/* find out where the selection is */
	if (!window->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		return;
	}

	/* get the selected text */
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
	auto tempBuf = new TextBuffer;
	tempBuf->BufSetAllEx(fileString);

	/* search the string and do the replacements in the temporary buffer */
	replaceLen = strlen(replaceString);
	found = true;
	beginPos = 0;
	cursorPos = 0;
	realOffset = 0;
	while (found) {
		found = SearchString(fileString, searchString, SEARCH_FORWARD, searchType, FALSE, beginPos, &startPos, &endPos, &extentBW, &extentFW, GetWindowDelimiters(window));
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

		/* replace the string and compensate for length change */
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
							GetWindowDelimiters(window),
							defaultRegexFlags(searchType));

			if (!substSuccess) {
				/*  The substitution failed. Primary reason for this would be
				    a result string exceeding SEARCHMAX. */

				cancelSubst = prefOrUserCancelsSubst(window->shell_, TheDisplay);

				if (cancelSubst) {
					/*  No point in trying other substitutions.  */
					break;
				}
			}

			tempBuf->BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceResult);
			replaceLen = strlen(replaceResult);
		} else {
			/* at this point plain substitutions (should) always work */
			tempBuf->BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceString);
			substSuccess = True;
		}

		realOffset += replaceLen - (endPos - startPos);
		/* start again after match unless match was empty, then endPos+1 */
		beginPos = (startPos == endPos) ? endPos + 1 : endPos;
		cursorPos = endPos;
		if (fileString[endPos] == '\0')
			break;
	}

	if (anyFound) {
		if (substSuccess || !cancelSubst) {
			/*  Either the substitution was successful (the common case) or the
			    user does not care and wants to have a faulty replacement.  */

			/* replace the selected range in the real buffer */
			window->buffer_->BufReplaceEx(selStart, selEnd, tempBuf->BufAsStringEx());

			/* set the insert point at the end of the last replacement */
			TextSetCursorPos(window->lastFocus_, selStart + cursorPos + realOffset);

			/* leave non-rectangular selections selected (rect. ones after replacement
			   are less useful since left/right positions are randomly adjusted) */
			if (!isRect) {
				window->buffer_->BufSelect(selStart, selEnd + realOffset);
			}
		}
	} else {
		/*  Nothing found, tell the user about it  */
		if (GetPrefSearchDlogs()) {
			/* Avoid bug in Motif 1.1 by putting away search dialog
			   before DialogF */
			if (window->findDlog_ && XtIsManaged(window->findDlog_) && !XmToggleButtonGetState(window->findKeepBtn_))
				XtUnmanageChild(window->findDlog_);
			if (window->replaceDlog_ && XtIsManaged(window->replaceDlog_) && !XmToggleButtonGetState(window->replaceKeepBtn_))
				unmanageReplaceDialogs(window);
			DialogF(DF_INF, window->shell_, 1, "String not found", "String was not found", "OK");
		} else
			XBell(TheDisplay, 0);
	}

	delete tempBuf;
	return;
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString".
** Also adds the search and replace strings to the global search history.
*/
bool ReplaceAll(Document *window, const char *searchString, const char *replaceString, int searchType) {
	char *newFileString;
	int copyStart, copyEnd, replacementLen;

	/* reject empty string */
	if (*searchString == '\0')
		return false;

	/* save a copy of search and replace strings in the search history */
	saveSearchHistory(searchString, replaceString, searchType, FALSE);

	/* view the entire text buffer from the text area widget as a string */
	view::string_view fileString = window->buffer_->BufAsStringEx();

	newFileString = ReplaceAllInString(fileString, searchString, replaceString, searchType, &copyStart, &copyEnd, &replacementLen, GetWindowDelimiters(window));

	if(!newFileString) {
		if (window->multiFileBusy_) {
			window->replaceFailed_ = TRUE; /* only needed during multi-file
			                                 replacements */
		} else if (GetPrefSearchDlogs()) {
			if (window->findDlog_ && XtIsManaged(window->findDlog_) && !XmToggleButtonGetState(window->findKeepBtn_))
				XtUnmanageChild(window->findDlog_);
			if (window->replaceDlog_ && XtIsManaged(window->replaceDlog_) && !XmToggleButtonGetState(window->replaceKeepBtn_))
				unmanageReplaceDialogs(window);
			DialogF(DF_INF, window->shell_, 1, "String not found", "String was not found", "OK");
		} else
			XBell(TheDisplay, 0);
		return false;
	}

	/* replace the contents of the text widget with the substituted text */
	window->buffer_->BufReplaceEx(copyStart, copyEnd, newFileString);

	/* Move the cursor to the end of the last replacement */
	TextSetCursorPos(window->lastFocus_, copyStart + replacementLen);

	XtFree(newFileString);
	return true;
}

/*
** Replace all occurences of "searchString" in "inString" with "replaceString"
** and return an allocated string covering the range between the start of the
** first replacement (returned in "copyStart", and the end of the last
** replacement (returned in "copyEnd")
*/
char *ReplaceAllInString(view::string_view inString, const char *searchString, const char *replaceString, int searchType, int *copyStart, int *copyEnd, int *replacementLength, const char *delimiters) {
	int beginPos, startPos, endPos, lastEndPos;
	int found, nFound, removeLen, replaceLen, copyLen, addLen;
	char *outString, *fillPtr;
	int searchExtentBW, searchExtentFW;

	/* reject empty string */
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
			/* start next after match unless match was empty, then endPos+1 */
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
			/* start next after match unless match was empty, then endPos+1 */
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
				XBell(TheDisplay, 0);
			}
		} else {
			if ((startPos <= beginPos && window->iSearchLastBeginPos_ > beginPos) || (startPos > beginPos && window->iSearchLastBeginPos_ <= beginPos)) {
				XBell(TheDisplay, 0);
			}
		}
	}
}

/*
** Search the text in "window", attempting to match "searchString"
*/
bool SearchWindow(Document *window, SearchDirection direction, const char *searchString, int searchType, int searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW) {
	bool found;
	int resp;
	int fileEnd = window->buffer_->BufGetLength() - 1;
	bool outsideBounds;

	/* reject empty string */
	if (*searchString == '\0')
		return false;

	/* get the entire text buffer from the text area widget */
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
	if (window->iSearchStartPos_ == -1) { /* normal search */
		found = !outsideBounds && SearchString(fileString, searchString, direction, searchType, FALSE, beginPos, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window));
		/* Avoid Motif 1.1 bug by putting away search dialog before DialogF */
		if (window->findDlog_ && XtIsManaged(window->findDlog_) && !XmToggleButtonGetState(window->findKeepBtn_))
			XtUnmanageChild(window->findDlog_);
		if (window->replaceDlog_ && XtIsManaged(window->replaceDlog_) && !XmToggleButtonGetState(window->replaceKeepBtn_))
			unmanageReplaceDialogs(window);
		if (!found) {
			if (searchWrap) {
				if (direction == SEARCH_FORWARD && beginPos != 0) {
					if (GetPrefBeepOnSearchWrap()) {
						XBell(TheDisplay, 0);
					} else if (GetPrefSearchDlogs()) {
						resp = DialogF(DF_QUES, window->shell_, 2, "Wrap Search", "Continue search from\nbeginning of file?", "Continue", "Cancel");
						if (resp == 2) {
							return false;
						}
					}
					found = SearchString(fileString, searchString, direction, searchType, FALSE, 0, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window));
				} else if (direction == SEARCH_BACKWARD && beginPos != fileEnd) {
					if (GetPrefBeepOnSearchWrap()) {
						XBell(TheDisplay, 0);
					} else if (GetPrefSearchDlogs()) {
						resp = DialogF(DF_QUES, window->shell_, 2, "Wrap Search", "Continue search\nfrom end of file?", "Continue", "Cancel");
						if (resp == 2) {
							return false;
						}
					}
					found = SearchString(fileString, searchString, direction, searchType, FALSE, fileEnd + 1, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window));
				}
			}
			if (!found) {
				if (GetPrefSearchDlogs()) {
					DialogF(DF_INF, window->shell_, 1, "String not found", "String was not found", "OK");
				} else {
					XBell(TheDisplay, 0);
				}
			}
		}
	} else { /* incremental search */
		if (outsideBounds && searchWrap) {
			if (direction == SEARCH_FORWARD)
				beginPos = 0;
			else
				beginPos = fileEnd + 1;
			outsideBounds = FALSE;
		}
		found = !outsideBounds && SearchString(fileString, searchString, direction, searchType, searchWrap, beginPos, startPos, endPos, extentBW, extentFW, GetWindowDelimiters(window));
		if (found) {
			iSearchTryBeepOnWrap(window, direction, beginPos, *startPos);
		} else
			XBell(TheDisplay, 0);
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
bool SearchString(view::string_view string, const char *searchString, SearchDirection direction, int searchType, bool wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters) {
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
	}
	return false; /* never reached, just makes compilers happy */
}

/*
** Parses a search type description string. If the string contains a valid
** search type description, returns TRUE and writes the corresponding
** SearchType in searchType. Returns FALSE and leaves searchType untouched
** otherwise. (Originally written by Markus Schwarzenberg; slightly adapted).
*/
int StringToSearchType(const char *string, int *searchType) {
	int i;
	for (i = 0; searchTypeStrings[i]; i++) {
		if (!strcmp(string, searchTypeStrings[i])) {
			break;
		}
	}
	if (!searchTypeStrings[i]) {
		return FALSE;
	}
	*searchType = i;
	return TRUE;
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
			/* matched first character */
			auto ucPtr = ucString.begin();
			auto lcPtr = lcString.begin();
			const char *tempPtr = filePtr;
			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;
				if (ucPtr == ucString.end()                                                            /* matched whole string */
			    	&& (cignore_R || isspace((unsigned char)*tempPtr) || strchr(delimiters, *tempPtr)) /* next char right delimits word ? */
			    	&& (cignore_L || filePtr == &string[0] ||                                          /* border case */
			        	isspace((unsigned char)filePtr[-1]) || strchr(delimiters, filePtr[-1])))       /* next char left delimits word ? */ {
					*startPos = filePtr - &string[0];
					*endPos = tempPtr - &string[0];
					return true;
				}
			}
		}
		
		return false;
	};


	/* If there is no language mode, we use the default list of delimiters */
	if(!delimiters)
		delimiters = GetPrefDelimiters();

	if (isspace((unsigned char)searchString[0]) || strchr(delimiters, searchString[0])) {
		cignore_L = true;
	}

	if (isspace((unsigned char)searchString[searchString.size() - 1]) || strchr(delimiters, searchString[searchString.size() - 1])) {
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
		/* search from beginPos to end of string */
		for (auto filePtr = string.begin() + beginPos; filePtr != string.end(); filePtr++) {
			if(DOSEARCHWORD2(filePtr)) {
				return true;
			}
		}
		if (!wrap)
			return FALSE;

		/* search from start of file to beginPos */
		for (auto filePtr = string.begin(); filePtr <= string.begin() + beginPos; filePtr++) {
			if(DOSEARCHWORD2(filePtr)) {
				return true;
			}
		}
		return FALSE;
	} else {
		/* SEARCH_BACKWARD */
		/* search from beginPos to start of file. A negative begin pos */
		/* says begin searching from the far end of the file */
		if (beginPos >= 0) {
			for (auto filePtr = string.begin() + beginPos; filePtr >= string.begin(); filePtr--) {
				if(DOSEARCHWORD2(filePtr)) {
					return true;
				}
			}
		}
		if (!wrap)
			return FALSE;
		/* search from end of file to beginPos */
		/*... this strlen call is extreme inefficiency, but it's not obvious */
		/* how to get the text string length from the text widget (under 1.1)*/
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
			/* matched first character */
			auto ucPtr   = ucString.begin();
			auto lcPtr   = lcString.begin();
			const char *tempPtr = filePtr;

			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;
				if (ucPtr == ucString.end()) {
					/* matched whole string */
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

		/* search from beginPos to end of string */
		for (auto filePtr = mid; filePtr != last; ++filePtr) {
			if(DOSEARCH2(filePtr)) {
				return true;
			}
		}

		if (!wrap) {
			return false;
		}

		/* search from start of file to beginPos */
		// TODO(eteran): this used to include "mid", but that seems redundant given that we already looked there
		//               in the first loop
		for (auto filePtr = first; filePtr != mid; ++filePtr) {
			if(DOSEARCH2(filePtr)) {
				return true;
			}
		}

		return false;
	} else {
		/* SEARCH_BACKWARD */
		/* search from beginPos to start of file.  A negative begin pos	*/
		/* says begin searching from the far end of the file */

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

		/* search from end of file to beginPos */
		/* how to get the text string length from the text widget (under 1.1)*/
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

		/* search from beginPos to end of string */
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

		/* if wrap turned off, we're done */
		if (!wrap) {
			return false;
		}

		/* search from the beginning of the string to beginPos */
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

		/* search from beginPos to start of file.  A negative begin pos	*/
		/* says begin searching from the far end of the file.		*/
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

		/* if wrap turned off, we're done */
		if (!wrap) {
			return false;
		}

		/* search from the end of the string to beginPos */
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
		return  toupper((unsigned char)ch);
	});
	return str;
}

static std::string downCaseStringEx(view::string_view inString) {
	std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
		return  tolower((unsigned char)ch);
	});
	return str;
}

/*
** resetFindTabGroup & resetReplaceTabGroup are really gruesome kludges to
** set the keyboard traversal.  XmProcessTraversal does not work at
** all on these dialogs.  ...It seems to have started working around
** Motif 1.1.2
*/
static void resetFindTabGroup(Document *window) {
	XmProcessTraversal(window->findText_, XmTRAVERSE_CURRENT);
}
static void resetReplaceTabGroup(Document *window) {
	XmProcessTraversal(window->replaceText_, XmTRAVERSE_CURRENT);
}

/*
** Return TRUE if "searchString" exactly matches the text in the window's
** current primary selection using search algorithm "searchType".  If true,
** also return the position of the selection in "left" and "right".
*/
static bool searchMatchesSelection(Document *window, const char *searchString, int searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW) {
	int selLen, selStart, selEnd, startPos, endPos, extentBW, extentFW, beginPos;
	int regexLookContext = isRegexType(searchType) ? 1000 : 0;
	std::string string;
	int rectStart, rectEnd, lineStart = 0;
	bool isRect;

	/* find length of selection, give up on no selection or too long */
	if (!window->buffer_->BufGetEmptySelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		return false;
	}

	if (selEnd - selStart > SEARCHMAX) {
		return false;
	}

	/* if the selection is rectangular, don't match if it spans lines */
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

	/* search for the string in the selection (we are only interested 	*/
	/* in an exact match, but the procedure SearchString does important */
	/* stuff like applying the correct matching algorithm)		*/
	bool found = SearchString(string, searchString, SEARCH_FORWARD, searchType, FALSE, beginPos, &startPos, &endPos, &extentBW, &extentFW, GetWindowDelimiters(window));

	/* decide if it is an exact match */
	if (!found) {
		return false;
	}

	if (startPos != beginPos || endPos - beginPos != selLen) {
		return false;
	}

	/* return the start and end of the selection */
	if (isRect) {
		GetSimpleSelection(window->buffer_, left, right);
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
static void saveSearchHistory(const char *searchString, const char *replaceString, int searchType, int isIncremental) {
	char *sStr, *rStr;
	static int currentItemIsIncremental = FALSE;

	/* Cancel accumulation of contiguous incremental searches (even if the
	   information is not worthy of saving) if search is not incremental */
	if (!isIncremental)
		currentItemIsIncremental = FALSE;

	/* Don't save empty search strings */
	if (searchString[0] == '\0')
		return;

	/* If replaceString is nullptr, duplicate the last one (if any) */
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
		Document::for_each([](Document *w) {
			if (w->IsTopDocument()) {
				XtSetSensitive(w->findAgainItem_, True);
				XtSetSensitive(w->replaceFindAgainItem_, True);
				XtSetSensitive(w->replaceAgainItem_, True);
			}
		});
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
static int historyIndex(int nCycles) {
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
static const char *searchTypeArg(int searchType) {
	if (0 <= searchType && searchType < N_SEARCH_TYPES) {
		return searchTypeStrings[searchType];
	}
	return searchTypeStrings[SEARCH_LITERAL];
}

/*
** Return a pointer to the string describing search wrap for search action
** routine parameters (see menu.c for processing of action routines)
*/
static const char *searchWrapArg(int searchWrap) {
	if (searchWrap) {
		return "wrap";
	}
	return "nowrap";
}

/*
** Return a pointer to the string describing search direction for search action
** routine parameters (see menu.c for processing of action routines)
*/
static const char *directionArg(SearchDirection direction) {
	if (direction == SEARCH_BACKWARD)
		return "backward";
	return "forward";
}

/*
** Checks whether a search mode in one of the regular expression modes.
*/
static int isRegexType(int searchType) {
	return searchType == SEARCH_REGEX || searchType == SEARCH_REGEX_NOCASE;
}

/*
** Returns the default flags for regular expression matching, given a
** regular expression search mode.
*/
static int defaultRegexFlags(int searchType) {
	switch (searchType) {
	case SEARCH_REGEX:
		return REDFLT_STANDARD;
	case SEARCH_REGEX_NOCASE:
		return REDFLT_CASE_INSENSITIVE;
	default:
		/* We should never get here, but just in case ... */
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
static void findRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchRegex = XmToggleButtonGetState(w);
	int searchCaseSense = XmToggleButtonGetState(window->findCaseToggle_);

	/* In sticky mode, restore the state of the Case Sensitive button */
	if (GetPrefStickyCaseSenseBtn()) {
		if (searchRegex) {
			window->findLastLiteralCase_ = searchCaseSense;
			XmToggleButtonSetState(window->findCaseToggle_, window->findLastRegexCase_, False);
		} else {
			window->findLastRegexCase_ = searchCaseSense;
			XmToggleButtonSetState(window->findCaseToggle_, window->findLastLiteralCase_, False);
		}
	}
	/* make the Whole Word button insensitive for regex searches */
	XtSetSensitive(window->findWordToggle_, !searchRegex);
}

static void replaceRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchRegex = XmToggleButtonGetState(w);
	int searchCaseSense = XmToggleButtonGetState(window->replaceCaseToggle_);

	/* In sticky mode, restore the state of the Case Sensitive button */
	if (GetPrefStickyCaseSenseBtn()) {
		if (searchRegex) {
			window->replaceLastLiteralCase_ = searchCaseSense;
			XmToggleButtonSetState(window->replaceCaseToggle_, window->replaceLastRegexCase_, False);
		} else {
			window->replaceLastRegexCase_ = searchCaseSense;
			XmToggleButtonSetState(window->replaceCaseToggle_, window->replaceLastLiteralCase_, False);
		}
	}
	/* make the Whole Word button insensitive for regex searches */
	XtSetSensitive(window->replaceWordToggle_, !searchRegex);
}

static void iSearchRegExpToggleCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchRegex = XmToggleButtonGetState(w);
	int searchCaseSense = XmToggleButtonGetState(window->iSearchCaseToggle_);

	/* In sticky mode, restore the state of the Case Sensitive button */
	if (GetPrefStickyCaseSenseBtn()) {
		if (searchRegex) {
			window->iSearchLastLiteralCase_ = searchCaseSense;
			XmToggleButtonSetState(window->iSearchCaseToggle_, window->iSearchLastRegexCase_, False);
		} else {
			window->iSearchLastRegexCase_ = searchCaseSense;
			XmToggleButtonSetState(window->iSearchCaseToggle_, window->iSearchLastLiteralCase_, False);
		}
	}
	/* The iSearch bar has no Whole Word button to enable/disable. */
}
static void findCaseToggleCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchCaseSense = XmToggleButtonGetState(w);

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (XmToggleButtonGetState(window->findRegexToggle_))
		window->findLastRegexCase_ = searchCaseSense;
	else
		window->findLastLiteralCase_ = searchCaseSense;
}

static void replaceCaseToggleCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	int searchCaseSense = XmToggleButtonGetState(w);

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (XmToggleButtonGetState(window->replaceRegexToggle_))
		window->replaceLastRegexCase_ = searchCaseSense;
	else
		window->replaceLastLiteralCase_ = searchCaseSense;
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
