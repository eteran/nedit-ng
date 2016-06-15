/*******************************************************************************
*                                                                              *
* TextDisplay.c - Display text from a text buffer                              *
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
* June 15, 1995                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QMessageBox>

#include "TextDisplay.h"
#include "Document.h"
#include "StyleTableEntry.h"
#include "TextBuffer.h"
#include "DragStates.h"
#include "MultiClickStates.h"
#include "text.h"
#include "textP.h"
#include "nedit.h"
#include "calltips.h"
#include "highlight.h"
#include "Rangeset.h"
#include "RangesetTable.h"
#include "TextHelper.h"
#include "util/misc.h"
#include "util/MotifHelper.h"
#include "util/memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <algorithm>

#include <Xm/Xm.h>
#include <X11/Xatom.h>
#include <Xm/Text.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/Label.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>

namespace {

/* Number of pixels of motion from the initial (grab-focus) button press
   required to begin recognizing a mouse drag for the purpose of making a
   selection */
const int SELECT_THRESHOLD = 5;

// Length of delay in milliseconds for vertical autoscrolling
const int VERTICAL_SCROLL_DELAY = 50;

const int CALLTIP_EDGE_GUARD = 5;

/* temporary structure for passing data to the event handler for completing
   selection requests (the hard way, via xlib calls) */
struct selectNotifyInfo {
	int action;
	XtIntervalId timeoutProcID;
	Time timeStamp;
	Widget widget;
	char *actionText;
	int length;
};

const int N_SELECT_TARGETS = 7;
const int N_ATOMS          = 11;

const int NEDIT_HIDE_CURSOR_MASK = (KeyPressMask);
const int NEDIT_SHOW_CURSOR_MASK = (FocusChangeMask | PointerMotionMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask);

Cursor empty_cursor = 0;

/* Masks for text drawing methods.  These are or'd together to form an
   integer which describes what drawing calls to use to draw a string */
const int FILL_SHIFT         = 8;
const int SECONDARY_SHIFT    = 9;
const int PRIMARY_SHIFT      = 10;
const int HIGHLIGHT_SHIFT    = 11;
const int STYLE_LOOKUP_SHIFT = 0;
const int BACKLIGHT_SHIFT    = 12;

const int FILL_MASK         = (1 << FILL_SHIFT);
const int SECONDARY_MASK    = (1 << SECONDARY_SHIFT);
const int PRIMARY_MASK      = (1 << PRIMARY_SHIFT);
const int HIGHLIGHT_MASK    = (1 << HIGHLIGHT_SHIFT);
const int STYLE_LOOKUP_MASK = (0xff << STYLE_LOOKUP_SHIFT);
const int BACKLIGHT_MASK    = (0xff << BACKLIGHT_SHIFT);

const int RANGESET_SHIFT = 20;
const int RANGESET_MASK  = (0x3F << RANGESET_SHIFT);

/* If you use both 32-Bit Style mask layout:
   Bits +----------------+----------------+----------------+----------------+
    hex |1F1E1D1C1B1A1918|1716151413121110| F E D C B A 9 8| 7 6 5 4 3 2 1 0|
    dec |3130292827262524|2322212019181716|151413121110 9 8| 7 6 5 4 3 2 1 0|
        +----------------+----------------+----------------+----------------+
   Type |             r r| r r r r b b b b| b b b b H 1 2 F| s s s s s s s s|
        +----------------+----------------+----------------+----------------+
   where: s - style lookup value (8 bits)
        F - fill (1 bit)
        2 - secondary selection  (1 bit)
        1 - primary selection (1 bit)
        H - highlight (1 bit)
        b - backlighting index (8 bits)
        r - rangeset index (6 bits)
   This leaves 6 "unused" bits */

/* Maximum displayable line length (how many characters will fit across the
   widest window).  This amount of memory is temporarily allocated from the
   stack in the redisplayLine routine for drawing strings */
const int MAX_DISP_LINE_LEN = 1000;

enum positionTypes {
	CURSOR_POS,
	CHARACTER_POS
};

enum atomIndex {
	A_TEXT, 
	A_TARGETS, 
	A_MULTIPLE, 
	A_TIMESTAMP, 
	A_INSERT_SELECTION, 
	A_DELETE, 
	A_CLIPBOARD, 
	A_INSERT_INFO, 
	A_ATOM_PAIR, 
	A_MOTIF_DESTINATION, 
	A_COMPOUND_TEXT
};

/* Results passed back to the convert proc processing an INSERT_SELECTION
   request, by getInsertSelection when the selection to insert has been
   received and processed */
enum insertResultFlags {
	INSERT_WAITING, 
	UNSUCCESSFUL_INSERT, 
	SUCCESSFUL_INSERT
};

/* Actions for selection notify event handler upon receiving confermation
   of a successful convert selection request */
enum selectNotifyActions {
	UNSELECT_SECONDARY, 
	REMOVE_SECONDARY, 
	EXCHANGE_SECONDARY
};

/*
** Find the left and right margins of text between "start" and "end" in
** buffer "buf".  Note that "start is assumed to be at the start of a line.
*/
// TODO(eteran): make this a member of TextBuffer?
void findTextMargins(TextBuffer *buf, int start, int end, int *leftMargin, int *rightMargin) {
	char c;
	int pos, width = 0, maxWidth = 0, minWhite = INT_MAX, inWhite = true;

	for (pos = start; pos < end; pos++) {
		c = buf->BufGetCharacter(pos);
		if (inWhite && c != ' ' && c != '\t') {
			inWhite = False;
			if (width < minWhite)
				minWhite = width;
		}
		if (c == '\n') {
			if (width > maxWidth)
				maxWidth = width;
			width = 0;
			inWhite = true;
		} else
			width += TextBuffer::BufCharWidth(c, width, buf->tabDist_, buf->nullSubsChar_);
	}
	if (width > maxWidth)
		maxWidth = width;
	*leftMargin = minWhite == INT_MAX ? 0 : minWhite;
	*rightMargin = maxWidth;
}

int min3(int i1, int i2, int i3) {
	return std::min(i1, std::min(i2, i3));
}

int max3(int i1, int i2, int i3) {
    return std::max(i1, std::max(i2, i3));
}

/*
** Maintain boundaries of changed region between two buffers which
** start out with identical contents, but diverge through insertion,
** deletion, and replacement, such that the buffers can be reconciled
** by replacing the changed region of either buffer with the changed
** region of the other.
**
** rangeStart is the beginning of the modification region in the shared
** coordinates of both buffers (which are identical up to rangeStart).
** modRangeEnd is the end of the changed region for the buffer being
** modified, unmodRangeEnd is the end of the region for the buffer NOT
** being modified.  A value of -1 in rangeStart indicates that there
** have been no modifications so far.
*/
void trackModifyRange(int *rangeStart, int *modRangeEnd, int *unmodRangeEnd, int modPos, int nInserted, int nDeleted) {
	if (*rangeStart == -1) {
		*rangeStart    = modPos;
		*modRangeEnd   = modPos + nInserted;
		*unmodRangeEnd = modPos + nDeleted;
	} else {
		if (modPos < *rangeStart) {
			*rangeStart = modPos;
		}
		
		if (modPos + nDeleted > *modRangeEnd) {
			*unmodRangeEnd += modPos + nDeleted - *modRangeEnd;
			*modRangeEnd = modPos + nInserted;
		} else {
			*modRangeEnd += nInserted - nDeleted;
		}
	}
}

/*
** Find a text position in buffer "buf" by counting forward or backward
** from a reference position with known line number
*/
// TODO(eteran): make this a member of TextBuffer?
int findRelativeLineStart(const TextBuffer *buf, int referencePos, int referenceLineNum, int newLineNum) {
	if (newLineNum < referenceLineNum)
		return buf->BufCountBackwardNLines(referencePos, referenceLineNum - newLineNum);
	else if (newLineNum > referenceLineNum)
		return buf->BufCountForwardNLines(referencePos, newLineNum - referenceLineNum);
	return buf->BufStartOfLine(referencePos);
}

/*
** Maintain a cache of interned atoms.  To reference one, use the constant
** from the enum, atomIndex, above.
*/
static Atom getAtom(Display *display, int atomNum) {
	static Atom atomList[N_ATOMS] = {0};
	static const char *atomNames[N_ATOMS] = {"TEXT", "TARGETS", "MULTIPLE", "TIMESTAMP", "INSERT_SELECTION", "DELETE", "CLIPBOARD", "INSERT_INFO", "ATOM_PAIR", "MOTIF_DESTINATION", "COMPOUND_TEXT"};

	if (atomList[atomNum] == 0)
		atomList[atomNum] = XInternAtom(display, atomNames[atomNum], False);
	return atomList[atomNum];
}

/*
** Called when data arrives from request resulting from processing an
** INSERT_SELECTION request.  If everything is in order, inserts it at
** the cursor or replaces pending delete selection in widget "w", and sets
** the flag passed in clientData to SUCCESSFUL_INSERT or UNSUCCESSFUL_INSERT
** depending on the success of the operation.
*/
void getInsertSelectionCB(Widget w, XtPointer clientData, Atom *selType, Atom *type, XtPointer value, unsigned long *length, int *format) {

	(void)selType;

	auto textD      = textD_of(w);
	TextBuffer *buf = textD->TextGetBuffer();
	auto resultFlag = static_cast<int *>(clientData);

	// Confirm that the returned value is of the correct type 
	if (*type != XA_STRING || *format != 8 || value == nullptr) {
		XtFree((char *)value);
		*resultFlag = UNSUCCESSFUL_INSERT;
		return;
	}

	// Copy the string just to make space for the null character 
	std::string string(static_cast<char *>(value), *length);

	/* If the string contains ascii-nul characters, substitute something
	   else, or give up, warn, and refuse */
	if (!buf->BufSubstituteNullCharsEx(string)) {
		fprintf(stderr, "Too much binary data, giving up\n");
		XtFree((char *)value);
		return;
	}

	// Insert it in the text widget 
	textD->TextInsertAtCursorEx(string, nullptr, true, text_of(w).P_autoWrapPastedText);
	*resultFlag = SUCCESSFUL_INSERT;

	// This callback is required to free the memory passed to it thru value 
	XtFree((char *)value);
}

/*
** Selection converter procedure used by the widget when it is the selection
** owner to provide data in the format requested by the selection requestor.
**
** Note: Memory left in the *value field is freed by Xt as long as there is no
** done_proc procedure registered in the XtOwnSelection call where this
** procdeure is registered
*/
Boolean convertSelectionCB(Widget w, Atom *selType, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format) {
	XSelectionRequestEvent *event = XtGetSelectionRequest(w, *selType, nullptr);
	auto buf = textD_of(w)->TextGetBuffer();
	Display *display = XtDisplay(w);
	Atom *targets, dummyAtom;
	unsigned long nItems, dummyULong;
	Atom *reqAtoms;
	int getFmt, result = INSERT_WAITING;
	XEvent nextEvent;

	// target is text, string, or compound text 
	if (*target == XA_STRING || *target == getAtom(display, A_TEXT) || *target == getAtom(display, A_COMPOUND_TEXT)) {
		/* We really don't directly support COMPOUND_TEXT, but recent
		   versions gnome-terminal incorrectly ask for it, even though
		   don't declare that we do.  Just reply in string format. */
		   
		// TODO(eteran): inefficient copy here, but we plan to deprecate X Toolkit direct usage anyway
		std::string s = buf->BufGetSelectionTextEx();
		buf->BufUnsubstituteNullCharsEx(s);
		
		char *str = XtStringDup(s);
		   
		*type   = XA_STRING;
		*value  = str;
		*length = s.size();
		*format = 8;

		return true;
	}

	// target is "TARGETS", return a list of targets we can handle 
	if (*target == getAtom(display, A_TARGETS)) {
		targets = (Atom *)XtMalloc(sizeof(Atom) * N_SELECT_TARGETS);
		
		targets[0] = XA_STRING;
		targets[1] = getAtom(display, A_TEXT);
		targets[2] = getAtom(display, A_TARGETS);
		targets[3] = getAtom(display, A_MULTIPLE);
		targets[4] = getAtom(display, A_TIMESTAMP);
		targets[5] = getAtom(display, A_INSERT_SELECTION);
		targets[6] = getAtom(display, A_DELETE);
		
		*type   = XA_ATOM;
		*value  = targets;
		*length = N_SELECT_TARGETS;
		*format = 32;
		return true;
	}

	/* target is "INSERT_SELECTION":  1) get the information about what
	   selection and target to use to get the text to insert, from the
	   property named in the property field of the selection request event.
	   2) initiate a get value request for the selection and target named
	   in the property, and WAIT until it completes */
	if (*target == getAtom(display, A_INSERT_SELECTION)) {
		if (text_of(w).P_readOnly)
			return false;
		if (XGetWindowProperty(event->display, event->requestor, event->property, 0, 2, False, AnyPropertyType, &dummyAtom, &getFmt, &nItems, &dummyULong, (uint8_t **)&reqAtoms) != Success || getFmt != 32 || nItems != 2)
			return false;
		if (reqAtoms[1] != XA_STRING)
			return false;
		XtGetSelectionValue(w, reqAtoms[0], reqAtoms[1], getInsertSelectionCB, &result, event->time);
		XFree((char *)reqAtoms);
		while (result == INSERT_WAITING) {
			XtAppNextEvent(XtWidgetToApplicationContext(w), &nextEvent);
			XtDispatchEvent(&nextEvent);
		}
		*type = getAtom(display, A_INSERT_SELECTION);
		*format = 8;
		*value = nullptr;
		*length = 0;
		return result == SUCCESSFUL_INSERT;
	}

	// target is "DELETE": delete primary selection 
	if (*target == getAtom(display, A_DELETE)) {
		buf->BufRemoveSelected();
		*length = 0;
		*format = 8;
		*type = getAtom(display, A_DELETE);
		*value = nullptr;
		return true;
	}

	/* targets TIMESTAMP and MULTIPLE are handled by the toolkit, any
	   others are unrecognized, return False */
	return false;
}

void loseSelectionCB(Widget w, Atom *selType) {

	(void)selType;

	auto tw = reinterpret_cast<TextWidget>(w);
	TextSelection *sel = &textD_of(tw)->TextGetBuffer()->primary_;
	char zeroWidth = sel->rectangular ? sel->zeroWidth : 0;

	/* For zero width rect. sel. we give up the selection but keep the
	    zero width tag. */
	textD_of(tw)->setSelectionOwner(false);
	textD_of(tw)->TextGetBuffer()->BufUnselect();
	sel->zeroWidth = zeroWidth;
}

/*
** This routine is called every time there is a modification made to the
** buffer to which this callback is attached, with an argument of the text
** widget that has been designated (by HandleXSelections) to handle its
** selections.  It checks if the status of the selection in the buffer
** has changed since last time, and owns or disowns the X selection depending
** on the status of the primary selection in the buffer.  If it is not allowed
** to take ownership of the selection, it unhighlights the text in the buffer
** (Being in the middle of a modify callback, this has a somewhat complicated
** result, since later callbacks will see the second modifications first).
*/
void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg) {

	(void)pos;
	(void)nInserted;
	(void)nRestyled;
	(void)nDeleted;
	(void)deletedText;

	TextWidget w = static_cast<TextWidget>(cbArg);
	Time time    = XtLastTimestampProcessed(XtDisplay((Widget)w));
	int selected = textD_of(w)->TextGetBuffer()->primary_.selected;
	int isOwner  = textD_of(w)->getSelectionOwner();

	/* If the widget owns the selection and the buffer text is still selected,
	   or if the widget doesn't own it and there's no selection, do nothing */
	if ((isOwner && selected) || (!isOwner && !selected))
		return;

	/* Don't disown the selection here.  Another application (namely: klipper)
	   may try to take it when it thinks nobody has the selection.  We then
	   lose it, making selection-based macro operations fail.  Disowning
	   is really only for when the widget is destroyed to avoid a convert
	   callback from firing at a bad time. */

	// Take ownership of the selection 
	if (!XtOwnSelection((Widget)w, XA_PRIMARY, time, convertSelectionCB, loseSelectionCB, nullptr)) {
		textD_of(w)->TextGetBuffer()->BufUnselect();
	} else {
		textD_of(w)->setSelectionOwner(true);
	}
}

/*
** Called when data arrives from a request for the PRIMARY selection.  If
** everything is in order, it inserts it at the cursor in the requesting
** widget.
*/
void getSelectionCB(Widget w, XtPointer clientData, Atom *selType, Atom *type, XtPointer value, unsigned long *length, int *format) {

	(void)selType;

	auto textD = textD_of(w);
	int isColumnar = *(int *)clientData;
	int cursorLineStart, cursorPos, column, row;

	// Confirm that the returned value is of the correct type 
	if (*type != XA_STRING || *format != 8) {
		XtFree((char *)value);
		return;
	}

	/* Copy the string just to make space for the null character (this may
	   not be necessary, XLib documentation claims a nullptr is already added,
	   but the Xt documentation for this routine makes no such claim) */
	std::string string(static_cast<char *>(value), *length);

	/* If the string contains ascii-nul characters, substitute something
	   else, or give up, warn, and refuse */
	if (!textD->TextGetBuffer()->BufSubstituteNullCharsEx(string)) {
		fprintf(stderr, "Too much binary data, giving up\n");
		XtFree((char *)value);
		return;
	}

	// Insert it in the text widget 
	if (isColumnar) {
		cursorPos = textD->TextDGetInsertPosition();
		cursorLineStart = textD->TextGetBuffer()->BufStartOfLine(cursorPos);
		textD->TextDXYToUnconstrainedPosition(
			textD_of(w)->getButtonDownCoord(),
			&row,
			&column);
			
		textD->TextGetBuffer()->BufInsertColEx(column, cursorLineStart, string, nullptr, nullptr);
		textD->TextDSetInsertPosition(textD->TextGetBuffer()->cursorPosHint_);
	} else
		textD->TextInsertAtCursorEx(string, nullptr, False, text_of(w).P_autoWrapPastedText);

	/* The selection requstor is required to free the memory passed
	   to it via value */
	XtFree((char *)value);
}

/*
** Event handler for SelectionNotify events, to finish off INSERT_SELECTION
** requests which must be done through the lower
** level (and more complicated) XLib selection mechanism.  Matches the
** time stamp in the request against the time stamp stored when the selection
** request was made to find the selectionNotify event that it was installed
** to catch.  When it finds the correct event, it does the action it was
** installed to do, and removes itself and its backup timer (which would do
** the clean up if the selectionNotify event never arrived.)
*/
void selectNotifyEH(Widget w, XtPointer data, XEvent *event, Boolean *continueDispatch) {

	(void)continueDispatch;

	auto buf    = textD_of(w)->TextGetBuffer();
	auto e      = reinterpret_cast<XSelectionEvent *>(event);
	auto cbInfo = static_cast<selectNotifyInfo *>(data);
	int selStart, selEnd;

	/* Check if this was the selection request for which this handler was
	   set up, if not, do nothing */
	if (event->type != SelectionNotify || e->time != cbInfo->timeStamp)
		return;

	/* The time stamp matched, remove this event handler and its
	   backup timer procedure */
	XtRemoveEventHandler(w, 0, true, selectNotifyEH, data);
	XtRemoveTimeOut(cbInfo->timeoutProcID);

	/* Check if the request succeeded, if not, beep, remove any existing
	   secondary selection, and return */
	if (e->property == None) {
		QApplication::beep();
		buf->BufSecondaryUnselect();
		XtDisownSelection(w, XA_SECONDARY, e->time);
		XtFree(cbInfo->actionText);
		delete cbInfo;
		return;
	}

	/* Do the requested action, if the action is exchange, also clean up
	   the properties created for returning the primary selection and making
	   the MULTIPLE target request */
	if (cbInfo->action == REMOVE_SECONDARY) {
		buf->BufRemoveSecSelect();
	} else if (cbInfo->action == EXCHANGE_SECONDARY) {
		std::string string(cbInfo->actionText, cbInfo->length);

		selStart = buf->secondary_.start;
		if (buf->BufSubstituteNullCharsEx(string)) {
			buf->BufReplaceSecSelectEx(string);
			if (buf->secondary_.rectangular) {
				/*... it would be nice to re-select, but probably impossible */
				textD_of(w)->TextDSetInsertPosition(buf->cursorPosHint_);
			} else {
				selEnd = selStart + cbInfo->length;
				buf->BufSelect(selStart, selEnd);
				textD_of(w)->TextDSetInsertPosition(selEnd);
			}
		} else {
			fprintf(stderr, "Too much binary data\n");
		}
	}
	buf->BufSecondaryUnselect();
	XtDisownSelection(w, XA_SECONDARY, e->time);
	XtFree(cbInfo->actionText);
	delete cbInfo;
}

/*
** Xt timer procedure for timeouts on XConvertSelection requests, cleans up
** after a complete failure of the selection mechanism to return a selection
** notify event for a convert selection request
*/
void selectNotifyTimerProc(XtPointer clientData, XtIntervalId *id) {

	(void)id;

	auto cbInfo = static_cast<selectNotifyInfo *>(clientData);
	TextBuffer *buf = textD_of(cbInfo->widget)->TextGetBuffer();

	fprintf(stderr, "NEdit: timeout on selection request\n");
	XtRemoveEventHandler(cbInfo->widget, 0, true, selectNotifyEH, cbInfo);
	buf->BufSecondaryUnselect();
	XtDisownSelection(cbInfo->widget, XA_SECONDARY, cbInfo->timeStamp);
	XtFree(cbInfo->actionText);
	delete cbInfo;
}

/*
** Selection converter procedure used by the widget to (temporarily) provide
** the secondary selection data to a single requestor who has been asked
** to insert it.
*/
Boolean convertSecondaryCB(Widget w, Atom *selType, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format) {

	(void)selType;

	auto buf = textD_of(w)->TextGetBuffer();

	// target must be string 
	if (*target != XA_STRING && *target != getAtom(XtDisplay(w), A_TEXT))
		return false;

	/* Return the contents of the secondary selection.  The memory allocated
	   here is freed by the X toolkit */
	   
	// TODO(eteran): inefficient copy here, but we plan to deprecate X Toolkit direct usage anyway
	std::string s = buf->BufGetSecSelectTextEx();
	buf->BufUnsubstituteNullCharsEx(s);
	
	char *str = XtStringDup(s);
	
	*type   = XA_STRING;
	*value  = str;
	*length = s.size();
	*format = 8;

	return true;
}

void loseSecondaryCB(Widget w, Atom *selType) {

	(void)selType;
	(void)w;

	/* do nothing, secondary selections are transient anyhow, and it
	   will go away on its own */
}

/*
** Called when data arrives from an X primary selection request for the
** purpose of exchanging the primary and secondary selections.
** If everything is in order, stores the retrieved text temporarily and
** initiates a request to replace the primary selection with this widget's
** secondary selection.
*/
void getExchSelCB(Widget w, XtPointer clientData, Atom *selType, Atom *type, XtPointer value, unsigned long *length, int *format) {

	(void)selType;
	(void)clientData;

	// Confirm that there is a value and it is of the correct type 
	if (*length == 0 || !value || *type != XA_STRING || *format != 8) {
		XtFree((char *)value);
		QApplication::beep();
		textD_of(w)->TextGetBuffer()->BufSecondaryUnselect();
		return;
	}

	/* Request the selection owner to replace the primary selection with
	   this widget's secondary selection.  When complete, replace this
	   widget's secondary selection with text "value" and free it. */
	textD_of(w)->sendSecondary(XtLastTimestampProcessed(XtDisplay(w)), XA_PRIMARY, EXCHANGE_SECONDARY, (char *)value, *length);
}

/*
** Selection converter procedure used by the widget when it owns the Motif
** destination, to handle INSERT_SELECTION requests.
*/
Boolean convertMotifDestCB(Widget w, Atom *selType, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format) {
	XSelectionRequestEvent *event = XtGetSelectionRequest(w, *selType, nullptr);
	Display *display = XtDisplay(w);
	Atom *targets, dummyAtom;
	unsigned long nItems, dummyULong;
	Atom *reqAtoms;
	int getFmt, result = INSERT_WAITING;
	XEvent nextEvent;

	// target is "TARGETS", return a list of targets it can handle 
	if (*target == getAtom(display, A_TARGETS)) {
		targets = (Atom *)XtMalloc(sizeof(Atom) * 3);
		
		targets[0] = getAtom(display, A_TARGETS);
		targets[1] = getAtom(display, A_TIMESTAMP);
		targets[2] = getAtom(display, A_INSERT_SELECTION);
		
		*type   = XA_ATOM;
		*value  = targets;
		*length = 3;
		*format = 32;
		return true;
	}

	/* target is "INSERT_SELECTION":  1) get the information about what
	   selection and target to use to get the text to insert, from the
	   property named in the property field of the selection request event.
	   2) initiate a get value request for the selection and target named
	   in the property, and WAIT until it completes */
	if (*target == getAtom(display, A_INSERT_SELECTION)) {
		if (text_of(w).P_readOnly)
			return false;
		if (XGetWindowProperty(event->display, event->requestor, event->property, 0, 2, False, AnyPropertyType, &dummyAtom, &getFmt, &nItems, &dummyULong, (uint8_t **)&reqAtoms) != Success || getFmt != 32 || nItems != 2)
			return false;
		if (reqAtoms[1] != XA_STRING)
			return false;
		XtGetSelectionValue(w, reqAtoms[0], reqAtoms[1], getInsertSelectionCB, &result, event->time);
		XFree((char *)reqAtoms);
		while (result == INSERT_WAITING) {
			XtAppNextEvent(XtWidgetToApplicationContext(w), &nextEvent);
			XtDispatchEvent(&nextEvent);
		}
		*type = getAtom(display, A_INSERT_SELECTION);
		*format = 8;
		*value = nullptr;
		*length = 0;
		return result == SUCCESSFUL_INSERT;
	}

	/* target TIMESTAMP is handled by the toolkit and not passed here, any
	   others are unrecognized */
	return false;
}

void loseMotifDestCB(Widget w, Atom *selType) {

	(void)selType;
	
	auto textD = textD_of(w);

	textD->setMotifDestOwner(false);
	if (textD->getCursorStyle() == CARET_CURSOR) {
		textD->TextDSetCursorStyle(DIM_CURSOR);
	}
}

/*
** look at an action procedure's arguments to see if argument "key" has been
** specified in the argument list
*/
bool hasKey(const char *key, const String *args, const Cardinal *nArgs) {

	for (int i = 0; i < (int)*nArgs; i++) {
		if (strcasecmp(args[i], key) == 0) {
			return true;
		}
	}
	return false;
}

void ringIfNecessary(bool silent) {
	if (!silent) {
		QApplication::beep();
	}
}


}

static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id);
static GC allocateGC(Widget w, unsigned long valueMask, unsigned long foreground, unsigned long background, Font font, unsigned long dynamicMask, unsigned long dontCareMask);
static Pixel allocBGColor(Widget w, char *colorName, int *ok);
static int countLinesEx(view::string_view string);
static void bufModifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
static void bufPreDeleteCB(int pos, int nDeleted, void *cbArg);
static void releaseGC(Widget w, GC gc);
static void vScrollCB(Widget w, XtPointer clientData, XtPointer callData);
static void visibilityEH(Widget w, XtPointer data, XEvent *event, Boolean *continueDispatch);
static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData);

TextDisplay::TextDisplay(Widget widget,
						 Widget hScrollBar, 
						 Widget vScrollBar, 
						 Position left, 
						 Position top, 
						 Position width, 
						 Position height, 
						 Position lineNumLeft, 
						 Position lineNumWidth, 
						 TextBuffer *buffer, 
						 XFontStruct *fontStruct,
						 Pixel bgPixel, 
						 Pixel fgPixel, 
						 Pixel selectFGPixel, 
						 Pixel selectBGPixel, 
						 Pixel highlightFGPixel, 
						 Pixel highlightBGPixel, 
						 Pixel cursorFGPixel, 
						 Pixel lineNumFGPixel, 
						 bool continuousWrap, 
						 int wrapMargin, 
						 XmString bgClassString, 
						 Pixel calltipFGPixel, 
						 Pixel calltipBGPixel) {

	this->w                  = widget;
	this->rect               = { left, top, width, height };	
	this->cursorOn           = true;
	this->cursorPos          = 0;
	this->cursor             = Point{-100, -100};
	this->cursorToHint       = NO_HINT;
	this->cursorStyle        = NORMAL_CURSOR;
	this->cursorPreferredCol = -1;
	this->buffer             = buffer;
	this->firstChar          = 0;
	this->lastChar           = 0;
	this->nBufferLines       = 0;
	this->topLineNum         = 1;
	this->absTopLineNum      = 1;
	this->needAbsTopLineNum  = false;
	this->horizOffset        = 0;
	this->visibility         = VisibilityUnobscured;
	this->hScrollBar         = hScrollBar;
	this->vScrollBar         = vScrollBar;
	this->fontStruct         = fontStruct;
	this->ascent             = fontStruct->ascent;
	this->descent            = fontStruct->descent;
	this->fixedFontWidth     = fontStruct->min_bounds.width == fontStruct->max_bounds.width ? fontStruct->min_bounds.width : -1;
	this->styleBuffer        = nullptr;
	this->styleTable         = nullptr;
	this->nStyles            = 0;
	this->bgPixel            = bgPixel;
	this->fgPixel            = fgPixel;
	this->selectFGPixel      = selectFGPixel;
	this->highlightFGPixel   = highlightFGPixel;
	this->selectBGPixel      = selectBGPixel;
	this->highlightBGPixel   = highlightBGPixel;
	this->lineNumFGPixel     = lineNumFGPixel;
	this->cursorFGPixel      = cursorFGPixel;
	this->wrapMargin         = wrapMargin;
	this->continuousWrap     = continuousWrap;	
	this->styleGC            = allocateGC(this->w, 0, 0, 0, fontStruct->fid, GCClipMask | GCForeground | GCBackground, GCArcMode);
	this->lineNumLeft        = lineNumLeft;
	this->lineNumWidth       = lineNumWidth;
	this->nVisibleLines      = (height - 1) / (this->ascent + this->descent) + 1;
	
	allocateFixedFontGCs(
		fontStruct, 
		bgPixel, 
		fgPixel, 
		selectFGPixel, 
		selectBGPixel, 
		highlightFGPixel, 
		highlightBGPixel, 
		lineNumFGPixel);
	
	XGCValues gcValues;
	gcValues.foreground = cursorFGPixel;
	
	this->cursorFGGC        = XtGetGC(widget, GCForeground, &gcValues);
	this->lineStarts        = new int[this->nVisibleLines];
	this->lineStarts[0]     = 0;
	this->calltipW          = nullptr;
	this->calltipShell      = nullptr;
	this->calltip.ID        = 0;
	this->calltipFGPixel    = calltipFGPixel;
	this->calltipBGPixel    = calltipBGPixel;
	
	for (int i = 1; i < this->nVisibleLines; i++) {
		this->lineStarts[i] = -1;
	}
	
	this->bgClassPixel = nullptr;
	this->bgClass = nullptr;
	TextDSetupBGClasses(widget, bgClassString, &this->bgClassPixel, &this->bgClass, bgPixel);

	this->suppressResync   = false;
	this->nLinesDeleted    = 0;
	this->modifyingTabDist = 0;
	this->pointerHidden    = false;
	graphicsExposeQueue_   = nullptr;

	/* Attach an event handler to the widget so we can know the visibility
	   (used for choosing the fastest drawing method) */
	XtAddEventHandler(widget, VisibilityChangeMask, false, visibilityEH, this);

	/* Attach the callback to the text buffer for receiving modification
	   information */
	if (buffer) {
		buffer->BufAddModifyCB(bufModifiedCB, this);
		buffer->BufAddPreDeleteCB(bufPreDeleteCB, this);
	}

	// Initialize the scroll bars and attach movement callbacks
	if (vScrollBar) {
		XtVaSetValues(vScrollBar, XmNminimum, 1, XmNmaximum, 2, XmNsliderSize, 1, XmNrepeatDelay, 10, XmNvalue, 1, nullptr);
		XtAddCallback(vScrollBar, XmNdragCallback, vScrollCB, (XtPointer)this);
		XtAddCallback(vScrollBar, XmNvalueChangedCallback, vScrollCB, (XtPointer)this);
	}
	
	if (hScrollBar) {
		XtVaSetValues(hScrollBar, XmNminimum, 0, XmNmaximum, 1, XmNsliderSize, 1, XmNrepeatDelay, 10, XmNvalue, 0, XmNincrement, fontStruct->max_bounds.width, nullptr);
		XtAddCallback(hScrollBar, XmNdragCallback, hScrollCB, (XtPointer)this);
		XtAddCallback(hScrollBar, XmNvalueChangedCallback, hScrollCB, (XtPointer)this);
	}

	// Update the display to reflect the contents of the buffer
	if(buffer)
		bufModifiedCB(0, buffer->BufGetLength(), 0, 0, std::string(), this);

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();
	
	/* Start with the cursor blanked (widgets don't have focus on creation,
	   the initial FocusIn event will unblank it and get blinking started) */
	this->cursorOn = false;

	// Initialize the widget variables
	this->autoScrollProcID   = 0;
	this->cursorBlinkProcID  = 0;
	this->dragState          = NOT_CLICKED;
	this->multiClickState    = NO_CLICKS;
	this->lastBtnDown        = 0;
	this->selectionOwner     = false;
	this->motifDestOwner     = false;
	this->emTabsBeforeCursor = 0;
}

/*
** Free a text display and release its associated memory.  Note, the text
** BUFFER that the text display displays is a separate entity and is not
** freed, nor are the style buffer or style table.
*/
TextDisplay::~TextDisplay() {

	this->StopHandlingXSelections();
	TextBuffer *buf = this->textBuffer();
	
	
	if (buf->modifyProcs_.empty()) {
		delete buf;
	}

	if (this->cursorBlinkProcID != 0) {
		XtRemoveTimeOut(this->cursorBlinkProcID);
	}


	this->buffer->BufRemoveModifyCB(bufModifiedCB, this);
	this->buffer->BufRemovePreDeleteCB(bufPreDeleteCB, this);
	releaseGC(this->w, this->gc);
	releaseGC(this->w, this->selectGC);
	releaseGC(this->w, this->highlightGC);
	releaseGC(this->w, this->selectBGGC);
	releaseGC(this->w, this->highlightBGGC);
	releaseGC(this->w, this->styleGC);
	releaseGC(this->w, this->lineNumGC);

	delete[] this->lineStarts;

	while (TextDPopGraphicExposeQueueEntry()) {
	}

	delete [] this->bgClassPixel;
	delete [] this->bgClass;
}

/*
** Attach a text buffer to display, replacing the current buffer (if any)
*/
void TextDisplay::TextDSetBuffer(TextBuffer *buffer) {
	/* If the text display is already displaying a buffer, clear it off
	   of the display and remove our callback from it */
	if (this->buffer) {
		bufModifiedCB(0, 0, this->buffer->BufGetLength(), 0, std::string(), this);
		this->buffer->BufRemoveModifyCB(bufModifiedCB, this);
		this->buffer->BufRemovePreDeleteCB(bufPreDeleteCB, this);
	}

	/* Add the buffer to the display, and attach a callback to the buffer for
	   receiving modification information when the buffer contents change */
	this->buffer = buffer;
	buffer->BufAddModifyCB(bufModifiedCB, this);
	buffer->BufAddPreDeleteCB(bufPreDeleteCB, this);

	// Update the display
	bufModifiedCB(0, buffer->BufGetLength(), 0, 0, std::string(), this);
}

