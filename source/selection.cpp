/*******************************************************************************
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
#include <QInputDialog>
#include <QString>

#include "selection.h"
#include "TextBuffer.h"
#include "text.h"
#include "nedit.h"
#include "file.h"
#include "window.h"
#include "menu.h"
#include "preferences.h"
#include "Document.h"
#include "server.h"
#include "../util/fileUtils.h"

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <climits>
#include <sys/param.h>

#include <glob.h>

#include <Xm/Xm.h>
#include <X11/Xatom.h>


static void getAnySelectionCB(Widget widget, XtPointer client_data, Atom *selection, Atom *type, XtPointer value, unsigned long *length, int *format);

static void gotoCB(Widget widget, Document *window, Atom *sel, Atom *type, char *value, int *length, int *format);
static void fileCB(Widget widget, Document *window, Atom *sel, Atom *type, char *value, int *length, int *format);

static void processMarkEvent(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch, char *action, int extend);
static void markTimeoutProc(XtPointer clientData, XtIntervalId *id);
static void markKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch);
static void gotoMarkKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch);
static void gotoMarkExtendKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch);
static void maintainSelection(TextSelection *sel, int pos, int nInserted, int nDeleted);
static void maintainPosition(int *position, int modPos, int nInserted, int nDeleted);

/*
** Extract the line and column number from the text string.
** Set the line and/or column number to -1 if not specified, and return -1 if
** both line and column numbers are not specified.
*/
int StringToLineAndCol(const char *text, int *lineNum, int *column) {
	char *endptr;
	long tempNum;
	int textLen;

	// Get line number 
	tempNum = strtol(text, &endptr, 10);

	// If user didn't specify a line number, set lineNum to -1 
	if (endptr == text) {
		*lineNum = -1;
	} else if (tempNum >= INT_MAX) {
		*lineNum = INT_MAX;
	} else if (tempNum < 0) {
		*lineNum = 0;
	} else {
		*lineNum = tempNum;
	}

	// Find the next digit 
	for (textLen = strlen(endptr); textLen > 0; endptr++, textLen--) {
		if (isdigit((unsigned char)*endptr) || *endptr == '-' || *endptr == '+') {
			break;
		}
	}

	// Get column 
	if (*endptr != '\0') {
		tempNum = strtol(endptr, nullptr, 10);
		if (tempNum >= INT_MAX) {
			*column = INT_MAX;
		} else if (tempNum < 0) {
			*column = 0;
		} else {
			*column = tempNum;
		}
	} else {
		*column = -1;
	}

	return *lineNum == -1 && *column == -1 ? -1 : 0;
}

void GotoLineNumber(Document *window) {

	const int DF_MAX_PROMPT_LENGTH = 2048;

	char lineNumText[DF_MAX_PROMPT_LENGTH], *params[1];
	int lineNum;
	int column;


	bool ok;
	QString text = QInputDialog::getText(nullptr /*parent*/, QLatin1String("Goto Line Number"), QLatin1String("Goto Line (and/or Column)  Number:"), QLineEdit::Normal, QString(), &ok);

	if (!ok)
		return;

	if (StringToLineAndCol(lineNumText, &lineNum, &column) == -1) {
		QApplication::beep();
		return;
	}
	
	// TODO(eteran): this is temporary, until we have a better mechanism for this
	snprintf(lineNumText, sizeof(lineNumText), "%s", text.toLatin1().data());
	
	params[0] = lineNumText;
	
	// NOTE(eteran): XtCallActionProc seems to be roughly equivalent to Q_EMIT
	//               given a similarly connected signal/slot
	XtCallActionProc(window->lastFocus_, "goto_line_number", nullptr, params, 1);
}

void GotoSelectedLineNumber(Document *window, Time time) {
	XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)gotoCB, window, time);
}

void OpenSelectedFile(Document *window, Time time) {
	XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)fileCB, window, time);
}

