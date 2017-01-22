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

#include "DocumentWidget.h"
#include "MainWindow.h"
#include "TextArea.h"
#include "TextHelper.h"
#include "TextDisplay.h"
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
#include "util/fileUtils.h"

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

static void fileCB(Widget widget, Document *window, Atom *sel, Atom *type, char *value, int *length, int *format);

static void processMarkEvent(Widget w, XtPointer clientData, XEvent *event, Boolean *continueDispatch, char *action, int extend);
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
		if (isdigit((uint8_t)*endptr) || *endptr == '-' || *endptr == '+') {
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
		strcpy(filename, window->path_.toLatin1().data());
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
				EditExistingFile(GetPrefOpenInTab() ? window : nullptr, QLatin1String(filename), QLatin1String(pathname), 0, nullptr, False, nullptr, GetPrefOpenInTab(), False);
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

void SelectNumberedLineEx(DocumentWidget *document, TextArea *area, int lineNum) {
    int i, lineStart = 0, lineEnd;

    // count lines to find the start and end positions for the selection
    if (lineNum < 1)
        lineNum = 1;
    lineEnd = -1;
    for (i = 1; i <= lineNum && lineEnd < document->buffer_->BufGetLength(); i++) {
        lineStart = lineEnd + 1;
        lineEnd = document->buffer_->BufEndOfLine( lineStart);
    }

    // highlight the line
    if (i > lineNum) {
        // Line was found
        if (lineEnd < document->buffer_->BufGetLength()) {
            document->buffer_->BufSelect(lineStart, lineEnd + 1);
        } else {
            // Don't select past the end of the buffer !
            document->buffer_->BufSelect(lineStart, document->buffer_->BufGetLength());
        }
    } else {
        /* Line was not found -> position the selection & cursor at the end
           without making a real selection and beep */
        lineStart = document->buffer_->BufGetLength();
        document->buffer_->BufSelect(lineStart, lineStart);
        QApplication::beep();
    }
    document->MakeSelectionVisible(area);

    area->TextSetCursorPos(lineStart);
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
	
	auto textD = window->lastFocus();
	textD->TextSetCursorPos(lineStart);
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
		XtCallActionProc(window->lastFocus_, action, event, const_cast<char **>(params), extend ? 2 : 1);
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

void AddMarkEx(MainWindow *window, DocumentWidget *document, TextArea *area, QChar label) {

    Q_UNUSED(window);

    int index;

    /* look for a matching mark to re-use, or advance
       nMarks to create a new one */
    label = label.toUpper();

    for (index = 0; index < document->nMarks_; index++) {
        if (document->markTable_[index].label == label.toLatin1())
            break;
    }
    if (index >= MAX_MARKS) {
        fprintf(stderr, "no more marks allowed\n"); // shouldn't happen
        return;
    }

    if (index == document->nMarks_) {
        document->nMarks_++;
    }

    // store the cursor location and selection position in the table
    document->markTable_[index].label = label.toLatin1();
    memcpy(&document->markTable_[index].sel, &document->buffer_->primary_, sizeof(TextSelection));

    document->markTable_[index].cursorPos = area->TextGetCursorPos();
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