/*
** Attach (or remove) highlight information in text display and redisplay.
** Highlighting information consists of a style buffer which parallels the
** normal text buffer, but codes font and color information for the display;
** a style table which translates style buffer codes (indexed by buffer
** character - 65 (ASCII code for 'A')) into fonts and colors; and a callback
** mechanism for as-needed highlighting, triggered by a style buffer entry of
** "unfinishedStyle".  Style buffer can trigger additional redisplay during
** a normal buffer modification if the buffer contains a primary selection
** (see extendRangeForStyleMods for more information on this protocol).
**
** Style buffers, tables and their associated memory are managed by the caller.
*/
void TextDisplay::TextDAttachHighlightData(TextBuffer *styleBuffer, StyleTableEntry *styleTable, int nStyles, char unfinishedStyle, unfinishedStyleCBProc unfinishedHighlightCB, void *cbArg) {
	this->styleBuffer           = styleBuffer;
	this->styleTable            = styleTable;
	this->nStyles               = nStyles;
	this->unfinishedStyle       = unfinishedStyle;
	this->unfinishedHighlightCB = unfinishedHighlightCB;
	this->highlightCBArg        = cbArg;

	/* Call TextDSetFont to combine font information from style table and
	   primary font, adjust font-related parameters, and then redisplay */
	TextDSetFont(this->fontStruct);
}

// Change the (non syntax-highlit) colors
void TextDisplay::TextDSetColors(Pixel textFgP, Pixel textBgP, Pixel selectFgP, Pixel selectBgP, Pixel hiliteFgP, Pixel hiliteBgP, Pixel lineNoFgP, Pixel cursorFgP) {
	XGCValues values;
	Display *d = XtDisplay(this->w);

	// Update the stored pixels
	this->fgPixel          = textFgP;
	this->bgPixel          = textBgP;
	this->selectFGPixel    = selectFgP;
	this->selectBGPixel    = selectBgP;
	this->highlightFGPixel = hiliteFgP;
	this->highlightBGPixel = hiliteBgP;
	this->lineNumFGPixel   = lineNoFgP;
	this->cursorFGPixel    = cursorFgP;

	releaseGC(this->w, this->gc);
	releaseGC(this->w, this->selectGC);
	releaseGC(this->w, this->selectBGGC);
	releaseGC(this->w, this->highlightGC);
	releaseGC(this->w, this->highlightBGGC);
	releaseGC(this->w, this->lineNumGC);
	allocateFixedFontGCs(this->fontStruct, textBgP, textFgP, selectFgP, selectBgP, hiliteFgP, hiliteBgP, lineNoFgP);

	// Change the cursor GC (the cursor GC is not shared).
	values.foreground = cursorFgP;
	XChangeGC(d, this->cursorFGGC, GCForeground, &values);

	// Redisplay
	TextDRedisplayRect(this->rect);
	redrawLineNumbers(true);
}

/*
** Change the (non highlight) font
*/
void TextDisplay::TextDSetFont(XFontStruct *fontStruct) {
	Display *display = XtDisplay(this->w);
	int i, maxAscent = fontStruct->ascent, maxDescent = fontStruct->descent;
	int width, height, fontWidth;
	Pixel bgPixel, fgPixel, selectFGPixel, selectBGPixel;
	Pixel highlightFGPixel, highlightBGPixel, lineNumFGPixel;
	XGCValues values;
	XFontStruct *styleFont;

	// If font size changes, cursor will be redrawn in a new position
	blankCursorProtrusions();

	/* If there is a (syntax highlighting) style table in use, find the new
	   maximum font height for this text display */
	for (i = 0; i < this->nStyles; i++) {
		styleFont = this->styleTable[i].font;
		if (styleFont != nullptr && styleFont->ascent > maxAscent)
			maxAscent = styleFont->ascent;
		if (styleFont != nullptr && styleFont->descent > maxDescent)
			maxDescent = styleFont->descent;
	}
	this->ascent = maxAscent;
	this->descent = maxDescent;

	// If all of the current fonts are fixed and match in width, compute
	fontWidth = fontStruct->max_bounds.width;
	if (fontWidth != fontStruct->min_bounds.width)
		fontWidth = -1;
	else {
		for (i = 0; i < this->nStyles; i++) {
			styleFont = this->styleTable[i].font;
			if (styleFont != nullptr && (styleFont->max_bounds.width != fontWidth || styleFont->max_bounds.width != styleFont->min_bounds.width))
				fontWidth = -1;
		}
	}
	this->fixedFontWidth = fontWidth;

	// Don't let the height dip below one line, or bad things can happen
	if (this->rect.height < maxAscent + maxDescent)
		this->rect.height = maxAscent + maxDescent;

	/* Change the font.  In most cases, this means re-allocating the
	   affected GCs (they are shared with other widgets, and if the primary
	   font changes, must be re-allocated to change it). Unfortunately,
	   this requres recovering all of the colors from the existing GCs */
	this->fontStruct = fontStruct;
	XGetGCValues(display, this->gc, GCForeground | GCBackground, &values);
	fgPixel = values.foreground;
	bgPixel = values.background;
	XGetGCValues(display, this->selectGC, GCForeground | GCBackground, &values);
	selectFGPixel = values.foreground;
	selectBGPixel = values.background;
	XGetGCValues(display, this->highlightGC, GCForeground | GCBackground, &values);
	highlightFGPixel = values.foreground;
	highlightBGPixel = values.background;
	XGetGCValues(display, this->lineNumGC, GCForeground, &values);
	lineNumFGPixel = values.foreground;

	releaseGC(this->w, this->gc);
	releaseGC(this->w, this->selectGC);
	releaseGC(this->w, this->highlightGC);
	releaseGC(this->w, this->selectBGGC);
	releaseGC(this->w, this->highlightBGGC);
	releaseGC(this->w, this->lineNumGC);

	allocateFixedFontGCs(fontStruct, bgPixel, fgPixel, selectFGPixel, selectBGPixel, highlightFGPixel, highlightBGPixel, lineNumFGPixel);
	XSetFont(display, this->styleGC, fontStruct->fid);

	// Do a full resize to force recalculation of font related parameters
	width  = this->rect.width;
	height = this->rect.height;
	this->rect.width  = 0;
	this->rect.height = 0;
	TextDResize(width, height);

	/* if the shell window doesn't get resized, and the new fonts are
	   of smaller sizes, sometime we get some residual text on the
	   blank space at the bottom part of text area. Clear it here. */
	clearRect(this->gc, this->rect.left, this->rect.top + this->rect.height - maxAscent - maxDescent, this->rect.width, maxAscent + maxDescent);

	// Redisplay
	TextDRedisplayRect(this->rect);

	// Clean up line number area in case spacing has changed
	redrawLineNumbers(true);
}

int TextDisplay::TextDMinFontWidth(Boolean considerStyles) {
	int fontWidth = this->fontStruct->max_bounds.width;
	
	if (considerStyles) {
		for (int i = 0; i < this->nStyles; ++i) {
			int thisWidth = (this->styleTable[i].font)->min_bounds.width;
			if (thisWidth < fontWidth) {
				fontWidth = thisWidth;
			}
		}
	}
	return (fontWidth);
}

int TextDisplay::TextDMaxFontWidth(Boolean considerStyles) {
	int fontWidth = this->fontStruct->max_bounds.width;
	
	if (considerStyles) {
		for (int i = 0; i < this->nStyles; ++i) {
			int thisWidth = (this->styleTable[i].font)->max_bounds.width;
			if (thisWidth > fontWidth) {
				fontWidth = thisWidth;
			}
		}
	}
	return (fontWidth);
}

/*
** Change the size of the displayed text area
*/
void TextDisplay::TextDResize(int width, int height) {
	int oldVisibleLines = this->nVisibleLines;
	int canRedraw = XtWindow(this->w) != 0;
	int newVisibleLines = height / (this->ascent + this->descent);
	int redrawAll = false;
	int oldWidth = this->rect.width;
	int exactHeight = height - height % (this->ascent + this->descent);

	this->rect.width = width;
	this->rect.height = height;

	/* In continuous wrap mode, a change in width affects the total number of
	   lines in the buffer, and can leave the top line number incorrect, and
	   the top character no longer pointing at a valid line start */
	if (this->continuousWrap && this->wrapMargin == 0 && width != oldWidth) {
		int oldFirstChar = this->firstChar;
		this->nBufferLines = TextDCountLines(0, this->buffer->BufGetLength(), true);
		this->firstChar = TextDStartOfLine(this->firstChar);
		this->topLineNum = TextDCountLines(0, this->firstChar, True) + 1;
		redrawAll = true;
		offsetAbsLineNum(oldFirstChar);
	}

	/* reallocate and update the line starts array, which may have changed
	   size and/or contents. (contents can change in continuous wrap mode
	   when the width changes, even without a change in height) */
	if (oldVisibleLines < newVisibleLines) {
		delete[] this->lineStarts;
		this->lineStarts = new int[newVisibleLines];
	}

	this->nVisibleLines = newVisibleLines;
	calcLineStarts(0, newVisibleLines);
	calcLastChar();

	/* if the window became shorter, there may be partially drawn
	   text left at the bottom edge, which must be cleaned up */
	if (canRedraw && oldVisibleLines > newVisibleLines && exactHeight != height)
		XClearArea(XtDisplay(this->w), XtWindow(this->w), this->rect.left, this->rect.top + exactHeight, this->rect.width, height - exactHeight, false);

	/* if the window became taller, there may be an opportunity to display
	   more text by scrolling down */
	if (canRedraw && oldVisibleLines < newVisibleLines && this->topLineNum + this->nVisibleLines > this->nBufferLines)
		setScroll(std::max<int>(1, this->nBufferLines - this->nVisibleLines + 2 + text_of(w).P_cursorVPadding), this->horizOffset, false, false);

	/* Update the scroll bar page increment size (as well as other scroll
	   bar parameters.  If updating the horizontal range caused scrolling,
	   redraw */
	updateVScrollBarRange();
	if (updateHScrollBarRange())
		redrawAll = true;

	// If a full redraw is needed
	if (redrawAll && canRedraw)
		TextDRedisplayRect(this->rect);

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();

	/* Refresh the line number display to draw more line numbers, or
	   erase extras */
	redrawLineNumbers(true);

	// Redraw the calltip
	TextDRedrawCalltip(0);
}

/*
** Refresh a rectangle of the text display.  left and top are in coordinates of
** the text drawing window
*/
void TextDisplay::TextDRedisplayRect(const Rect &rect) {
	TextDRedisplayRect(rect.left, rect.top, rect.width, rect.height);
}

void TextDisplay::TextDRedisplayRect(int left, int top, int width, int height) {
	int fontHeight, firstLine, lastLine, line;

	// find the line number range of the display
	fontHeight = this->ascent + this->descent;
	firstLine = (top - this->rect.top - fontHeight + 1) / fontHeight;
	lastLine = (top + height - this->rect.top) / fontHeight;

	/* If the graphics contexts are shared using XtAllocateGC, their
	   clipping rectangles may have changed since the last use */
	resetClipRectangles();

	// draw the lines of text
	for (line = firstLine; line <= lastLine; line++)
		redisplayLine(line, left, left + width, 0, INT_MAX);

	// draw the line numbers if exposed area includes them
	if (this->lineNumWidth != 0 && left <= this->lineNumLeft + this->lineNumWidth)
		redrawLineNumbers(false);
}

/*
** Refresh all of the text between buffer positions "start" and "end"
** not including the character at the position "end".
** If end points beyond the end of the buffer, refresh the whole display
** after pos, including blank lines which are not technically part of
** any range of characters.
*/
void TextDisplay::textDRedisplayRange(int start, int end) {
	int i, startLine, lastLine, startIndex, endIndex;

	// If the range is outside of the displayed text, just return
	if (end < this->firstChar || (start > this->lastChar && !emptyLinesVisible()))
		return;

	// Clean up the starting and ending values
	if (start < 0)
		start = 0;
	if (start > this->buffer->BufGetLength())
		start = this->buffer->BufGetLength();
	if (end < 0)
		end = 0;
	if (end > this->buffer->BufGetLength())
		end = this->buffer->BufGetLength();

	// Get the starting and ending lines
	if (start < this->firstChar) {
		start = this->firstChar;
	}

	if (!posToVisibleLineNum(start, &startLine)) {
		startLine = this->nVisibleLines - 1;
	}

	if (end >= this->lastChar) {
		lastLine = this->nVisibleLines - 1;
	} else {
		if (!posToVisibleLineNum(end, &lastLine)) {
			// shouldn't happen
			lastLine = this->nVisibleLines - 1;
		}
	}

	// Get the starting and ending positions within the lines
	startIndex = (this->lineStarts[startLine] == -1) ? 0 : start - this->lineStarts[startLine];
	if (end >= this->lastChar) {
		/*  Request to redisplay beyond this->lastChar, so tell
		    redisplayLine() to display everything to infy.  */
		endIndex = INT_MAX;
	} else if (this->lineStarts[lastLine] == -1) {
		/*  Here, lastLine is determined by posToVisibleLineNum() (see
		    if/else above) but deemed to be out of display according to
		    this->lineStarts. */
		endIndex = 0;
	} else {
		endIndex = end - this->lineStarts[lastLine];
	}

	/* Reset the clipping rectangles for the drawing GCs which are shared
	   using XtAllocateGC, and may have changed since the last use */
	resetClipRectangles();

	/* If the starting and ending lines are the same, redisplay the single
	   line between "start" and "end" */
	if (startLine == lastLine) {
		redisplayLine(startLine, 0, INT_MAX, startIndex, endIndex);
		return;
	}

	// Redisplay the first line from "start"
	redisplayLine(startLine, 0, INT_MAX, startIndex, INT_MAX);

	// Redisplay the lines in between at their full width
	for (i = startLine + 1; i < lastLine; i++)
		redisplayLine(i, 0, INT_MAX, 0, INT_MAX);

	// Redisplay the last line to "end"
	redisplayLine(lastLine, 0, INT_MAX, 0, endIndex);
}

/*
** Set the scroll position of the text display vertically by line number and
** horizontally by pixel offset from the left margin
*/
void TextDisplay::TextDSetScroll(int topLineNum, int horizOffset) {
	int sliderSize, sliderMax;
	int vPadding = (int)(text_of(w).P_cursorVPadding);

	// Limit the requested scroll position to allowable values
	if (topLineNum < 1)
		topLineNum = 1;
	else if ((topLineNum > this->topLineNum) && (topLineNum > (this->nBufferLines + 2 - this->nVisibleLines + vPadding)))
		topLineNum = std::max<int>(this->topLineNum, this->nBufferLines + 2 - this->nVisibleLines + vPadding);
	XtVaGetValues(this->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
	if (horizOffset < 0)
		horizOffset = 0;
	if (horizOffset > sliderMax - sliderSize)
		horizOffset = sliderMax - sliderSize;

	setScroll(topLineNum, horizOffset, True, true);
}

/*
** Get the current scroll position for the text display, in terms of line
** number of the top line and horizontal pixel offset from the left margin
*/
void TextDisplay::TextDGetScroll(int *topLineNum, int *horizOffset) {
	*topLineNum = this->topLineNum;
	*horizOffset = this->horizOffset;
}

/*
** Set the position of the text insertion cursor for text display "textD"
*/
void TextDisplay::TextDSetInsertPosition(int newPos) {
	// make sure new position is ok, do nothing if it hasn't changed
	if (newPos == this->cursorPos)
		return;
	if (newPos < 0)
		newPos = 0;
	if (newPos > this->buffer->BufGetLength())
		newPos = this->buffer->BufGetLength();

	// cursor movement cancels vertical cursor motion column
	this->cursorPreferredCol = -1;

	// erase the cursor at it's previous position
	TextDBlankCursor();

	// draw it at its new position
	this->cursorPos = newPos;
	this->cursorOn = true;
	textDRedisplayRange(this->cursorPos - 1, this->cursorPos + 1);
}

void TextDisplay::TextDBlankCursor() {
	if (!this->cursorOn)
		return;

	blankCursorProtrusions();
	this->cursorOn = false;
	textDRedisplayRange(this->cursorPos - 1, this->cursorPos + 1);
}

void TextDisplay::TextDUnblankCursor() {
	if (!this->cursorOn) {
		this->cursorOn = true;
		textDRedisplayRange(this->cursorPos - 1, this->cursorPos + 1);
	}
}

void TextDisplay::TextDSetCursorStyle(CursorStyles style) {
	this->cursorStyle = style;
	blankCursorProtrusions();
	if (this->cursorOn) {
		textDRedisplayRange(this->cursorPos - 1, this->cursorPos + 1);
	}
}

void TextDisplay::TextDSetWrapMode(int wrap, int wrapMargin) {
	this->wrapMargin = wrapMargin;
	this->continuousWrap = wrap;

	// wrapping can change change the total number of lines, re-count
	this->nBufferLines = TextDCountLines(0, this->buffer->BufGetLength(), true);

	/* changing wrap margins wrap or changing from wrapped mode to non-wrapped
	   can leave the character at the top no longer at a line start, and/or
	   change the line number */
	this->firstChar = TextDStartOfLine(this->firstChar);
	this->topLineNum = TextDCountLines(0, this->firstChar, True) + 1;
	resetAbsLineNum();

	// update the line starts array
	calcLineStarts(0, this->nVisibleLines);
	calcLastChar();

	/* Update the scroll bar page increment size (as well as other scroll
	   bar parameters) */
	updateVScrollBarRange();
	updateHScrollBarRange();

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();

	// Do a full redraw
	TextDRedisplayRect(0, this->rect.top, this->rect.width + this->rect.left, this->rect.height);
}

int TextDisplay::TextDGetInsertPosition() const {
	return this->cursorPos;
}

/*
** Insert "text" at the current cursor location.  This has the same
** effect as inserting the text into the buffer using BufInsertEx and
** then moving the insert position after the newly inserted text, except
** that it's optimized to do less redrawing.
*/
void TextDisplay::TextDInsertEx(view::string_view text) {
	int pos = this->cursorPos;

	this->cursorToHint = pos + text.size();
	this->buffer->BufInsertEx(pos, text);
	this->cursorToHint = NO_HINT;
}

/*
** Insert "text" (which must not contain newlines), overstriking the current
** cursor location.
*/
void TextDisplay::TextDOverstrikeEx(view::string_view text) {
	int startPos    = this->cursorPos;
	TextBuffer *buf = this->buffer;
	int lineStart   = buf->BufStartOfLine(startPos);
	int textLen     = text.size();
	int p, endPos, indent, startIndent, endIndent;

	std::string paddedText;
	bool paddedTextSet = false;

	// determine how many displayed character positions are covered
	startIndent = this->buffer->BufCountDispChars(lineStart, startPos);
	indent = startIndent;
	for (char ch : text) {
		indent += TextBuffer::BufCharWidth(ch, indent, buf->tabDist_, buf->nullSubsChar_);
	}
	endIndent = indent;

	/* find which characters to remove, and if necessary generate additional
	   padding to make up for removed control characters at the end */
	indent = startIndent;
	for (p = startPos;; p++) {
		if (p == buf->BufGetLength())
			break;
		char ch = buf->BufGetCharacter(p);
		if (ch == '\n')
			break;
		indent += TextBuffer::BufCharWidth(ch, indent, buf->tabDist_, buf->nullSubsChar_);
		if (indent == endIndent) {
			p++;
			break;
		} else if (indent > endIndent) {
			if (ch != '\t') {
				p++;

				std::string padded;
				padded.append(text.begin(), text.end());
				padded.append(indent - endIndent, ' ');
				paddedText = std::move(padded);
				paddedTextSet = true;
			}
			break;
		}
	}
	endPos = p;

	this->cursorToHint = startPos + textLen;
	buf->BufReplaceEx(startPos, endPos, !paddedTextSet ? text : paddedText);
	this->cursorToHint = NO_HINT;
}

/*
** Translate window coordinates to the nearest text cursor position.
*/
int TextDisplay::TextDXYToPosition(Point coord) {
	return xyToPos(coord.x, coord.y, CURSOR_POS);
}

/*
** Translate window coordinates to the nearest character cell.
*/
int TextDisplay::TextDXYToCharPos(Point coord) {
	return xyToPos(coord.x, coord.y, CHARACTER_POS);
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
void TextDisplay::TextDXYToUnconstrainedPosition(Point coord, int *row, int *column) {
	xyToUnconstrainedPos(coord.x, coord.y, row, column, CURSOR_POS);
}

/*
** Translate line and column to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
int TextDisplay::TextDLineAndColToPos(int lineNum, int column) {
	int i;
	int lineEnd;
	int lineStart = 0;
	
	// Count lines
	if (lineNum < 1) {
		lineNum = 1;
	}
	
	lineEnd = -1;
	for (i = 1; i <= lineNum && lineEnd < this->buffer->BufGetLength(); i++) {
		lineStart = lineEnd + 1;
		lineEnd = this->buffer->BufEndOfLine(lineStart);
	}

	// If line is beyond end of buffer, position at last character in buffer
	if (lineNum >= i) {
		return lineEnd;
	}

	// Start character index at zero
	int charIndex = 0;

	// Only have to count columns if column isn't zero (or negative)
	if (column > 0) {
		
		int charLen = 0;
		
		// Count columns, expanding each character
		std::string lineStr = this->buffer->BufGetRangeEx(lineStart, lineEnd);
		int outIndex = 0;
		for (i = lineStart; i < lineEnd; i++, charIndex++) {
			char expandedChar[MAX_EXP_CHAR_LEN];
			charLen = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, this->buffer->tabDist_, this->buffer->nullSubsChar_);
			if (outIndex + charLen >= column)
				break;
			outIndex += charLen;

			// NOTE(eteran): previous code leaked here lineStr here!
		}

		/* If the column is in the middle of an expanded character, put cursor
		 * in front of character if in first half of character, and behind
		 * character if in last half of character
		 */
		if (column >= outIndex + (charLen / 2))
			charIndex++;

		// If we are beyond the end of the line, back up one space
		if ((i >= lineEnd) && (charIndex > 0))
			charIndex--;
	}

	// Position is the start of the line plus the index into line buffer
	return lineStart + charIndex;
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** False if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
int TextDisplay::TextDPositionToXY(int pos, int *x, int *y) {
	int charIndex;
	int lineStartPos;
	int fontHeight;
	int lineLen;
	int visLineNum;
	int outIndex;
	int xStep;
	char expandedChar[MAX_EXP_CHAR_LEN];

	// If position is not displayed, return false
	if (pos < this->firstChar || (pos > this->lastChar && !emptyLinesVisible()))
		return false;

	// Calculate y coordinate
	if (!posToVisibleLineNum(pos, &visLineNum))
		return false;
	fontHeight = this->ascent + this->descent;
	*y = this->rect.top + visLineNum * fontHeight + fontHeight / 2;

	/* Get the text, length, and  buffer position of the line. If the position
	   is beyond the end of the buffer and should be at the first position on
	   the first empty line, don't try to get or scan the text  */
	lineStartPos = this->lineStarts[visLineNum];
	if (lineStartPos == -1) {
		*x = this->rect.left - this->horizOffset;
		return true;
	}
	lineLen = visLineLength(visLineNum);
	std::string lineStr = this->buffer->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);

	/* Step through character positions from the beginning of the line
	   to "pos" to calculate the x coordinate */
	xStep = this->rect.left - this->horizOffset;
	outIndex = 0;
	for (charIndex = 0; charIndex < pos - lineStartPos; charIndex++) {
		int charLen = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, this->buffer->tabDist_, this->buffer->nullSubsChar_);
		int charStyle = styleOfPos(lineStartPos, lineLen, charIndex, outIndex, lineStr[charIndex]);
		xStep += stringWidth(expandedChar, charLen, charStyle);
		outIndex += charLen;
	}
	*x = xStep;
	return true;
}

/*
** If the text widget is maintaining a line number count appropriate to "pos"
** return the line and column numbers of pos, otherwise return False.  If
** continuous wrap mode is on, returns the absolute line number (as opposed to
** the wrapped line number which is used for scrolling).  THIS ROUTINE ONLY
** WORKS FOR DISPLAYED LINES AND, IN CONTINUOUS WRAP MODE, ONLY WHEN THE
** ABSOLUTE LINE NUMBER IS BEING MAINTAINED.  Otherwise, it returns False.
*/
int TextDisplay::TextDPosToLineAndCol(int pos, int *lineNum, int *column) {
	TextBuffer *buf = this->buffer;

	/* In continuous wrap mode, the absolute (non-wrapped) line count is
	   maintained separately, as needed.  Only return it if we're actually
	   keeping track of it and pos is in the displayed text */
	if (this->continuousWrap) {
		if (!maintainingAbsTopLineNum() || pos < this->firstChar || pos > this->lastChar)
			return false;
		*lineNum = this->absTopLineNum + buf->BufCountLines(this->firstChar, pos);
		*column = buf->BufCountDispChars(buf->BufStartOfLine(pos), pos);
		return true;
	}

	// Only return the data if pos is within the displayed text
	if (!posToVisibleLineNum(pos, lineNum))
		return false;
	*column = buf->BufCountDispChars(this->lineStarts[*lineNum], pos);
	*lineNum += this->topLineNum;
	return true;
}

/*
** Return True if position (x, y) is inside of the primary selection
*/
int TextDisplay::TextDInSelection(Point p) {
	int row;
	int column;
	int pos = xyToPos(p.x, p.y, CHARACTER_POS);
	TextBuffer *buf = this->buffer;

	xyToUnconstrainedPos(p.x, p.y, &row, &column, CHARACTER_POS);
	if (buf->primary_.rangeTouchesRectSel(this->firstChar, this->lastChar))
		column = TextDOffsetWrappedColumn(row, column);
	return buf->primary_.inSelection(pos, buf->BufStartOfLine(pos), column);
}

/*
** Correct a column number based on an unconstrained position (as returned by
** TextDXYToUnconstrainedPosition) to be relative to the last actual newline
** in the buffer before the row and column position given, rather than the
** last line start created by line wrapping.  This is an adapter
** for rectangular selections and code written before continuous wrap mode,
** which thinks that the unconstrained column is the number of characters
** from the last newline.  Obviously this is time consuming, because it
** invloves character re-counting.
*/
int TextDisplay::TextDOffsetWrappedColumn(int row, int column) {
	int lineStart, dispLineStart;

	if (!this->continuousWrap || row < 0 || row > this->nVisibleLines)
		return column;
	dispLineStart = this->lineStarts[row];
	if (dispLineStart == -1)
		return column;
	lineStart = this->buffer->BufStartOfLine(dispLineStart);
	return column + this->buffer->BufCountDispChars(lineStart, dispLineStart);
}

/*
** Correct a row number from an unconstrained position (as returned by
** TextDXYToUnconstrainedPosition) to a straight number of newlines from the
** top line of the display.  Because rectangular selections are based on
** newlines, rather than display wrapping, and anywhere a rectangular selection
** needs a row, it needs it in terms of un-wrapped lines.
*/
int TextDisplay::TextDOffsetWrappedRow(int row) const {
	if (!this->continuousWrap || row < 0 || row > this->nVisibleLines)
		return row;
	return this->buffer->BufCountLines(this->firstChar, this->lineStarts[row]);
}

/*
** Scroll the display to bring insertion cursor into view.
**
** Note: it would be nice to be able to do this without counting lines twice
** (setScroll counts them too) and/or to count from the most efficient
** starting point, but the efficiency of this routine is not as important to
** the overall performance of the text display.
*/
void TextDisplay::TextDMakeInsertPosVisible() {
	int hOffset, topLine, x, y;
	int cursorPos = this->cursorPos;
	int linesFromTop = 0;
	int cursorVPadding = (int)text_of(w).P_cursorVPadding;

	hOffset = this->horizOffset;
	topLine = this->topLineNum;

	// Don't do padding if this is a mouse operation
	bool do_padding = ((this->dragState == NOT_CLICKED) && (cursorVPadding > 0));

	// Find the new top line number
	if (cursorPos < this->firstChar) {
		topLine -= TextDCountLines(cursorPos, this->firstChar, false);
		// linesFromTop = 0;
	} else if (cursorPos > this->lastChar && !emptyLinesVisible()) {
		topLine += TextDCountLines(this->lastChar - (wrapUsesCharacter(this->lastChar) ? 0 : 1), cursorPos, false);
		linesFromTop = this->nVisibleLines - 1;
	} else if (cursorPos == this->lastChar && !emptyLinesVisible() && !wrapUsesCharacter(this->lastChar)) {
		topLine++;
		linesFromTop = this->nVisibleLines - 1;
	} else {
		// Avoid extra counting if cursorVPadding is disabled
		if (do_padding)
			linesFromTop = TextDCountLines(this->firstChar, cursorPos, true);
	}
	if (topLine < 1) {
		fprintf(stderr, "nedit: internal consistency check tl1 failed\n");
		topLine = 1;
	}

	if (do_padding) {
		// Keep the cursor away from the top or bottom of screen.
		if (this->nVisibleLines <= 2 * (int)cursorVPadding) {
			topLine += (linesFromTop - this->nVisibleLines / 2);
			topLine = std::max(topLine, 1);
		} else if (linesFromTop < (int)cursorVPadding) {
			topLine -= (cursorVPadding - linesFromTop);
			topLine = std::max(topLine, 1);
		} else if (linesFromTop > this->nVisibleLines - (int)cursorVPadding - 1) {
			topLine += (linesFromTop - (this->nVisibleLines - cursorVPadding - 1));
		}
	}

	/* Find the new setting for horizontal offset (this is a bit ungraceful).
	   If the line is visible, just use TextDPositionToXY to get the position
	   to scroll to, otherwise, do the vertical scrolling first, then the
	   horizontal */
	if (!TextDPositionToXY(cursorPos, &x, &y)) {
		setScroll(topLine, hOffset, True, true);
		if (!TextDPositionToXY(cursorPos, &x, &y))
			return; // Give up, it's not worth it (but why does it fail?)
	}
	if (x > this->rect.left + this->rect.width)
		hOffset += x - (this->rect.left + this->rect.width);
	else if (x < this->rect.left)
		hOffset += x - this->rect.left;

	// Do the scroll
	setScroll(topLine, hOffset, True, true);
}