/*
** Getting the current selection by making the request, and then blocking
** (processing events) while waiting for a reply.  On failure (timeout or
** bad format) returns nullptr, otherwise returns the contents of the selection.
*/
QString GetAnySelectionEx(Document *window) {
	static char waitingMarker[1] = "";
	char *selText = waitingMarker;
	XEvent nextEvent;

	/* If the selection is in the window's own buffer get it from there,
	   but substitute null characters as if it were an external selection */
	if (window->buffer_->primary_.selected) {
		std::string text = window->buffer_->BufGetSelectionTextEx();
		window->buffer_->BufUnsubstituteNullCharsEx(text);
		return QString::fromStdString(text);
	}

	// Request the selection value to be delivered to getAnySelectionCB 
	XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, getAnySelectionCB, &selText, XtLastTimestampProcessed(XtDisplay(window->textArea_)));

	// Wait for the value to appear 
	while (selText == waitingMarker) {
		XtAppNextEvent(XtWidgetToApplicationContext(window->textArea_), &nextEvent);
		ServerDispatchEvent(&nextEvent);
	}
	
	if(!selText) {
		return QString();
	}
	
	QString s = QLatin1String(selText);
	XtFree(selText);
	return s;
}

static void gotoCB(Widget widget, Document *window, Atom *sel, Atom *type, char *value, int *length, int *format) {

	(void)sel;

	// two integers and some space in between 
	char lineText[(TYPE_INT_STR_SIZE(int)*2) + 5];
	int rc, lineNum, column, position, curCol;

	// skip if we can't get the selection data, or it's obviously not a number 
	if (*type == XT_CONVERT_FAIL || value == nullptr) {
		QApplication::beep();
		return;
	}
	if (((size_t)*length) > sizeof(lineText) - 1) {
		QApplication::beep();
		XtFree(value);
		return;
	}
	// should be of type text??? 
	if (*format != 8) {
		fprintf(stderr, "NEdit: Can't handle non 8-bit text\n");
		QApplication::beep();
		XtFree(value);
		return;
	}
	strncpy(lineText, value, sizeof(lineText));
	lineText[sizeof(lineText) - 1] = '\0';

	rc = StringToLineAndCol(lineText, &lineNum, &column);
	XtFree(value);
	if (rc == -1) {
		QApplication::beep();
		return;
	}

	// User specified column, but not line number 
	if (lineNum == -1) {
		position = TextGetCursorPos(widget);
		if (TextPosToLineAndCol(widget, position, &lineNum, &curCol) == False) {
			QApplication::beep();
			return;
		}
	}
	// User didn't specify a column 
	else if (column == -1) {
		SelectNumberedLine(window, lineNum);
		return;
	}

	position = TextLineAndColToPos(widget, lineNum, column);
	if (position == -1) {
		QApplication::beep();
		return;
	}
	TextSetCursorPos(widget, position);
}

static void fileCB(Widget widget, Document *window, Atom *sel, Atom *type, char *value, int *length, int *format) {

	(void)widget;
	(void)sel;

	char nameText[MAXPATHLEN], includeName[MAXPATHLEN];
	char filename[MAXPATHLEN], pathname[MAXPATHLEN];
	char *inPtr, *outPtr;
	static char includeDir[] = "/usr/include/";

	/* get the string, or skip if we can't get the selection data, or it's
	   obviously not a file name */
	if (*type == XT_CONVERT_FAIL || value == nullptr) {
		QApplication::beep();
		return;
	}
	if (*length > MAXPATHLEN || *length == 0) {
		QApplication::beep();
		XtFree(value);
		return;
	}
	// should be of type text??? 
	if (*format != 8) {
		fprintf(stderr, "NEdit: Can't handle non 8-bit text\n");
		QApplication::beep();
		XtFree(value);
		return;
	}
	strncpy(nameText, value, *length);
	XtFree(value);
	nameText[*length] = '\0';

	// extract name from #include syntax 
	if (sscanf(nameText, "#include \"%[^\"]\"", includeName) == 1)
		strcpy(nameText, includeName);
	else if (sscanf(nameText, "#include <%[^<>]>", includeName) == 1)
		sprintf(nameText, "%s%s", includeDir, includeName);

	// strip whitespace from name 
	for (inPtr = nameText, outPtr = nameText; *inPtr != '\0'; inPtr++)
		if (*inPtr != ' ' && *inPtr != '\t' && *inPtr != '\n')
			*outPtr++ = *inPtr;
	*outPtr = '\0';

	// Process ~ characters in name 
	ExpandTilde(nameText);

	// If path name is relative, make it refer to current window's directory 
	if (nameText[0] != '/') {
		strcpy(filename, window->path_.c_str());
		strcat(filename, nameText);
		strcpy(nameText, filename);
	}

	// Expand wildcards in file name. 

	{
		glob_t globbuf;
		int i;

		glob(nameText, GLOB_NOCHECK, nullptr, &globbuf);
		for (i = 0; i < (int)globbuf.gl_pathc; i++) {
			if (ParseFilename(globbuf.gl_pathv[i], filename, pathname) != 0)
				QApplication::beep();
			else
				EditExistingFile(GetPrefOpenInTab() ? window : nullptr, filename, pathname, 0, nullptr, False, nullptr, GetPrefOpenInTab(), False);
		}
		globfree(&globbuf);
	}

	CheckCloseDim();
}

