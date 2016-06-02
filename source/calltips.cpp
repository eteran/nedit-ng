/*******************************************************************************
*                                                                              *
* calltips.c -- Calltip UI functions  (calltip *file* functions are in tags.c) *
*                                                                              *
* Copyright (C) 2002 Nathaniel Gray                                            *
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
* April, 1997                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>

#include "text.h"
#include "textP.h"
#include "MotifHelper.h"
#include "calltips.h"
#include "TextDisplay.h"
#include "Document.h"
#include "TextHelper.h"
#include "misc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <Xm/Label.h>

static std::string expandAllTabsEx(view::string_view text, int tab_width);

/*
** Pop-down a calltip if one exists, else do nothing
*/
void KillCalltip(Document *window, int calltipID) {
	TextDisplay *textD = textD_of(window->lastFocus_);
	textD->TextDKillCalltip(calltipID);
}

/*
** Is a calltip displayed?  Returns the calltip ID of the currently displayed
** calltip, or 0 if there is no calltip displayed.  If called with
** calltipID != 0, returns 0 unless there is a calltip being
** displayed with that calltipID.
*/
int GetCalltipID(Document *window, int calltipID) {
	TextDisplay *textD = textD_of(window->lastFocus_);
	if (calltipID == 0)
		return textD->calltip.ID;
	else {
		if (calltipID == textD->calltip.ID)
			return calltipID;
		else
			return 0;
	}
}

/*
** Returns a new string with each \t replaced with tab_width spaces or
** a pointer to text if there were no tabs.
** Note that this is dumb replacement, not smart tab-like behavior!  The goal
** is to prevent tabs from turning into squares in calltips, not to get the
** formatting just right.
*/
std::string expandAllTabsEx(view::string_view text, int tab_width) {
	int nTabs = 0;

	// First count 'em
	for(char ch : text) {
		if (ch == '\t') {
			++nTabs;
		}
	}
	
	if (nTabs == 0) {
		return text.to_string();
	}

	// Allocate the new string
	size_t len = text.size() + (tab_width - 1) * nTabs;
	
	std::string textCpy;
	textCpy.reserve(len);
	
	auto cCpy = std::back_inserter(textCpy);
	
	// Now replace 'em
	for(char ch : text) {
		if (ch == '\t') {
			for (int i = 0; i < tab_width; ++i) {
				*cCpy++ = ' ';
			}
		} else {
			*cCpy++ = ch;
		}
	}
	
	
	return textCpy;
}

/*
** Pop-up a calltip.
** If a calltip is already being displayed it is destroyed and replaced with
** the new calltip.  Returns the ID of the calltip or 0 on failure.
*/
int ShowCalltip(Document *window, view::string_view text, bool anchored, int pos, int hAlign, int vAlign, int alignMode) {
	static int StaticCalltipID = 1;
	auto textD = textD_of(window->lastFocus_);
	int rel_x, rel_y;
	Position txtX, txtY;

	// Destroy any previous calltip 
	textD->TextDKillCalltip(0);

	// Expand any tabs in the calltip and make it an XmString 
	std::string textCpy = expandAllTabsEx(text, textD->buffer->BufGetTabDistance());

	XmString str = XmStringCreateLtoREx(textCpy, XmFONTLIST_DEFAULT_TAG);

	// Get the location/dimensions of the text area 
	XtVaGetValues(textD->w, XmNx, &txtX, XmNy, &txtY, nullptr);

	// Create the calltip widget on first request 
	if (textD->calltipW == nullptr) {
		Arg args[10];
		int argcnt = 0;
		XtSetArg(args[argcnt], XmNsaveUnder, True);
		argcnt++;
		XtSetArg(args[argcnt], XmNallowShellResize, True);
		argcnt++;

		textD->calltipShell = CreatePopupShellWithBestVis((String) "calltipshell", overrideShellWidgetClass, textD->w, args, argcnt);

		/* Might want to make this a read-only XmText eventually so that
		    users can copy from it */
		textD->calltipW = XtVaCreateManagedWidget("calltip", xmLabelWidgetClass, textD->calltipShell, XmNborderWidth, 1, // Thin borders 
		                                          XmNhighlightThickness, 0, XmNalignment, XmALIGNMENT_BEGINNING, XmNforeground, textD->calltipFGPixel, XmNbackground, textD->calltipBGPixel, nullptr);
	}

	// Set the text on the label 
	XtVaSetValues(textD->calltipW, XmNlabelString, str, nullptr);
	XmStringFree(str);

	// Figure out where to put the tip 
	if (anchored) {
		// Put it at the specified position 
		// If position is not displayed, return 0 
		if (pos < textD->firstChar || pos > textD->lastChar) {
			QApplication::beep();
			return 0;
		}
		textD->calltip.pos = pos;
	} else {
		/* Put it next to the cursor, or in the center of the window if the
		    cursor is offscreen and mode != strict */
		if (!textD->TextDPositionToXY(textD->cursorPos, &rel_x, &rel_y)) {
			if (alignMode == TIP_STRICT) {
				QApplication::beep();
				return 0;
			}
			textD->calltip.pos = -1;
		} else
			// Store the x-offset for use when redrawing 
			textD->calltip.pos = rel_x;
	}

	// Should really bounds-check these enumerations... 
	textD->calltip.ID = StaticCalltipID;
	textD->calltip.anchored = anchored;
	textD->calltip.hAlign = hAlign;
	textD->calltip.vAlign = vAlign;
	textD->calltip.alignMode = alignMode;

	/* Increment the static calltip ID.  Macro variables can only be int,
	    not unsigned, so have to work to keep it > 0 on overflow */
	if (++StaticCalltipID <= 0)
		StaticCalltipID = 1;

	// Realize the calltip's shell so that its width & height are known 
	XtRealizeWidget(textD->calltipShell);
	// Move the calltip and pop it up 
	textD->TextDRedrawCalltip(0);
	XtPopup(textD->calltipShell, XtGrabNone);
	return textD->calltip.ID;
}