/*
** Return the current preferred column along with the current
** visible line index (-1 if not visible) and the lineStartPos
** of the current insert position.
*/
int TextDisplay::TextDPreferredColumn(int *visLineNum, int *lineStartPos) {
	int column;

	/* Find the position of the start of the line.  Use the line starts array
	if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (posToVisibleLineNum(this->cursorPos, visLineNum)) {
		*lineStartPos = this->lineStarts[*visLineNum];
	} else {
		*lineStartPos =TextDStartOfLine(this->cursorPos);
		*visLineNum = -1;
	}

	// Decide what column to move to, if there's a preferred column use that
	column = (this->cursorPreferredCol >= 0) ? this->cursorPreferredCol : this->buffer->BufCountDispChars(*lineStartPos, this->cursorPos);
	return (column);
}

/*
** Return the insert position of the requested column given
** the lineStartPos.
*/
int TextDisplay::TextDPosOfPreferredCol(int column, int lineStartPos) {
	int newPos;

	newPos = this->buffer->BufCountForwardDispChars(lineStartPos, column);
	if (this->continuousWrap) {
		newPos = std::min(newPos, TextDEndOfLine(lineStartPos, True));
	}
	return (newPos);
}

/*
** Cursor movement functions
*/
int TextDisplay::TextDMoveRight() {
	if (this->cursorPos >= this->buffer->BufGetLength())
		return false;
	TextDSetInsertPosition(this->cursorPos + 1);
	return true;
}

int TextDisplay::TextDMoveLeft() {
	if (this->cursorPos <= 0)
		return false;
	TextDSetInsertPosition(this->cursorPos - 1);
	return true;
}

int TextDisplay::TextDMoveUp(bool absolute) {
	int lineStartPos, column, prevLineStartPos, newPos, visLineNum;

	/* Find the position of the start of the line.  Use the line starts array
	   if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (absolute) {
		lineStartPos = this->buffer->BufStartOfLine(this->cursorPos);
		visLineNum = -1;
	} else if (posToVisibleLineNum(this->cursorPos, &visLineNum))
		lineStartPos = this->lineStarts[visLineNum];
	else {
		lineStartPos =TextDStartOfLine(this->cursorPos);
		visLineNum = -1;
	}
	if (lineStartPos == 0)
		return false;

	// Decide what column to move to, if there's a preferred column use that
	column = this->cursorPreferredCol >= 0 ? this->cursorPreferredCol : this->buffer->BufCountDispChars(lineStartPos, this->cursorPos);

	// count forward from the start of the previous line to reach the column
	if (absolute) {
		prevLineStartPos = this->buffer->BufCountBackwardNLines(lineStartPos, 1);
	} else if (visLineNum != -1 && visLineNum != 0) {
		prevLineStartPos = this->lineStarts[visLineNum - 1];
	} else {
		prevLineStartPos = TextDCountBackwardNLines(lineStartPos, 1);
	}

	newPos = this->buffer->BufCountForwardDispChars(prevLineStartPos, column);
	if (this->continuousWrap && !absolute)
		newPos = std::min(newPos, TextDEndOfLine(prevLineStartPos, True));

	// move the cursor
	TextDSetInsertPosition(newPos);

	// if a preferred column wasn't aleady established, establish it
	this->cursorPreferredCol = column;

	return true;
}

int TextDisplay::TextDMoveDown(bool absolute) {
	int lineStartPos, column, nextLineStartPos, newPos, visLineNum;

	if (this->cursorPos == this->buffer->BufGetLength()) {
		return false;
	}

	if (absolute) {
		lineStartPos = this->buffer->BufStartOfLine(this->cursorPos);
		visLineNum = -1;
	} else if (posToVisibleLineNum(this->cursorPos, &visLineNum)) {
		lineStartPos = this->lineStarts[visLineNum];
	} else {
		lineStartPos =TextDStartOfLine(this->cursorPos);
		visLineNum = -1;
	}

	column = this->cursorPreferredCol >= 0 ? this->cursorPreferredCol : this->buffer->BufCountDispChars(lineStartPos, this->cursorPos);

	if (absolute)
		nextLineStartPos = this->buffer->BufCountForwardNLines(lineStartPos, 1);
	else
		nextLineStartPos = TextDCountForwardNLines(lineStartPos, 1, true);

	newPos = this->buffer->BufCountForwardDispChars(nextLineStartPos, column);

	if (this->continuousWrap && !absolute) {
		newPos = std::min(newPos, TextDEndOfLine(nextLineStartPos, True));
	}

	TextDSetInsertPosition(newPos);
	this->cursorPreferredCol = column;

	return true;
}

/*
** Same as BufCountLines, but takes in to account wrapping if wrapping is
** turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as True to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
*/
int TextDisplay::TextDCountLines(int startPos, int endPos, int startPosIsLineStart) {
	int retLines, retPos, retLineStart, retLineEnd;

	// If we're not wrapping use simple (and more efficient) BufCountLines
	if (!this->continuousWrap)
		return this->buffer->BufCountLines(startPos, endPos);

	wrappedLineCounter(this->buffer, startPos, endPos, INT_MAX, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLines;
}

/*
** Same as BufCountForwardNLines, but takes in to account line breaks when
** wrapping is turned on. If the caller knows that startPos is at a line start,
** it can pass "startPosIsLineStart" as True to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
*/
int TextDisplay::TextDCountForwardNLines(const int startPos, const unsigned nLines, const Boolean startPosIsLineStart) {
	int retLines, retPos, retLineStart, retLineEnd;

	// if we're not wrapping use more efficient BufCountForwardNLines
	if (!this->continuousWrap)
		return this->buffer->BufCountForwardNLines(startPos, nLines);

	// wrappedLineCounter can't handle the 0 lines case
	if (nLines == 0)
		return startPos;

	// use the common line counting routine to count forward
	wrappedLineCounter(this->buffer, startPos, this->buffer->BufGetLength(), nLines, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retPos;
}

/*
** Same as BufEndOfLine, but takes in to account line breaks when wrapping
** is turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as True to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
**
** Note that the definition of the end of a line is less clear when continuous
** wrap is on.  With continuous wrap off, it's just a pointer to the newline
** that ends the line.  When it's on, it's the character beyond the last
** DISPLAYABLE character on the line, where a whitespace character which has
** been "converted" to a newline for wrapping is not considered displayable.
** Also note that, a line can be wrapped at a non-whitespace character if the
** line had no whitespace.  In this case, this routine returns a pointer to
** the start of the next line.  This is also consistent with the model used by
** visLineLength.
*/
int TextDisplay::TextDEndOfLine(int pos, const Boolean startPosIsLineStart) {
	int retLines, retPos, retLineStart, retLineEnd;

	// If we're not wrapping use more efficient BufEndOfLine
	if (!this->continuousWrap)
		return this->buffer->BufEndOfLine(pos);

	if (pos == this->buffer->BufGetLength())
		return pos;
	wrappedLineCounter(this->buffer, pos, this->buffer->BufGetLength(), 1, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLineEnd;
}

/*
** Same as BufStartOfLine, but returns the character after last wrap point
** rather than the last newline.
*/
int TextDisplay::TextDStartOfLine(int pos) const {
	int retLines, retPos, retLineStart, retLineEnd;

	// If we're not wrapping, use the more efficient BufStartOfLine
	if (!this->continuousWrap)
		return this->buffer->BufStartOfLine(pos);

	wrappedLineCounter(this->buffer, this->buffer->BufStartOfLine(pos), pos, INT_MAX, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLineStart;
}

/*
** Same as BufCountBackwardNLines, but takes in to account line breaks when
** wrapping is turned on.
*/
int TextDisplay::TextDCountBackwardNLines(int startPos, int nLines) {
	TextBuffer *buf = this->buffer;
	int pos;
	int retLines;
	int retPos;
	int retLineStart;
	int retLineEnd;

	// If we're not wrapping, use the more efficient BufCountBackwardNLines
	if (!this->continuousWrap)
		return this->buffer->BufCountBackwardNLines(startPos, nLines);

	pos = startPos;
	while (true) {
		int lineStart = buf->BufStartOfLine(pos);
		wrappedLineCounter(this->buffer, lineStart, pos, INT_MAX, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retLines > nLines)
			return TextDCountForwardNLines(lineStart, retLines - nLines, true);
		nLines -= retLines;
		pos = lineStart - 1;
		if (pos < 0)
			return 0;
		nLines -= 1;
	}
}

/*
** Callback attached to the text buffer to receive delete information before
** the modifications are actually made.
*/
static void bufPreDeleteCB(int pos, int nDeleted, void *cbArg) {
	auto textD = static_cast<TextDisplay *>(cbArg);
	if (textD->continuousWrap && (textD->getFixedFontWidth() == -1 || textD->getModifyingTabDist()))
		/* Note: we must perform this measurement, even if there is not a
		   single character deleted; the number of "deleted" lines is the
		   number of visual lines spanned by the real line in which the
		   modification takes place.
		   Also, a modification of the tab distance requires the same
		   kind of calculations in advance, even if the font width is "fixed",
		   because when the width of the tab characters changes, the layout
		   of the text may be completely different. */
		textD->measureDeletedLines(pos, nDeleted);
	else
		textD->setSuppressResync(false); // Probably not needed, but just in case
}

/*
** Callback attached to the text buffer to receive modification information
*/
static void bufModifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg) {

	int linesInserted;
	int linesDeleted;
	int startDispPos;
	int endDispPos;
	auto textD = static_cast<TextDisplay *>(cbArg);
	TextBuffer *buf  = textD->TextGetBuffer();
	int oldFirstChar = textD->getFirstChar();
	int scrolled;
	int origCursorPos = textD->getCursorPos();
	int wrapModStart;
	int wrapModEnd;

	// buffer modification cancels vertical cursor motion column
	if (nInserted != 0 || nDeleted != 0)
		textD->setCursorPreferredCol(-1);

	/* Count the number of lines inserted and deleted, and in the case
	   of continuous wrap mode, how much has changed */
	if (textD->continuousWrap) {
		textD->findWrapRangeEx(deletedText, pos, nInserted, nDeleted, &wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);
	} else {
		linesInserted = nInserted == 0 ? 0 : buf->BufCountLines(pos, pos + nInserted);
		linesDeleted = nDeleted == 0 ? 0 : countLinesEx(deletedText);
	}

	// Update the line starts and topLineNum
	if (nInserted != 0 || nDeleted != 0) {
		if (textD->continuousWrap) {
			textD->updateLineStarts( wrapModStart, wrapModEnd - wrapModStart, nDeleted + pos - wrapModStart + (wrapModEnd - (pos + nInserted)), linesInserted, linesDeleted, &scrolled);
		} else {
			textD->updateLineStarts( pos, nInserted, nDeleted, linesInserted, linesDeleted, &scrolled);
		}
	} else
		scrolled = false;

	/* If we're counting non-wrapped lines as well, maintain the absolute
	   (non-wrapped) line number of the text displayed */
	if (textD->maintainingAbsTopLineNum() && (nInserted != 0 || nDeleted != 0)) {
		if (pos + nDeleted < oldFirstChar)
			textD->setAbsTopLineNum(textD->getAbsTopLineNum() + buf->BufCountLines(pos, pos + nInserted) - countLinesEx(deletedText));
		else if (pos < oldFirstChar)
			textD->resetAbsLineNum();
	}

	// Update the line count for the whole buffer
	textD->setNumberBufferLines(textD->getNumberBufferLines() + linesInserted - linesDeleted);

	/* Update the scroll bar ranges (and value if the value changed).  Note
	   that updating the horizontal scroll bar range requires scanning the
	   entire displayed text, however, it doesn't seem to hurt performance
	   much.  Note also, that the horizontal scroll bar update routine is
	   allowed to re-adjust horizOffset if there is blank space to the right
	   of all lines of text. */
	textD->updateVScrollBarRange();
	scrolled |= textD->updateHScrollBarRange();

	// Update the cursor position
	if (textD->getCursorToHint() != NO_HINT) {
		textD->setCursorPos(textD->getCursorToHint());
		textD->setCursorToHint(NO_HINT);
	} else if (textD->getCursorPos() > pos) {
		if (textD->getCursorPos() < pos + nDeleted) {
			textD->setCursorPos(pos);
		} else {
			textD->setCursorPos(textD->getCursorPos() + nInserted - nDeleted);
		}
	}

	// If the changes caused scrolling, re-paint everything and we're done.
	if (scrolled) {
		textD->blankCursorProtrusions();
		textD->TextDRedisplayRect(0, textD->getRect().top, textD->getRect().width + textD->getRect().left, textD->getRect().height);
		if (textD->getStyleBuffer()) { // See comments in extendRangeForStyleMods
			textD->getStyleBuffer()->primary_.selected = false;
			textD->getStyleBuffer()->primary_.zeroWidth = false;
		}
		return;
	}

	/* If the changes didn't cause scrolling, decide the range of characters
	   that need to be re-painted.  Also if the cursor position moved, be
	   sure that the redisplay range covers the old cursor position so the
	   old cursor gets erased, and erase the bits of the cursor which extend
	   beyond the left and right edges of the text. */
	startDispPos = textD->continuousWrap ? wrapModStart : pos;
	if (origCursorPos == startDispPos && textD->getCursorPos() != startDispPos)
		startDispPos = std::min(startDispPos, origCursorPos - 1);
	if (linesInserted == linesDeleted) {
		if (nInserted == 0 && nDeleted == 0)
			endDispPos = pos + nRestyled;
		else {
			endDispPos = textD->continuousWrap ? wrapModEnd : buf->BufEndOfLine(pos + nInserted) + 1;
			if (origCursorPos >= startDispPos && (origCursorPos <= endDispPos || endDispPos == buf->BufGetLength()))
				textD->blankCursorProtrusions();
		}
		/* If more than one line is inserted/deleted, a line break may have
		   been inserted or removed in between, and the line numbers may
		   have changed. If only one line is altered, line numbers cannot
		   be affected (the insertion or removal of a line break always
		   results in at least two lines being redrawn). */
		if (linesInserted > 1)
			textD->redrawLineNumbers(false);
	} else { // linesInserted != linesDeleted
		endDispPos = textD->getLastChar() + 1;
		if (origCursorPos >= pos)
			textD->blankCursorProtrusions();
		textD->redrawLineNumbers(false);
	}

	/* If there is a style buffer, check if the modification caused additional
	   changes that need to be redisplayed.  (Redisplaying separately would
	   cause double-redraw on almost every modification involving styled
	   text).  Extend the redraw range to incorporate style changes */
	if (textD->getStyleBuffer()) {
		textD->extendRangeForStyleMods(&startDispPos, &endDispPos);
	}

	// Redisplay computed range
	textD->textDRedisplayRange(startDispPos, endDispPos);
}

/*
** In continuous wrap mode, internal line numbers are calculated after
** wrapping.  A separate non-wrapped line count is maintained when line
** numbering is turned on.  There is some performance cost to maintaining this
** line count, so normally absolute line numbers are not tracked if line
** numbering is off.  This routine allows callers to specify that they still
** want this line count maintained (for use via TextDPosToLineAndCol).
** More specifically, this allows the line number reported in the statistics
** line to be calibrated in absolute lines, rather than post-wrapped lines.
*/
void TextDisplay::TextDMaintainAbsLineNum(int state) {
	this->needAbsTopLineNum = state;
	resetAbsLineNum();
}

/*
** Returns the absolute (non-wrapped) line number of the first line displayed.
** Returns 0 if the absolute top line number is not being maintained.
*/
int TextDisplay::getAbsTopLineNum() {

	if (!this->continuousWrap) {
		return this->topLineNum;
	}
		
	if (maintainingAbsTopLineNum()) {
		return this->absTopLineNum;
	}
		
	return 0;
}

/*
** Re-calculate absolute top line number for a change in scroll position.
*/
void TextDisplay::offsetAbsLineNum(int oldFirstChar) {
	if (this->maintainingAbsTopLineNum()) {
		if (this->firstChar < oldFirstChar) {
			this->absTopLineNum -= this->buffer->BufCountLines(this->firstChar, oldFirstChar);
		} else {
			this->absTopLineNum += this->buffer->BufCountLines(oldFirstChar, this->firstChar);
		}
	}
}

/*
** Return true if a separate absolute top line number is being maintained
** (for displaying line numbers or showing in the statistics line).
*/
int TextDisplay::maintainingAbsTopLineNum() const {
	return this->continuousWrap && (this->lineNumWidth != 0 || this->needAbsTopLineNum);
}

/*
** Count lines from the beginning of the buffer to reestablish the
** absolute (non-wrapped) top line number.  If mode is not continuous wrap,
** or the number is not being maintained, does nothing.
*/
void TextDisplay::resetAbsLineNum() {
	this->absTopLineNum = 1;
	offsetAbsLineNum(0);
}

/*
** Find the line number of position "pos" relative to the first line of
** displayed text. Returns False if the line is not displayed.
*/
int TextDisplay::posToVisibleLineNum(int pos, int *lineNum) {
	int i;

	if (pos < this->firstChar)
		return false;
	if (pos > this->lastChar) {
		if (emptyLinesVisible()) {
			if (this->lastChar < this->buffer->BufGetLength()) {
				if (!posToVisibleLineNum(this->lastChar, lineNum)) {
					fprintf(stderr, "nedit: Consistency check ptvl failed\n");
					return false;
				}
				return ++(*lineNum) <= this->nVisibleLines - 1;
			} else {
				posToVisibleLineNum(std::max(this->lastChar - 1, 0), lineNum);
				return true;
			}
		}
		return false;
	}

	for (i = this->nVisibleLines - 1; i >= 0; i--) {
		if (this->lineStarts[i] != -1 && pos >= this->lineStarts[i]) {
			*lineNum = i;
			return true;
		}
	}

	return false;
}

/*
** Redisplay the text on a single line represented by "visLineNum" (the
** number of lines down from the top of the display), limited by
** "leftClip" and "rightClip" window coordinates and "leftCharIndex" and
** "rightCharIndex" character positions (not including the character at
** position "rightCharIndex").
**
** The cursor is also drawn if it appears on the line.
*/
void TextDisplay::redisplayLine(int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex) {
	
	TextBuffer *buf = this->buffer;
	int i;
	int x;
	int y;
	int startX;
	int charIndex;
	int lineStartPos;
	int lineLen;
	int fontHeight;
	int stdCharWidth;
	int charWidth;
	int startIndex;
	int style;
	int charLen;
	int outStartIndex;
	int outIndex;
	int cursorX = 0;
	bool hasCursor = false;
	int dispIndexOffset;
	int cursorPos = this->cursorPos, y_orig;
	char expandedChar[MAX_EXP_CHAR_LEN];
	char outStr[MAX_DISP_LINE_LEN];
	char *outPtr;
	char baseChar;
	std::string lineStr;

	// If line is not displayed, skip it
	if (visLineNum < 0 || visLineNum >= this->nVisibleLines)
		return;

	// Shrink the clipping range to the active display area
	leftClip = std::max(this->rect.left, leftClip);
	rightClip = std::min(rightClip, this->rect.left + this->rect.width);

	if (leftClip > rightClip) {
		return;
	}

	// Calculate y coordinate of the string to draw
	fontHeight = this->ascent + this->descent;
	y = this->rect.top + visLineNum * fontHeight;

	// Get the text, length, and  buffer position of the line to display
	lineStartPos = this->lineStarts[visLineNum];
	if (lineStartPos == -1) {
		lineLen = 0;
	} else {
		lineLen = visLineLength(visLineNum);
		lineStr = buf->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);
	}

	/* Space beyond the end of the line is still counted in units of characters
	   of a standardized character width (this is done mostly because style
	   changes based on character position can still occur in this region due
	   to rectangular selections).  stdCharWidth must be non-zero to prevent a
	   potential infinite loop if x does not advance */
	stdCharWidth = this->fontStruct->max_bounds.width;
	if (stdCharWidth <= 0) {
		fprintf(stderr, "nedit: Internal Error, bad font measurement\n");
		return;
	}

	/* Rectangular selections are based on "real" line starts (after a newline
	   or start of buffer).  Calculate the difference between the last newline
	   position and the line start we're using.  Since scanning back to find a
	   newline is expensive, only do so if there's actually a rectangular
	   selection which needs it */
	if (this->continuousWrap && (buf->primary_.rangeTouchesRectSel(lineStartPos, lineStartPos + lineLen) || buf->secondary_.rangeTouchesRectSel(lineStartPos, lineStartPos + lineLen) || buf->highlight_.rangeTouchesRectSel(lineStartPos, lineStartPos + lineLen))) {
		dispIndexOffset = buf->BufCountDispChars(buf->BufStartOfLine(lineStartPos), lineStartPos);
	} else
		dispIndexOffset = 0;

	/* Step through character positions from the beginning of the line (even if
	   that's off the left edge of the displayed area) to find the first
	   character position that's not clipped, and the x coordinate for drawing
	   that character */
	x = this->rect.left - this->horizOffset;
	outIndex = 0;

	for (charIndex = 0;; charIndex++) {
		baseChar = '\0';
		charLen = charIndex >= lineLen ? 1 : TextBuffer::BufExpandCharacter(baseChar = lineStr[charIndex], outIndex, expandedChar, buf->tabDist_, buf->nullSubsChar_);
		style = styleOfPos(lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, baseChar);
		charWidth = charIndex >= lineLen ? stdCharWidth : stringWidth(expandedChar, charLen, style);

		if (x + charWidth >= leftClip && charIndex >= leftCharIndex) {
			startIndex = charIndex;
			outStartIndex = outIndex;
			startX = x;
			break;
		}
		x += charWidth;
		outIndex += charLen;
	}

	/* Scan character positions from the beginning of the clipping range, and
	   draw parts whenever the style changes (also note if the cursor is on
	   this line, and where it should be drawn to take advantage of the x
	   position which we've gone to so much trouble to calculate) */
	outPtr = outStr;
	outIndex = outStartIndex;
	x = startX;
	for (charIndex = startIndex; charIndex < rightCharIndex; charIndex++) {
		if (lineStartPos + charIndex == cursorPos) {
			if (charIndex < lineLen || (charIndex == lineLen && cursorPos >= buf->BufGetLength())) {
				hasCursor = true;
				cursorX = x - 1;
			} else if (charIndex == lineLen) {
				if (wrapUsesCharacter(cursorPos)) {
					hasCursor = true;
					cursorX = x - 1;
				}
			}
		}

		baseChar = '\0';
		charLen = charIndex >= lineLen ? 1 : TextBuffer::BufExpandCharacter(baseChar = lineStr[charIndex], outIndex, expandedChar, buf->tabDist_, buf->nullSubsChar_);
		int charStyle = styleOfPos(lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, baseChar);
		for (i = 0; i < charLen; i++) {
			if (i != 0 && charIndex < lineLen && lineStr[charIndex] == '\t') {
				charStyle = styleOfPos(lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, '\t');
			}

			if (charStyle != style) {
				drawString(style, startX, y, x, outStr, outPtr - outStr);
				outPtr = outStr;
				startX = x;
				style = charStyle;
			}

			if (charIndex < lineLen) {
				*outPtr = expandedChar[i];
				charWidth = stringWidth(&expandedChar[i], 1, charStyle);
			} else {
				charWidth = stdCharWidth;
			}

			outPtr++;
			x += charWidth;
			outIndex++;
		}

		if (outPtr - outStr + MAX_EXP_CHAR_LEN >= MAX_DISP_LINE_LEN || x >= rightClip) {
			break;
		}
	}

	// Draw the remaining style segment
	drawString(style, startX, y, x, outStr, outPtr - outStr);

	/* Draw the cursor if part of it appeared on the redisplayed part of
	   this line.  Also check for the cases which are not caught as the
	   line is scanned above: when the cursor appears at the very end
	   of the redisplayed section. */
	y_orig = this->cursor.y;
	if (this->cursorOn) {
		if (hasCursor) {
			drawCursor(cursorX, y);
		} else if (charIndex < lineLen && (lineStartPos + charIndex + 1 == cursorPos) && x == rightClip) {
			if (cursorPos >= buf->BufGetLength()) {
				drawCursor(x - 1, y);
			} else {
				if (wrapUsesCharacter(cursorPos)) {
					drawCursor(x - 1, y);
				}
			}
		} else if ((lineStartPos + rightCharIndex) == cursorPos) {
			drawCursor(x - 1, y);
		}
	}

	// If the y position of the cursor has changed, redraw the calltip
	if (hasCursor && (y_orig != this->cursor.y || y_orig != y))
		TextDRedrawCalltip(0);
}

/*
** Draw a string or blank area according to parameter "style", using the
** appropriate colors and drawing method for that style, with top left
** corner at x, y.  If style says to draw text, use "string" as source of
** characters, and draw "nChars", if style is FILL, erase
** rectangle where text would have drawn from x to toX and from y to
** the maximum y extent of the current font(s).
*/
void TextDisplay::drawString(int style, int x, int y, int toX, char *string, int nChars) {

	GC gc;
	GC bgGC;
	XGCValues gcValues;
	XFontStruct *fs     = this->fontStruct;
	Pixel bground       = this->bgPixel;
	Pixel fground       = this->fgPixel;
	bool underlineStyle = false;

	// Don't draw if widget isn't realized
	if (XtWindow(this->w) == 0)
		return;

	// select a GC
	if (style & (STYLE_LOOKUP_MASK | BACKLIGHT_MASK | RANGESET_MASK)) {
		gc = bgGC = this->styleGC;
	} else if (style & HIGHLIGHT_MASK) {
		gc = this->highlightGC;
		bgGC = this->highlightBGGC;
	} else if (style & PRIMARY_MASK) {
		gc = this->selectGC;
		bgGC = this->selectBGGC;
	} else {
		gc = bgGC = this->gc;
	}

	if (gc == this->styleGC) {

		// we have work to do
		StyleTableEntry *styleRec;
		/* Set font, color, and gc depending on style.  For normal text, GCs
		   for normal drawing, or drawing within a selection or highlight are
		   pre-allocated and pre-configured.  For syntax highlighting, GCs are
		   configured here, on the fly. */
		if (style & STYLE_LOOKUP_MASK) {
			styleRec = &this->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A];
			underlineStyle = styleRec->underline;
			fs = styleRec->font;
			gcValues.font = fs->fid;
			fground = styleRec->color;
			// here you could pick up specific select and highlight fground
		} else {
			styleRec = nullptr;
			gcValues.font = fs->fid;
			fground = this->fgPixel;
		}

		/* Background color priority order is:
		   1 Primary(Selection), 2 Highlight(Parens),
		   3 Rangeset, 4 SyntaxHighlightStyle,
		   5 Backlight (if NOT fill), 6 DefaultBackground */
		bground = (style & PRIMARY_MASK)                           ? this->selectBGPixel :
		          (style & HIGHLIGHT_MASK)                         ? this->highlightBGPixel :
				  (style & RANGESET_MASK)                          ? this->getRangesetColor((style & RANGESET_MASK) >> RANGESET_SHIFT, bground) :
				  (styleRec && !styleRec->bgColorName.isNull())    ? styleRec->bgColor :
				  (style & BACKLIGHT_MASK) && !(style & FILL_MASK) ? this->bgClassPixel[(style >> BACKLIGHT_SHIFT) & 0xff] :
				  this->bgPixel;


		if (fground == bground) { // B&W kludge
			fground = this->bgPixel;
		}

		// set up gc for clearing using the foreground color entry
		gcValues.foreground = bground;
		gcValues.background = bground;
		XChangeGC(XtDisplay(this->w), gc, GCFont | GCForeground | GCBackground, &gcValues);
	}

	// Draw blank area rather than text, if that was the request
	if (style & FILL_MASK) {
		// wipes out to right hand edge of widget
		if (toX >= this->rect.left) {
			clearRect(bgGC, std::max(x, this->rect.left), y, toX - std::max(x, this->rect.left), this->ascent + this->descent);
		}
		return;
	}

	/* If any space around the character remains unfilled (due to use of
	   different sized fonts for highlighting), fill in above or below
	   to erase previously drawn characters */
	if (fs->ascent < this->ascent)
		clearRect(bgGC, x, y, toX - x, this->ascent - fs->ascent);
	if (fs->descent < this->descent)
		clearRect(bgGC, x, y + this->ascent + fs->descent, toX - x, this->descent - fs->descent);

	// set up gc for writing text (set foreground properly)
	if (bgGC == this->styleGC) {
		gcValues.foreground = fground;
		XChangeGC(XtDisplay(this->w), gc, GCForeground, &gcValues);
	}

	// Draw the string using gc and font set above
	XDrawImageString(XtDisplay(this->w), XtWindow(this->w), gc, x, y + this->ascent, string, nChars);

	// Underline if style is secondary selection
	if (style & SECONDARY_MASK || underlineStyle) {
		// restore foreground in GC (was set to background by clearRect())
		gcValues.foreground = fground;
		XChangeGC(XtDisplay(this->w), gc, GCForeground, &gcValues);
		// draw underline
		XDrawLine(XtDisplay(this->w), XtWindow(this->w), gc, x, y + this->ascent, toX - 1, y + this->ascent);
	}
}

/*
** Clear a rectangle with the appropriate background color for "style"
*/
void TextDisplay::clearRect(GC gc, const Rect &rect) {
	clearRect(gc, rect.left, rect.top, rect.width, rect.height);
}

void TextDisplay::clearRect(GC gc, int x, int y, int width, int height) {

	// A width of zero means "clear to end of window" to XClearArea
	if (width == 0 || XtWindow(this->w) == 0)
		return;

	if (gc == this->gc) {
		XClearArea(XtDisplay(this->w), XtWindow(this->w), x, y, width, height, false);
	} else {
		XFillRectangle(XtDisplay(this->w), XtWindow(this->w), gc, x, y, width, height);
	}
}

/*
** Draw a cursor with top center at x, y.
*/
void TextDisplay::drawCursor(int x, int y) {
	XSegment segs[5];
	int left, right, cursorWidth, midY;
	int fontWidth = this->fontStruct->min_bounds.width, nSegs = 0;
	int fontHeight = this->ascent + this->descent;
	int bot = y + fontHeight - 1;

	if (XtWindow(this->w) == 0 || x < this->rect.left - 1 || x > this->rect.left + this->rect.width)
		return;

	/* For cursors other than the block, make them around 2/3 of a character
	   width, rounded to an even number of pixels so that X will draw an
	   odd number centered on the stem at x. */
	cursorWidth = (fontWidth / 3) * 2;
	left = x - cursorWidth / 2;
	right = left + cursorWidth;

	// Create segments and draw cursor
	if (this->cursorStyle == CARET_CURSOR) {
		midY = bot - fontHeight / 5;
		segs[0].x1 = left;
		segs[0].y1 = bot;
		segs[0].x2 = x;
		segs[0].y2 = midY;
		segs[1].x1 = x;
		segs[1].y1 = midY;
		segs[1].x2 = right;
		segs[1].y2 = bot;
		segs[2].x1 = left;
		segs[2].y1 = bot;
		segs[2].x2 = x;
		segs[2].y2 = midY - 1;
		segs[3].x1 = x;
		segs[3].y1 = midY - 1;
		segs[3].x2 = right;
		segs[3].y2 = bot;
		nSegs = 4;
	} else if (this->cursorStyle == NORMAL_CURSOR) {
		segs[0].x1 = left;
		segs[0].y1 = y;
		segs[0].x2 = right;
		segs[0].y2 = y;
		segs[1].x1 = x;
		segs[1].y1 = y;
		segs[1].x2 = x;
		segs[1].y2 = bot;
		segs[2].x1 = left;
		segs[2].y1 = bot;
		segs[2].x2 = right;
		segs[2].y2 = bot;
		nSegs = 3;
	} else if (this->cursorStyle == HEAVY_CURSOR) {
		segs[0].x1 = x - 1;
		segs[0].y1 = y;
		segs[0].x2 = x - 1;
		segs[0].y2 = bot;
		segs[1].x1 = x;
		segs[1].y1 = y;
		segs[1].x2 = x;
		segs[1].y2 = bot;
		segs[2].x1 = x + 1;
		segs[2].y1 = y;
		segs[2].x2 = x + 1;
		segs[2].y2 = bot;
		segs[3].x1 = left;
		segs[3].y1 = y;
		segs[3].x2 = right;
		segs[3].y2 = y;
		segs[4].x1 = left;
		segs[4].y1 = bot;
		segs[4].x2 = right;
		segs[4].y2 = bot;
		nSegs = 5;
	} else if (this->cursorStyle == DIM_CURSOR) {
		midY = y + fontHeight / 2;
		segs[0].x1 = x;
		segs[0].y1 = y;
		segs[0].x2 = x;
		segs[0].y2 = y;
		segs[1].x1 = x;
		segs[1].y1 = midY;
		segs[1].x2 = x;
		segs[1].y2 = midY;
		segs[2].x1 = x;
		segs[2].y1 = bot;
		segs[2].x2 = x;
		segs[2].y2 = bot;
		nSegs = 3;
	} else if (this->cursorStyle == BLOCK_CURSOR) {
		right = x + fontWidth;
		segs[0].x1 = x;
		segs[0].y1 = y;
		segs[0].x2 = right;
		segs[0].y2 = y;
		segs[1].x1 = right;
		segs[1].y1 = y;
		segs[1].x2 = right;
		segs[1].y2 = bot;
		segs[2].x1 = right;
		segs[2].y1 = bot;
		segs[2].x2 = x;
		segs[2].y2 = bot;
		segs[3].x1 = x;
		segs[3].y1 = bot;
		segs[3].x2 = x;
		segs[3].y2 = y;
		nSegs = 4;
	}
	XDrawSegments(XtDisplay(this->w), XtWindow(this->w), this->cursorFGGC, segs, nSegs);

	// Save the last position drawn
	this->cursor.x = x;
	this->cursor.y = y;
}

/*
** Determine the drawing method to use to draw a specific character from "buf".
** "lineStartPos" gives the character index where the line begins, "lineIndex",
** the number of characters past the beginning of the line, and "dispIndex",
** the number of displayed characters past the beginning of the line.  Passing
** lineStartPos of -1 returns the drawing style for "no text".
**
** Why not just: textD->styleOfPos(pos)?  Because style applies to blank areas
** of the window beyond the text boundaries, and because this routine must also
** decide whether a position is inside of a rectangular selection, and do so
** efficiently, without re-counting character positions from the start of the
** line.
**
** Note that style is a somewhat incorrect name, drawing method would
** be more appropriate.
*/
int TextDisplay::styleOfPos(int lineStartPos, int lineLen, int lineIndex, int dispIndex, int thisChar) {
	TextBuffer *buf = this->buffer;
	TextBuffer *styleBuf = this->styleBuffer;
	int pos, style = 0;

	if (lineStartPos == -1 || buf == nullptr)
		return FILL_MASK;

	pos = lineStartPos + std::min(lineIndex, lineLen);

	if (lineIndex >= lineLen)
		style = FILL_MASK;
	else if (styleBuf) {
		style = (uint8_t)styleBuf->BufGetCharacter(pos);
		if (style == this->unfinishedStyle) {
			// encountered "unfinished" style, trigger parsing
			(this->unfinishedHighlightCB)(this, pos, this->highlightCBArg);
			style = (uint8_t)styleBuf->BufGetCharacter(pos);
		}
	}
	if (buf->primary_.inSelection(pos, lineStartPos, dispIndex))
		style |= PRIMARY_MASK;
	if (buf->highlight_.inSelection(pos, lineStartPos, dispIndex))
		style |= HIGHLIGHT_MASK;
	if (buf->secondary_.inSelection(pos, lineStartPos, dispIndex))
		style |= SECONDARY_MASK;
	// store in the RANGESET_MASK portion of style the rangeset index for pos
	if (buf->rangesetTable_) {
		int rangesetIndex = buf->rangesetTable_->RangesetIndex1ofPos(pos, true);
		style |= ((rangesetIndex << RANGESET_SHIFT) & RANGESET_MASK);
	}
	/* store in the BACKLIGHT_MASK portion of style the background color class
	   of the character thisChar */
	if (this->bgClass) {
		style |= (this->bgClass[(uint8_t)thisChar] << BACKLIGHT_SHIFT);
	}
	return style;
}

/*
** Find the width of a string in the font of a particular style
*/
int TextDisplay::stringWidth(const char *string, const int length, const int style) const {
	XFontStruct *fs;

	if (style & STYLE_LOOKUP_MASK)
		fs = this->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A].font;
	else
		fs = this->fontStruct;
	return XTextWidth(fs, (char *)string, (int)length);
}



/*
** Translate window coordinates to the nearest (insert cursor or character
** cell) text position.  The parameter posType specifies how to interpret the
** position: CURSOR_POS means translate the coordinates to the nearest cursor
** position, and CHARACTER_POS means return the position of the character
** closest to (x, y).
*/
int TextDisplay::xyToPos(int x, int y, int posType) {
	int charIndex;
	int lineStart;
	int lineLen;
	int fontHeight;
	int visLineNum;
	int xStep;
	int outIndex;
	char expandedChar[MAX_EXP_CHAR_LEN];

	// Find the visible line number corresponding to the y coordinate
	fontHeight = this->ascent + this->descent;
	visLineNum = (y - this->rect.top) / fontHeight;
	if (visLineNum < 0)
		return this->firstChar;
	if (visLineNum >= this->nVisibleLines)
		visLineNum = this->nVisibleLines - 1;

	// Find the position at the start of the line
	lineStart = this->lineStarts[visLineNum];

	// If the line start was empty, return the last position in the buffer
	if (lineStart == -1)
		return this->buffer->BufGetLength();

	// Get the line text and its length
	lineLen = visLineLength(visLineNum);
	std::string lineStr = this->buffer->BufGetRangeEx(lineStart, lineStart + lineLen);

	/* Step through character positions from the beginning of the line
	   to find the character position corresponding to the x coordinate */
	xStep = this->rect.left - this->horizOffset;
	outIndex = 0;
	for (charIndex = 0; charIndex < lineLen; charIndex++) {
		int charLen   = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, this->buffer->tabDist_, this->buffer->nullSubsChar_);
		int charStyle = this->styleOfPos(lineStart, lineLen, charIndex, outIndex, lineStr[charIndex]);
		int charWidth = this->stringWidth(expandedChar, charLen, charStyle);
		
		if (x < xStep + (posType == CURSOR_POS ? charWidth / 2 : charWidth)) {
			return lineStart + charIndex;
		}
		xStep += charWidth;
		outIndex += charLen;
	}

	/* If the x position was beyond the end of the line, return the position
	   of the newline at the end of the line */
	return lineStart + lineLen;
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font is
** proportional, since there are no absolute columns.  The parameter posType
** specifies how to interpret the position: CURSOR_POS means translate the
** coordinates to the nearest position between characters, and CHARACTER_POS
** means translate the position to the nearest character cell.
*/
void TextDisplay::xyToUnconstrainedPos(int x, int y, int *row, int *column, int posType) {
	int fontHeight = this->ascent + this->descent;
	int fontWidth = this->fontStruct->max_bounds.width;

	// Find the visible line number corresponding to the y coordinate
	*row = (y - this->rect.top) / fontHeight;
	if (*row < 0)
		*row = 0;
	if (*row >= this->nVisibleLines)
		*row = this->nVisibleLines - 1;
	*column = ((x - this->rect.left) + this->horizOffset + (posType == CURSOR_POS ? fontWidth / 2 : 0)) / fontWidth;
	if (*column < 0)
		*column = 0;
}

/*
** Offset the line starts array, topLineNum, firstChar and lastChar, for a new
** vertical scroll position given by newTopLineNum.  If any currently displayed
** lines will still be visible, salvage the line starts values, otherwise,
** count lines from the nearest known line start (start or end of buffer, or
** the closest value in the lineStarts array)
*/
void TextDisplay::offsetLineStarts(int newTopLineNum) {
	int oldTopLineNum = this->topLineNum;
	int oldFirstChar  = this->firstChar;
	int lineDelta     = newTopLineNum - oldTopLineNum;
	int nVisLines     = this->nVisibleLines;
	int *lineStarts   = this->lineStarts;
	int i;
	int lastLineNum;
	TextBuffer *buf   = this->buffer;

	// If there was no offset, nothing needs to be changed
	if (lineDelta == 0)
		return;

	/* {   int i;
	    printf("Scroll, lineDelta %d\n", lineDelta);
	    printf("lineStarts Before: ");
	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
	    printf("\n");
	} */

	/* Find the new value for firstChar by counting lines from the nearest
	   known line start (start or end of buffer, or the closest value in the
	   lineStarts array) */
	lastLineNum = oldTopLineNum + nVisLines - 1;
	if (newTopLineNum < oldTopLineNum && newTopLineNum < -lineDelta) {
		this->firstChar = TextDCountForwardNLines(0, newTopLineNum - 1, true);
		// printf("counting forward %d lines from start\n", newTopLineNum-1);
	} else if (newTopLineNum < oldTopLineNum) {
		this->firstChar = TextDCountBackwardNLines(this->firstChar, -lineDelta);
		// printf("counting backward %d lines from firstChar\n", -lineDelta);
	} else if (newTopLineNum < lastLineNum) {
		this->firstChar = lineStarts[newTopLineNum - oldTopLineNum];
		/* printf("taking new start from lineStarts[%d]\n",
		    newTopLineNum - oldTopLineNum); */
	} else if (newTopLineNum - lastLineNum < this->nBufferLines - newTopLineNum) {
		this->firstChar = TextDCountForwardNLines(lineStarts[nVisLines - 1], newTopLineNum - lastLineNum, true);
		/* printf("counting forward %d lines from start of last line\n",
		    newTopLineNum - lastLineNum); */
	} else {
		this->firstChar = TextDCountBackwardNLines(buf->BufGetLength(), this->nBufferLines - newTopLineNum + 1);
		/* printf("counting backward %d lines from end\n",
		        this->nBufferLines - newTopLineNum + 1); */
	}

	// Fill in the line starts array
	if (lineDelta < 0 && -lineDelta < nVisLines) {
		for (i = nVisLines - 1; i >= -lineDelta; i--)
			lineStarts[i] = lineStarts[i + lineDelta];
		this->calcLineStarts(0, -lineDelta);
	} else if (lineDelta > 0 && lineDelta < nVisLines) {
		for (i = 0; i < nVisLines - lineDelta; i++)
			lineStarts[i] = lineStarts[i + lineDelta];
		this->calcLineStarts(nVisLines - lineDelta, nVisLines - 1);
	} else
		this->calcLineStarts(0, nVisLines);

	// Set lastChar and topLineNum
	calcLastChar();
	this->topLineNum = newTopLineNum;

	/* If we're numbering lines or being asked to maintain an absolute line
	   number, re-calculate the absolute line number */
	offsetAbsLineNum(oldFirstChar);

	/* {   int i;
	    printf("lineStarts After: ");
	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
	    printf("\n");
	} */
}

/*
** Update the line starts array, topLineNum, firstChar and lastChar for text
** display "textD" after a modification to the text buffer, given by the
** position where the change began "pos", and the nmubers of characters
** and lines inserted and deleted.
*/
void TextDisplay::updateLineStarts(int pos, int charsInserted, int charsDeleted, int linesInserted, int linesDeleted, int *scrolled) {
	int *lineStarts = this->lineStarts;
	int i, lineOfPos, lineOfEnd, nVisLines = this->nVisibleLines;
	int charDelta = charsInserted - charsDeleted;
	int lineDelta = linesInserted - linesDeleted;

	/* {   int i;
	    printf("linesDeleted %d, linesInserted %d, charsInserted %d, charsDeleted %d\n",
	            linesDeleted, linesInserted, charsInserted, charsDeleted);
	    printf("lineStarts Before: ");
	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
	    printf("\n");
	} */
	/* If all of the changes were before the displayed text, the display
	   doesn't change, just update the top line num and offset the line
	   start entries and first and last characters */
	if (pos + charsDeleted < this->firstChar) {
		this->topLineNum += lineDelta;
		for (i = 0; i < nVisLines && lineStarts[i] != -1; i++)
			lineStarts[i] += charDelta;
		/* {   int i;
		    printf("lineStarts after delete doesn't touch: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		this->firstChar += charDelta;
		this->lastChar += charDelta;
		*scrolled = false;
		return;
	}

	/* The change began before the beginning of the displayed text, but
	   part or all of the displayed text was deleted */
	if (pos < this->firstChar) {
		// If some text remains in the window, anchor on that
		if (posToVisibleLineNum(pos + charsDeleted, &lineOfEnd) && ++lineOfEnd < nVisLines && lineStarts[lineOfEnd] != -1) {
			this->topLineNum = std::max(1, this->topLineNum + lineDelta);
			this->firstChar = TextDCountBackwardNLines(lineStarts[lineOfEnd] + charDelta, lineOfEnd);
			// Otherwise anchor on original line number and recount everything
		} else {
			if (this->topLineNum > this->nBufferLines + lineDelta) {
				this->topLineNum = 1;
				this->firstChar = 0;
			} else
				this->firstChar = TextDCountForwardNLines(0, this->topLineNum - 1, true);
		}
		this->calcLineStarts(0, nVisLines - 1);
		/* {   int i;
		    printf("lineStarts after delete encroaches: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		// calculate lastChar by finding the end of the last displayed line
		calcLastChar();
		*scrolled = true;
		return;
	}

	/* If the change was in the middle of the displayed text (it usually is),
	   salvage as much of the line starts array as possible by moving and
	   offsetting the entries after the changed area, and re-counting the
	   added lines or the lines beyond the salvaged part of the line starts
	   array */
	if (pos <= this->lastChar) {
		// find line on which the change began
		posToVisibleLineNum(pos, &lineOfPos);
		// salvage line starts after the changed area
		if (lineDelta == 0) {
			for (i = lineOfPos + 1; i < nVisLines && lineStarts[i] != -1; i++)
				lineStarts[i] += charDelta;
		} else if (lineDelta > 0) {
			for (i = nVisLines - 1; i >= lineOfPos + lineDelta + 1; i--)
				lineStarts[i] = lineStarts[i - lineDelta] + (lineStarts[i - lineDelta] == -1 ? 0 : charDelta);
		} else /* (lineDelta < 0) */ {
			for (i = std::max(0, lineOfPos + 1); i < nVisLines + lineDelta; i++)
				lineStarts[i] = lineStarts[i - lineDelta] + (lineStarts[i - lineDelta] == -1 ? 0 : charDelta);
		}
		/* {   int i;
		    printf("lineStarts after salvage: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		// fill in the missing line starts
		if (linesInserted >= 0)
			this->calcLineStarts(lineOfPos + 1, lineOfPos + linesInserted);
		if (lineDelta < 0)
			this->calcLineStarts(nVisLines + lineDelta, nVisLines);
		/* {   int i;
		    printf("lineStarts after recalculation: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		// calculate lastChar by finding the end of the last displayed line
		calcLastChar();
		*scrolled = false;
		return;
	}

	/* Change was past the end of the displayed text, but displayable by virtue
	   of being an insert at the end of the buffer into visible blank lines */
	if (emptyLinesVisible()) {
		posToVisibleLineNum(pos, &lineOfPos);
		this->calcLineStarts(lineOfPos, lineOfPos + linesInserted);
		calcLastChar();
		/* {
		    printf("lineStarts after insert at end: ");
		    for(int i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		*scrolled = false;
		return;
	}

	// Change was beyond the end of the buffer and not visible, do nothing
	*scrolled = false;
}

/*
** Scan through the text in the "textD"'s buffer and recalculate the line
** starts array values beginning at index "startLine" and continuing through
** (including) "endLine".  It assumes that the line starts entry preceding
** "startLine" (or textD->firstChar if startLine is 0) is good, and re-counts
** newlines to fill in the requested entries.  Out of range values for
** "startLine" and "endLine" are acceptable.
*/
void TextDisplay::calcLineStarts(int startLine, int endLine) {
	int startPos, bufLen = this->buffer->BufGetLength();
	int line, lineEnd, nextLineStart, nVis = this->nVisibleLines;
	int *lineStarts = this->lineStarts;

	// Clean up (possibly) messy input parameters
	if (nVis == 0)
		return;
	if (endLine < 0)
		endLine = 0;
	if (endLine >= nVis)
		endLine = nVis - 1;
	if (startLine < 0)
		startLine = 0;
	if (startLine >= nVis)
		startLine = nVis - 1;
	if (startLine > endLine)
		return;

	// Find the last known good line number -> position mapping
	if (startLine == 0) {
		lineStarts[0] = this->firstChar;
		startLine = 1;
	}
	startPos = lineStarts[startLine - 1];

	/* If the starting position is already past the end of the text,
	   fill in -1's (means no text on line) and return */
	if (startPos == -1) {
		for (line = startLine; line <= endLine; line++)
			lineStarts[line] = -1;
		return;
	}

	/* Loop searching for ends of lines and storing the positions of the
	   start of the next line in lineStarts */
	for (line = startLine; line <= endLine; line++) {
		findLineEnd(startPos, True, &lineEnd, &nextLineStart);
		startPos = nextLineStart;
		if (startPos >= bufLen) {
			/* If the buffer ends with a newline or line break, put
			   buf->BufGetLength() in the next line start position (instead of
			   a -1 which is the normal marker for an empty line) to
			   indicate that the cursor may safely be displayed there */
			if (line == 0 || (lineStarts[line - 1] != bufLen && lineEnd != nextLineStart)) {
				lineStarts[line] = bufLen;
				line++;
			}
			break;
		}
		lineStarts[line] = startPos;
	}

	// Set any entries beyond the end of the text to -1
	for (; line <= endLine; line++)
		lineStarts[line] = -1;
}

/*
** Given a TextDisplay with a complete, up-to-date lineStarts array, update
** the lastChar entry to point to the last buffer position displayed.
*/
void TextDisplay::calcLastChar() {
	int i;

	for (i = this->nVisibleLines - 1; i > 0 && this->lineStarts[i] == -1; i--) {
		;
	}
	this->lastChar = i < 0 ? 0 : TextDEndOfLine(this->lineStarts[i], true);
}

void TextDisplay::TextDImposeGraphicsExposeTranslation(int *xOffset, int *yOffset) const {

	// NOTE(eteran): won't this skip the first item in the queue? is that desirable?
	if (graphicsExposeQueue_) {
		if (graphicExposeTranslationEntry *thisGEQEntry = graphicsExposeQueue_->next) {
			*xOffset += thisGEQEntry->horizontal;
			*yOffset += thisGEQEntry->vertical;
		}
	}
}

bool TextDisplay::TextDPopGraphicExposeQueueEntry() {
	graphicExposeTranslationEntry *removedGEQEntry = graphicsExposeQueue_;

	if (removedGEQEntry) {
		graphicsExposeQueue_ = removedGEQEntry->next;
		delete removedGEQEntry;
		return true;
	}

	return false;
}

void TextDisplay::TextDTranlateGraphicExposeQueue(int xOffset, int yOffset, bool appendEntry) {

	graphicExposeTranslationEntry *newGEQEntry = nullptr;

	if (appendEntry) {
		newGEQEntry = new graphicExposeTranslationEntry;
		newGEQEntry->next       = nullptr;
		newGEQEntry->horizontal = xOffset;
		newGEQEntry->vertical   = yOffset;
	}

	if (graphicsExposeQueue_) {

		// NOTE(eteran): won't this skip the first item in the queue? is that desirable?
		auto iter = graphicsExposeQueue_;
		for(; iter->next; iter = iter->next) {
			iter->next->horizontal += xOffset;
			iter->next->vertical   += yOffset;
		}

		if (appendEntry) {
			iter->next = newGEQEntry;
		}
	} else {
		if (appendEntry) {
			graphicsExposeQueue_ = newGEQEntry;
		}
	}
}

void TextDisplay::setScroll(int topLineNum, int horizOffset, int updateVScrollBar, int updateHScrollBar) {
	int fontHeight  = this->ascent + this->descent;
	int origHOffset = this->horizOffset;
	int lineDelta   = this->topLineNum - topLineNum;
	int xOffset;
	int yOffset;
	int exactHeight = this->rect.height - this->rect.height % (this->ascent + this->descent);

	/* Do nothing if scroll position hasn't actually changed or there's no
	   window to draw in yet */
	if (XtWindow(this->w) == 0 || (this->horizOffset == horizOffset && this->topLineNum == topLineNum))
		return;

	/* If part of the cursor is protruding beyond the text clipping region,
	   clear it off */
	blankCursorProtrusions();

	/* If the vertical scroll position has changed, update the line
	   starts array and related counters in the text display */
	offsetLineStarts(topLineNum);

	// Just setting this->horizOffset is enough information for redisplay
	this->horizOffset = horizOffset;

	/* Update the scroll bar positions if requested, note: updating the
	   horizontal scroll bars can have the further side-effect of changing
	   the horizontal scroll position, this->horizOffset */
	if (updateVScrollBar && this->vScrollBar) {
		updateVScrollBarRange();
	}
	if (updateHScrollBar && this->hScrollBar) {
		updateHScrollBarRange();
	}

	/* Redisplay everything if the window is partially obscured (since
	   it's too hard to tell what displayed areas are salvageable) or
	   if there's nothing to recover because the scroll distance is large */
	xOffset = origHOffset - this->horizOffset;
	yOffset = lineDelta * fontHeight;
	if (this->visibility != VisibilityUnobscured || abs(xOffset) > this->rect.width || abs(yOffset) > exactHeight) {
		TextDTranlateGraphicExposeQueue(xOffset, yOffset, false);
		TextDRedisplayRect(this->rect);
	} else {
		/* If the window is not obscured, paint most of the window using XCopyArea
		   from existing displayed text, and redraw only what's necessary */
		// Recover the useable window areas by moving to the proper location
		int srcX   = this->rect.left + (xOffset >= 0 ? 0 : -xOffset);
		int dstX   = this->rect.left + (xOffset >= 0 ? xOffset : 0);
		int width  = this->rect.width - abs(xOffset);
		int srcY   = this->rect.top + (yOffset >= 0 ? 0 : -yOffset);
		int dstY   = this->rect.top + (yOffset >= 0 ? yOffset : 0);
		int height = exactHeight - abs(yOffset);
		resetClipRectangles();
		TextDTranlateGraphicExposeQueue(xOffset, yOffset, true);
		XCopyArea(XtDisplay(this->w), XtWindow(this->w), XtWindow(this->w), this->gc, srcX, srcY, width, height, dstX, dstY);
		// redraw the un-recoverable parts
		if (yOffset > 0) {
			TextDRedisplayRect(this->rect.left, this->rect.top, this->rect.width, yOffset);
		} else if (yOffset < 0) {
			TextDRedisplayRect(this->rect.left, this->rect.top + this->rect.height + yOffset, this->rect.width, -yOffset);
		}
		if (xOffset > 0) {
			TextDRedisplayRect(this->rect.left, this->rect.top, xOffset, this->rect.height);
		} else if (xOffset < 0) {
			TextDRedisplayRect(this->rect.left + this->rect.width + xOffset, this->rect.top, -xOffset, this->rect.height);
		}
		// Restore protruding parts of the cursor
		textDRedisplayRange(this->cursorPos - 1, this->cursorPos + 1);
	}

	/* Refresh line number/calltip display if its up and we've scrolled
	    vertically */
	if (lineDelta != 0) {
		redrawLineNumbers(false);
		TextDRedrawCalltip(0);
	}

	HandleAllPendingGraphicsExposeNoExposeEvents(nullptr);
}

/*
** Update the minimum, maximum, slider size, page increment, and value
** for vertical scroll bar.
*/
void TextDisplay::updateVScrollBarRange() {
	int sliderSize, sliderMax, sliderValue;

	if (!this->vScrollBar) {
		return;
	}

	/* The Vert. scroll bar value and slider size directly represent the top
	   line number, and the number of visible lines respectively.  The scroll
	   bar maximum value is chosen to generally represent the size of the whole
	   buffer, with minor adjustments to keep the scroll bar widget happy */
	sliderSize = std::max(this->nVisibleLines, 1); // Avoid X warning (size < 1)
	sliderValue = this->topLineNum;
	sliderMax = std::max<int>(this->nBufferLines + 2 + text_of(w).P_cursorVPadding, sliderSize + sliderValue);
	XtVaSetValues(this->vScrollBar, XmNmaximum, sliderMax, XmNsliderSize, sliderSize, XmNpageIncrement, std::max<int>(1, this->nVisibleLines - 1), XmNvalue, sliderValue, nullptr);
}

/*
** Update the minimum, maximum, slider size, page increment, and value
** for the horizontal scroll bar.  If scroll position is such that there
** is blank space to the right of all lines of text, scroll back (adjust
** horizOffset but don't redraw) to take up the slack and position the
** right edge of the text at the right edge of the display.
**
** Note, there is some cost to this routine, since it scans the whole range
** of displayed text, particularly since it's usually called for each typed
** character!
*/
int TextDisplay::updateHScrollBarRange() {
	int i, maxWidth = 0, sliderMax, sliderWidth;
	int origHOffset = this->horizOffset;

	if (this->hScrollBar == nullptr || !XtIsManaged(this->hScrollBar))
		return false;

	// Scan all the displayed lines to find the width of the longest line
	for (i = 0; i < this->nVisibleLines && this->lineStarts[i] != -1; i++)
		maxWidth = std::max(this->measureVisLine(i), maxWidth);

	/* If the scroll position is beyond what's necessary to keep all lines
	   in view, scroll to the left to bring the end of the longest line to
	   the right margin */
	if (maxWidth < this->rect.width + this->horizOffset && this->horizOffset > 0)
		this->horizOffset = std::max(0, maxWidth - this->rect.width);

	// Readjust the scroll bar
	sliderWidth = this->rect.width;
	sliderMax = std::max(maxWidth, sliderWidth + this->horizOffset);
	XtVaSetValues(this->hScrollBar, XmNmaximum, sliderMax, XmNsliderSize, sliderWidth, XmNpageIncrement, std::max<int>(this->rect.width - 100, 10), XmNvalue, this->horizOffset, nullptr);

	// Return True if scroll position was changed
	return origHOffset != this->horizOffset;
}

/*
** Define area for drawing line numbers.  A width of 0 disables line
** number drawing.
*/
void TextDisplay::TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft) {
	int newWidth = this->rect.width + this->rect.left - textLeft;
	this->lineNumLeft = lineNumLeft;
	this->lineNumWidth = lineNumWidth;
	this->rect.left = textLeft;
	XClearWindow(XtDisplay(this->w), XtWindow(this->w));
	resetAbsLineNum();
	TextDResize(newWidth, this->rect.height);
	TextDRedisplayRect(0, this->rect.top, INT_MAX, this->rect.height);
}

/*
** Refresh the line number area.  If clearAll is false, writes only over
** the character cell areas.  Setting clearAll to True will clear out any
** stray marks outside of the character cell area, which might have been
** left from before a resize or font change.
*/
void TextDisplay::redrawLineNumbers(int clearAll) {
	int y;
	int line;
	int visLine;
	int nCols;
	char lineNumString[12];
	int lineHeight = this->ascent + this->descent;
	int charWidth  = this->fontStruct->max_bounds.width;
	XRectangle clipRect;
	Display *display = XtDisplay(this->w);

	/* Don't draw if lineNumWidth == 0 (line numbers are hidden), or widget is
	   not yet realized */
	if (this->lineNumWidth == 0 || XtWindow(this->w) == 0)
		return;

	/* Make sure we reset the clipping range for the line numbers GC, because
	   the GC may be shared (eg, if the line numbers and text have the same
	   color) and therefore the clipping ranges may be invalid. */
	clipRect.x = this->lineNumLeft;
	clipRect.y = this->rect.top;
	clipRect.width = this->lineNumWidth;
	clipRect.height = this->rect.height;
	XSetClipRectangles(display, this->lineNumGC, 0, 0, &clipRect, 1, Unsorted);

	// Erase the previous contents of the line number area, if requested
	if (clearAll)
		XClearArea(XtDisplay(this->w), XtWindow(this->w), this->lineNumLeft, this->rect.top, this->lineNumWidth, this->rect.height, false);

	// Draw the line numbers, aligned to the text
	nCols = std::min(11, this->lineNumWidth / charWidth);
	y = this->rect.top;
	line = this->getAbsTopLineNum();
	for (visLine = 0; visLine < this->nVisibleLines; visLine++) {
		int lineStart = this->lineStarts[visLine];
		if (lineStart != -1 && (lineStart == 0 || this->buffer->BufGetCharacter(lineStart - 1) == '\n')) {
			sprintf(lineNumString, "%*d", nCols, line);
			XDrawImageString(XtDisplay(this->w), XtWindow(this->w), this->lineNumGC, this->lineNumLeft, y + this->ascent, lineNumString, strlen(lineNumString));
			line++;
		} else {
			XClearArea(XtDisplay(this->w), XtWindow(this->w), this->lineNumLeft, y, this->lineNumWidth, this->ascent + this->descent, false);
			if (visLine == 0)
				line++;
		}
		y += lineHeight;
	}
}

void TextDisplay::hScrollCallback(XtPointer clientData, XtPointer callData) {

	(void)clientData;

	int newValue = reinterpret_cast<XmScrollBarCallbackStruct *>(callData)->value;

	if (newValue == this->getHorizOffset())
		return;
		
	this->setScroll(this->getTopLineNum(), newValue, false, false);
}

void TextDisplay::vScrollCallback(XtPointer clientData, XtPointer callData) {

	(void)clientData;

	int newValue = reinterpret_cast<XmScrollBarCallbackStruct *>(callData)->value;
	int lineDelta = newValue - this->getTopLineNum();

	if (lineDelta == 0)
		return;
		
	this->setScroll(newValue, this->getHorizOffset(), false, true);
}


/*
** Callbacks for drag or valueChanged on scroll bars
*/
static void vScrollCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;

	auto textD = static_cast<TextDisplay *>(clientData);
	textD->vScrollCallback(clientData, callData);
}

static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;

	auto textD = static_cast<TextDisplay *>(clientData);
	textD->hScrollCallback(clientData, callData);
}

static void visibilityEH(Widget w, XtPointer data, XEvent *event, Boolean *continueDispatch) {
	(void)w;
	(void)continueDispatch;

	/* Record whether the window is fully visible or not.  This information
	   is used for choosing the scrolling methodology for optimal performance,
	   if the window is partially obscured, XCopyArea may not work */

	auto textD = static_cast<TextDisplay *>(data);
	
	textD->setVisibility(reinterpret_cast<XVisibilityEvent *>(event)->state);
}

/*
** Count the number of newlines in a null-terminated text string;
*/
static int countLinesEx(view::string_view string) {
	int lineCount = 0;

	for (char ch : string) {
		if (ch == '\n')
			lineCount++;
	}

	return lineCount;
}

/*
** Return the width in pixels of the displayed line pointed to by "visLineNum"
*/
int TextDisplay::measureVisLine(int visLineNum) {
	int i;
	int width = 0;
	int len;
	int lineLen = visLineLength(visLineNum);
	int charCount = 0;
	int lineStartPos = this->lineStarts[visLineNum];
	char expandedChar[MAX_EXP_CHAR_LEN];

	if (!this->styleBuffer) {
		for (i = 0; i < lineLen; i++) {
			len = this->buffer->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
			width += XTextWidth(this->fontStruct, expandedChar, len);
			charCount += len;
		}
	} else {
		for (i = 0; i < lineLen; i++) {
			len = this->buffer->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
			int style = (uint8_t)this->styleBuffer->BufGetCharacter(lineStartPos + i) - ASCII_A;
			width += XTextWidth(this->styleTable[style].font, expandedChar, len);
			charCount += len;
		}
	}
	return width;
}

/*
** Return true if there are lines visible with no corresponding buffer text
*/
int TextDisplay::emptyLinesVisible() const {
	return this->nVisibleLines > 0 && this->lineStarts[this->nVisibleLines - 1] == -1;
}

/*
** When the cursor is at the left or right edge of the text, part of it
** sticks off into the clipped region beyond the text.  Normal redrawing
** can not overwrite this protruding part of the cursor, so it must be
** erased independently by calling this routine.
*/
void TextDisplay::blankCursorProtrusions() {
	int x;
	int width;
	int cursorX    = this->cursor.x;
	int cursorY    = this->cursor.y;
	int fontWidth  = this->fontStruct->max_bounds.width;
	int fontHeight = this->ascent + this->descent;
	int left       = this->rect.left;
	int right      = left + this->rect.width;

	int cursorWidth = (fontWidth / 3) * 2;
	if (cursorX >= left - 1 && cursorX <= left + cursorWidth / 2 - 1) {
		x = cursorX - cursorWidth / 2;
		width = left - x;
	} else if (cursorX >= right - cursorWidth / 2 && cursorX <= right) {
		x = right;
		width = cursorX + cursorWidth / 2 + 2 - right;
	} else
		return;

	XClearArea(XtDisplay(this->w), XtWindow(this->w), x, cursorY, width, fontHeight, false);
}

/*
** Allocate shared graphics contexts used by the widget, which must be
** re-allocated on a font change.
*/
void TextDisplay::allocateFixedFontGCs(XFontStruct *fontStruct, Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel lineNumFGPixel) {
	this->gc            = allocateGC(this->w, GCFont | GCForeground | GCBackground, fgPixel, bgPixel, fontStruct->fid, GCClipMask, GCArcMode);
	this->selectGC      = allocateGC(this->w, GCFont | GCForeground | GCBackground, selectFGPixel, selectBGPixel, fontStruct->fid, GCClipMask, GCArcMode);
	this->selectBGGC    = allocateGC(this->w, GCForeground, selectBGPixel, 0, fontStruct->fid, GCClipMask, GCArcMode);
	this->highlightGC   = allocateGC(this->w, GCFont | GCForeground | GCBackground, highlightFGPixel, highlightBGPixel, fontStruct->fid, GCClipMask, GCArcMode);
	this->highlightBGGC = allocateGC(this->w, GCForeground, highlightBGPixel, 0, fontStruct->fid, GCClipMask, GCArcMode);
	this->lineNumGC     = allocateGC(this->w, GCFont | GCForeground | GCBackground, lineNumFGPixel, bgPixel, fontStruct->fid, GCClipMask, GCArcMode);
}

/*
** X11R4 does not have the XtAllocateGC function for sharing graphics contexts
** with changeable fields.  Unfortunately the R4 call for creating shared
** graphics contexts (XtGetGC) is rarely useful because most widgets need
** to be able to set and change clipping, and that makes the GC unshareable.
**
** This function allocates and returns a gc, using XtAllocateGC if possible,
** or XCreateGC on X11R4 systems where XtAllocateGC is not available.
*/
static GC allocateGC(Widget w, unsigned long valueMask, unsigned long foreground, unsigned long background, Font font, unsigned long dynamicMask, unsigned long dontCareMask) {
	XGCValues gcValues;

	gcValues.font = font;
	gcValues.background = background;
	gcValues.foreground = foreground;
#if defined(XlibSpecificationRelease) && XlibSpecificationRelease > 4
	return XtAllocateGC(w, 0, valueMask, &gcValues, dynamicMask, dontCareMask);
#else
	return XCreateGC(XtDisplay(w), RootWindowOfScreen(XtScreen(w)), valueMask, &gcValues);
#endif
}

/*
** Release a gc allocated with allocateGC above
*/
static void releaseGC(Widget w, GC gc) {
#if defined(XlibSpecificationRelease) && XlibSpecificationRelease > 4
	XtReleaseGC(w, gc);
#else
	XFreeGC(XtDisplay(w), gc);
#endif
}

/*
** resetClipRectangles sets the clipping rectangles for GCs which clip
** at the text boundary (as opposed to the window boundary).  These GCs
** are shared such that the drawing styles are constant, but the clipping
** rectangles are allowed to change among different users of the GCs (the
** GCs were created with XtAllocGC).  This routine resets them so the clipping
** rectangles are correct for this text display.
*/
void TextDisplay::resetClipRectangles() {
	XRectangle clipRect;
	Display *display = XtDisplay(this->w);

	clipRect.x = this->rect.left;
	clipRect.y = this->rect.top;
	clipRect.width = this->rect.width;
	clipRect.height = this->rect.height - this->rect.height % (this->ascent + this->descent);

	XSetClipRectangles(display, this->gc, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, this->selectGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, this->highlightGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, this->selectBGGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, this->highlightBGGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, this->styleGC, 0, 0, &clipRect, 1, Unsorted);
}

/*
** Return the length of a line (number of displayable characters) by examining
** entries in the line starts array rather than by scanning for newlines
*/
int TextDisplay::visLineLength(int visLineNum) {
	int nextLineStart, lineStartPos = this->lineStarts[visLineNum];

	if (lineStartPos == -1)
		return 0;
	if (visLineNum + 1 >= this->nVisibleLines)
		return this->lastChar - lineStartPos;
	nextLineStart = this->lineStarts[visLineNum + 1];
	if (nextLineStart == -1)
		return this->lastChar - lineStartPos;
	if (wrapUsesCharacter(nextLineStart - 1))
		return nextLineStart - 1 - lineStartPos;
	return nextLineStart - lineStartPos;
}

/** When continuous wrap is on, and the user inserts or deletes characters,
** wrapping can happen before and beyond the changed position.  This routine
** finds the extent of the changes, and counts the deleted and inserted lines
** over that range.  It also attempts to minimize the size of the range to
** what has to be counted and re-displayed, so the results can be useful
** both for delimiting where the line starts need to be recalculated, and
** for deciding what part of the text to redisplay.
*/
void TextDisplay::findWrapRangeEx(view::string_view deletedText, int pos, int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd, int *linesInserted, int *linesDeleted) {
	int length;
	int retPos;
	int retLines;
	int retLineStart;
	int retLineEnd;
	TextBuffer *buf = this->buffer;
	int nVisLines   = this->nVisibleLines;
	int *lineStarts = this->lineStarts;
	int countFrom;
	int countTo;
	int lineStart;
	int adjLineStart;
	int visLineNum = 0;
	int nLines = 0;

	/*
	** Determine where to begin searching: either the previous newline, or
	** if possible, limit to the start of the (original) previous displayed
	** line, using information from the existing line starts array
	*/
	if (pos >= this->firstChar && pos <= this->lastChar) {
		int i;
		for (i = nVisLines - 1; i > 0; i--)
			if (lineStarts[i] != -1 && pos >= lineStarts[i])
				break;
		if (i > 0) {
			countFrom = lineStarts[i - 1];
			visLineNum = i - 1;
		} else
			countFrom = buf->BufStartOfLine(pos);
	} else
		countFrom = buf->BufStartOfLine(pos);

	/*
	** Move forward through the (new) text one line at a time, counting
	** displayed lines, and looking for either a real newline, or for the
	** line starts to re-sync with the original line starts array
	*/
	lineStart = countFrom;
	*modRangeStart = countFrom;
	while (true) {

		/* advance to the next line.  If the line ended in a real newline
		   or the end of the buffer, that's far enough */
		wrappedLineCounter(buf, lineStart, buf->BufGetLength(), 1, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retPos >= buf->BufGetLength()) {
			countTo = buf->BufGetLength();
			*modRangeEnd = countTo;
			if (retPos != retLineEnd)
				nLines++;
			break;
		} else
			lineStart = retPos;
		nLines++;
		if (lineStart > pos + nInserted && buf->BufGetCharacter(lineStart - 1) == '\n') {
			countTo = lineStart;
			*modRangeEnd = lineStart;
			break;
		}

		/* Don't try to resync in continuous wrap mode with non-fixed font
		   sizes; it would result in a chicken-and-egg dependency between
		   the calculations for the inserted and the deleted lines.
		       If we're in that mode, the number of deleted lines is calculated in
		       advance, without resynchronization, so we shouldn't resynchronize
		       for the inserted lines either. */
		if (this->suppressResync) {
			continue;
		}

		/* check for synchronization with the original line starts array
		   before pos, if so, the modified range can begin later */
		if (lineStart <= pos) {
			while (visLineNum < nVisLines && lineStarts[visLineNum] < lineStart)
				visLineNum++;
			if (visLineNum < nVisLines && lineStarts[visLineNum] == lineStart) {
				countFrom = lineStart;
				nLines = 0;
				if (visLineNum + 1 < nVisLines && lineStarts[visLineNum + 1] != -1)
					*modRangeStart = std::min(pos, lineStarts[visLineNum + 1] - 1);
				else
					*modRangeStart = countFrom;
			} else
				*modRangeStart = std::min(*modRangeStart, lineStart - 1);
		}

		/* check for synchronization with the original line starts array
		       after pos, if so, the modified range can end early */
		else if (lineStart > pos + nInserted) {
			adjLineStart = lineStart - nInserted + nDeleted;
			while (visLineNum < nVisLines && lineStarts[visLineNum] < adjLineStart)
				visLineNum++;
			if (visLineNum < nVisLines && lineStarts[visLineNum] != -1 && lineStarts[visLineNum] == adjLineStart) {
				countTo = TextDEndOfLine(lineStart, true);
				*modRangeEnd = lineStart;
				break;
			}
		}
	}
	*linesInserted = nLines;

	/* Count deleted lines between countFrom and countTo as the text existed
	   before the modification (that is, as if the text between pos and
	   pos+nInserted were replaced by "deletedText").  This extra context is
	   necessary because wrapping can occur outside of the modified region
	   as a result of adding or deleting text in the region. This is done by
	   creating a TextBuffer containing the deleted text and the necessary
	   additional context, and calling the wrappedLineCounter on it.

	   NOTE: This must not be done in continuous wrap mode when the font
	     width is not fixed. In that case, the calculation would try
	     to access style information that is no longer available (deleted
	     text), or out of date (updated highlighting), possibly leading
	     to completely wrong calculations and/or even crashes eventually.
	     (This is not theoretical; it really happened.)

	     In that case, the calculation of the number of deleted lines
	     has happened before the buffer was modified (only in that case,
	     because resynchronization of the line starts is impossible
	     in that case, which makes the whole calculation less efficient).
	*/
	if (this->suppressResync) {
		*linesDeleted = this->nLinesDeleted;
		this->suppressResync = false;
		return;
	}

	length = (pos - countFrom) + nDeleted + (countTo - (pos + nInserted));
	auto deletedTextBuf = new TextBuffer(length);
	if (pos > countFrom)
		deletedTextBuf->BufCopyFromBuf(this->buffer, countFrom, pos, 0);
	if (nDeleted != 0)
		deletedTextBuf->BufInsertEx(pos - countFrom, deletedText);
	if (countTo > pos + nInserted)
		deletedTextBuf->BufCopyFromBuf(this->buffer, pos + nInserted, countTo, pos - countFrom + nDeleted);
	/* Note that we need to take into account an offset for the style buffer:
	   the deletedTextBuf can be out of sync with the style buffer. */
	wrappedLineCounter(deletedTextBuf, 0, length, INT_MAX, True, countFrom, &retPos, &retLines, &retLineStart, &retLineEnd);
	delete deletedTextBuf;
	*linesDeleted = retLines;
	this->suppressResync = false;
}

/*
** This is a stripped-down version of the findWrapRange() function above,
** intended to be used to calculate the number of "deleted" lines during
** a buffer modification. It is called _before_ the modification takes place.
**
** This function should only be called in continuous wrap mode with a
** non-fixed font width. In that case, it is impossible to calculate
** the number of deleted lines, because the necessary style information
** is no longer available _after_ the modification. In other cases, we
** can still perform the calculation afterwards (possibly even more
** efficiently).
*/
void TextDisplay::measureDeletedLines(int pos, int nDeleted) {
	int retPos;
	int retLines;
	int retLineStart;
	int retLineEnd;
	TextBuffer *buf = this->buffer;
	int nVisLines   = this->nVisibleLines;
	int *lineStarts = this->lineStarts;
	int countFrom;
	int lineStart;
	int nLines = 0;
	/*
	** Determine where to begin searching: either the previous newline, or
	** if possible, limit to the start of the (original) previous displayed
	** line, using information from the existing line starts array
	*/
	if (pos >= this->firstChar && pos <= this->lastChar) {
		int i;
		for (i = nVisLines - 1; i > 0; i--)
			if (lineStarts[i] != -1 && pos >= lineStarts[i])
				break;
		if (i > 0) {
			countFrom = lineStarts[i - 1];
		} else
			countFrom = buf->BufStartOfLine(pos);
	} else
		countFrom = buf->BufStartOfLine(pos);

	/*
	** Move forward through the (new) text one line at a time, counting
	** displayed lines, and looking for either a real newline, or for the
	** line starts to re-sync with the original line starts array
	*/
	lineStart = countFrom;
	while (true) {
		/* advance to the next line.  If the line ended in a real newline
		   or the end of the buffer, that's far enough */
		wrappedLineCounter(buf, lineStart, buf->BufGetLength(), 1, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retPos >= buf->BufGetLength()) {
			if (retPos != retLineEnd)
				nLines++;
			break;
		} else
			lineStart = retPos;
		nLines++;
		if (lineStart > pos + nDeleted && buf->BufGetCharacter(lineStart - 1) == '\n') {
			break;
		}

		/* Unlike in the findWrapRange() function above, we don't try to
		   resync with the line starts, because we don't know the length
		   of the inserted text yet, nor the updated style information.

		   Because of that, we also shouldn't resync with the line starts
		   after the modification either, because we must perform the
		   calculations for the deleted and inserted lines in the same way.

		   This can result in some unnecessary recalculation and redrawing
		   overhead, and therefore we should only use this two-phase mode
		   of calculation when it's really needed (continuous wrap + variable
		   font width). */
	}
	this->nLinesDeleted = nLines;
	this->suppressResync = true;
}

/*
** Count forward from startPos to either maxPos or maxLines (whichever is
** reached first), and return all relevant positions and line count.
** The provided TextBuffer may differ from the actual text buffer of the
** widget. In that case it must be a (partial) copy of the actual text buffer
** and the styleBufOffset argument must indicate the starting position of the
** copy, to take into account the correct style information.
**
** Returned values:
**
**   retPos:	    Position where counting ended.  When counting lines, the
**  	    	    position returned is the start of the line "maxLines"
**  	    	    lines beyond "startPos".
**   retLines:	    Number of line breaks counted
**   retLineStart:  Start of the line where counting ended
**   retLineEnd:    End position of the last line traversed
*/
void TextDisplay::wrappedLineCounter(const TextBuffer *buf, const int startPos, const int maxPos, const int maxLines, const Boolean startPosIsLineStart, const int styleBufOffset, int *retPos, int *retLines, int *retLineStart, int *retLineEnd) const {
	int lineStart, newLineStart = 0, b, p, colNum, wrapMargin;
	int maxWidth, width, countPixels, i, foundBreak;
	int nLines = 0, tabDist = this->buffer->tabDist_;
	char nullSubsChar = this->buffer->nullSubsChar_;

	/* If the font is fixed, or there's a wrap margin set, it's more efficient
	   to measure in columns, than to count pixels.  Determine if we can count
	   in columns (countPixels == False) or must count pixels (countPixels ==
	   True), and set the wrap target for either pixels or columns */
	if (this->fixedFontWidth != -1 || this->wrapMargin != 0) {
		countPixels = false;
		wrapMargin = this->wrapMargin != 0 ? this->wrapMargin : this->rect.width / this->fixedFontWidth;
		maxWidth = INT_MAX;
	} else {
		countPixels = true;
		wrapMargin = INT_MAX;
		maxWidth = this->rect.width;
	}

	/* Find the start of the line if the start pos is not marked as a
	   line start. */
	if (startPosIsLineStart)
		lineStart = startPos;
	else
		lineStart = TextDStartOfLine(startPos);

	/*
	** Loop until position exceeds maxPos or line count exceeds maxLines.
	** (actually, contines beyond maxPos to end of line containing maxPos,
	** in case later characters cause a word wrap back before maxPos)
	*/
	colNum = 0;
	width = 0;
	for (p = lineStart; p < buf->BufGetLength(); p++) {
		uint8_t c = buf->BufGetCharacter(p);

		/* If the character was a newline, count the line and start over,
		   otherwise, add it to the width and column counts */
		if (c == '\n') {
			if (p >= maxPos) {
				*retPos = maxPos;
				*retLines = nLines;
				*retLineStart = lineStart;
				*retLineEnd = maxPos;
				return;
			}
			nLines++;
			if (nLines >= maxLines) {
				*retPos = p + 1;
				*retLines = nLines;
				*retLineStart = p + 1;
				*retLineEnd = p;
				return;
			}
			lineStart = p + 1;
			colNum = 0;
			width = 0;
		} else {
			colNum += TextBuffer::BufCharWidth(c, colNum, tabDist, nullSubsChar);
			if (countPixels)
				width += this->measurePropChar(c, colNum, p + styleBufOffset);
		}

		/* If character exceeded wrap margin, find the break point
		   and wrap there */
		if (colNum > wrapMargin || width > maxWidth) {
			foundBreak = false;
			for (b = p; b >= lineStart; b--) {
				c = buf->BufGetCharacter(b);
				if (c == '\t' || c == ' ') {
					newLineStart = b + 1;
					if (countPixels) {
						colNum = 0;
						width = 0;
						for (i = b + 1; i < p + 1; i++) {
							width += this->measurePropChar(buf->BufGetCharacter(i), colNum, i + styleBufOffset);
							colNum++;
						}
					} else
						colNum = buf->BufCountDispChars(b + 1, p + 1);
					foundBreak = true;
					break;
				}
			}
			if (!foundBreak) { // no whitespace, just break at margin
				newLineStart = std::max(p, lineStart + 1);
				colNum = TextBuffer::BufCharWidth(c, colNum, tabDist, nullSubsChar);
				if (countPixels)
					width = this->measurePropChar(c, colNum, p + styleBufOffset);
			}
			if (p >= maxPos) {
				*retPos = maxPos;
				*retLines = maxPos < newLineStart ? nLines : nLines + 1;
				*retLineStart = maxPos < newLineStart ? lineStart : newLineStart;
				*retLineEnd = maxPos;
				return;
			}
			nLines++;
			if (nLines >= maxLines) {
				*retPos = foundBreak ? b + 1 : std::max<int>(p, lineStart + 1);
				*retLines = nLines;
				*retLineStart = lineStart;
				*retLineEnd = foundBreak ? b : p;
				return;
			}
			lineStart = newLineStart;
		}
	}

	// reached end of buffer before reaching pos or line target
	*retPos       = buf->BufGetLength();
	*retLines     = nLines;
	*retLineStart = lineStart;
	*retLineEnd   = buf->BufGetLength();
}

/*
** Measure the width in pixels of a character "c" at a particular column
** "colNum" and buffer position "pos".  This is for measuring characters in
** proportional or mixed-width highlighting fonts.
**
** A note about proportional and mixed-width fonts: the mixed width and
** proportional font code in nedit does not get much use in general editing,
** because nedit doesn't allow per-language-mode fonts, and editing programs
** in a proportional font is usually a bad idea, so very few users would
** choose a proportional font as a default.  There are still probably mixed-
** width syntax highlighting cases where things don't redraw properly for
** insertion/deletion, though static display and wrapping and resizing
** should now be solid because they are now used for online help display.
*/
int TextDisplay::measurePropChar(const char c, const int colNum, const int pos) const {
	int charLen, style;
	char expChar[MAX_EXP_CHAR_LEN];
	TextBuffer *styleBuf = this->styleBuffer;

	charLen = TextBuffer::BufExpandCharacter(c, colNum, expChar, this->buffer->tabDist_, this->buffer->nullSubsChar_);
	if(!styleBuf) {
		style = 0;
	} else {
		style = (uint8_t)styleBuf->BufGetCharacter(pos);
		if (style == this->unfinishedStyle) {
			// encountered "unfinished" style, trigger parsing
			(this->unfinishedHighlightCB)(this, pos, this->highlightCBArg);
			style = (uint8_t)styleBuf->BufGetCharacter(pos);
		}
	}
	return this->stringWidth(expChar, charLen, style);
}

/*
** Finds both the end of the current line and the start of the next line.  Why?
** In continuous wrap mode, if you need to know both, figuring out one from the
** other can be expensive or error prone.  The problem comes when there's a
** trailing space or tab just before the end of the buffer.  To translate an
** end of line value to or from the next lines start value, you need to know
** whether the trailing space or tab is being used as a line break or just a
** normal character, and to find that out would otherwise require counting all
** the way back to the beginning of the line.
*/
void TextDisplay::findLineEnd(int startPos, int startPosIsLineStart, int *lineEnd, int *nextLineStart) {
	int retLines, retLineStart;

	// if we're not wrapping use more efficient BufEndOfLine
	if (!this->continuousWrap) {
		*lineEnd = this->buffer->BufEndOfLine(startPos);
		*nextLineStart = std::min<int>(this->buffer->BufGetLength(), *lineEnd + 1);
		return;
	}

	// use the wrapped line counter routine to count forward one line
	wrappedLineCounter(this->buffer, startPos, this->buffer->BufGetLength(), 1, startPosIsLineStart, 0, nextLineStart, &retLines, &retLineStart, lineEnd);
}

/*
** Line breaks in continuous wrap mode usually happen at newlines or
** whitespace.  This line-terminating character is not included in line
** width measurements and has a special status as a non-visible character.
** However, lines with no whitespace are wrapped without the benefit of a
** line terminating character, and this distinction causes endless trouble
** with all of the text display code which was originally written without
** continuous wrap mode and always expects to wrap at a newline character.
**
** Given the position of the end of the line, as returned by TextDEndOfLine
** or BufEndOfLine, this returns true if there is a line terminating
** character, and false if there's not.  On the last character in the
** buffer, this function can't tell for certain whether a trailing space was
** used as a wrap point, and just guesses that it wasn't.  So if an exact
** accounting is necessary, don't use this function.
*/
int TextDisplay::wrapUsesCharacter(int lineEndPos) {
	char c;

	if (!this->continuousWrap || lineEndPos == this->buffer->BufGetLength())
		return true;

	c = this->buffer->BufGetCharacter(lineEndPos);
	return c == '\n' || ((c == '\t' || c == ' ') && lineEndPos + 1 != this->buffer->BufGetLength());
}

/*
** Decide whether the user needs (or may need) a horizontal scroll bar,
** and manage or unmanage the scroll bar widget accordingly.  The H.
** scroll bar is only hidden in continuous wrap mode when it's absolutely
** certain that the user will not need it: when wrapping is set
** to the window edge, or when the wrap margin is strictly less than
** the longest possible line.
*/
void TextDisplay::hideOrShowHScrollBar() {
	if (this->continuousWrap && (this->wrapMargin == 0 || this->wrapMargin * this->fontStruct->max_bounds.width < this->rect.width))
		XtUnmanageChild(this->hScrollBar);
	else
		XtManageChild(this->hScrollBar);
}

/*
** Extend the range of a redraw request (from *start to *end) with additional
** redraw requests resulting from changes to the attached style buffer (which
** contains auxiliary information for coloring or styling text).
*/
void TextDisplay::extendRangeForStyleMods(int *start, int *end) {
	TextSelection *sel = &this->styleBuffer->primary_;
	int extended = false;

	/* The peculiar protocol used here is that modifications to the style
	   buffer are marked by selecting them with the buffer's primary selection.
	   The style buffer is usually modified in response to a modify callback on
	   the text buffer BEFORE TextDisplay.c's modify callback, so that it can keep
	   the style buffer in step with the text buffer.  The style-update
	   callback can't just call for a redraw, because TextDisplay hasn't processed
	   the original text changes yet.  Anyhow, to minimize redrawing and to
	   avoid the complexity of scheduling redraws later, this simple protocol
	   tells the text display's buffer modify callback to extend it's redraw
	   range to show the text color/and font changes as well. */
	if (sel->selected) {
		if (sel->start < *start) {
			*start = sel->start;
			extended = true;
		}
		if (sel->end > *end) {
			*end = sel->end;
			extended = true;
		}
	}

	/* If the selection was extended due to a style change, and some of the
	   fonts don't match in spacing, extend redraw area to end of line to
	   redraw characters exposed by possible font size changes */
	if (this->fixedFontWidth == -1 && extended)
		*end = this->buffer->BufEndOfLine(*end) + 1;
}

/**********************  Backlight Functions ******************************/
/*
** Allocate a read-only (shareable) colormap cell for a named color, from the
** the default colormap of the screen on which the widget (w) is displayed. If
** the colormap is full and there's no suitable substitute, print an error on
** stderr, and return the widget's background color as a backup.
*/
static Pixel allocBGColor(Widget w, char *colorName, int *ok) {
	*ok = 1;
	return AllocColor(w, colorName);
}

Pixel TextDisplay::getRangesetColor(int ind, Pixel bground) {

	TextBuffer *buf;
	RangesetTable *tab;
	Pixel color;
	int valid;

	if (ind > 0) {
		ind--;
		buf = this->buffer;
		tab = buf->rangesetTable_;

		valid = tab->RangesetTableGetColorValid(ind, &color);
		if (valid == 0) {
			char *color_name = tab->RangesetTableGetColorName(ind);
			if (color_name)
				color = allocBGColor(this->w, color_name, &valid);
			tab->RangesetTableAssignColorPixel(ind, color, valid);
		}
		if (valid > 0) {
			return color;
		}
	}
	return bground;
}

/*
** Read the background color class specification string in str, allocating the
** necessary colors, and allocating and setting up the character->class_no and
** class_no->pixel map arrays, returned via *pp_bgClass and *pp_bgClassPixel
** respectively.
** Note: the allocation of class numbers could be more intelligent: there can
** never be more than 256 of these (one per character); but I don't think
** there'll be a pressing need. I suppose the scanning of the specification
** could be better too, but then, who cares!
*/
void TextDisplay::TextDSetupBGClasses(Widget w, XmString str, Pixel **pp_bgClassPixel, uint8_t **pp_bgClass, Pixel bgPixelDefault) {
	uint8_t bgClass[256];
	Pixel bgClassPixel[256];
	int class_no = 0;
	char *semicol;

	char *s = reinterpret_cast<char *>(str);

	int lo, hi, dummy;
	char *pos;

	delete [] *pp_bgClass;
	delete [] *pp_bgClassPixel;

	*pp_bgClassPixel = nullptr;
	*pp_bgClass      = nullptr;

	if (!s)
		return;

	// default for all chars is class number zero, for standard background
	memset(bgClassPixel, 0, sizeof bgClassPixel);
	memset(bgClass, 0, sizeof bgClass);
	bgClassPixel[0] = bgPixelDefault;
	/* since class no == 0 in a "style" has no set bits in BACKLIGHT_MASK
	   (see styleOfPos()), when drawString() is called for text with a
	   backlight class no of zero, bgClassPixel[0] is never consulted, and
	   the default background color is chosen. */

	/* The format of the class string s is:
	          low[-high]{,low[-high]}:color{;low-high{,low[-high]}:color}
	      eg
	          32-255:#f0f0f0;1-31,127:red;128-159:orange;9-13:#e5e5e5
	   where low and high represent a character range between ordinal
	   ASCII values. Using strtol() allows automatic octal, dec and hex
	   reading of low and high. The example format sets backgrounds as follows:
	          char   1 - 8    colored red     (control characters)
	          char   9 - 13   colored #e5e5e5 (isspace() control characters)
	          char  14 - 31   colored red     (control characters)
	          char  32 - 126  colored #f0f0f0
	          char 127        colored red     (delete character)
	          char 128 - 159  colored orange  ("shifted" control characters)
	          char 160 - 255  colored #f0f0f0
	   Notice that some of the later ranges overwrite the class values defined
	   for earlier ones (eg the first clause, 32-255:#f0f0f0 sets the DEL
	   character background color to #f0f0f0; it is then set to red by the
	   clause 1-31,127:red). */

	while (s && class_no < 255) {
		class_no++; // simple class alloc scheme
		size_t was_semicol = 0;
		bool is_good = true;
		if ((semicol = strchr(s, ';'))) {
			*semicol = '\0'; // null-terminate low[-high]:color clause
			was_semicol = 1;
		}

		/* loop over ranges before the color spec, assigning the characters
		   in the ranges to the current class number */
		for (lo = hi = strtol(s, &pos, 0); is_good; lo = hi = strtol(pos + 1, &pos, 0)) {
			if (pos && *pos == '-')
				hi = strtol(pos + 1, &pos, 0); // get end of range
			is_good = (pos && 0 <= lo && lo <= hi && hi <= 255);
			if (is_good)
				while (lo <= hi)
					bgClass[lo++] = (uint8_t)class_no;
			if (*pos != ',')
				break;
		}
		if ((is_good = (is_good && *pos == ':'))) {
			is_good = (*pos++ != '\0'); // pos now points to color
			bgClassPixel[class_no] = allocBGColor(w, pos, &dummy);
		}
		if (!is_good) {
			// complain? this class spec clause (in string s) was faulty
		}

		// end of loop iterator clauses
		if (was_semicol)
			*semicol = ';'; // un-null-terminate low[-high]:color clause
		s = semicol + was_semicol;
	}

	/* when we get here, we've set up our class table and class-to-pixel table
	   in local variables: now put them into the "real thing" */
	class_no++; // bigger than all valid class_nos
	*pp_bgClass      = new uint8_t[256];
	*pp_bgClassPixel = new Pixel[class_no];

	if (!*pp_bgClass || !*pp_bgClassPixel) {
		delete [] *pp_bgClass;
		delete [] *pp_bgClassPixel;
		return;
	}

	std::copy_n(bgClass, 256, *pp_bgClass);
	std::copy_n(bgClassPixel, class_no, *pp_bgClassPixel);
}

/*
** Fetch text from the widget's buffer, adding wrapping newlines to emulate
** effect acheived by wrapping in the text display in continuous wrap mode.
*/
std::string TextDisplay::TextGetWrappedEx(int startPos, int endPos) {

	TextBuffer *buf = this->buffer;

	if (!text_of(w).P_continuousWrap || startPos == endPos) {
		return buf->BufGetRangeEx(startPos, endPos);
	}

	/* Create a text buffer with a good estimate of the size that adding
	   newlines will expand it to.  Since it's a text buffer, if we guess
	   wrong, it will fail softly, and simply expand the size */
	auto outBuf = mem::make_unique<TextBuffer>((endPos - startPos) + (endPos - startPos) / 5);
	int outPos = 0;

	/* Go (displayed) line by line through the buffer, adding newlines where
	   the text is wrapped at some character other than an existing newline */
	int fromPos = startPos;
	int toPos = TextDCountForwardNLines(startPos, 1, false);
	while (toPos < endPos) {
		outBuf->BufCopyFromBuf(buf, fromPos, toPos, outPos);
		outPos += toPos - fromPos;
		char c = outBuf->BufGetCharacter(outPos - 1);
		if (c == ' ' || c == '\t')
			outBuf->BufReplaceEx(outPos - 1, outPos, "\n");
		else if (c != '\n') {
			outBuf->BufInsertEx(outPos, "\n");
			outPos++;
		}
		fromPos = toPos;
		toPos = TextDCountForwardNLines(fromPos, 1, true);
	}
	outBuf->BufCopyFromBuf(buf, fromPos, endPos, outPos);

	// return the contents of the output buffer as a string
	std::string outString = outBuf->BufGetAllEx();
	return outString;
}

void TextDisplay::TextCopyClipboard(Time time) {
	cancelDrag();

	if (!this->buffer->primary_.selected) {
		QApplication::beep();
		return;
	}

	CopyToClipboard(time);
}

void TextDisplay::TextCutClipboard(Time time) {

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (!this->buffer->primary_.selected) {
		QApplication::beep();
		return;
	}

	TakeMotifDestination(time);
	CopyToClipboard(time);
	this->buffer->BufRemoveSelected();
	TextDSetInsertPosition(this->buffer->cursorPosHint_);
	this->checkAutoShowInsertPos();
}

int TextDisplay::TextFirstVisibleLine() const {
	return this->topLineNum;
}

	int TextDisplay::TextFirstVisiblePos() const {
	return this->firstChar;
}

/*
** Set the text buffer which this widget will display and interact with.
** The currently attached buffer is automatically freed, ONLY if it has
** no additional modify procs attached (as it would if it were being
** displayed by another text widget).
*/
void TextDisplay::TextSetBuffer(TextBuffer *buffer) {
	TextBuffer *oldBuf = this->buffer;

	StopHandlingXSelections();
	TextDSetBuffer(buffer);
	if (oldBuf->modifyProcs_.empty())
		delete oldBuf;
}

/*
** Set the cursor position
*/
void TextDisplay::TextSetCursorPos(int pos) {
	TextDSetInsertPosition(pos);
	this->checkAutoShowInsertPos();
	callCursorMovementCBs(nullptr);
}

/*
** Return the cursor position
*/
int TextDisplay::TextGetCursorPos() const {
	return TextDGetInsertPosition();
}

int TextDisplay::TextLastVisiblePos() const {
	return lastChar;
}

int TextDisplay::TextNumVisibleLines() const {
	return this->nVisibleLines;
}

int TextDisplay::TextVisibleWidth() const {
	return this->rect.width;
}

/*
** Get the buffer associated with this text widget.  Note that attaching
** additional modify callbacks to the buffer will prevent it from being
** automatically freed when the widget is destroyed.
*/
TextBuffer *TextDisplay::TextGetBuffer() {
	return this->buffer;
}


/*
** Insert text "chars" at the cursor position, respecting pending delete
** selections, overstrike, and handling cursor repositioning as if the text
** had been typed.  If autoWrap is on wraps the text to fit within the wrap
** margin, auto-indenting where the line was wrapped (but nowhere else).
** "allowPendingDelete" controls whether primary selections in the widget are
** treated as pending delete selections (True), or ignored (False). "event"
** is optional and is just passed on to the cursor movement callbacks.
*/
void TextDisplay::TextInsertAtCursorEx(view::string_view chars, XEvent *event, bool allowPendingDelete, bool allowWrap) {
	int wrapMargin, colNum, lineStartPos, cursorPos;
	TextWidget tw   = reinterpret_cast<TextWidget>(w);
	TextBuffer *buf = this->buffer;
	int fontWidth   = this->fontStruct->max_bounds.width;
	int replaceSel, singleLine, breakAt = 0;

	// Don't wrap if auto-wrap is off or suppressed, or it's just a newline
	if (!allowWrap || !text_of(tw).P_autoWrap || (chars[0] == '\n' && chars[1] == '\0')) {
		simpleInsertAtCursorEx(chars, event, allowPendingDelete);
		return;
	}

	/* If this is going to be a pending delete operation, the real insert
	   position is the start of the selection.  This will make rectangular
	   selections wrap strangely, but this routine should rarely be used for
	   them, and even more rarely when they need to be wrapped. */
	replaceSel = allowPendingDelete && pendingSelection();
	cursorPos = replaceSel ? buf->primary_.start : TextDGetInsertPosition();

	/* If the text is only one line and doesn't need to be wrapped, just insert
	   it and be done (for efficiency only, this routine is called for each
	   character typed). (Of course, it may not be significantly more efficient
	   than the more general code below it, so it may be a waste of time!) */
	wrapMargin = text_of(tw).P_wrapMargin != 0 ? text_of(tw).P_wrapMargin : this->rect.width / fontWidth;
	lineStartPos = buf->BufStartOfLine(cursorPos);
	colNum = buf->BufCountDispChars(lineStartPos, cursorPos);

	auto it = chars.begin();
	for (; it != chars.end() && *it != '\n'; it++) {
		colNum += TextBuffer::BufCharWidth(*it, colNum, buf->tabDist_, buf->nullSubsChar_);
	}

	singleLine = it == chars.end();
	if (colNum < wrapMargin && singleLine) {
		simpleInsertAtCursorEx(chars, event, true);
		return;
	}

	// Wrap the text
	std::string lineStartText = buf->BufGetRangeEx(lineStartPos, cursorPos);
	std::string wrappedText = wrapTextEx(lineStartText, chars, lineStartPos, wrapMargin, replaceSel ? nullptr : &breakAt);

	/* Insert the text.  Where possible, use TextDInsert which is optimized
	   for less redraw. */
	if (replaceSel) {
		buf->BufReplaceSelectedEx(wrappedText);
		TextDSetInsertPosition(buf->cursorPosHint_);
	} else if (text_of(tw).P_overstrike) {
		if (breakAt == 0 && singleLine)
			TextDOverstrikeEx(wrappedText);
		else {
			buf->BufReplaceEx(cursorPos - breakAt, cursorPos, wrappedText);
			TextDSetInsertPosition(buf->cursorPosHint_);
		}
	} else {
		if (breakAt == 0) {
			TextDInsertEx(wrappedText);
		} else {
			buf->BufReplaceEx(cursorPos - breakAt, cursorPos, wrappedText);
			TextDSetInsertPosition(buf->cursorPosHint_);
		}
	}
	this->checkAutoShowInsertPos();
	callCursorMovementCBs(event);
}


/*
** Wrap multi-line text in argument "text" to be inserted at the end of the
** text on line "startLine" and return the result.  If "breakBefore" is
** non-nullptr, allow wrapping to extend back into "startLine", in which case
** the returned text will include the wrapped part of "startLine", and
** "breakBefore" will return the number of characters at the end of
** "startLine" that were absorbed into the returned string.  "breakBefore"
** will return zero if no characters were absorbed into the returned string.
** The buffer offset of text in the widget's text buffer is needed so that
** smart indent (which can be triggered by wrapping) can search back farther
** in the buffer than just the text in startLine.
*/
std::string TextDisplay::wrapTextEx(view::string_view startLine, view::string_view text, int bufOffset, int wrapMargin, int *breakBefore) {
	TextBuffer *buf = this->buffer;
	int startLineLen = startLine.size();
	int colNum, pos, lineStartPos, limitPos, breakAt, charsAdded;
	int firstBreak = -1, tabDist = buf->tabDist_;
	std::string wrappedText;

	// Create a temporary text buffer and load it with the strings
	auto wrapBuf = new TextBuffer;
	wrapBuf->BufInsertEx(0, startLine);
	wrapBuf->BufAppendEx(text);

	/* Scan the buffer for long lines and apply wrapLine when wrapMargin is
	   exceeded.  limitPos enforces no breaks in the "startLine" part of the
	   string (if requested), and prevents re-scanning of long unbreakable
	   lines for each character beyond the margin */
	colNum = 0;
	pos = 0;
	lineStartPos = 0;
	limitPos = breakBefore == nullptr ? startLineLen : 0;
	while (pos < wrapBuf->BufGetLength()) {
		char c = wrapBuf->BufGetCharacter(pos);
		if (c == '\n') {
			lineStartPos = limitPos = pos + 1;
			colNum = 0;
		} else {
			colNum += TextBuffer::BufCharWidth(c, colNum, tabDist, buf->nullSubsChar_);
			if (colNum > wrapMargin) {
				if (!wrapLine(wrapBuf, bufOffset, lineStartPos, pos, limitPos, &breakAt, &charsAdded)) {
					limitPos = std::max<int>(pos, limitPos);
				} else {
					lineStartPos = limitPos = breakAt + 1;
					pos += charsAdded;
					colNum = wrapBuf->BufCountDispChars(lineStartPos, pos + 1);
					if (firstBreak == -1)
						firstBreak = breakAt;
				}
			}
		}
		pos++;
	}

	// Return the wrapped text, possibly including part of startLine
	if(!breakBefore) {
		wrappedText = wrapBuf->BufGetRangeEx(startLineLen, wrapBuf->BufGetLength());
	} else {
		*breakBefore = firstBreak != -1 && firstBreak < startLineLen ? startLineLen - firstBreak : 0;
		wrappedText = wrapBuf->BufGetRangeEx(startLineLen - *breakBefore, wrapBuf->BufGetLength());
	}
	delete wrapBuf;
	return wrappedText;
}


/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursorEx, but without the complicated auto-wrap
** scanning and re-formatting.
*/
void TextDisplay::simpleInsertAtCursorEx(view::string_view chars, XEvent *event, bool allowPendingDelete) {

	TextBuffer *buf = this->buffer;

	if (allowPendingDelete && pendingSelection()) {
		buf->BufReplaceSelectedEx(chars);
		TextDSetInsertPosition(buf->cursorPosHint_);
	} else if (text_of(w).P_overstrike) {

		size_t index = chars.find('\n');
		if(index != view::string_view::npos) {
			TextDInsertEx(chars);
		} else {
			TextDOverstrikeEx(chars);
		}
	} else {
		TextDInsertEx(chars);
	}

	this->checkAutoShowInsertPos();
	callCursorMovementCBs(event);
}


/*
** Return true if pending delete is on and there's a selection contiguous
** with the cursor ready to be deleted.  These criteria are used to decide
** if typing a character or inserting something should delete the selection
** first.
*/
int TextDisplay::pendingSelection() {
	TextSelection *sel = &this->buffer->primary_;
	int pos = TextDGetInsertPosition();

	return text_of(w).P_pendingDelete && sel->selected && pos >= sel->start && pos <= sel->end;
}

/*
** Wraps the end of a line beginning at lineStartPos and ending at lineEndPos
** in "buf", at the last white-space on the line >= limitPos.  (The implicit
** assumption is that just the last character of the line exceeds the wrap
** margin, and anywhere on the line we can wrap is correct).  Returns False if
** unable to wrap the line.  "breakAt", returns the character position at
** which the line was broken,
**
** Auto-wrapping can also trigger auto-indent.  The additional parameter
** bufOffset is needed when auto-indent is set to smart indent and the smart
** indent routines need to scan far back in the buffer.  "charsAdded" returns
** the number of characters added to acheive the auto-indent.  wrapMargin is
** used to decide whether auto-indent should be skipped because the indent
** string itself would exceed the wrap margin.
*/
int TextDisplay::wrapLine(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int limitPos, int *breakAt, int *charsAdded) {

	auto tw = reinterpret_cast<TextWidget>(w);

	int p;
	int column;
	
	/* Scan backward for whitespace or BOL.  If BOL, return false, no
	   whitespace in line at which to wrap */
	for (p = lineEndPos;; p--) {
		if (p < lineStartPos || p < limitPos) {
			return false;
		}

		char c = buf->BufGetCharacter(p);
		if (c == '\t' || c == ' ') {
			break;
		}
	}

	/* Create an auto-indent string to insert to do wrap.  If the auto
	   indent string reaches the wrap position, slice the auto-indent
	   back off and return to the left margin */
	std::string indentStr;
	if (text_of(tw).P_autoIndent || text_of(tw).P_smartIndent) {
		indentStr = createIndentStringEx(buf, bufOffset, lineStartPos, lineEndPos, &column);
		if (column >= p - lineStartPos) {
			indentStr.resize(1);
		}
	} else {
		indentStr = "\n";
	}

	/* Replace the whitespace character with the auto-indent string
	   and return the stats */
	buf->BufReplaceEx(p, p + 1, indentStr);

	*breakAt = p;
	*charsAdded = indentStr.size() - 1;
	return true;
}


/*
** Create and return an auto-indent string to add a newline at lineEndPos to a
** line starting at lineStartPos in buf.  "buf" may or may not be the real
** text buffer for the widget.  If it is not the widget's text buffer it's
** offset position from the real buffer must be specified in "bufOffset" to
** allow the smart-indent routines to scan back as far as necessary. The
** string length is returned in "length" (or "length" can be passed as nullptr,
** and the indent column is returned in "column" (if non nullptr).
*/
std::string TextDisplay::createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *column) {


	auto tw = reinterpret_cast<TextWidget>(w);
	int indent = -1;
	int tabDist = this->buffer->tabDist_;
	int i;
	int useTabs = this->buffer->useTabs_;
	smartIndentCBStruct smartIndent;

	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (text_of(tw).P_smartIndent && (lineStartPos == 0 || buf == this->buffer)) {
		smartIndent.reason        = NEWLINE_INDENT_NEEDED;
		smartIndent.pos           = lineEndPos + bufOffset;
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped    = nullptr;
		XtCallCallbacks((Widget)tw, textNsmartIndentCallback, &smartIndent);
		indent = smartIndent.indentRequest;
	}

	// If smart indent wasn't used, measure the indent distance of the line
	if (indent == -1) {
		indent = 0;
		for (int pos = lineStartPos; pos < lineEndPos; pos++) {
			char c = buf->BufGetCharacter(pos);
			if (c != ' ' && c != '\t') {
				break;
			}
				
			if (c == '\t') {
				indent += tabDist - (indent % tabDist);
			} else {
				indent++;
			}
		}
	}

	// Allocate and create a string of tabs and spaces to achieve the indent
	std::string indentStr;
	indentStr.reserve(indent + 2);

	auto indentPtr = std::back_inserter(indentStr);

	*indentPtr++ = '\n';
	if (useTabs) {
		for (i = 0; i < indent / tabDist; i++)
			*indentPtr++ = '\t';
		for (i = 0; i < indent % tabDist; i++)
			*indentPtr++ = ' ';
	} else {
		for (i = 0; i < indent; i++)
			*indentPtr++ = ' ';
	}

	// Return any requested stats
	if(column) {
		*column = indent;
	}

	return indentStr;
}


/*
**  Sets the caret to on or off and restart the caret blink timer.
**  This could be used by other modules to modify the caret's blinking.
*/
void TextDisplay::ResetCursorBlink(bool startsBlanked) {

	auto tw = reinterpret_cast<TextWidget>(w);

	if (text_of(tw).P_cursorBlinkRate != 0) {
		if (this->cursorBlinkProcID != 0) {
			XtRemoveTimeOut(this->cursorBlinkProcID);
		}

		if (startsBlanked) {
			TextDBlankCursor();
		} else {
			TextDUnblankCursor();
		}

		this->cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)tw), text_of(tw).P_cursorBlinkRate, cursorBlinkTimerProc, tw);
	}
}