static void getAnySelectionCB(Widget widget, XtPointer client_data, Atom *selection, Atom *type, XtPointer value, unsigned long *length, int *format) {

	(void)widget;
	(void)selection;

	auto result = static_cast<char **>(client_data);

	// Confirm that the returned value is of the correct type 
	if (*type != XA_STRING || *format != 8) {
		QApplication::beep();
		XtFree((char *)value);
		*result = nullptr;
		return;
	}

	// Append a null, and return the string 
	*result = XtMalloc(*length + 1);
	strncpy(*result, (char *)value, *length);
	XtFree((char *)value);
	(*result)[*length] = '\0';
}

void SelectNumberedLine(Document *window, int lineNum) {
	int i, lineStart = 0, lineEnd;

	// count lines to find the start and end positions for the selection 
	if (lineNum < 1)
		lineNum = 1;
	lineEnd = -1;
	for (i = 1; i <= lineNum && lineEnd < window->buffer_->BufGetLength(); i++) {
		lineStart = lineEnd + 1;
		lineEnd = window->buffer_->BufEndOfLine( lineStart);
	}

	// highlight the line 
	if (i > lineNum) {
		// Line was found 
		if (lineEnd < window->buffer_->BufGetLength()) {
			window->buffer_->BufSelect(lineStart, lineEnd + 1);
		} else {
			// Don't select past the end of the buffer ! 
			window->buffer_->BufSelect(lineStart, window->buffer_->BufGetLength());
		}
	} else {
		/* Line was not found -> position the selection & cursor at the end
		   without making a real selection and beep */
		lineStart = window->buffer_->BufGetLength();
		window->buffer_->BufSelect(lineStart, lineStart);
		QApplication::beep();
	}
	window->MakeSelectionVisible(window->lastFocus_);
	TextSetCursorPos(window->lastFocus_, lineStart);
}

void MarkDialog(Document *window) {

	const int DF_MAX_PROMPT_LENGTH = 2048;

	char letterText[DF_MAX_PROMPT_LENGTH];
	char *params[1];
		
	bool ok;
	QString result = QInputDialog::getText(
		nullptr /*window->shell_*/, 
		QLatin1String("Mark"), 
		QLatin1String("Enter a single letter label to use for recalling\n"
		              "the current selection and cursor position.\n\n"
		              "(To skip this dialog, use the accelerator key,\n"
		              "followed immediately by a letter key (a-z))"), 
		QLineEdit::Normal, 
		QString(),
		&ok);
		
	if(!ok) {
		return;
	}
	
	strcpy(letterText, result.toLatin1().data());

	if (strlen(letterText) != 1 || !isalpha((unsigned char)letterText[0])) {
		QApplication::beep();
		return;
	}
	
	params[0] = letterText;
	XtCallActionProc(window->lastFocus_, "mark", nullptr, params, 1);
}