/*
** Xt timer procedure for cursor blinking
*/
void TextDisplay::cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	TextWidget w = static_cast<TextWidget>(clientData);
	TextDisplay *textD = textD_of(w);

	// Blink the cursor
	if (textD->cursorOn) {
		textD->TextDBlankCursor();
	} else {
		textD->TextDUnblankCursor();
	}

	// re-establish the timer proc (this routine) to continue processing
	textD->cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), text_of(w).P_cursorBlinkRate, cursorBlinkTimerProc, w);
}


void TextDisplay::ShowHidePointer(bool hidePointer) {

	auto tw = reinterpret_cast<TextWidget>(w);

	if (text_of(tw).P_hidePointer) {
		if (hidePointer != this->pointerHidden) {
			if (hidePointer) {
				// Don't listen for keypresses any more
				XtRemoveEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, false, handleHidePointer, nullptr);
				// Switch to empty cursor
				XDefineCursor(XtDisplay(w), XtWindow(w), empty_cursor);

				this->pointerHidden = true;

				// Listen to mouse movement, focus change, and button presses
				XtAddEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK, false, handleShowPointer, nullptr);
			} else {
				// Don't listen to mouse/focus events any more
				XtRemoveEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK, false, handleShowPointer, nullptr);
				// Switch to regular cursor
				XUndefineCursor(XtDisplay(w), XtWindow(w));

				this->pointerHidden = false;

				// Listen for keypresses now
				XtAddEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, false, handleHidePointer, nullptr);
			}
		}
	}
}

// Hide the pointer while the user is typing
void TextDisplay::handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	textD_of(w)->ShowHidePointer(true);
}


// Restore the pointer if the mouse moves or focus changes
void TextDisplay::handleShowPointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	textD_of(w)->ShowHidePointer(false);
}


void TextDisplay::TextPasteClipboard(Time time) {
	cancelDrag();
	if (checkReadOnly()) {
		return;
	}
	
	TakeMotifDestination(time);
	InsertClipboard(false);
	callCursorMovementCBs(nullptr);
}

void TextDisplay::TextColPasteClipboard(Time time) {
	cancelDrag();
	if (checkReadOnly()) {
		return;
	}
	
	TakeMotifDestination(time);
	InsertClipboard(true);
	callCursorMovementCBs(nullptr);
}


/*
** Set this widget to be the owner of selections made in it's attached
** buffer (text buffers may be shared among several text widgets).
*/
void TextDisplay::TextHandleXSelections() {
	HandleXSelections();
}


bool TextDisplay::checkReadOnly() const {
	if (text_of(w).P_readOnly) {
		QApplication::beep();
		return true;
	}
	return false;
}


/*
** Cancel any drag operation that might be in progress.  Should be included
** in nearly every key event to cleanly end any dragging before edits are made
** which might change the insert position or the content of the buffer during
** a drag operation)
*/
void TextDisplay::cancelDrag() {
	int dragState = this->dragState;

	if (this->autoScrollProcID != 0) {
		XtRemoveTimeOut(this->autoScrollProcID);
	}

	switch(dragState) {
	case SECONDARY_DRAG:
	case SECONDARY_RECT_DRAG:
		this->buffer->BufSecondaryUnselect();
		break;
	case PRIMARY_BLOCK_DRAG:
		CancelBlockDrag();
		break;
	case MOUSE_PAN:
		XUngrabPointer(XtDisplay(w), CurrentTime);
		break;
	case NOT_CLICKED:
		this->dragState = DRAG_CANCELED;
		break;
	}
}


void TextDisplay::checkAutoShowInsertPos() {

	if (text_of(w).P_autoShowInsertPos) {
		this->TextDMakeInsertPosVisible();
	}
}

/*
** Do operations triggered by cursor movement: Call cursor movement callback
** procedure(s), and cancel marker indicating that the cursor is after one or
** more just-entered emulated tabs (spaces to be deleted as a unit).
*/
void TextDisplay::callCursorMovementCBs(XEvent *event) {
	this->emTabsBeforeCursor = 0;
	XtCallCallbacks(this->w, textNcursorMovementCallback, (XtPointer)event);
}

void TextDisplay::HandleAllPendingGraphicsExposeNoExposeEvents(XEvent *event) {
	XEvent foundEvent;
	bool invalidRect = true;
	Rect rect;

	if (event) {
		adjustRectForGraphicsExposeOrNoExposeEvent(event, &invalidRect, &rect);
	}
	
	while (XCheckIfEvent(XtDisplay(w), &foundEvent, findGraphicsExposeOrNoExposeEvent, (XPointer)w)) {
		adjustRectForGraphicsExposeOrNoExposeEvent(&foundEvent, &invalidRect, &rect);
	}
	
	if (!invalidRect) {
		TextDRedisplayRect(rect);
	}
}