void GotoMarkDialog(Document *window, int extend) {

	const int DF_MAX_PROMPT_LENGTH = 2048;

	char letterText[DF_MAX_PROMPT_LENGTH];
	const char *params[2];
	
	bool ok;
	QString result = QInputDialog::getText(
		nullptr /*window->shell_*/, 
		QLatin1String("Goto Mark"), 
		QLatin1String("Enter the single letter label used to mark\n"
		              "the selection and/or cursor position.\n\n"
		              "(To skip this dialog, use the accelerator\n"
		              "key, followed immediately by the letter)"), 
		QLineEdit::Normal, 
		QString(),
		&ok);
		
	if(!ok) {
		return;
	}
	
	strcpy(letterText, result.toLatin1().data());	

	if (strlen(letterText) != 1 || !isalpha((unsigned char)letterText[0])) {
		QApplication::beep();
		return;
	}
	
	params[0] = letterText;
	params[1] = "extend";
	XtCallActionProc(window->lastFocus_, "goto_mark", nullptr, (char **)params, extend ? 2 : 1);
}

/*
** Process a command to mark a selection.  Expects the user to continue
** the command by typing a label character.  Handles both correct user
** behavior (type a character a-z) or bad behavior (do nothing or type
** something else).
*/
void BeginMarkCommand(Document *window) {
	XtInsertEventHandler(window->lastFocus_, KeyPressMask, False, markKeyCB, window, XtListHead);
	window->markTimeoutID_ = XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell_), 4000, markTimeoutProc, window->lastFocus_);
}

/*
** Process a command to go to a marked selection.  Expects the user to
** continue the command by typing a label character.  Handles both correct
** user behavior (type a character a-z) or bad behavior (do nothing or type
** something else).
*/
void BeginGotoMarkCommand(Document *window, int extend) {
	XtInsertEventHandler(window->lastFocus_, KeyPressMask, False, extend ? gotoMarkExtendKeyCB : gotoMarkKeyCB, window, XtListHead);
	window->markTimeoutID_ = XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell_), 4000, markTimeoutProc, window->lastFocus_);
}

/*
** Xt timer procedure for removing event handler if user failed to type a
** mark character withing the allowed time
*/
static void markTimeoutProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	Widget w = static_cast<Widget>(clientData);
	Document *window = Document::WidgetToWindow(w);

	XtRemoveEventHandler(w, KeyPressMask, False, markKeyCB, window);
	XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkKeyCB, window);
	XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkExtendKeyCB, window);
	window->markTimeoutID_ = 0;
}

/*
** Temporary event handlers for keys pressed after the mark or goto-mark
** commands, If the key is valid, grab the key event and call the action
** procedure to mark (or go to) the selection, otherwise, remove the handler
** and give up.
*/
static void processMarkEvent(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch, char *action, int extend) {

	(void)clientData;

	auto e = reinterpret_cast<XKeyEvent *>(event);
	Document *window = Document::WidgetToWindow(w);
	Modifiers modifiers;
	KeySym keysym;
	const char *params[2];
	char string[2];

	XtTranslateKeycode(TheDisplay, e->keycode, e->state, &modifiers, &keysym);
	if ((keysym >= 'A' && keysym <= 'Z') || (keysym >= 'a' && keysym <= 'z')) {
		string[0] = toupper(keysym);
		string[1] = '\0';
		params[0] = string;
		params[1] = "extend";
		XtCallActionProc(window->lastFocus_, action, event, (char **)params, extend ? 2 : 1);
		*continueDispatch = False;
	}
	XtRemoveEventHandler(w, KeyPressMask, False, markKeyCB, window);
	XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkKeyCB, window);
	XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkExtendKeyCB, window);
	XtRemoveTimeOut(window->markTimeoutID_);
}
static void markKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch) {
	processMarkEvent(w, clientData, event, continueDispatch, (String) "mark", False);
}
static void gotoMarkKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch) {
	processMarkEvent(w, clientData, event, continueDispatch, (String) "goto_mark", False);
}
static void gotoMarkExtendKeyCB(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch) {
	processMarkEvent(w, clientData, event, continueDispatch, (String) "goto_mark", True);
}