void TextDisplay::adjustRectForGraphicsExposeOrNoExposeEvent(XEvent *event, bool *first, Rect *rect) {
	adjustRectForGraphicsExposeOrNoExposeEvent(event, first, &rect->left, &rect->top, &rect->width, &rect->height);
}

void TextDisplay::adjustRectForGraphicsExposeOrNoExposeEvent(XEvent *event, bool *first, int *left, int *top, int *width, int *height) {
	bool removeQueueEntry = false;

	if (event->type == GraphicsExpose) {
		XGraphicsExposeEvent *e = &event->xgraphicsexpose;
		int x = e->x;
		int y = e->y;

		TextDImposeGraphicsExposeTranslation(&x, &y);
		if (*first) {
			*left   = x;
			*top    = y;
			*width  = e->width;
			*height = e->height;

			*first = false;
		} else {
			int prev_left = *left;
			int prev_top = *top;

			*left   = std::min(*left, x);
			*top    = std::min(*top, y);
			*width  = std::max(prev_left + *width, x + e->width) - *left;
			*height = std::max(prev_top + *height, y + e->height) - *top;
		}
		if (e->count == 0) {
			removeQueueEntry = true;
		}
	} else if (event->type == NoExpose) {
		removeQueueEntry = true;
	}

	if (removeQueueEntry) {
		TextDPopGraphicExposeQueueEntry();
	}
}


Bool TextDisplay::findGraphicsExposeOrNoExposeEvent(Display *theDisplay, XEvent *event, XPointer arg) {
	if ((theDisplay == event->xany.display) && (event->type == GraphicsExpose || event->type == NoExpose) && ((Widget)arg == XtWindowToWidget(event->xany.display, event->xany.window))) {
		return true;
	} else {
		return false;
	}
}


void TextDisplay::TextDKillCalltip(int calltipID) {
	if (this->calltip.ID == 0)
		return;
	if (calltipID == 0 || calltipID == this->calltip.ID) {
		XtPopdown(this->calltipShell);
		this->calltip.ID = 0;
	}
}


/*
** Update the position of the current calltip if one exists, else do nothing
*/
void TextDisplay::TextDRedrawCalltip(int calltipID) {
	int lineHeight = this->ascent + this->descent;
	Position txtX, txtY, borderWidth, abs_x, abs_y, tipWidth, tipHeight;
	XWindowAttributes screenAttr;
	int rel_x, rel_y, flip_delta;

	if (this->calltip.ID == 0)
		return;
	if (calltipID != 0 && calltipID != this->calltip.ID)
		return;

	// Get the location/dimensions of the text area 
	XtVaGetValues(this->w, XmNx, &txtX, XmNy, &txtY, nullptr);

	if (this->calltip.anchored) {
		// Put it at the anchor position 
		if (!this->TextDPositionToXY(this->calltip.pos, &rel_x, &rel_y)) {
			if (this->calltip.alignMode == TIP_STRICT)
				this->TextDKillCalltip(this->calltip.ID);
			return;
		}
	} else {
		if (this->calltip.pos < 0) {
			/* First display of tip with cursor offscreen (detected in
			    ShowCalltip) */
			this->calltip.pos = this->rect.width / 2;
			this->calltip.hAlign = TIP_CENTER;
			rel_y = this->rect.height / 3;
		} else if (!this->TextDPositionToXY(this->cursorPos, &rel_x, &rel_y)) {
			// Window has scrolled and tip is now offscreen 
			if (this->calltip.alignMode == TIP_STRICT)
				this->TextDKillCalltip(this->calltip.ID);
			return;
		}
		rel_x = this->calltip.pos;
	}

	XtVaGetValues(this->calltipShell, XmNwidth, &tipWidth, XmNheight, &tipHeight, XmNborderWidth, &borderWidth, nullptr);
	rel_x += borderWidth;
	rel_y += lineHeight / 2 + borderWidth;

	// Adjust rel_x for horizontal alignment modes 
	if (this->calltip.hAlign == TIP_CENTER)
		rel_x -= tipWidth / 2;
	else if (this->calltip.hAlign == TIP_RIGHT)
		rel_x -= tipWidth;

	// Adjust rel_y for vertical alignment modes 
	if (this->calltip.vAlign == TIP_ABOVE) {
		flip_delta = tipHeight + lineHeight + 2 * borderWidth;
		rel_y -= flip_delta;
	} else
		flip_delta = -(tipHeight + lineHeight + 2 * borderWidth);

	XtTranslateCoords(this->w, rel_x, rel_y, &abs_x, &abs_y);

	// If we're not in strict mode try to keep the tip on-screen 
	if (this->calltip.alignMode == TIP_SLOPPY) {
		XGetWindowAttributes(XtDisplay(this->w), RootWindowOfScreen(XtScreen(this->w)), &screenAttr);

		// make sure tip doesn't run off right or left side of screen 
		if (abs_x + tipWidth >= screenAttr.width - CALLTIP_EDGE_GUARD)
			abs_x = screenAttr.width - tipWidth - CALLTIP_EDGE_GUARD;
		if (abs_x < CALLTIP_EDGE_GUARD)
			abs_x = CALLTIP_EDGE_GUARD;

		// Try to keep the tip onscreen vertically if possible 
		if (screenAttr.height > tipHeight && offscreenV(&screenAttr, abs_y, tipHeight)) {
			// Maybe flipping from below to above (or vice-versa) will help 
			if (!offscreenV(&screenAttr, abs_y + flip_delta, tipHeight))
				abs_y += flip_delta;
			// Make sure the tip doesn't end up *totally* offscreen 
			else if (abs_y + tipHeight < 0)
				abs_y = CALLTIP_EDGE_GUARD;
			else if (abs_y >= screenAttr.height)
				abs_y = screenAttr.height - tipHeight - CALLTIP_EDGE_GUARD;
			// If no case applied, just go with the default placement. 
		}
	}

	XtVaSetValues(this->calltipShell, XmNx, abs_x, XmNy, abs_y, nullptr);
}

bool TextDisplay::offscreenV(XWindowAttributes *screenAttr, int top, int height) {
	return (top < CALLTIP_EDGE_GUARD || top + height >= screenAttr->height - CALLTIP_EDGE_GUARD);
}


/*
** Start the process of dragging the current primary-selected text across
** the window (move by dragging, as opposed to dragging to create the
** selection)
*/
void TextDisplay::BeginBlockDrag() {

	TextBuffer *buf    = this->buffer;
	int fontHeight     = this->fontStruct->ascent + this->fontStruct->descent;
	int fontWidth      = this->fontStruct->max_bounds.width;
	TextSelection *sel = &buf->primary_;
	int nLines;
	int mousePos;
	int lineStart;
	int x;
	int y;
	int lineEnd;

	/* Save a copy of the whole text buffer as a backup, and for
	   deriving changes */
	this->dragOrigBuf = new TextBuffer;
	this->dragOrigBuf->BufSetTabDistance(buf->tabDist_);
	this->dragOrigBuf->useTabs_ = buf->useTabs_;

	std::string text = buf->BufGetAllEx();
	this->dragOrigBuf->BufSetAllEx(text);

	if (sel->rectangular)
		this->dragOrigBuf->BufRectSelect(sel->start, sel->end, sel->rectStart, sel->rectEnd);
	else
		this->dragOrigBuf->BufSelect(sel->start, sel->end);

	/* Record the mouse pointer offsets from the top left corner of the
	   selection (the position where text will actually be inserted In dragging
	   non-rectangular selections)  */
	if (sel->rectangular) {
		this->dragXOffset = this->btnDownCoord.x + this->horizOffset - this->rect.left - sel->rectStart * fontWidth;
	} else {
		if (!this->TextDPositionToXY(sel->start, &x, &y)) {
			x = buf->BufCountDispChars(this->TextDStartOfLine(sel->start), sel->start) * fontWidth + this->rect.left - this->horizOffset;
		}
		this->dragXOffset = this->btnDownCoord.x - x;
	}

	mousePos = this->TextDXYToPosition(btnDownCoord);
	nLines = buf->BufCountLines(sel->start, mousePos);
	
	this->dragYOffset = nLines * fontHeight + (((this->btnDownCoord.y - text_of(w).P_marginHeight) % fontHeight) - fontHeight / 2);
	this->dragNLines  = buf->BufCountLines(sel->start, sel->end);

	/* Record the current drag insert position and the information for
	   undoing the fictional insert of the selection in its new position */
	this->dragInsertPos = sel->start;
	this->dragInserted = sel->end - sel->start;
	if (sel->rectangular) {
		auto testBuf = mem::make_unique<TextBuffer>();

		std::string testText = buf->BufGetRangeEx(sel->start, sel->end);
		testBuf->BufSetTabDistance(buf->tabDist_);
		testBuf->useTabs_ = buf->useTabs_;
		testBuf->BufSetAllEx(testText);

		testBuf->BufRemoveRect(0, sel->end - sel->start, sel->rectStart, sel->rectEnd);
		this->dragDeleted = testBuf->BufGetLength();
		this->dragRectStart = sel->rectStart;
	} else {
		this->dragDeleted = 0;
		this->dragRectStart = 0;
	}

	this->dragType            = DRAG_MOVE;
	this->dragSourceDeletePos = sel->start;
	this->dragSourceInserted  = this->dragDeleted;
	this->dragSourceDeleted   = this->dragInserted;

	/* For non-rectangular selections, fill in the rectangular information in
	   the selection for overlay mode drags which are done rectangularly */
	if (!sel->rectangular) {
		lineStart = buf->BufStartOfLine(sel->start);
		if (this->dragNLines == 0) {
			this->dragOrigBuf->primary_.rectStart = buf->BufCountDispChars(lineStart, sel->start);
			this->dragOrigBuf->primary_.rectEnd   = buf->BufCountDispChars(lineStart, sel->end);
		} else {
			lineEnd = buf->BufGetCharacter(sel->end - 1) == '\n' ? sel->end - 1 : sel->end;
			findTextMargins(buf, lineStart, lineEnd, &this->dragOrigBuf->primary_.rectStart, &this->dragOrigBuf->primary_.rectEnd);
		}
	}

	// Set the drag state to announce an ongoing block-drag 
	this->dragState = PRIMARY_BLOCK_DRAG;

	// Call the callback announcing the start of a block drag 
	XtCallCallbacks(w, textNdragStartCallback, nullptr);
}


/*
** Reposition the primary-selected text that is being dragged as a block
** for a new mouse position of (x, y)
*/
void TextDisplay::BlockDragSelection(Point pos, int dragType) {
	
	TextBuffer *buf        = this->buffer;
	int fontHeight         = this->fontStruct->ascent + this->fontStruct->descent;
	int fontWidth          = this->fontStruct->max_bounds.width;
	TextBuffer *origBuf    = this->dragOrigBuf;
	int dragXOffset        = this->dragXOffset;
	TextSelection *origSel = &origBuf->primary_;
	int rectangular        = origSel->rectangular;
	int oldDragType        = this->dragType;
	int nLines             = this->dragNLines;
	int insLineNum;
	int insLineStart;
	int insRectStart;
	int insRectEnd;
	int insStart;
	int modRangeStart   = -1;
	int tempModRangeEnd = -1;
	int bufModRangeEnd  = -1;
	int referenceLine;
	int referencePos;
	int tempStart;
	int tempEnd;
	int insertInserted;
	int insertDeleted;
	int row;
	int column;
	int origSelLineEnd;
	int sourceInserted;
	int sourceDeleted;
	int sourceDeletePos;

	if (this->dragState != PRIMARY_BLOCK_DRAG) {
		return;
	}

	/* The operation of block dragging is simple in theory, but not so simple
	   in practice.  There is a backup buffer (this->dragOrigBuf) which
	   holds a copy of the buffer as it existed before the drag.  When the
	   user drags the mouse to a new location, this routine is called, and
	   a temporary buffer is created and loaded with the local part of the
	   buffer (from the backup) which might be changed by the drag.  The
	   changes are all made to this temporary buffer, and the parts of this
	   buffer which then differ from the real (displayed) buffer are used to
	   replace those parts, thus one replace operation serves as both undo
	   and modify.  This double-buffering of the operation prevents excessive
	   redrawing (though there is still plenty of needless redrawing due to
	   re-selection and rectangular operations).

	   The hard part is keeping track of the changes such that a single replace
	   operation will do everyting.  This is done using a routine called
	   trackModifyRange which tracks expanding ranges of changes in the two
	   buffers in modRangeStart, tempModRangeEnd, and bufModRangeEnd. */

	/* Create a temporary buffer for accumulating changes which will
	   eventually be replaced in the real buffer.  Load the buffer with the
	   range of characters which might be modified in this drag step
	   (this could be tighter, but hopefully it's not too slow) */
	auto tempBuf = new TextBuffer;
	tempBuf->tabDist_ = buf->tabDist_;
	tempBuf->useTabs_ = buf->useTabs_;
	tempStart         = min3(this->dragInsertPos, origSel->start, buf->BufCountBackwardNLines(this->firstChar, nLines + 2));
	tempEnd           = buf->BufCountForwardNLines(max3(this->dragInsertPos, origSel->start, this->lastChar), nLines + 2) + origSel->end - origSel->start;
	
	std::string text  = origBuf->BufGetRangeEx(tempStart, tempEnd);
	tempBuf->BufSetAllEx(text);

	// If the drag type is USE_LAST, use the last dragType applied 
	if (dragType == USE_LAST) {
		dragType = this->dragType;
	}
	
	bool overlay = (dragType == DRAG_OVERLAY_MOVE) || (dragType == DRAG_OVERLAY_COPY);

	/* Overlay mode uses rectangular selections whether or not the original
	   was rectangular.  To use a plain selection as if it were rectangular,
	   the start and end positions need to be moved to the line boundaries
	   and trailing newlines must be excluded */
	int origSelLineStart = origBuf->BufStartOfLine(origSel->start);

	if (!rectangular && origBuf->BufGetCharacter(origSel->end - 1) == '\n') {
		origSelLineEnd = origSel->end - 1;
	} else {
		origSelLineEnd = origBuf->BufEndOfLine(origSel->end);
	}
	
	if (!rectangular && overlay && nLines != 0) {
		dragXOffset -= fontWidth * (origSel->rectStart - (origSel->start - origSelLineStart));
	}

	/* If the drag operation is of a different type than the last one, and the
	   operation is a move, expand the modified-range to include undoing the
	   text-removal at the site from which the text was dragged. */
	if (dragType != oldDragType && this->dragSourceDeleted != 0) {
		trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, this->dragSourceDeletePos, this->dragSourceInserted, this->dragSourceDeleted);
	}

	/* Do, or re-do the original text removal at the site where a move began.
	   If this part has not changed from the last call, do it silently to
	   bring the temporary buffer in sync with the real (displayed)
	   buffer.  If it's being re-done, track the changes to complete the
	   redo operation begun above */
	if (dragType == DRAG_MOVE || dragType == DRAG_OVERLAY_MOVE) {
		if (rectangular || overlay) {
			int prevLen = tempBuf->BufGetLength();
			int origSelLen = origSelLineEnd - origSelLineStart;
			
			if (overlay) {
				tempBuf->BufClearRect(origSelLineStart - tempStart, origSelLineEnd - tempStart, origSel->rectStart, origSel->rectEnd);
			} else {
				tempBuf->BufRemoveRect(origSelLineStart - tempStart, origSelLineEnd - tempStart, origSel->rectStart, origSel->rectEnd);
			}
			
			sourceDeletePos = origSelLineStart;
			sourceInserted = origSelLen - prevLen + tempBuf->BufGetLength();
			sourceDeleted = origSelLen;
		} else {
			tempBuf->BufRemove(origSel->start - tempStart, origSel->end - tempStart);
			sourceDeletePos = origSel->start;
			sourceInserted  = 0;
			sourceDeleted   = origSel->end - origSel->start;
		}
		
		if (dragType != oldDragType) {
			trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, sourceDeletePos, sourceInserted, sourceDeleted);
		}
		
	} else {
		sourceDeletePos = 0;
		sourceInserted  = 0;
		sourceDeleted   = 0;
	}

	/* Expand the modified-range to include undoing the insert from the last
	   call. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, this->dragInsertPos, this->dragInserted, this->dragDeleted);

	/* Find the line number and column of the insert position.  Note that in
	   continuous wrap mode, these must be calculated as if the text were
	   not wrapped */
	this->TextDXYToUnconstrainedPosition(
		Point{
			std::max(0, pos.x - dragXOffset), 
			std::max(0, pos.y - (this->dragYOffset % fontHeight))
		},
		&row, &column);
		
		
	column = this->TextDOffsetWrappedColumn(row, column);
	row    = this->TextDOffsetWrappedRow(row);
	insLineNum = row + this->topLineNum - this->dragYOffset / fontHeight;

	/* find a common point of reference between the two buffers, from which
	   the insert position line number can be translated to a position */
	if (this->firstChar > modRangeStart) {
		referenceLine = this->topLineNum - buf->BufCountLines(modRangeStart, this->firstChar);
		referencePos = modRangeStart;
	} else {
		referencePos = this->firstChar;
		referenceLine = this->topLineNum;
	}

	/* find the position associated with the start of the new line in the
	   temporary buffer */
	insLineStart = findRelativeLineStart(tempBuf, referencePos - tempStart, referenceLine, insLineNum) + tempStart;
	if (insLineStart - tempStart == tempBuf->BufGetLength())
		insLineStart = tempBuf->BufStartOfLine(insLineStart - tempStart) + tempStart;

	// Find the actual insert position 
	if (rectangular || overlay) {
		insStart = insLineStart;
		insRectStart = column;
	} else { // note, this will fail with proportional fonts 
		insStart = tempBuf->BufCountForwardDispChars(insLineStart - tempStart, column) + tempStart;
		insRectStart = 0;
	}

	/* If the position is the same as last time, don't bother drawing (it
	   would be nice if this decision could be made earlier) */
	if (insStart == this->dragInsertPos && insRectStart == this->dragRectStart && dragType == oldDragType) {
		delete tempBuf;
		return;
	}

	// Do the insert in the temporary buffer 
	if (rectangular || overlay) {

		std::string insText = origBuf->BufGetTextInRectEx(origSelLineStart, origSelLineEnd, origSel->rectStart, origSel->rectEnd);
		if (overlay) {
			tempBuf->BufOverlayRectEx(insStart - tempStart, insRectStart, insRectStart + origSel->rectEnd - origSel->rectStart, insText, &insertInserted, &insertDeleted);
		} else {
			tempBuf->BufInsertColEx(insRectStart, insStart - tempStart, insText, &insertInserted, &insertDeleted);
		}
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, insertInserted, insertDeleted);

	} else {
		std::string insText = origBuf->BufGetSelectionTextEx();
		tempBuf->BufInsertEx(insStart - tempStart, insText);
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, origSel->end - origSel->start, 0);
		insertInserted = origSel->end - origSel->start;
		insertDeleted = 0;
	}

	// Make the changes in the real buffer 
	std::string repText = tempBuf->BufGetRangeEx(modRangeStart - tempStart, tempModRangeEnd - tempStart);
	delete tempBuf;
	this->TextDBlankCursor();
	buf->BufReplaceEx(modRangeStart, bufModRangeEnd, repText);

	// Store the necessary information for undoing this step 
	this->dragInsertPos       = insStart;
	this->dragRectStart       = insRectStart;
	this->dragInserted        = insertInserted;
	this->dragDeleted         = insertDeleted;
	this->dragSourceDeletePos = sourceDeletePos;
	this->dragSourceInserted  = sourceInserted;
	this->dragSourceDeleted   = sourceDeleted;
	this->dragType            = dragType;

	// Reset the selection and cursor position 
	if (rectangular || overlay) {
		insRectEnd = insRectStart + origSel->rectEnd - origSel->rectStart;
		buf->BufRectSelect(insStart, insStart + insertInserted, insRectStart, insRectEnd);
		this->TextDSetInsertPosition(buf->BufCountForwardDispChars(buf->BufCountForwardNLines(insStart, this->dragNLines), insRectEnd));
	} else {
		buf->BufSelect(insStart, insStart + origSel->end - origSel->start);
		this->TextDSetInsertPosition(insStart + origSel->end - origSel->start);
	}
	
	this->TextDUnblankCursor();
	XtCallCallbacks(w, textNcursorMovementCallback, nullptr);
	this->emTabsBeforeCursor = 0;
}

/*
** Complete a block text drag operation
*/
void TextDisplay::FinishBlockDrag() {
	
	int modRangeStart = -1;
	int origModRangeEnd;
	int bufModRangeEnd;

	/* Find the changed region of the buffer, covering both the deletion
	   of the selected text at the drag start position, and insertion at
	   the drag destination */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, this->dragSourceDeletePos, this->dragSourceInserted, this->dragSourceDeleted);
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, this->dragInsertPos, this->dragInserted, this->dragDeleted);

	// Get the original (pre-modified) range of text from saved backup buffer 
	std::string deletedText = this->dragOrigBuf->BufGetRangeEx(modRangeStart, origModRangeEnd);

	// Free the backup buffer 
	delete this->dragOrigBuf;

	// Return to normal drag state 
	this->dragState = NOT_CLICKED;

	// Call finish-drag calback 
	dragEndCBStruct endStruct;
	endStruct.startPos       = modRangeStart;
	endStruct.nCharsDeleted  = origModRangeEnd - modRangeStart;
	endStruct.nCharsInserted = bufModRangeEnd  - modRangeStart;
	endStruct.deletedText    = deletedText;

	XtCallCallbacks(w, textNdragEndCallback, &endStruct);
}

/*
** Cancel a block drag operation
*/
void TextDisplay::CancelBlockDrag() {
	TextBuffer *buf = this->buffer;
	TextBuffer *origBuf = this->dragOrigBuf;
	TextSelection *origSel = &origBuf->primary_;
	int modRangeStart = -1, origModRangeEnd, bufModRangeEnd;

	/* If the operation was a move, make the modify range reflect the
	   removal of the text from the starting position */
	if (this->dragSourceDeleted != 0)
		trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, this->dragSourceDeletePos, this->dragSourceInserted, this->dragSourceDeleted);

	/* Include the insert being undone from the last step in the modified
	   range. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, this->dragInsertPos, this->dragInserted, this->dragDeleted);

	// Make the changes in the buffer 
	std::string repText = origBuf->BufGetRangeEx(modRangeStart, origModRangeEnd);
	buf->BufReplaceEx(modRangeStart, bufModRangeEnd, repText);

	// Reset the selection and cursor position 
	if (origSel->rectangular)
		buf->BufRectSelect(origSel->start, origSel->end, origSel->rectStart, origSel->rectEnd);
	else
		buf->BufSelect(origSel->start, origSel->end);
	this->TextDSetInsertPosition(buf->cursorPosHint_);
	XtCallCallbacks(w, textNcursorMovementCallback, nullptr);
	this->emTabsBeforeCursor = 0;

	// Free the backup buffer 
	delete origBuf;

	// Indicate end of drag 
	this->dragState = DRAG_CANCELED;

	// Call finish-drag calback 
	dragEndCBStruct endStruct;	
	endStruct.startPos       = 0;
	endStruct.nCharsDeleted  = 0;
	endStruct.nCharsInserted = 0;
	XtCallCallbacks(w, textNdragEndCallback, &endStruct);
}


/*
** Insert the X CLIPBOARD selection at the cursor position.  If isColumnar,
** do an BufInsertColEx for a columnar paste instead of BufInsertEx.
*/
void TextDisplay::InsertClipboard(bool isColumnar) {
	unsigned long length, retLength;

	auto buf   = this->buffer;
	int cursorLineStart, column, cursorPos;
	long id = 0;

	/* Get the clipboard contents.  Note: this code originally used the
	   CLIPBOARD selection, rather than the Motif clipboard interface.  It
	   was changed because Motif widgets in the same application would hang
	   when users pasted data from nedit text widgets.  This happened because
	   the XmClipboard routines used by the widgets do blocking event reads,
	   preventing a response by a selection owner in the same application.
	   While the Motif clipboard routines as they are used below, limit the
	   size of the data that be transferred via the clipboard, and are
	   generally slower and buggier, they do preserve the clipboard across
	   widget destruction and even program termination. */
	if (SpinClipboardInquireLength(XtDisplay(w), XtWindow(w), (String) "STRING", &length) != ClipboardSuccess || length == 0) {
		/*
		 * Possibly, the clipboard can remain in a locked state after
		 * a failure, so we try to remove the lock, just to be sure.
		 */
		SpinClipboardUnlock(XtDisplay(w), XtWindow(w));
		return;
	}
	auto string = new char[length + 1];
	if (SpinClipboardRetrieve(XtDisplay(w), XtWindow(w), (String) "STRING", string, length, &retLength, &id) != ClipboardSuccess || retLength == 0) {
		XtFree(string);
		/*
		 * Possibly, the clipboard can remain in a locked state after
		 * a failure, so we try to remove the lock, just to be sure.
		 */
		SpinClipboardUnlock(XtDisplay(w), XtWindow(w));
		return;
	}
	string[retLength] = '\0';
	
	std::string contents(string, retLength);
	delete [] string;

	/* If the string contains ascii-nul characters, substitute something
	   else, or give up, warn, and refuse */
	if (!buf->BufSubstituteNullCharsEx(contents)) {
		fprintf(stderr, "Too much binary data, text not pasted\n");
		return;
	}

	// Insert it in the text widget 
	if (isColumnar && !buf->primary_.selected) {
		cursorPos = this->TextDGetInsertPosition();
		cursorLineStart = buf->BufStartOfLine(cursorPos);
		column = buf->BufCountDispChars(cursorLineStart, cursorPos);
		if (text_of(w).P_overstrike) {
			buf->BufOverlayRectEx(cursorLineStart, column, -1, contents, nullptr, nullptr);
		} else {
			buf->BufInsertColEx(column, cursorLineStart, contents, nullptr, nullptr);
		}
		this->TextDSetInsertPosition(buf->BufCountForwardDispChars(cursorLineStart, column));
		if (text_of(w).P_autoShowInsertPos)
			this->TextDMakeInsertPosVisible();
	} else
		this->TextInsertAtCursorEx(contents, nullptr, true, text_of(w).P_autoWrapPastedText);
}


/*
** Copy the primary selection to the clipboard
*/
void TextDisplay::CopyToClipboard(Time time) {
	long itemID = 0;

	// Get the selected text, if there's no selection, do nothing 
	std::string text = this->buffer->BufGetSelectionTextEx();
	if (text.empty()) {
		return;
	}

	/* If the string contained ascii-nul characters, something else was
	   substituted in the buffer.  Put the nulls back */
	int length = text.size();
	this->buffer->BufUnsubstituteNullCharsEx(text);

	// Shut up LessTif 
	if (SpinClipboardLock(XtDisplay(w), XtWindow(w)) != ClipboardSuccess) {
		return;
	}

	/* Use the XmClipboard routines to copy the text to the clipboard.
	   If errors occur, just give up.  */
	XmString s = XmStringCreateSimpleEx("NEdit");
	int stat = SpinClipboardStartCopy(XtDisplay(w), XtWindow(w), s, time, w, nullptr, &itemID);
	XmStringFree(s);
	
	if (stat != ClipboardSuccess) {
		SpinClipboardUnlock(XtDisplay(w), XtWindow(w));
		return;
	}

	/* Note that we were previously passing length + 1 here, but I suspect
	   that this was inconsistent with the somewhat ambiguous policy of
	   including a terminating null but not mentioning it in the length */

	if (SpinClipboardCopy(XtDisplay(w), XtWindow(w), itemID, (String) "STRING", (char *)text.data(), length, 0, nullptr) != ClipboardSuccess) {
		SpinClipboardEndCopy(XtDisplay(w), XtWindow(w), itemID);
		SpinClipboardUnlock(XtDisplay(w), XtWindow(w));
		return;
	}

	SpinClipboardEndCopy(XtDisplay(w), XtWindow(w), itemID);
	SpinClipboardUnlock(XtDisplay(w), XtWindow(w));
}

/*
** Designate text widget "w" to be the selection owner for primary selections
** in its attached buffer (a buffer can be attached to multiple text widgets).
*/
void TextDisplay::HandleXSelections() {
	auto buf = this->buffer;

	// Remove any existing selection handlers for other widgets 
	for (const auto &pair : buf->modifyProcs_) {
		if (pair.first == modifiedCB) {
			buf->BufRemoveModifyCB(pair.first, pair.second);
			break;
		}
	}

	// Add a handler with this widget as the CB arg (and thus the sel. owner) 
	this->buffer->BufAddModifyCB(modifiedCB, w);
}

/*
** Discontinue ownership of selections for widget "w"'s attached buffer
** (if "w" was the designated selection owner)
*/
void TextDisplay::StopHandlingXSelections() {
	auto buf = this->buffer;


	for (auto it = buf->modifyProcs_.begin(); it != buf->modifyProcs_.end(); ++it) {
		auto &pair = *it;
		if (pair.first == modifiedCB && pair.second == w) {
			buf->modifyProcs_.erase(it);
			return;
		}
	}
}

/*
** Insert the X PRIMARY selection (from whatever window currently owns it)
** at the cursor position.
*/
void TextDisplay::InsertPrimarySelection(Time time, bool isColumnar) {
	static int isColFlag;

	/* Theoretically, strange things could happen if the user managed to get
	   in any events between requesting receiving the selection data, however,
	   getSelectionCB simply inserts the selection at the cursor.  Don't
	   bother with further measures until real problems are observed. */
	isColFlag = isColumnar;
	XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, getSelectionCB, &isColFlag, time);
}

/*
** Insert the contents of the PRIMARY selection at the cursor position in
** widget "w" and delete the contents of the selection in its current owner
** (if the selection owner supports DELETE targets).
*/
void TextDisplay::MovePrimarySelection( Time time, bool isColumnar) {
	static Atom targets[2] = {XA_STRING};
	static int isColFlag;
	static XtPointer clientData[2] = {&isColFlag, &isColFlag};

	targets[1] = getAtom(XtDisplay(w), A_DELETE);
	isColFlag = isColumnar;
	/* some strangeness here: the selection callback appears to be getting
	   clientData[1] for targets[0] */
	XtGetSelectionValues(w, XA_PRIMARY, targets, 2, getSelectionCB, clientData, time);
}

/*
** Exchange Primary and secondary selections (to be called by the widget
** with the secondary selection)
*/
void TextDisplay::ExchangeSelections(Time time) {
	if (!this->buffer->secondary_.selected)
		return;

	/* Initiate an long series of events: 
	** 1) get the primary selection,
	** 2) replace the primary selection with this widget's secondary, 
	** 3) replace this widget's secondary with the text returned from getting 
	**    the primary selection.  This could be done with a much more efficient 
	**    MULTIPLE request following ICCCM conventions, but the X toolkit 
	**    MULTIPLE handling routines can't handle INSERT_SELECTION requests 
	**    inside of MULTIPLE requests, because they don't allow access to the 
	**    requested property atom in  inside of an XtConvertSelectionProc.  
	**    It's simply not worth duplicating all of Xt's selection handling 
	**    routines for a little performance, and this would make the code 
	**    incompatible with Motif text widgets */
	XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, getExchSelCB, nullptr, time);
}

/*
** Insert the secondary selection at the motif destination by initiating
** an INSERT_SELECTION request to the current owner of the MOTIF_DESTINATION
** selection.  Upon completion, unselect the secondary selection.  If
** "removeAfter" is true, also delete the secondary selection from the
** widget's buffer upon completion.
*/
void TextDisplay::SendSecondarySelection(Time time, bool removeAfter) {
	textD_of(w)->sendSecondary(time, getAtom(XtDisplay(w), A_MOTIF_DESTINATION), removeAfter ? REMOVE_SECONDARY : UNSELECT_SECONDARY, nullptr, 0);
}


/*
** Take ownership of the MOTIF_DESTINATION selection.  This is Motif's private
** selection type for designating a widget to receive the result of
** secondary quick action requests.  The NEdit text widget uses this also
** for compatibility with Motif text widgets.
*/
void TextDisplay::TakeMotifDestination(Time time) {

	if (this->motifDestOwner || text_of(w).P_readOnly)
		return;

	// Take ownership of the MOTIF_DESTINATION selection 
	if (!XtOwnSelection(w, getAtom(XtDisplay(w), A_MOTIF_DESTINATION), time, convertMotifDestCB, loseMotifDestCB, nullptr)) {
		return;
	}
	
	this->motifDestOwner = true;
}

int TextDisplay::fontAscent() const {
	return this->ascent;
}

int TextDisplay::fontDescent() const {
	return this->descent;
}

Pixel TextDisplay::foregroundPixel() const {
	return this->fgPixel;
}

Pixel TextDisplay::backgroundPixel() const {
	return this->bgPixel;
}

TextBuffer *TextDisplay::textBuffer() const {
	return this->buffer;
}

int TextDisplay::getLineNumWidth() const {
	return this->lineNumWidth;
}

int TextDisplay::getLineNumLeft() const {
	return this->lineNumLeft;
}

Rect TextDisplay::getRect() const {
	return this->rect;
}

Point TextDisplay::getMouseCoord() const {
	return this->mouseCoord;
}

void TextDisplay::setMouseCoord(const Point &point) {
	this->mouseCoord = point;
}

int TextDisplay::getBufferLinesCount() const {
	return this->nBufferLines;
}

XtIntervalId TextDisplay::getCursorBlinkProcID() const {
	return this->cursorBlinkProcID;
}

TextBuffer *TextDisplay::getStyleBuffer() const {
	return this->styleBuffer;
}

void TextDisplay::setStyleBuffer(TextBuffer *buffer) {
	this->styleBuffer = buffer;
}

void TextDisplay::setModifyingTabDist(int tabDist) {
	this->modifyingTabDist = tabDist;
}

int TextDisplay::getModifyingTabDist() const {
	return this->modifyingTabDist;
}

CallTip &TextDisplay::getCalltip() {
	return this->calltip;
}

int TextDisplay::getFirstChar() const {
	return this->firstChar;
}

int TextDisplay::getLastChar() const {
	return this->lastChar;
}

int TextDisplay::getCursorPos() const {
	return this->cursorPos;
}

void TextDisplay::setCursorPos(int pos) {
	this->cursorPos = pos;
}

int TextDisplay::getAnchor() const {
	return this->anchor;
}

void TextDisplay::focusInAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	/* I don't entirely understand the traversal mechanism in Motif widgets,
	   particularly, what leads to this widget getting a focus-in event when
	   it does not actually have the input focus.  The temporary solution is
	   to do the comparison below, and not show the cursor when Motif says
	   we don't have focus, but keep looking for the real answer */

	if (this->w != XmGetFocusWidget(this->w)) {
		return;
	}

	// If the timer is not already started, start it
	if (text_of(this->w).P_cursorBlinkRate != 0 && this->getCursorBlinkProcID() == 0) {
		this->cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext(this->w), text_of(this->w).P_cursorBlinkRate, cursorBlinkTimerProc, this->w);
	}

	// Change the cursor to active style
	if (text_of(this->w).P_overstrike) {
		this->TextDSetCursorStyle(BLOCK_CURSOR);
	} else {
		this->TextDSetCursorStyle((text_of(this->w).P_heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR));
	}
	
	this->TextDUnblankCursor();

	// Notify Motif input manager that widget has focus
	XmImVaSetFocusValues(this->w, nullptr);

	// Call any registered focus-in callbacks
	XtCallCallbacks(this->w, textNfocusCallback, (XtPointer)event);
}

void TextDisplay::focusOutAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	// Remove the cursor blinking timer procedure
	if (this->cursorBlinkProcID != 0) {
		XtRemoveTimeOut(this->cursorBlinkProcID);
	}
	
	this->cursorBlinkProcID = 0;

	// Leave a dim or destination cursor
	this->TextDSetCursorStyle(this->motifDestOwner ? CARET_CURSOR : DIM_CURSOR);
	this->TextDUnblankCursor();

	// If there's a calltip displayed, kill it.
	this->TextDKillCalltip(0);

	// Call any registered focus-out callbacks
	XtCallCallbacks(this->w, textNlosingFocusCallback, (XtPointer)event);
}

void TextDisplay::deselectAllAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;
	(void)args;
	(void)nArgs;

	this->cancelDrag();
	this->textBuffer()->BufUnselect();
}

void TextDisplay::extendEndAP(XEvent *event, String *args, Cardinal *nArgs) {
	(void)args;
	(void)nArgs;
	(void)event;

	XButtonEvent *e = &event->xbutton;

	if (this->dragState == PRIMARY_CLICKED && this->lastBtnDown <= e->time + XtGetMultiClickTime(XtDisplay(this->w))) {
		this->multiClickState++;
	}
	
	endDrag();
}

void TextDisplay::backwardCharacterAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (!this->TextDMoveLeft()) {
		ringIfNecessary(silent);
	}
	
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::backwardWordAP(XEvent *event, String *args, Cardinal *nArgs) {

	TextBuffer *buf = this->buffer;
	int pos;
	int insertPos = this->TextDGetInsertPosition();
	const char *delimiters = text_of(this->w).P_delimiters;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}
	
	pos = std::max(insertPos - 1, 0);
	while (strchr(delimiters, buf->BufGetCharacter(pos)) != nullptr && pos > 0) {
		pos--;
	}
	
	pos = startOfWord(pos);

	this->TextDSetInsertPosition(pos);
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::forwardWordAP(XEvent *event, String *args, Cardinal *nArgs) {

	TextBuffer *buf = this->buffer;
	int pos, insertPos = this->TextDGetInsertPosition();
	const char *delimiters = text_of(w).P_delimiters;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (insertPos == buf->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}
	pos = insertPos;

	if (hasKey("tail", args, nArgs)) {
		for (; pos < buf->BufGetLength(); pos++) {
			if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
				break;
			}
		}
		if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
			pos = endOfWord(pos);
		}
	} else {
		if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
			pos = endOfWord(pos);
		}
		for (; pos < buf->BufGetLength(); pos++) {
			if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
				break;
			}
		}
	}

	this->TextDSetInsertPosition(pos);
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::selectAllAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextBuffer *buf = this->textBuffer();

	this->cancelDrag();
	buf->BufSelect(0, buf->BufGetLength());
}

/*
** Reset drag state and cancel the auto-scroll timer
*/
void TextDisplay::endDrag() {

	if (this->autoScrollProcID != 0) {
		XtRemoveTimeOut(this->autoScrollProcID);
	}

	this->autoScrollProcID = 0;

	if (this->dragState == MOUSE_PAN) {
		XUngrabPointer(XtDisplay(this->w), CurrentTime);
	}

	this->dragState = NOT_CLICKED;
}


/*
** For actions involving cursor movement, "extend" keyword means incorporate
** the new cursor position in the selection, and lack of an "extend" keyword
** means cancel the existing selection
*/
void TextDisplay::checkMoveSelectionChange(XEvent *event, int startPos, String *args, Cardinal *nArgs) {

	if (hasKey("extend", args, nArgs)) {
		keyMoveExtendSelection(event, startPos, hasKey("rect", args, nArgs));
	} else {
		this->textBuffer()->BufUnselect();
	}
}

/*
** If a selection change was requested via a keyboard command for moving
** the insertion cursor (usually with the "extend" keyword), adjust the
** selection to include the new cursor position, or begin a new selection
** between startPos and the new cursor position with anchor at startPos.
*/
void TextDisplay::keyMoveExtendSelection(XEvent *event, int origPos, int rectangular) {

	XKeyEvent *e = &event->xkey;

	TextBuffer *buf    = this->buffer;
	TextSelection *sel = &buf->primary_;
	int newPos         = this->TextDGetInsertPosition();
	int startPos;
	int endPos;
	int startCol;
	int endCol;
	int newCol;
	int origCol;
	int anchor;
	int rectAnchor;
	int anchorLineStart;

	/* Moving the cursor does not take the Motif destination, but as soon as
	   the user selects something, grab it (I'm not sure if this distinction
	   actually makes sense, but it's what Motif was doing, back when their
	   secondary selections actually worked correctly) */
	this->TakeMotifDestination(e->time);

	if ((sel->selected || sel->zeroWidth) && sel->rectangular && rectangular) {
	
		// rect -> rect
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		startCol = std::min(this->rectAnchor, newCol);
		endCol = std::max(this->rectAnchor, newCol);
		startPos = buf->BufStartOfLine(std::min(this->getAnchor(), newPos));
		endPos = buf->BufEndOfLine(std::max(this->anchor, newPos));
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
		
	} else if (sel->selected && rectangular) { // plain -> rect
	
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		if (abs(newPos - sel->start) < abs(newPos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		anchorLineStart = buf->BufStartOfLine(anchor);
		rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
		this->anchor = anchor;
		this->rectAnchor = rectAnchor;
		buf->BufRectSelect(buf->BufStartOfLine(std::min(anchor, newPos)), buf->BufEndOfLine(std::max(anchor, newPos)), std::min(rectAnchor, newCol), std::max(rectAnchor, newCol));
		
	} else if (sel->selected && sel->rectangular) { // rect -> plain
	
		startPos = buf->BufCountForwardDispChars(buf->BufStartOfLine(sel->start), sel->rectStart);
		endPos = buf->BufCountForwardDispChars(buf->BufStartOfLine(sel->end), sel->rectEnd);
		if (abs(origPos - startPos) < abs(origPos - endPos)) {
			anchor = endPos;
		} else {
			anchor = startPos;
		} 
		buf->BufSelect(anchor, newPos);
		
	} else if (sel->selected) { // plain -> plain
	
		if (abs(origPos - sel->start) < abs(origPos - sel->end)) {
			anchor = sel->end;
		} else {
			anchor = sel->start;
		} 
		buf->BufSelect(anchor, newPos);
		
	} else if (rectangular) { // no sel -> rect
	
		origCol = buf->BufCountDispChars(buf->BufStartOfLine(origPos), origPos);
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		startCol = std::min(newCol, origCol);
		endCol = std::max(newCol, origCol);
		startPos = buf->BufStartOfLine(std::min(origPos, newPos));
		endPos = buf->BufEndOfLine(std::max(origPos, newPos));
		this->anchor = origPos;
		this->rectAnchor = origCol;
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
		
	} else { // no sel -> plain
	
		this->anchor = origPos;
		this->rectAnchor = buf->BufCountDispChars(buf->BufStartOfLine(origPos), origPos);
		buf->BufSelect(this->anchor, newPos);
	}
}

int TextDisplay::startOfWord(int pos) {
	int startPos;
	TextBuffer *buf = this->textBuffer();
	const char *delimiters = text_of(w).P_delimiters;
	char c = buf->BufGetCharacter(pos);

	if (c == ' ' || c == '\t') {
		if (!spanBackward(buf, pos, " \t", false, &startPos))
			return 0;
	} else if (strchr(delimiters, c)) {
		if (!spanBackward(buf, pos, delimiters, true, &startPos))
			return 0;
	} else {
		if (!buf->BufSearchBackwardEx(pos, delimiters, &startPos))
			return 0;
	}
	return std::min(pos, startPos + 1);
}

int TextDisplay::endOfWord(int pos) {
	int endPos;
	TextBuffer *buf = this->textBuffer();
	const char *delimiters = text_of(w).P_delimiters;
	char c = buf->BufGetCharacter(pos);

	if (c == ' ' || c == '\t') {
		if (!spanForward(buf, pos, " \t", false, &endPos))
			return buf->BufGetLength();
	} else if (strchr(delimiters, c)) {
		if (!spanForward(buf, pos, delimiters, true, &endPos))
			return buf->BufGetLength();
	} else {
		if (!buf->BufSearchForwardEx(pos, delimiters, &endPos))
			return buf->BufGetLength();
	}
	return endPos;
}

/*
** Search forwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character "startPos", and returning the
** result in "foundPos" returns True if found, false if not. If ignoreSpace
** is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
int TextDisplay::spanForward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos) {

	const char *c;

	int pos = startPos;
	while (pos < buf->BufGetLength()) {
		for (c = searchChars; *c != '\0'; c++)
			if (!(ignoreSpace && (*c == ' ' || *c == '\t' || *c == '\n')))
				if (buf->BufGetCharacter(pos) == *c)
					break;
		if (*c == 0) {
			*foundPos = pos;
			return True;
		}
		pos++;
	}
	*foundPos = buf->BufGetLength();
	return False;
}

/*
** Search backwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character BEFORE "startPos", returning the
** result in "foundPos" returns True if found, false if not. If ignoreSpace is
** set, then Space, Tab, and Newlines are ignored in searchChars.
*/
int TextDisplay::spanBackward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos) {
	int pos;
	const char *c;

	if (startPos == 0) {
		*foundPos = 0;
		return False;
	}
	pos = startPos == 0 ? 0 : startPos - 1;
	while (pos >= 0) {
		for (c = searchChars; *c != '\0'; c++)
			if (!(ignoreSpace && (*c == ' ' || *c == '\t' || *c == '\n')))
				if (buf->BufGetCharacter(pos) == *c)
					break;
		if (*c == 0) {
			*foundPos = pos;
			return True;
		}
		pos--;
	}
	*foundPos = 0;
	return False;
}

void TextDisplay::scrollToLineAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	int topLineNum;
	int horizOffset;
	int lineNum;

	if (*nArgs == 0 || sscanf(args[0], "%d", &lineNum) != 1) {
		return;
	}
	
	this->TextDGetScroll(&topLineNum, &horizOffset);
	this->TextDSetScroll(lineNum, horizOffset);
}

void TextDisplay::scrollUpAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	int topLineNum;
	int horizOffset;
	int nLines;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
		return;
	if (*nArgs == 2) {
		// Allow both 'page' and 'pages'
		if (strncmp(args[1], "page", 4) == 0)
			nLines *= this->nVisibleLines;

		// 'line' or 'lines' is the only other valid possibility
		else if (strncmp(args[1], "line", 4) != 0)
			return;
	}
	this->TextDGetScroll(&topLineNum, &horizOffset);
	this->TextDSetScroll(topLineNum - nLines, horizOffset);
}

void TextDisplay::scrollDownAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	int topLineNum;
	int horizOffset;
	int nLines;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1) {
		return;
	}

	if (*nArgs == 2) {
		// Allow both 'page' and 'pages'
		if (strncmp(args[1], "page", 4) == 0) {
			nLines *= this->nVisibleLines;

		// 'line' or 'lines' is the only other valid possibility
		} else if (strncmp(args[1], "line", 4) != 0) {
			return;
		}
	}
	
	this->TextDGetScroll(&topLineNum, &horizOffset);
	this->TextDSetScroll(topLineNum + nLines, horizOffset);
}

void TextDisplay::scrollLeftAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	int horizOffset;
	int nPixels;
	int sliderMax;
	int sliderSize;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nPixels) != 1) {
		return;
	}
	
	XtVaGetValues(this->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
	horizOffset = std::min(std::max(0, this->horizOffset - nPixels), sliderMax - sliderSize);
	
	if (this->horizOffset != horizOffset) {
		this->TextDSetScroll(this->topLineNum, horizOffset);
	}
}

void TextDisplay::scrollRightAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	int horizOffset;
	int nPixels;
	int sliderMax;
	int sliderSize;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nPixels) != 1) {
		return;
	}
	
	XtVaGetValues(this->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
	horizOffset = std::min(std::max(0, this->horizOffset + nPixels), sliderMax - sliderSize);
	
	if (this->horizOffset != horizOffset) {
		this->TextDSetScroll(this->topLineNum, horizOffset);
	}
}

void TextDisplay::toggleOverstrikeAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto tw = reinterpret_cast<TextWidget>(this->w);

	if (text_of(tw).P_overstrike) {
		text_of(tw).P_overstrike = false;
		this->TextDSetCursorStyle(text_of(tw).P_heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
	} else {
		text_of(tw).P_overstrike = true;
		if (this->cursorStyle == NORMAL_CURSOR || this->cursorStyle == HEAVY_CURSOR)
			this->TextDSetCursorStyle(BLOCK_CURSOR);
	}
}

void TextDisplay::pageLeftAP(XEvent *event, String *args, Cardinal *nArgs) {

	TextBuffer *buf  = this->buffer;
	int insertPos    = this->TextDGetInsertPosition();
	int maxCharWidth = this->fontStruct->max_bounds.width;
	int lineStartPos;
	int indent;
	int pos;
	int horizOffset;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		if (this->horizOffset == 0) {
			ringIfNecessary(silent);
			return;
		}
		horizOffset = std::max(0, this->horizOffset - this->rect.width);
		this->TextDSetScroll(this->topLineNum, horizOffset);
	} else {
		lineStartPos = buf->BufStartOfLine(insertPos);
		if (insertPos == lineStartPos && this->horizOffset == 0) {
			ringIfNecessary(silent);
			return;
		}
		indent = buf->BufCountDispChars(lineStartPos, insertPos);
		pos = buf->BufCountForwardDispChars(lineStartPos, std::max(0, indent - this->rect.width / maxCharWidth));
		this->TextDSetInsertPosition(pos);
		this->TextDSetScroll(this->topLineNum, std::max(0, this->horizOffset - this->rect.width));
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
	}
}

void TextDisplay::pageRightAP(XEvent *event, String *args, Cardinal *nArgs) {

	TextBuffer *buf    = this->buffer;
	int insertPos      = this->TextDGetInsertPosition();
	int maxCharWidth   = this->fontStruct->max_bounds.width;
	int oldHorizOffset = this->horizOffset;
	int sliderSize;
	int sliderMax;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		XtVaGetValues(this->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
		int horizOffset = std::min(this->horizOffset + this->rect.width, sliderMax - sliderSize);
		
		if (this->horizOffset == horizOffset) {
			ringIfNecessary(silent);
			return;
		}
		
		this->TextDSetScroll(this->topLineNum, horizOffset);
	} else {
		int lineStartPos = buf->BufStartOfLine(insertPos);
		int indent       = buf->BufCountDispChars(lineStartPos, insertPos);
		int pos          = buf->BufCountForwardDispChars(lineStartPos, indent + this->rect.width / maxCharWidth);
		
		this->TextDSetInsertPosition(pos);
		this->TextDSetScroll(this->topLineNum, this->horizOffset + this->rect.width);
		
		if (this->horizOffset == oldHorizOffset && insertPos == pos) {
			ringIfNecessary(silent);
		}
		
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
	}
}

void TextDisplay::nextPageAP(XEvent *event, String *args, Cardinal *nArgs) {

	TextBuffer *buf = this->buffer;
	int lastTopLine = std::max<int>(1, this->getBufferLinesCount() - (this->nVisibleLines - 2) + text_of(w).P_cursorVPadding);
	int insertPos = this->TextDGetInsertPosition();
	int column = 0;
	int visLineNum;
	int lineStartPos;
	int pos;
	int targetLine;
	int pageForwardCount = std::max(1, this->nVisibleLines - 1);
	
	bool silent         = hasKey("nobell", args, nArgs);
	bool maintainColumn = hasKey("column", args, nArgs);
	
	this->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) { // scrollbar only
		targetLine = std::min(this->topLineNum + pageForwardCount, lastTopLine);

		if (targetLine == this->topLineNum) {
			ringIfNecessary(silent);
			return;
		}
		this->TextDSetScroll(targetLine, this->horizOffset);
	} else if (hasKey("stutter", args, nArgs)) { // Mac style
		// move to bottom line of visible area
		// if already there, page down maintaining preferrred column
		targetLine = std::max(std::min(this->nVisibleLines - 1, this->getBufferLinesCount()), 0);
		column = this->TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == this->lineStarts[targetLine]) {
			if (insertPos >= buf->BufGetLength() || this->topLineNum == lastTopLine) {
				ringIfNecessary(silent);
				return;
			}
			targetLine = std::min(this->topLineNum + pageForwardCount, lastTopLine);
			pos = this->TextDCountForwardNLines(insertPos, pageForwardCount, false);
			if (maintainColumn) {
				pos = this->TextDPosOfPreferredCol(column, pos);
			}
			this->TextDSetInsertPosition(pos);
			this->TextDSetScroll(targetLine, this->horizOffset);
		} else {
			pos = this->lineStarts[targetLine];
			while (targetLine > 0 && pos == -1) {
				--targetLine;
				pos = this->lineStarts[targetLine];
			}
			if (lineStartPos == pos) {
				ringIfNecessary(silent);
				return;
			}
			if (maintainColumn) {
				pos = this->TextDPosOfPreferredCol(column, pos);
			}
			this->TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
		if (maintainColumn) {
			this->cursorPreferredCol = column;
		} else {
			this->cursorPreferredCol = -1;
		}
	} else { // "standard"
		if (insertPos >= buf->BufGetLength() && this->topLineNum == lastTopLine) {
			ringIfNecessary(silent);
			return;
		}
		if (maintainColumn) {
			column = this->TextDPreferredColumn(&visLineNum, &lineStartPos);
		}
		targetLine = this->topLineNum + this->nVisibleLines - 1;
		if (targetLine < 1)
			targetLine = 1;
		if (targetLine > lastTopLine)
			targetLine = lastTopLine;
		pos = this->TextDCountForwardNLines(insertPos, this->nVisibleLines - 1, false);
		if (maintainColumn) {
			pos = this->TextDPosOfPreferredCol(column, pos);
		}
		this->TextDSetInsertPosition(pos);
		this->TextDSetScroll(targetLine, this->horizOffset);
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
		if (maintainColumn) {
			this->cursorPreferredCol = column;
		} else {
			this->cursorPreferredCol = -1;
		}
	}
}

void TextDisplay::previousPageAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	int column = 0;
	int visLineNum;
	int lineStartPos;
	int pos;
	int targetLine;
	int pageBackwardCount = std::max(1, this->nVisibleLines - 1);
	
	bool silent         = hasKey("nobell", args, nArgs);
	bool maintainColumn = hasKey("column", args, nArgs);
	
	this->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) { // scrollbar only
		targetLine = std::max(this->topLineNum - pageBackwardCount, 1);

		if (targetLine == this->topLineNum) {
			ringIfNecessary(silent);
			return;
		}
		this->TextDSetScroll(targetLine, this->horizOffset);
	} else if (hasKey("stutter", args, nArgs)) { // Mac style
		// move to top line of visible area
		// if already there, page up maintaining preferrred column if required
		targetLine = 0;
		column = this->TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == this->lineStarts[targetLine]) {
			if (this->topLineNum == 1 && (maintainColumn || column == 0)) {
				ringIfNecessary(silent);
				return;
			}
			targetLine = std::max(this->topLineNum - pageBackwardCount, 1);
			pos = this->TextDCountBackwardNLines(insertPos, pageBackwardCount);
			if (maintainColumn) {
				pos = this->TextDPosOfPreferredCol(column, pos);
			}
			this->TextDSetInsertPosition(pos);
			this->TextDSetScroll(targetLine, this->horizOffset);
		} else {
			pos = this->lineStarts[targetLine];
			if (maintainColumn) {
				pos = this->TextDPosOfPreferredCol(column, pos);
			}
			this->TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
		if (maintainColumn) {
			this->cursorPreferredCol = column;
		} else {
			this->cursorPreferredCol = -1;
		}
	} else { // "standard"
		if (insertPos <= 0 && this->topLineNum == 1) {
			ringIfNecessary(silent);
			return;
		}
		if (maintainColumn) {
			column = this->TextDPreferredColumn(&visLineNum, &lineStartPos);
		}
		targetLine = this->topLineNum - (this->nVisibleLines - 1);
		if (targetLine < 1)
			targetLine = 1;
		pos = this->TextDCountBackwardNLines(insertPos, this->nVisibleLines - 1);
		if (maintainColumn) {
			pos = this->TextDPosOfPreferredCol(column, pos);
		}
		this->TextDSetInsertPosition(pos);
		this->TextDSetScroll(targetLine, this->horizOffset);
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
		if (maintainColumn) {
			this->cursorPreferredCol = column;
		} else {
			this->cursorPreferredCol = -1;
		}
	}
}

void TextDisplay::beginningOfFileAP(XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = this->TextDGetInsertPosition();

	this->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		if (this->topLineNum != 1) {
			this->TextDSetScroll(1, this->horizOffset);
		}
	} else {
		this->TextDSetInsertPosition(0);
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
	}
}

void TextDisplay::endOfFileAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	int lastTopLine;

	this->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		lastTopLine = std::max<int>(1, this->getBufferLinesCount() - (this->nVisibleLines - 2) + text_of(w).P_cursorVPadding);
		if (lastTopLine != this->topLineNum) {
			this->TextDSetScroll(lastTopLine, this->horizOffset);
		}
	} else {
		this->TextDSetInsertPosition(this->textBuffer()->BufGetLength());
		checkMoveSelectionChange(event, insertPos, args, nArgs);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
	}
}

void TextDisplay::beginningOfLineAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();

	this->cancelDrag();
	if (hasKey("absolute", args, nArgs))
		this->TextDSetInsertPosition(this->textBuffer()->BufStartOfLine(insertPos));
	else
		this->TextDSetInsertPosition(this->TextDStartOfLine(insertPos));
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
	this->cursorPreferredCol = 0;
}

void TextDisplay::endOfLineAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();

	this->cancelDrag();
	if (hasKey("absolute", args, nArgs))
		this->TextDSetInsertPosition(this->textBuffer()->BufEndOfLine(insertPos));
	else
		this->TextDSetInsertPosition(this->TextDEndOfLine(insertPos, false));
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
	this->cursorPreferredCol = -1;
}

void TextDisplay::processUpAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);
	bool abs    = hasKey("absolute", args, nArgs);

	this->cancelDrag();
	if (!this->TextDMoveUp(abs))
		ringIfNecessary(silent);
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::processShiftUpAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);
	bool abs    = hasKey("absolute", args, nArgs);

	this->cancelDrag();
	if (!this->TextDMoveUp(abs))
		ringIfNecessary(silent);
	keyMoveExtendSelection(event, insertPos, hasKey("rect", args, nArgs));
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::processDownAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);
	bool abs    = hasKey("absolute", args, nArgs);

	this->cancelDrag();
	if (!this->TextDMoveDown(abs))
		ringIfNecessary(silent);
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::processShiftDownAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);
	bool abs    = hasKey("absolute", args, nArgs);

	this->cancelDrag();
	if (!this->TextDMoveDown(abs))
		ringIfNecessary(silent);
	keyMoveExtendSelection(event, insertPos, hasKey("rect", args, nArgs));
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::processTabAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	TextBuffer *buf = this->buffer;
	TextSelection *sel = &buf->primary_;
	int emTabDist = text_of(w).P_emulateTabs;
	int emTabsBeforeCursor = this->emTabsBeforeCursor;
	int insertPos, indent, startIndent, toIndent, lineStart, tabWidth;

	if (this->checkReadOnly())
		return;
	this->cancelDrag();
	this->TakeMotifDestination(event->xkey.time);

	// If emulated tabs are off, just insert a tab
	if (emTabDist <= 0) {
		this->TextInsertAtCursorEx("\t", event, true, true);
		return;
	}

	/* Find the starting and ending indentation.  If the tab is to
	   replace an existing selection, use the start of the selection
	   instead of the cursor position as the indent.  When replacing
	   rectangular selections, tabs are automatically recalculated as
	   if the inserted text began at the start of the line */
	insertPos = pendingSelection() ? sel->start : this->TextDGetInsertPosition();
	lineStart = buf->BufStartOfLine(insertPos);
	if (pendingSelection() && sel->rectangular)
		insertPos = buf->BufCountForwardDispChars(lineStart, sel->rectStart);
	startIndent = buf->BufCountDispChars(lineStart, insertPos);
	toIndent = startIndent + emTabDist - (startIndent % emTabDist);
	if (pendingSelection() && sel->rectangular) {
		toIndent -= startIndent;
		startIndent = 0;
	}

	// Allocate a buffer assuming all the inserted characters will be spaces
	std::string outStr;
	outStr.reserve(toIndent - startIndent);

	// Add spaces and tabs to outStr until it reaches toIndent
	auto outPtr = std::back_inserter(outStr);
	indent = startIndent;
	while (indent < toIndent) {
		tabWidth = TextBuffer::BufCharWidth('\t', indent, buf->tabDist_, buf->nullSubsChar_);
		if (buf->useTabs_ && tabWidth > 1 && indent + tabWidth <= toIndent) {
			*outPtr++ = '\t';
			indent += tabWidth;
		} else {
			*outPtr++ = ' ';
			indent++;
		}
	}

	// Insert the emulated tab
	this->TextInsertAtCursorEx(outStr, event, true, true);

	// Restore and ++ emTabsBeforeCursor cleared by TextInsertAtCursorEx
	this->emTabsBeforeCursor = emTabsBeforeCursor + 1;

	buf->BufUnselect();
}


void TextDisplay::processCancelAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	int dragState = this->dragState;
	TextBuffer *buf = this->buffer;
	
	// If there's a calltip displayed, kill it.
	this->TextDKillCalltip(0);

	if (dragState == PRIMARY_DRAG || dragState == PRIMARY_RECT_DRAG) {
		buf->BufUnselect();
	}
	
	this->cancelDrag();
}

void TextDisplay::keySelectAP(XEvent *event, String *args, Cardinal *nArgs) {

	int stat;
	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (hasKey("left", args, nArgs))
		stat = this->TextDMoveLeft();
	else if (hasKey("right", args, nArgs))
		stat = this->TextDMoveRight();
	else if (hasKey("up", args, nArgs))
		stat = this->TextDMoveUp(false);
	else if (hasKey("down", args, nArgs))
		stat = this->TextDMoveDown(false);
	else {
		keyMoveExtendSelection(event, insertPos, hasKey("rect", args, nArgs));
		return;
	}
	
	if (!stat) {
		ringIfNecessary(silent);
	} else {
		keyMoveExtendSelection(event, insertPos, hasKey("rect", args, nArgs));
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
	}
}

void TextDisplay::forwardParagraphAP(XEvent *event, String *args, Cardinal *nArgs) {

	int pos;
	int insertPos = this->TextDGetInsertPosition();
	TextBuffer *buf = this->buffer;
	char c;
	static char whiteChars[] = " \t";
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (insertPos == buf->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}
	pos = std::min(buf->BufEndOfLine(insertPos) + 1, buf->BufGetLength());
	while (pos < buf->BufGetLength()) {
		c = buf->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos++;
		else
			pos = std::min(buf->BufEndOfLine(pos) + 1, buf->BufGetLength());
	}
	this->TextDSetInsertPosition(std::min(pos + 1, buf->BufGetLength()));
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::backwardParagraphAP(XEvent *event, String *args, Cardinal *nArgs) {

	int parStart;
	int pos;
	int insertPos = this->TextDGetInsertPosition();
	TextBuffer *buf = this->buffer;
	char c;
	static char whiteChars[] = " \t";
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}
	parStart = buf->BufStartOfLine(std::max(insertPos - 1, 0));
	pos = std::max(parStart - 2, 0);
	while (pos > 0) {
		c = buf->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos--;
		else {
			parStart = buf->BufStartOfLine(pos);
			pos = std::max(parStart - 2, 0);
		}
	}
	this->TextDSetInsertPosition(parStart);
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::forwardCharacterAP(XEvent *event, String *args, Cardinal *nArgs) {

	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (!this->TextDMoveRight()) {
		ringIfNecessary(silent);
	}
	checkMoveSelectionChange(event, insertPos, args, nArgs);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::deleteToEndOfLineAP(XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	int insertPos = this->TextDGetInsertPosition();
	int endOfLine;
	int silent = 0;

	silent = hasKey("nobell", args, nArgs);
	if (hasKey("absolute", args, nArgs))
		endOfLine = this->textBuffer()->BufEndOfLine(insertPos);
	else
		endOfLine = this->TextDEndOfLine(insertPos, false);
	this->cancelDrag();
	if (this->checkReadOnly())
		return;
	this->TakeMotifDestination(e->time);
	if (deletePendingSelection(event))
		return;
	if (insertPos == endOfLine) {
		ringIfNecessary(silent);
		return;
	}
	this->textBuffer()->BufRemove(insertPos, endOfLine);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::deleteToStartOfLineAP(XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	int insertPos = this->TextDGetInsertPosition();
	int startOfLine;
	int silent = 0;

	silent = hasKey("nobell", args, nArgs);
	if (hasKey("wrap", args, nArgs))
		startOfLine =this->TextDStartOfLine(insertPos);
	else
		startOfLine = this->textBuffer()->BufStartOfLine(insertPos);
	this->cancelDrag();
	if (this->checkReadOnly())
		return;
	this->TakeMotifDestination(e->time);
	if (deletePendingSelection(event))
		return;
	if (insertPos == startOfLine) {
		ringIfNecessary(silent);
		return;
	}
	this->textBuffer()->BufRemove(startOfLine, insertPos);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

/*
** If there's a selection, delete it and position the cursor where the
** selection was deleted.  (Called by routines which do deletion to check
** first for and do possible selection delete)
*/
int TextDisplay::deletePendingSelection(XEvent *event) {

	TextBuffer *buf = this->buffer;

	if (this->textBuffer()->primary_.selected) {
		buf->BufRemoveSelected();
		this->TextDSetInsertPosition(buf->cursorPosHint_);
		this->checkAutoShowInsertPos();
		this->callCursorMovementCBs(event);
		return true;
	} else
		return false;
}

void TextDisplay::deletePreviousCharacterAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XKeyEvent *e = &event->xkey;
	int insertPos = this->TextDGetInsertPosition();
	char c;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (this->checkReadOnly())
		return;

	this->TakeMotifDestination(e->time);
	if (deletePendingSelection(event))
		return;

	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}

	if (deleteEmulatedTab(event))
		return;

	if (text_of(w).P_overstrike) {
		c = this->textBuffer()->BufGetCharacter(insertPos - 1);
		if (c == '\n')
			this->textBuffer()->BufRemove(insertPos - 1, insertPos);
		else if (c != '\t')
			this->textBuffer()->BufReplaceEx(insertPos - 1, insertPos, " ");
	} else {
		this->textBuffer()->BufRemove(insertPos - 1, insertPos);
	}

	this->TextDSetInsertPosition(insertPos - 1);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::deleteNextCharacterAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XKeyEvent *e = &event->xkey;
	int insertPos = this->TextDGetInsertPosition();
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (this->checkReadOnly()) {
		return;
	}
		
	this->TakeMotifDestination(e->time);
	
	if (deletePendingSelection(event)) {
		return;
	}
		
	if (insertPos == this->textBuffer()->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}
	
	this->textBuffer()->BufRemove(insertPos, insertPos + 1);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::deletePreviousWordAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XKeyEvent *e = &event->xkey;
	int insertPos = this->TextDGetInsertPosition();
	int pos;
	int lineStart = this->textBuffer()->BufStartOfLine(insertPos);
	const char *delimiters = text_of(w).P_delimiters;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (this->checkReadOnly()) {
		return;
	}

	this->TakeMotifDestination(e->time);
	if (deletePendingSelection(event)) {
		return;
	}

	if (insertPos == lineStart) {
		ringIfNecessary(silent);
		return;
	}

	pos = std::max(insertPos - 1, 0);
	while (strchr(delimiters, this->textBuffer()->BufGetCharacter(pos)) != nullptr && pos != lineStart) {
		pos--;
	}

	pos = startOfWord(pos);
	this->textBuffer()->BufRemove(pos, insertPos);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

void TextDisplay::deleteNextWordAP(XEvent *event, String *args, Cardinal *nArgs) {

	XKeyEvent *e = &event->xkey;
	int insertPos = this->TextDGetInsertPosition();
	int pos;
	int lineEnd = this->textBuffer()->BufEndOfLine(insertPos);
	const char *delimiters = text_of(w).P_delimiters;
	bool silent = hasKey("nobell", args, nArgs);

	this->cancelDrag();
	if (this->checkReadOnly()) {
		return;
	}

	this->TakeMotifDestination(e->time);
	if (deletePendingSelection(event)) {
		return;
	}

	if (insertPos == lineEnd) {
		ringIfNecessary(silent);
		return;
	}

	pos = insertPos;
	while (strchr(delimiters, this->textBuffer()->BufGetCharacter(pos)) != nullptr && pos != lineEnd) {
		pos++;
	}

	pos = endOfWord(pos);
	this->textBuffer()->BufRemove(insertPos, pos);
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

/*
** Check if tab emulation is on and if there are emulated tabs before the
** cursor, and if so, delete an emulated tab as a unit.  Also finishes up
** by calling checkAutoShowInsertPos and callCursorMovementCBs, so the
** calling action proc can just return (this is necessary to preserve
** emTabsBeforeCursor which is otherwise cleared by callCursorMovementCBs).
*/
int TextDisplay::deleteEmulatedTab(XEvent *event) {

	TextBuffer *buf        = this->textBuffer();
	int emTabDist          = text_of(w).P_emulateTabs;
	int emTabsBeforeCursor = this->emTabsBeforeCursor;
	int startIndent, toIndent, insertPos, startPos, lineStart;
	int pos, indent, startPosIndent;
	char c;

	if (emTabDist <= 0 || emTabsBeforeCursor <= 0)
		return False;

	// Find the position of the previous tab stop
	insertPos = this->TextDGetInsertPosition();
	lineStart = buf->BufStartOfLine(insertPos);
	startIndent = buf->BufCountDispChars(lineStart, insertPos);
	toIndent = (startIndent - 1) - ((startIndent - 1) % emTabDist);

	/* Find the position at which to begin deleting (stop at non-whitespace
	   characters) */
	startPosIndent = indent = 0;
	startPos = lineStart;
	for (pos = lineStart; pos < insertPos; pos++) {
		c = buf->BufGetCharacter(pos);
		indent += TextBuffer::BufCharWidth(c, indent, buf->tabDist_, buf->nullSubsChar_);
		if (indent > toIndent)
			break;
		startPosIndent = indent;
		startPos = pos + 1;
	}

	// Just to make sure, check that we're not deleting any non-white chars
	for (pos = insertPos - 1; pos >= startPos; pos--) {
		c = buf->BufGetCharacter(pos);
		if (c != ' ' && c != '\t') {
			startPos = pos + 1;
			break;
		}
	}

	/* Do the text replacement and reposition the cursor.  If any spaces need
	   to be inserted to make up for a deleted tab, do a BufReplaceEx, otherwise,
	   do a BufRemove. */
	if (startPosIndent < toIndent) {

		std::string spaceString(toIndent - startPosIndent, ' ');

		buf->BufReplaceEx(startPos, insertPos, spaceString);
		this->TextDSetInsertPosition(startPos + toIndent - startPosIndent);
	} else {
		buf->BufRemove(startPos, insertPos);
		this->TextDSetInsertPosition(startPos);
	}

	/* The normal cursor movement stuff would usually be called by the action
	   routine, but this wraps around it to restore emTabsBeforeCursor */
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);

	/* Decrement and restore the marker for consecutive emulated tabs, which
	   would otherwise have been zeroed by callCursorMovementCBs */
	this->emTabsBeforeCursor = emTabsBeforeCursor - 1;
	return True;
}

void TextDisplay::deleteSelectionAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;

	this->cancelDrag();
	
	if (this->checkReadOnly()) {
		return;
	}
	
	this->TakeMotifDestination(e->time);
	deletePendingSelection(event);
}

void TextDisplay::newlineAP(XEvent *event, String *args, Cardinal *nArgs) {
	if (text_of(w).P_autoIndent || text_of(w).P_smartIndent) {
		newlineAndIndentAP(event, args, nArgs);
	} else {
		newlineNoIndentAP(event, args, nArgs);
	}
}

void TextDisplay::newlineNoIndentAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;
	
	this->cancelDrag();
	if (this->checkReadOnly()) {
		return;
	}
	
	this->TakeMotifDestination(e->time);
	simpleInsertAtCursorEx("\n", event, true);
	this->textBuffer()->BufUnselect();
}

void TextDisplay::newlineAndIndentAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextBuffer *buf = this->buffer;
	int column;

	if (this->checkReadOnly()) {
		return;
	}
	
	this->cancelDrag();
	this->TakeMotifDestination(e->time);

	/* Create a string containing a newline followed by auto or smart
	   indent string */
	int cursorPos = this->TextDGetInsertPosition();
	int lineStartPos = buf->BufStartOfLine(cursorPos);
	std::string indentStr = createIndentStringEx(buf, 0, lineStartPos, cursorPos, nullptr, &column);

	// Insert it at the cursor
	simpleInsertAtCursorEx(indentStr, event, true);

	if (text_of(tw).P_emulateTabs > 0) {
		/*  If emulated tabs are on, make the inserted indent deletable by
		    tab. Round this up by faking the column a bit to the right to
		    let the user delete half-tabs with one keypress.  */
		column += text_of(tw).P_emulateTabs - 1;
		this->emTabsBeforeCursor = column / text_of(tw).P_emulateTabs;
	}

	buf->BufUnselect();
}