void AddMark(Document *window, Widget widget, char label) {
	int index;

	/* look for a matching mark to re-use, or advance
	   nMarks to create a new one */
	label = toupper(label);
	for (index = 0; index < window->nMarks_; index++) {
		if (window->markTable_[index].label == label)
			break;
	}
	if (index >= MAX_MARKS) {
		fprintf(stderr, "no more marks allowed\n"); // shouldn't happen 
		return;
	}
	if (index == window->nMarks_)
		window->nMarks_++;

	// store the cursor location and selection position in the table 
	window->markTable_[index].label = label;
	memcpy(&window->markTable_[index].sel, &window->buffer_->primary_, sizeof(TextSelection));
	window->markTable_[index].cursorPos = TextGetCursorPos(widget);
}

void GotoMark(Document *window, Widget w, char label, int extendSel) {
	int index, oldStart, newStart, oldEnd, newEnd, cursorPos;
	TextSelection *sel, *oldSel;

	// look up the mark in the mark table 
	label = toupper(label);
	for (index = 0; index < window->nMarks_; index++) {
		if (window->markTable_[index].label == label)
			break;
	}
	if (index == window->nMarks_) {
		QApplication::beep();
		return;
	}

	// reselect marked the selection, and move the cursor to the marked pos 
	sel = &window->markTable_[index].sel;
	oldSel = &window->buffer_->primary_;
	cursorPos = window->markTable_[index].cursorPos;
	if (extendSel) {
		oldStart = oldSel->selected ? oldSel->start : TextGetCursorPos(w);
		oldEnd = oldSel->selected ? oldSel->end : TextGetCursorPos(w);
		newStart = sel->selected ? sel->start : cursorPos;
		newEnd = sel->selected ? sel->end : cursorPos;
		window->buffer_->BufSelect(oldStart < newStart ? oldStart : newStart, oldEnd > newEnd ? oldEnd : newEnd);
	} else {
		if (sel->selected) {
			if (sel->rectangular)
				window->buffer_->BufRectSelect(sel->start, sel->end, sel->rectStart, sel->rectEnd);
			else
				window->buffer_->BufSelect(sel->start, sel->end);
		} else
			window->buffer_->BufUnselect();
	}

	/* Move the window into a pleasing position relative to the selection
	   or cursor.   MakeSelectionVisible is not great with multi-line
	   selections, and here we will sometimes give it one.  And to set the
	   cursor position without first using the less pleasing capability
	   of the widget itself for bringing the cursor in to view, you have to
	   first turn it off, set the position, then turn it back on. */
	XtVaSetValues(w, textNautoShowInsertPos, False, nullptr);
	TextSetCursorPos(w, cursorPos);
	window->MakeSelectionVisible(window->lastFocus_);
	XtVaSetValues(w, textNautoShowInsertPos, True, nullptr);
}

/*
** Keep the marks in the windows book-mark table up to date across
** changes to the underlying buffer
*/
void UpdateMarkTable(Document *window, int pos, int nInserted, int nDeleted) {
	int i;

	for (i = 0; i < window->nMarks_; i++) {
		maintainSelection(&window->markTable_[i].sel, pos, nInserted, nDeleted);
		maintainPosition(&window->markTable_[i].cursorPos, pos, nInserted, nDeleted);
	}
}

/*
** Update a selection across buffer modifications specified by
** "pos", "nDeleted", and "nInserted".
*/
static void maintainSelection(TextSelection *sel, int pos, int nInserted, int nDeleted) {
	if (!sel->selected || pos > sel->end)
		return;
	maintainPosition(&sel->start, pos, nInserted, nDeleted);
	maintainPosition(&sel->end, pos, nInserted, nDeleted);
	if (sel->end <= sel->start)
		sel->selected = False;
}

/*
** Update a position across buffer modifications specified by
** "modPos", "nDeleted", and "nInserted".
*/
static void maintainPosition(int *position, int modPos, int nInserted, int nDeleted) {
	if (modPos > *position)
		return;
	if (modPos + nDeleted <= *position)
		*position += nInserted - nDeleted;
	else
		*position = modPos;
}