/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursorEx, but without the complicated auto-wrap
** scanning and re-formatting.
*/
void TextDisplay::simpleInsertAtCursorEx(view::string_view chars, XEvent *event, int allowPendingDelete) {

	TextBuffer *buf = this->buffer;

	if (allowPendingDelete && pendingSelection()) {
		buf->BufReplaceSelectedEx(chars);
		this->TextDSetInsertPosition(buf->cursorPosHint_);
	} else if (text_of(w).P_overstrike) {

		size_t index = chars.find('\n');
		if(index != view::string_view::npos) {
			this->TextDInsertEx(chars);
		} else {
			this->TextDOverstrikeEx(chars);
		}
	} else {
		this->TextDInsertEx(chars);
	}

	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

/*
** Create and return an auto-indent string to add a newline at lineEndPos to a
** line starting at lineStartPos in buf.  "buf" may or may not be the real
** text buffer for the widget.  If it is not the widget's text buffer it's
** offset position from the real buffer must be specified in "bufOffset" to
** allow the smart-indent routines to scan back as far as necessary. The
** string length is returned in "length" (or "length" can be passed as nullptr,
** and the indent column is returned in "column" (if non nullptr).
*/
std::string TextDisplay::createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *length, int *column) {

	int pos;
	int indent = -1;
	int tabDist = this->textBuffer()->tabDist_;
	int i;
	int useTabs = this->textBuffer()->useTabs_;
	char c;
	smartIndentCBStruct smartIndent;

	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (text_of(this->w).P_smartIndent && (lineStartPos == 0 || buf == this->buffer)) {
		
		smartIndent.reason        = NEWLINE_INDENT_NEEDED;
		smartIndent.pos           = lineEndPos + bufOffset;
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped    = nullptr;
		
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
		indent = smartIndent.indentRequest;
	}

	// If smart indent wasn't used, measure the indent distance of the line
	if (indent == -1) {
		indent = 0;
		for (pos = lineStartPos; pos < lineEndPos; pos++) {
			c = buf->BufGetCharacter(pos);
			if (c != ' ' && c != '\t')
				break;
			if (c == '\t')
				indent += tabDist - (indent % tabDist);
			else
				indent++;
		}
	}

	// Allocate and create a string of tabs and spaces to achieve the indent
	std::string indentStr;
	indentStr.reserve(indent + 2);

	auto indentPtr = std::back_inserter(indentStr);

	*indentPtr++ = '\n';
	if (useTabs) {
		for (i = 0; i < indent / tabDist; i++) {
			*indentPtr++ = '\t';
		}
		
		for (i = 0; i < indent % tabDist; i++) {
			*indentPtr++ = ' ';
		}
		
	} else {
		for (i = 0; i < indent; i++) {
			*indentPtr++ = ' ';
		}
	}

	// Return any requested stats
	if(length) {
		*length = indentStr.size();
	}
	
	if(column) {
		*column = indent;
	}

	return indentStr;
}

void TextDisplay::insertStringAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	smartIndentCBStruct smartIndent;

	if (*nArgs == 0)
		return;
	this->cancelDrag();
	if (this->checkReadOnly())
		return;
	if (text_of(w).P_smartIndent) {
		smartIndent.reason = CHAR_TYPED;
		smartIndent.pos = this->TextDGetInsertPosition();
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped = args[0];
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
	}
	this->TextInsertAtCursorEx(args[0], event, true, true);
	this->textBuffer()->BufUnselect();
}

void TextDisplay::selfInsertAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	Document *window = Document::WidgetToWindow(this->w);

	int status;
	XKeyEvent *e = &event->xkey;
	char chars[20];
	KeySym keysym;
	int nChars;
	smartIndentCBStruct smartIndent;


	nChars = XmImMbLookupString(w, &event->xkey, chars, 19, &keysym, &status);
	if (nChars == 0 || status == XLookupNone || status == XLookupKeySym || status == XBufferOverflow)
		return;

	this->cancelDrag();
	if (this->checkReadOnly())
		return;
	this->TakeMotifDestination(e->time);
	chars[nChars] = '\0';

	if (!window->buffer_->BufSubstituteNullChars(chars, nChars)) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error"), QLatin1String("Too much binary data"));
		return;
	}

	/* If smart indent is on, call the smart indent callback to check the
	   inserted character */
	if (text_of(w).P_smartIndent) {
		smartIndent.reason        = CHAR_TYPED;
		smartIndent.pos           = this->TextDGetInsertPosition();
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped    = chars;
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
	}
	this->TextInsertAtCursorEx(chars, event, true, true);
	this->textBuffer()->BufUnselect();
}

void TextDisplay::pasteClipboardAP(XEvent *event, String *args, Cardinal *nArgs) {

	if (hasKey("rect", args, nArgs)) {
		this->TextColPasteClipboard(event->xkey.time);
	} else {
		this->TextPasteClipboard(event->xkey.time);
	}
}

void TextDisplay::copyClipboardAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	this->TextCopyClipboard(event->xkey.time);
}

void TextDisplay::cutClipboardAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	this->TextCutClipboard(event->xkey.time);
}

void TextDisplay::mousePanAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;

	int lineHeight = this->ascent + this->descent;
	int topLineNum, horizOffset;
	static Cursor panCursor = 0;

	if (this->dragState == MOUSE_PAN) {
		this->TextDSetScroll((this->btnDownCoord.y - e->y + lineHeight / 2) / lineHeight, this->btnDownCoord.x - e->x);
	} else if (this->dragState == NOT_CLICKED) {
		this->TextDGetScroll(&topLineNum, &horizOffset);
		this->btnDownCoord.x = e->x + horizOffset;
		this->btnDownCoord.y = e->y + topLineNum * lineHeight;
		this->dragState = MOUSE_PAN;
		
		if (!panCursor) {
			panCursor = XCreateFontCursor(XtDisplay(w), XC_fleur);
		}
		
		XGrabPointer(XtDisplay(w), XtWindow(w), false, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, panCursor, CurrentTime);
	} else
		this->cancelDrag();
}

void TextDisplay::copyPrimaryAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XKeyEvent *e = &event->xkey;
	TextBuffer *buf = this->buffer;
	TextSelection *primary = &buf->primary_;
	bool rectangular = hasKey("rect", args, nArgs);
	int insertPos, col;

	this->cancelDrag();
	if (this->checkReadOnly())
		return;
	if (primary->selected && rectangular) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = this->TextDGetInsertPosition();
		col = buf->BufCountDispChars(buf->BufStartOfLine(insertPos), insertPos);
		buf->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
		this->TextDSetInsertPosition(buf->cursorPosHint_);

		this->checkAutoShowInsertPos();
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = this->TextDGetInsertPosition();
		buf->BufInsertEx(insertPos, textToCopy);
		this->TextDSetInsertPosition(insertPos + textToCopy.size());

		this->checkAutoShowInsertPos();
	} else if (rectangular) {
		if (!this->TextDPositionToXY(this->TextDGetInsertPosition(), &this->btnDownCoord.x, &this->btnDownCoord.y)) {
			return; // shouldn't happen
		}
		
		this->InsertPrimarySelection(e->time, true);
	} else {
		this->InsertPrimarySelection(e->time, false);
	}
}

void TextDisplay::cutPrimaryAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XKeyEvent *e = &event->xkey;
	TextBuffer *buf = this->buffer;
	TextSelection *primary = &buf->primary_;

	bool rectangular = hasKey("rect", args, nArgs);
	int insertPos, col;

	this->cancelDrag();
	if (this->checkReadOnly()) {
		return;
	}
	
	if (primary->selected && rectangular) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = this->TextDGetInsertPosition();
		col = buf->BufCountDispChars(buf->BufStartOfLine(insertPos), insertPos);
		buf->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
		this->TextDSetInsertPosition(buf->cursorPosHint_);

		buf->BufRemoveSelected();
		this->checkAutoShowInsertPos();
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = this->TextDGetInsertPosition();
		buf->BufInsertEx(insertPos, textToCopy);
		this->TextDSetInsertPosition(insertPos + textToCopy.size());

		buf->BufRemoveSelected();
		this->checkAutoShowInsertPos();
	} else if (rectangular) {
		if (!this->TextDPositionToXY(this->TextDGetInsertPosition(), &this->btnDownCoord.x, &this->btnDownCoord.y))
			return; // shouldn't happen
		this->MovePrimarySelection(e->time, true);
	} else {
		this->MovePrimarySelection(e->time, false);
	}
}

void TextDisplay::exchangeAP(XEvent *event, String *args, Cardinal *nArgs) {

	XButtonEvent *e = &event->xbutton;
	TextBuffer *buf = this->buffer;
	TextSelection *sec = &buf->secondary_, *primary = &buf->primary_;

	int newPrimaryStart, newPrimaryEnd, secWasRect;
	int dragState = this->dragState; // save before endDrag
	bool silent = hasKey("nobell", args, nArgs);

	endDrag();
	if (this->checkReadOnly())
		return;

	/* If there's no secondary selection here, or the primary and secondary
	   selection overlap, just beep and return */
	if (!sec->selected || (primary->selected && ((primary->start <= sec->start && primary->end > sec->start) || (sec->start <= primary->start && sec->end > primary->start)))) {
		buf->BufSecondaryUnselect();
		ringIfNecessary(silent);
		/* If there's no secondary selection, but the primary selection is
		   being dragged, we must not forget to finish the dragging.
		   Otherwise, modifications aren't recorded. */
		if (dragState == PRIMARY_BLOCK_DRAG)
			this->FinishBlockDrag();
		return;
	}

	// if the primary selection is in another widget, use selection routines
	if (!primary->selected) {
		this->ExchangeSelections(e->time);
		return;
	}

	// Both primary and secondary are in this widget, do the exchange here
	std::string primaryText = buf->BufGetSelectionTextEx();
	std::string secText     = buf->BufGetSecSelectTextEx();
	secWasRect = sec->rectangular;
	buf->BufReplaceSecSelectEx(primaryText);
	newPrimaryStart = primary->start;
	buf->BufReplaceSelectedEx(secText);
	newPrimaryEnd = newPrimaryStart + secText.size();

	buf->BufSecondaryUnselect();
	if (secWasRect) {
		this->TextDSetInsertPosition(buf->cursorPosHint_);
	} else {
		buf->BufSelect(newPrimaryStart, newPrimaryEnd);
		this->TextDSetInsertPosition(newPrimaryEnd);
	}
	this->checkAutoShowInsertPos();
}

void TextDisplay::moveToAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	int dragState   = this->dragState;
	TextBuffer *buf = this->buffer;
	
	TextSelection *secondary = &buf->secondary_;
	TextSelection *primary   = &buf->primary_;
	
	int insertPos;
	int rectangular = secondary->rectangular;
	int column;
	int lineStart;

	endDrag();
	
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED)) {
		return;
	}
	
	if (this->checkReadOnly()) {
		buf->BufSecondaryUnselect();
		return;
	}

	if (secondary->selected) {
		if (this->motifDestOwner) {
			std::string textToCopy = buf->BufGetSecSelectTextEx();
			if (primary->selected && rectangular) {
				insertPos = this->TextDGetInsertPosition();
				buf->BufReplaceSelectedEx(textToCopy);
				this->TextDSetInsertPosition(buf->cursorPosHint_);
			} else if (rectangular) {
				insertPos = this->TextDGetInsertPosition();
				lineStart = buf->BufStartOfLine(insertPos);
				column = buf->BufCountDispChars(lineStart, insertPos);
				buf->BufInsertColEx(column, lineStart, textToCopy, nullptr, nullptr);
				this->TextDSetInsertPosition(buf->cursorPosHint_);
			} else
				this->TextInsertAtCursorEx(textToCopy, event, true, text_of(w).P_autoWrapPastedText);

			buf->BufRemoveSecSelect();
			buf->BufSecondaryUnselect();
		} else
			this->SendSecondarySelection(e->time, true);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetRangeEx(primary->start, primary->end);
		this->TextDSetInsertPosition(this->TextDXYToPosition(Point{e->x, e->y}));
		this->TextInsertAtCursorEx(textToCopy, event, false, text_of(w).P_autoWrapPastedText);

		buf->BufRemoveSelected();
		buf->BufUnselect();
	} else {
		this->TextDSetInsertPosition(this->TextDXYToPosition(Point{e->x, e->y}));
		this->MovePrimarySelection(e->time, false);
	}
}

void TextDisplay::moveToOrEndDragAP(XEvent *event, String *args, Cardinal *nArgs) {

	int dragState = this->dragState;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		moveToAP(event, args, nArgs);
		return;
	}

	this->FinishBlockDrag();
}

void TextDisplay::endDragAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	if (this->dragState == PRIMARY_BLOCK_DRAG) {
		this->FinishBlockDrag();
	} else {
		endDrag();
	}
}

void TextDisplay::copyToOrEndDragAP(XEvent *event, String *args, Cardinal *nArgs) {
	int dragState = this->dragState;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		copyToAP(event, args, nArgs);
		return;
	}
	
	this->FinishBlockDrag();
}

void TextDisplay::copyToAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);
	int dragState = this->dragState;
	TextBuffer *buf = this->buffer;
	TextSelection *secondary = &buf->secondary_, *primary = &buf->primary_;
	int rectangular = secondary->rectangular;
	int insertPos, lineStart, column;

	endDrag();
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
		return;
	if (!(secondary->selected && !this->motifDestOwner)) {
		if (this->checkReadOnly()) {
			buf->BufSecondaryUnselect();
			return;
		}
	}
	if (secondary->selected) {
		if (this->motifDestOwner) {
			this->TextDBlankCursor();
			std::string textToCopy = buf->BufGetSecSelectTextEx();
			if (primary->selected && rectangular) {
				insertPos = this->TextDGetInsertPosition();
				buf->BufReplaceSelectedEx(textToCopy);
				this->TextDSetInsertPosition(buf->cursorPosHint_);
			} else if (rectangular) {
				insertPos = this->TextDGetInsertPosition();
				lineStart = buf->BufStartOfLine(insertPos);
				column = buf->BufCountDispChars(lineStart, insertPos);
				buf->BufInsertColEx(column, lineStart, textToCopy, nullptr, nullptr);
				this->TextDSetInsertPosition(buf->cursorPosHint_);
			} else
				this->TextInsertAtCursorEx(textToCopy, event, true, text_of(tw).P_autoWrapPastedText);

			buf->BufSecondaryUnselect();
			this->TextDUnblankCursor();
		} else
			this->SendSecondarySelection(e->time, false);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		this->TextDSetInsertPosition(this->TextDXYToPosition(Point{e->x, e->y}));
		this->TextInsertAtCursorEx(textToCopy, event, false, text_of(tw).P_autoWrapPastedText);
	} else {
		this->TextDSetInsertPosition(this->TextDXYToPosition(Point{e->x, e->y}));
		this->InsertPrimarySelection(e->time, false);
	}
}

void TextDisplay::secondaryOrDragStartAP(XEvent *event, String *args, Cardinal *nArgs) {

	XMotionEvent *e = &event->xmotion;
	TextBuffer *buf = this->buffer;

	/* If the click was outside of the primary selection, this is not
	   a drag, start a secondary selection */
	if (!buf->primary_.selected || !this->TextDInSelection(Point{e->x, e->y})) {
		secondaryStartAP(event, args, nArgs);
		return;
	}

	if (this->checkReadOnly())
		return;

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   drag, and where to drag to */
	this->btnDownCoord = Point{e->x, e->y};
	this->dragState    = CLICKED_IN_SELECTION;
}

void TextDisplay::secondaryAdjustAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XMotionEvent *e = &event->xmotion;
	int dragState = this->dragState;
	bool rectDrag = hasKey("rect", args, nArgs);

	// Make sure the proper initialization was done on mouse down
	if (dragState != SECONDARY_DRAG && dragState != SECONDARY_RECT_DRAG && dragState != SECONDARY_CLICKED)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (this->dragState == SECONDARY_CLICKED) {
		if (abs(e->x - this->btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - this->btnDownCoord.y) > SELECT_THRESHOLD)
			this->dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;
		else
			return;
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	this->dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll({e->x, e->y});

	// Adjust the selection
	adjustSecondarySelection({e->x, e->y});
}

void TextDisplay::secondaryOrDragAdjustAP(XEvent *event, String *args, Cardinal *nArgs) {
	
	XMotionEvent *e = &event->xmotion;
	int dragState = this->dragState;

	/* Only dragging of blocks of text is handled in this action proc.
	   Otherwise, defer to secondaryAdjust to handle the rest */
	if (dragState != CLICKED_IN_SELECTION && dragState != PRIMARY_BLOCK_DRAG) {
		secondaryAdjustAP(event, args, nArgs);
		return;
	}

	/* Decide whether the mouse has moved far enough from the
	   initial mouse down to be considered a drag */
	if (this->dragState == CLICKED_IN_SELECTION) {
		if (abs(e->x - this->btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - this->btnDownCoord.y) > SELECT_THRESHOLD) {
			this->BeginBlockDrag();
		} else {
			return;
		}
	}

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll({e->x, e->y});

	// Adjust the selection
	this->BlockDragSelection(
		Point{e->x, e->y}, 
		hasKey("overlay", args, nArgs) ? 
			(hasKey("copy", args, nArgs) ? DRAG_OVERLAY_COPY : DRAG_OVERLAY_MOVE) : 
			(hasKey("copy", args, nArgs) ? DRAG_COPY         : DRAG_MOVE));
}

/*
** Given a new mouse pointer location, pass the position on to the
** autoscroll timer routine, and make sure the timer is on when it's
** needed and off when it's not.
*/
void TextDisplay::checkAutoScroll(const Point &coord) {

	// Is the pointer in or out of the window?
	bool inWindow = 
		coord.x >= textD_of(w)->rect.left && 
		coord.x < w->core.width - text_of(w).P_marginWidth && 
		coord.y >= text_of(w).P_marginHeight && 
		coord.y < w->core.height - text_of(w).P_marginHeight;


	// If it's in the window, cancel the timer procedure
	if (inWindow) {
		if (this->autoScrollProcID != 0) {
			XtRemoveTimeOut(this->autoScrollProcID);
		}
		this->autoScrollProcID = 0;
		return;
	}

	// If the timer is not already started, start it
	if (this->autoScrollProcID == 0) {
		this->autoScrollProcID = XtAppAddTimeOut(XtWidgetToApplicationContext(w), 0, autoScrollTimerProc, w);
	}

	// Pass on the newest mouse location to the autoscroll routine
	this->setMouseCoord(coord);
}

void TextDisplay::adjustSecondarySelection(const Point &coord) {

	TextBuffer *buf = this->buffer;
	int row, col, startCol, endCol, startPos, endPos;
	int newPos = this->TextDXYToPosition(coord);

	if (this->dragState == SECONDARY_RECT_DRAG) {
		this->TextDXYToUnconstrainedPosition(coord, &row, &col);
		col      = this->TextDOffsetWrappedColumn(row, col);
		startCol = std::min(this->rectAnchor, col);
		endCol   = std::max(this->rectAnchor, col);
		startPos = buf->BufStartOfLine(std::min(this->getAnchor(), newPos));
		endPos   = buf->BufEndOfLine(std::max(this->getAnchor(), newPos));
		buf->BufSecRectSelect(startPos, endPos, startCol, endCol);
	} else {
		this->textBuffer()->BufSecondarySelect(this->getAnchor(), newPos);
	}
}

/*
** Xt timer procedure for autoscrolling
*/
static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	TextWidget w = static_cast<TextWidget>(clientData);
	TextDisplay *textD = textD_of(w);
	int topLineNum;
	int horizOffset;
	int cursorX;
	int y;
	int fontWidth    = textD->fontStruct->max_bounds.width;
	int fontHeight   = textD->fontStruct->ascent + textD->fontStruct->descent;
	Point mouseCoord = textD->getMouseCoord();

	/* For vertical autoscrolling just dragging the mouse outside of the top
	   or bottom of the window is sufficient, for horizontal (non-rectangular)
	   scrolling, see if the position where the CURSOR would go is outside */
	int newPos = textD->TextDXYToPosition(mouseCoord);
	
	if (textD->getDragState() == PRIMARY_RECT_DRAG) {
		cursorX = mouseCoord.x;
	} else if (!textD->TextDPositionToXY(newPos, &cursorX, &y)) {
		cursorX = mouseCoord.x;
	}

	/* Scroll away from the pointer, 1 character (horizontal), or 1 character
	   for each fontHeight distance from the mouse to the text (vertical) */
	textD->TextDGetScroll(&topLineNum, &horizOffset);
	
	if (cursorX >= (int)w->core.width - text_of(w).P_marginWidth) {
		horizOffset += fontWidth;
	} else if (mouseCoord.x < textD->getRect().left) {
		horizOffset -= fontWidth;
	}
		
	if (mouseCoord.y >= (int)w->core.height - text_of(w).P_marginHeight) {
		topLineNum += 1 + ((mouseCoord.y - (int)w->core.height - text_of(w).P_marginHeight) / fontHeight) + 1;
	} else if (mouseCoord.y < text_of(w).P_marginHeight) {
		topLineNum -= 1 + ((text_of(w).P_marginHeight - mouseCoord.y) / fontHeight);
	}
	
	textD->TextDSetScroll(topLineNum, horizOffset);

	/* Continue the drag operation in progress.  If none is in progress
	   (safety check) don't continue to re-establish the timer proc */
	switch(textD->getDragState()) {
	case PRIMARY_DRAG:
		textD->adjustSelection(mouseCoord);
		break;
	case PRIMARY_RECT_DRAG:
		textD->adjustSelection(mouseCoord);
		break;
	case SECONDARY_DRAG:
		textD->adjustSecondarySelection(mouseCoord);
		break;
	case SECONDARY_RECT_DRAG:
		textD->adjustSecondarySelection(mouseCoord);
		break;
	case PRIMARY_BLOCK_DRAG:
		textD->BlockDragSelection(mouseCoord, USE_LAST);
		break;
	default:
		textD->setAutoScrollProcID(0);
		return;
	}
		   
	// re-establish the timer proc (this routine) to continue processing
	const XtIntervalId autoScrollID = XtAppAddTimeOut(
		XtWidgetToApplicationContext(
		(Widget)w), 
		mouseCoord.y >= text_of(w).P_marginHeight && mouseCoord.y < w->core.height - text_of(w).P_marginHeight ? (VERTICAL_SCROLL_DELAY * fontWidth) / fontHeight : VERTICAL_SCROLL_DELAY,
		autoScrollTimerProc, w);
		
	textD->setAutoScrollProcID(autoScrollID);
}

/*
** Adjust the selection as the mouse is dragged to position: (x, y).
*/
void TextDisplay::adjustSelection(const Point &coord) {

	TextBuffer *buf = this->buffer;
	int row;
	int col;
	int startCol;
	int endCol;
	int startPos;
	int endPos;
	int newPos = this->TextDXYToPosition(coord);

	// Adjust the selection
	if (this->dragState == PRIMARY_RECT_DRAG) {
	
		this->TextDXYToUnconstrainedPosition(coord, &row, &col);
		col      = this->TextDOffsetWrappedColumn(row, col);
		startCol = std::min(this->rectAnchor, col);
		endCol   = std::max(this->rectAnchor, col);
		startPos = buf->BufStartOfLine(std::min(this->getAnchor(), newPos));
		endPos   = buf->BufEndOfLine(std::max(this->getAnchor(), newPos));
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
		
	} else if (this->multiClickState == ONE_CLICK) {
		startPos = this->startOfWord(std::min(this->getAnchor(), newPos));
		endPos   = this->endOfWord(std::max(this->getAnchor(), newPos));
		buf->BufSelect(startPos, endPos);
		newPos   = newPos < this->getAnchor() ? startPos : endPos;
		
	} else if (this->multiClickState == TWO_CLICKS) {
		startPos = buf->BufStartOfLine(std::min(this->getAnchor(), newPos));
		endPos   = buf->BufEndOfLine(std::max(this->getAnchor(), newPos));
		buf->BufSelect(startPos, std::min(endPos + 1, buf->BufGetLength()));
		newPos   = newPos < this->getAnchor() ? startPos : endPos;
	} else
		buf->BufSelect(this->getAnchor(), newPos);

	// Move the cursor
	this->TextDSetInsertPosition(newPos);
	this->callCursorMovementCBs(nullptr);
}

void TextDisplay::secondaryStartAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XMotionEvent *e = &event->xmotion;
	TextBuffer *buf = this->buffer;
	TextSelection *sel = &buf->secondary_;
	int anchor, pos, row, column;

	// Find the new anchor point and make the new selection
	pos = this->TextDXYToPosition(Point{e->x, e->y});
	if (sel->selected) {
		if (abs(pos - sel->start) < abs(pos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		buf->BufSecondarySelect(anchor, pos);
	} else {
		anchor = pos;
	}

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   selection, (and where the selection began) */
	this->btnDownCoord = Point{e->x, e->y};
	this->anchor       = pos;

	this->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);

	column = this->TextDOffsetWrappedColumn(row, column);
	this->rectAnchor = column;
	this->dragState = SECONDARY_CLICKED;
}

void TextDisplay::extendStartAP(XEvent *event, String *args, Cardinal *nArgs) {
	XMotionEvent *e = &event->xmotion;

	TextBuffer *buf = this->buffer;
	TextSelection *sel = &buf->primary_;
	int anchor, rectAnchor, anchorLineStart, newPos, row, column;

	// Find the new anchor point for the rest of this drag operation
	newPos = this->TextDXYToPosition(Point{e->x, e->y});
	this->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);
	column = this->TextDOffsetWrappedColumn(row, column);
	if (sel->selected) {
		if (sel->rectangular) {
			rectAnchor = column < (sel->rectEnd + sel->rectStart) / 2 ? sel->rectEnd : sel->rectStart;
			anchorLineStart = buf->BufStartOfLine(newPos < (sel->end + sel->start) / 2 ? sel->end : sel->start);
			anchor = buf->BufCountForwardDispChars(anchorLineStart, rectAnchor);
		} else {
			if (abs(newPos - sel->start) < abs(newPos - sel->end))
				anchor = sel->end;
			else
				anchor = sel->start;
			anchorLineStart = buf->BufStartOfLine(anchor);
			rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
		}
	} else {
		anchor = this->TextDGetInsertPosition();
		anchorLineStart = buf->BufStartOfLine(anchor);
		rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
	}
	this->anchor = anchor;
	this->rectAnchor = rectAnchor;

	// Make the new selection
	if (hasKey("rect", args, nArgs))
		buf->BufRectSelect(buf->BufStartOfLine(std::min(anchor, newPos)), buf->BufEndOfLine(std::max(anchor, newPos)), std::min(rectAnchor, column), std::max(rectAnchor, column));
	else
		buf->BufSelect(std::min(anchor, newPos), std::max(anchor, newPos));

	/* Never mind the motion threshold, go right to dragging since
	   extend-start is unambiguously the start of a selection */
	this->dragState = PRIMARY_DRAG;

	// Don't do by-word or by-line adjustment, just by character
	this->multiClickState = NO_CLICKS;

	// Move the cursor
	this->TextDSetInsertPosition(newPos);
	this->callCursorMovementCBs(event);
}

void TextDisplay::extendAdjustAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	XMotionEvent *e = &event->xmotion;
	int dragState = this->dragState;
	bool rectDrag = hasKey("rect", args, nArgs);

	// Make sure the proper initialization was done on mouse down
	if (dragState != PRIMARY_DRAG && dragState != PRIMARY_CLICKED && dragState != PRIMARY_RECT_DRAG)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (this->dragState == PRIMARY_CLICKED) {
		if (abs(e->x - this->btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - this->btnDownCoord.y) > SELECT_THRESHOLD)
			this->dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
		else
			return;
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	this->dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll({e->x, e->y});

	// Adjust the selection and move the cursor
	adjustSelection({e->x, e->y});
}

void TextDisplay::grabFocusAP(XEvent *event, String *args, Cardinal *nArgs) {

	XButtonEvent *e = &event->xbutton;
	Time lastBtnDown = this->lastBtnDown;
	int row;
	int column;

	/* Indicate state for future events, PRIMARY_CLICKED indicates that
	   the proper initialization has been done for primary dragging and/or
	   multi-clicking.  Also record the timestamp for multi-click processing */
	this->dragState = PRIMARY_CLICKED;
	this->lastBtnDown = e->time;

	/* Become owner of the MOTIF_DESTINATION selection, making this widget
	   the designated recipient of secondary quick actions in Motif XmText
	   widgets and in other NEdit text widgets */
	this->TakeMotifDestination(e->time);

	// Check for possible multi-click sequence in progress
	if (this->multiClickState != NO_CLICKS) {
		if (e->time < lastBtnDown + XtGetMultiClickTime(XtDisplay(w))) {
			if (this->multiClickState == ONE_CLICK) {
				selectWord(e->x);
				this->callCursorMovementCBs(event);
				return;
			} else if (this->multiClickState == TWO_CLICKS) {
				selectLine();
				this->callCursorMovementCBs(event);
				return;
			} else if (this->multiClickState == THREE_CLICKS) {
				this->textBuffer()->BufSelect(0, this->textBuffer()->BufGetLength());
				return;
			} else if (this->multiClickState > THREE_CLICKS)
				this->multiClickState = NO_CLICKS;
		} else
			this->multiClickState = NO_CLICKS;
	}

	// Clear any existing selections
	this->textBuffer()->BufUnselect();

	// Move the cursor to the pointer location
	moveDestinationAP(event, args, nArgs);

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events and clicking can decide when and
	   where to begin a primary selection */
	this->btnDownCoord = Point{e->x, e->y};
	this->anchor = this->TextDGetInsertPosition();
	this->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);
	column = this->TextDOffsetWrappedColumn(row, column);
	this->rectAnchor = column;
}

void TextDisplay::moveDestinationAP(XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	XButtonEvent *e = &event->xbutton;

	// Get input focus
	XmProcessTraversal(w, XmTRAVERSE_CURRENT);

	// Move the cursor
	this->TextDSetInsertPosition(this->TextDXYToPosition(Point{e->x, e->y}));
	this->checkAutoShowInsertPos();
	this->callCursorMovementCBs(event);
}

/*
** Select the word or whitespace adjacent to the cursor, and move the cursor
** to its end.  pointerX is used as a tie-breaker, when the cursor is at the
** boundary between a word and some white-space.  If the cursor is on the
** left, the word or space on the left is used.  If it's on the right, that
** is used instead.
*/
void TextDisplay::selectWord(int pointerX) {

	TextBuffer *buf = this->buffer;
	int x;
	int y;
	int insertPos = this->TextDGetInsertPosition();

	this->TextDPositionToXY(insertPos, &x, &y);
	
	if (pointerX < x && insertPos > 0 && buf->BufGetCharacter(insertPos - 1) != '\n') {
		insertPos--;
	}
	
	buf->BufSelect(startOfWord(insertPos), endOfWord(insertPos));
}

/*
** Select the line containing the cursor, including the terminating newline,
** and move the cursor to its end.
*/
void TextDisplay::selectLine() {

	int insertPos = this->TextDGetInsertPosition();
	int endPos;
	int startPos;

	endPos = this->textBuffer()->BufEndOfLine(insertPos);
	startPos = this->textBuffer()->BufStartOfLine(insertPos);
	this->textBuffer()->BufSelect(startPos, std::min(endPos + 1, this->textBuffer()->BufGetLength()));
	this->TextDSetInsertPosition(endPos);
}

/*
** Send an INSERT_SELECTION request to "sel".
** Upon completion, do the action specified by "action" (one of enum
** selectNotifyActions) using "actionText" and freeing actionText (if
** not nullptr) when done.
*/
void TextDisplay::sendSecondary(Time time, Atom sel, int action, char *actionText, int actionTextLen) {

	static Atom selInfoProp[2] = {XA_SECONDARY, XA_STRING};
	Display *disp = XtDisplay(w);
	XtAppContext context = XtWidgetToApplicationContext(w);

	// Take ownership of the secondary selection, give up if we can't 
	if (!XtOwnSelection(w, XA_SECONDARY, time, convertSecondaryCB, loseSecondaryCB, nullptr)) {
		this->buffer->BufSecondaryUnselect();
		return;
	}

	/* Set up a property on this window to pass along with the
	   INSERT_SELECTION request to tell the MOTIF_DESTINATION owner what
	   selection and what target from that selection to insert */
	XChangeProperty(disp, XtWindow(w), getAtom(disp, A_INSERT_INFO), getAtom(disp, A_ATOM_PAIR), 32, PropModeReplace, (uint8_t *)selInfoProp, 2 /* 1? */);

	/* Make INSERT_SELECTION request to the owner of selection "sel"
	   to do the insert.  This must be done using XLib calls to specify
	   the property with the information about what to insert.  This
	   means it also requires an event handler to see if the request
	   succeeded or not, and a backup timer to clean up if the select
	   notify event is never returned */
	XConvertSelection(XtDisplay(w), sel, getAtom(disp, A_INSERT_SELECTION), getAtom(disp, A_INSERT_INFO), XtWindow(w), time);
	
	auto cbInfo = new selectNotifyInfo;
	cbInfo->action       = action;
	cbInfo->timeStamp    = time;
	cbInfo->widget       = (Widget)w;
	cbInfo->actionText   = actionText;
	cbInfo->length       = actionTextLen;
	XtAddEventHandler(w, 0, true, selectNotifyEH, cbInfo);
	cbInfo->timeoutProcID = XtAppAddTimeOut(context, XtAppGetSelectionTimeout(context), selectNotifyTimerProc, cbInfo);
}

CursorStyles TextDisplay::getCursorStyle() const {
	return this->cursorStyle;
}

int TextDisplay::getCursorToHint() const {
	return this->cursorToHint;
}

bool TextDisplay::getSelectionOwner() {
	return this->selectionOwner;
}

void TextDisplay::setSelectionOwner(bool value) {
	this->selectionOwner = value;
}

int TextDisplay::getDragState() const {
	return this->dragState;
}

void TextDisplay::setCursorToHint(int value) {
	this->cursorToHint = value;
}

int TextDisplay::getTopLineNum() const {
	return this->topLineNum;
}

Point TextDisplay::getButtonDownCoord() const {
	return this->btnDownCoord;
}

int TextDisplay::getHorizOffset() const {
	return this->horizOffset;
}

int TextDisplay::getCursorPreferredCol() const {
	return this->cursorPreferredCol;
}

void TextDisplay::setCursorPreferredCol(int value) {
	this->cursorPreferredCol = value;
}

void TextDisplay::setSuppressResync(bool value) {
	this->suppressResync = value;
}

void TextDisplay::setVisibility(int value) {
	this->visibility = value;
}

int TextDisplay::getFixedFontWidth() const {
	return this->fixedFontWidth;
}

void TextDisplay::setNumberBufferLines(int count) {
	this->nBufferLines = count;
}

int TextDisplay::getNumberBufferLines() const {
	return this->nBufferLines;
}

void TextDisplay::setMotifDestOwner(bool value) {
	this->motifDestOwner = value;
}

void TextDisplay::setAutoScrollProcID(XtIntervalId id) {
	this->autoScrollProcID = id;
}

void TextDisplay::setAbsTopLineNum(int value) {
	this->absTopLineNum = value;
}
