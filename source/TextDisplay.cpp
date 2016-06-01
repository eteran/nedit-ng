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
#include "StyleTableEntry.h"
#include "TextDisplay.h"
#include "TextBuffer.h"
#include "text.h"
#include "textP.h"
#include "nedit.h"
#include "calltips.h"
#include "highlight.h"
#include "Rangeset.h"
#include "RangesetTable.h"
#include "textSel.h"
#include "textDrag.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <algorithm>

#include <Xm/Xm.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/Label.h>
#include <X11/Shell.h>

#define NEDIT_HIDE_CURSOR_MASK (KeyPressMask)
#define NEDIT_SHOW_CURSOR_MASK (FocusChangeMask | PointerMotionMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask)

// Macro for getting the TextPart from a textD
#define TEXT_OF_TEXTD(t) (reinterpret_cast<TextWidget>((t)->w)->text)

namespace {

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

}


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


	XGCValues gcValues;

	this->w = widget;
	this->top = top;
	this->left = left;
	this->width = width;
	this->height = height;
	this->cursorOn = true;
	this->cursorPos = 0;
	this->cursorX = -100;
	this->cursorY = -100;
	this->cursorToHint = NO_HINT;
	this->cursorStyle = NORMAL_CURSOR;
	this->cursorPreferredCol = -1;
	this->buffer = buffer;
	this->firstChar = 0;
	this->lastChar = 0;
	this->nBufferLines = 0;
	this->topLineNum = 1;
	this->absTopLineNum = 1;
	this->needAbsTopLineNum = false;
	this->horizOffset = 0;
	this->visibility = VisibilityUnobscured;
	this->hScrollBar = hScrollBar;
	this->vScrollBar = vScrollBar;
	this->fontStruct = fontStruct;
	this->ascent = fontStruct->ascent;
	this->descent = fontStruct->descent;
	this->fixedFontWidth = fontStruct->min_bounds.width == fontStruct->max_bounds.width ? fontStruct->min_bounds.width : -1;
	this->styleBuffer = nullptr;
	this->styleTable = nullptr;
	this->nStyles = 0;
	this->bgPixel = bgPixel;
	this->fgPixel = fgPixel;
	this->selectFGPixel = selectFGPixel;
	this->highlightFGPixel = highlightFGPixel;
	this->selectBGPixel = selectBGPixel;
	this->highlightBGPixel = highlightBGPixel;
	this->lineNumFGPixel = lineNumFGPixel;
	this->cursorFGPixel = cursorFGPixel;
	this->wrapMargin = wrapMargin;
	this->continuousWrap = continuousWrap;
	
	allocateFixedFontGCs(fontStruct, bgPixel, fgPixel, selectFGPixel, selectBGPixel, highlightFGPixel, highlightBGPixel, lineNumFGPixel);
	
	this->styleGC       = allocateGC(this->w, 0, 0, 0, fontStruct->fid, GCClipMask | GCForeground | GCBackground, GCArcMode);
	this->lineNumLeft   = lineNumLeft;
	this->lineNumWidth  = lineNumWidth;
	this->nVisibleLines = (height - 1) / (this->ascent + this->descent) + 1;
	
	gcValues.foreground = cursorFGPixel;
	
	this->cursorFGGC = XtGetGC(widget, GCForeground, &gcValues);
	this->lineStarts = new int[this->nVisibleLines];
	this->lineStarts[0] = 0;
	this->calltipW = nullptr;
	this->calltipShell = nullptr;
	this->calltip.ID = 0;
	this->calltipFGPixel = calltipFGPixel;
	this->calltipBGPixel = calltipBGPixel;
	for (int i = 1; i < this->nVisibleLines; i++) {
		this->lineStarts[i] = -1;
	}
	this->bgClassPixel = nullptr;
	this->bgClass = nullptr;
	TextDSetupBGClasses(widget, bgClassString, &this->bgClassPixel, &this->bgClass, bgPixel);

	this->suppressResync   = 0;
	this->nLinesDeleted    = 0;
	this->modifyingTabDist = 0;
	this->pointerHidden    = false;
	graphicsExposeQueue_   = nullptr;

	/* Attach an event handler to the widget so we can know the visibility
	   (used for choosing the fastest drawing method) */
	XtAddEventHandler(widget, VisibilityChangeMask, False, visibilityEH, this);

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
}

/*
** Free a text display and release its associated memory.  Note, the text
** BUFFER that the text display displays is a separate entity and is not
** freed, nor are the style buffer or style table.
*/
TextDisplay::~TextDisplay() {
	this->buffer->BufRemoveModifyCB(bufModifiedCB, this);
	this->buffer->BufRemovePreDeleteCB(bufPreDeleteCB, this);
	releaseGC(this->w, this->gc);
	releaseGC(this->w, this->selectGC);
	releaseGC(this->w, this->highlightGC);
	releaseGC(this->w, this->selectBGGC);
	releaseGC(this->w, this->highlightBGGC);
	releaseGC(this->w, this->styleGC);
	releaseGC(this->w, this->lineNumGC);

	delete[] this -> lineStarts;

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
	TextDRedisplayRect(this->left, this->top, this->width, this->height);
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
	if (this->height < maxAscent + maxDescent)
		this->height = maxAscent + maxDescent;

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
	width  = this->width;
	height = this->height;
	this->width  = 0;
	this->height = 0;
	TextDResize(width, height);

	/* if the shell window doesn't get resized, and the new fonts are
	   of smaller sizes, sometime we get some residual text on the
	   blank space at the bottom part of text area. Clear it here. */
	clearRect(this->gc, this->left, this->top + this->height - maxAscent - maxDescent, this->width, maxAscent + maxDescent);

	// Redisplay
	TextDRedisplayRect(this->left, this->top, this->width, this->height);

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
	int oldWidth = this->width;
	int exactHeight = height - height % (this->ascent + this->descent);

	this->width = width;
	this->height = height;

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
		XClearArea(XtDisplay(this->w), XtWindow(this->w), this->left, this->top + exactHeight, this->width, height - exactHeight, false);

	/* if the window became taller, there may be an opportunity to display
	   more text by scrolling down */
	if (canRedraw && oldVisibleLines < newVisibleLines && this->topLineNum + this->nVisibleLines > this->nBufferLines)
		setScroll(std::max<int>(1, this->nBufferLines - this->nVisibleLines + 2 + TEXT_OF_TEXTD(this).cursorVPadding), this->horizOffset, False, false);

	/* Update the scroll bar page increment size (as well as other scroll
	   bar parameters.  If updating the horizontal range caused scrolling,
	   redraw */
	updateVScrollBarRange();
	if (updateHScrollBarRange())
		redrawAll = true;

	// If a full redraw is needed
	if (redrawAll && canRedraw)
		TextDRedisplayRect(this->left, this->top, this->width, this->height);

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
void TextDisplay::TextDRedisplayRect(int left, int top, int width, int height) {
	int fontHeight, firstLine, lastLine, line;

	// find the line number range of the display
	fontHeight = this->ascent + this->descent;
	firstLine = (top - this->top - fontHeight + 1) / fontHeight;
	lastLine = (top + height - this->top) / fontHeight;

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
	int vPadding = (int)(TEXT_OF_TEXTD(this).cursorVPadding);

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

void TextDisplay::TextDSetCursorStyle(int style) {
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
	TextDRedisplayRect(0, this->top, this->width + this->left, this->height);
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
	*y = this->top + visLineNum * fontHeight + fontHeight / 2;

	/* Get the text, length, and  buffer position of the line. If the position
	   is beyond the end of the buffer and should be at the first position on
	   the first empty line, don't try to get or scan the text  */
	lineStartPos = this->lineStarts[visLineNum];
	if (lineStartPos == -1) {
		*x = this->left - this->horizOffset;
		return true;
	}
	lineLen = visLineLength(visLineNum);
	std::string lineStr = this->buffer->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);

	/* Step through character positions from the beginning of the line
	   to "pos" to calculate the x coordinate */
	xStep = this->left - this->horizOffset;
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
	int cursorVPadding = (int)TEXT_OF_TEXTD(this).cursorVPadding;

	hOffset = this->horizOffset;
	topLine = this->topLineNum;

	// Don't do padding if this is a mouse operation
	bool do_padding = ((TEXT_OF_TEXTD(this).dragState == NOT_CLICKED) && (cursorVPadding > 0));

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
			topLine = std::max<int>(topLine, 1);
		} else if (linesFromTop < (int)cursorVPadding) {
			topLine -= (cursorVPadding - linesFromTop);
			topLine = std::max<int>(topLine, 1);
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
	if (x > this->left + this->width)
		hOffset += x - (this->left + this->width);
	else if (x < this->left)
		hOffset += x - this->left;

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
		newPos = std::min<int>(newPos, TextDEndOfLine(lineStartPos, True));
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
		newPos = std::min<int>(newPos, TextDEndOfLine(prevLineStartPos, True));

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
		newPos = std::min<int>(newPos, TextDEndOfLine(nextLineStartPos, True));
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
	if (textD->continuousWrap && (textD->fixedFontWidth == -1 || textD->modifyingTabDist))
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
		textD->suppressResync = 0; // Probably not needed, but just in case
}

/*
** Callback attached to the text buffer to receive modification information
*/
static void bufModifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg) {
	int linesInserted, linesDeleted, startDispPos, endDispPos;
	auto textD = static_cast<TextDisplay *>(cbArg);
	TextBuffer *buf = textD->buffer;
	int oldFirstChar = textD->firstChar;
	int scrolled, origCursorPos = textD->cursorPos;
	int wrapModStart, wrapModEnd;

	// buffer modification cancels vertical cursor motion column
	if (nInserted != 0 || nDeleted != 0)
		textD->cursorPreferredCol = -1;

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
			textD->absTopLineNum += buf->BufCountLines(pos, pos + nInserted) - countLinesEx(deletedText);
		else if (pos < oldFirstChar)
			textD->resetAbsLineNum();
	}

	// Update the line count for the whole buffer
	textD->nBufferLines += linesInserted - linesDeleted;

	/* Update the scroll bar ranges (and value if the value changed).  Note
	   that updating the horizontal scroll bar range requires scanning the
	   entire displayed text, however, it doesn't seem to hurt performance
	   much.  Note also, that the horizontal scroll bar update routine is
	   allowed to re-adjust horizOffset if there is blank space to the right
	   of all lines of text. */
	textD->updateVScrollBarRange();
	scrolled |= textD->updateHScrollBarRange();

	// Update the cursor position
	if (textD->cursorToHint != NO_HINT) {
		textD->cursorPos = textD->cursorToHint;
		textD->cursorToHint = NO_HINT;
	} else if (textD->cursorPos > pos) {
		if (textD->cursorPos < pos + nDeleted)
			textD->cursorPos = pos;
		else
			textD->cursorPos += nInserted - nDeleted;
	}

	// If the changes caused scrolling, re-paint everything and we're done.
	if (scrolled) {
		textD->blankCursorProtrusions();
		textD->TextDRedisplayRect(0, textD->top, textD->width + textD->left, textD->height);
		if (textD->styleBuffer) { // See comments in extendRangeForStyleMods
			textD->styleBuffer->primary_.selected = false;
			textD->styleBuffer->primary_.zeroWidth = false;
		}
		return;
	}

	/* If the changes didn't cause scrolling, decide the range of characters
	   that need to be re-painted.  Also if the cursor position moved, be
	   sure that the redisplay range covers the old cursor position so the
	   old cursor gets erased, and erase the bits of the cursor which extend
	   beyond the left and right edges of the text. */
	startDispPos = textD->continuousWrap ? wrapModStart : pos;
	if (origCursorPos == startDispPos && textD->cursorPos != startDispPos)
		startDispPos = std::min<int>(startDispPos, origCursorPos - 1);
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
		endDispPos = textD->lastChar + 1;
		if (origCursorPos >= pos)
			textD->blankCursorProtrusions();
		textD->redrawLineNumbers(false);
	}

	/* If there is a style buffer, check if the modification caused additional
	   changes that need to be redisplayed.  (Redisplaying separately would
	   cause double-redraw on almost every modification involving styled
	   text).  Extend the redraw range to incorporate style changes */
	if (textD->styleBuffer) {
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
	if (!this->continuousWrap)
		return this->topLineNum;
	if (maintainingAbsTopLineNum())
		return this->absTopLineNum;
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
				posToVisibleLineNum(std::max<int>(this->lastChar - 1, 0), lineNum);
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
	leftClip = std::max<int>(this->left, leftClip);
	rightClip = std::min<int>(rightClip, this->left + this->width);

	if (leftClip > rightClip) {
		return;
	}

	// Calculate y coordinate of the string to draw
	fontHeight = this->ascent + this->descent;
	y = this->top + visLineNum * fontHeight;

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
	x = this->left - this->horizOffset;
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
	y_orig = this->cursorY;
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
	if (hasCursor && (y_orig != this->cursorY || y_orig != y))
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
		if (toX >= this->left) {
			clearRect(bgGC, std::max<int>(x, this->left), y, toX - std::max<int>(x, this->left), this->ascent + this->descent);
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

	if (XtWindow(this->w) == 0 || x < this->left - 1 || x > this->left + this->width)
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
	this->cursorX = x;
	this->cursorY = y;
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

	pos = lineStartPos + std::min<int>(lineIndex, lineLen);

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
	visLineNum = (y - this->top) / fontHeight;
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
	xStep = this->left - this->horizOffset;
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
	*row = (y - this->top) / fontHeight;
	if (*row < 0)
		*row = 0;
	if (*row >= this->nVisibleLines)
		*row = this->nVisibleLines - 1;
	*column = ((x - this->left) + this->horizOffset + (posType == CURSOR_POS ? fontWidth / 2 : 0)) / fontWidth;
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
			this->topLineNum = std::max<int>(1, this->topLineNum + lineDelta);
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
			for (i = std::max<int>(0, lineOfPos + 1); i < nVisLines + lineDelta; i++)
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
	int exactHeight = this->height - this->height % (this->ascent + this->descent);

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
	if (this->visibility != VisibilityUnobscured || abs(xOffset) > this->width || abs(yOffset) > exactHeight) {
		TextDTranlateGraphicExposeQueue(xOffset, yOffset, false);
		TextDRedisplayRect(this->left, this->top, this->width, this->height);
	} else {
		/* If the window is not obscured, paint most of the window using XCopyArea
		   from existing displayed text, and redraw only what's necessary */
		// Recover the useable window areas by moving to the proper location
		int srcX   = this->left + (xOffset >= 0 ? 0 : -xOffset);
		int dstX   = this->left + (xOffset >= 0 ? xOffset : 0);
		int width  = this->width - abs(xOffset);
		int srcY   = this->top + (yOffset >= 0 ? 0 : -yOffset);
		int dstY   = this->top + (yOffset >= 0 ? yOffset : 0);
		int height = exactHeight - abs(yOffset);
		resetClipRectangles();
		TextDTranlateGraphicExposeQueue(xOffset, yOffset, true);
		XCopyArea(XtDisplay(this->w), XtWindow(this->w), XtWindow(this->w), this->gc, srcX, srcY, width, height, dstX, dstY);
		// redraw the un-recoverable parts
		if (yOffset > 0) {
			TextDRedisplayRect(this->left, this->top, this->width, yOffset);
		} else if (yOffset < 0) {
			TextDRedisplayRect(this->left, this->top + this->height + yOffset, this->width, -yOffset);
		}
		if (xOffset > 0) {
			TextDRedisplayRect(this->left, this->top, xOffset, this->height);
		} else if (xOffset < 0) {
			TextDRedisplayRect(this->left + this->width + xOffset, this->top, -xOffset, this->height);
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

	if (!this->vScrollBar)
		return;

	/* The Vert. scroll bar value and slider size directly represent the top
	   line number, and the number of visible lines respectively.  The scroll
	   bar maximum value is chosen to generally represent the size of the whole
	   buffer, with minor adjustments to keep the scroll bar widget happy */
	sliderSize = std::max<int>(this->nVisibleLines, 1); // Avoid X warning (size < 1)
	sliderValue = this->topLineNum;
	sliderMax = std::max<int>(this->nBufferLines + 2 + TEXT_OF_TEXTD(this).cursorVPadding, sliderSize + sliderValue);
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
		maxWidth = std::max<int>(this->measureVisLine(i), maxWidth);

	/* If the scroll position is beyond what's necessary to keep all lines
	   in view, scroll to the left to bring the end of the longest line to
	   the right margin */
	if (maxWidth < this->width + this->horizOffset && this->horizOffset > 0)
		this->horizOffset = std::max<int>(0, maxWidth - this->width);

	// Readjust the scroll bar
	sliderWidth = this->width;
	sliderMax = std::max<int>(maxWidth, sliderWidth + this->horizOffset);
	XtVaSetValues(this->hScrollBar, XmNmaximum, sliderMax, XmNsliderSize, sliderWidth, XmNpageIncrement, std::max<int>(this->width - 100, 10), XmNvalue, this->horizOffset, nullptr);

	// Return True if scroll position was changed
	return origHOffset != this->horizOffset;
}

/*
** Define area for drawing line numbers.  A width of 0 disables line
** number drawing.
*/
void TextDisplay::TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft) {
	int newWidth = this->width + this->left - textLeft;
	this->lineNumLeft = lineNumLeft;
	this->lineNumWidth = lineNumWidth;
	this->left = textLeft;
	XClearWindow(XtDisplay(this->w), XtWindow(this->w));
	resetAbsLineNum();
	TextDResize(newWidth, this->height);
	TextDRedisplayRect(0, this->top, INT_MAX, this->height);
}

/*
** Refresh the line number area.  If clearAll is False, writes only over
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
	clipRect.y = this->top;
	clipRect.width = this->lineNumWidth;
	clipRect.height = this->height;
	XSetClipRectangles(display, this->lineNumGC, 0, 0, &clipRect, 1, Unsorted);

	// Erase the previous contents of the line number area, if requested
	if (clearAll)
		XClearArea(XtDisplay(this->w), XtWindow(this->w), this->lineNumLeft, this->top, this->lineNumWidth, this->height, false);

	// Draw the line numbers, aligned to the text
	nCols = std::min<int>(11, this->lineNumWidth / charWidth);
	y = this->top;
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

/*
** Callbacks for drag or valueChanged on scroll bars
*/
static void vScrollCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;

	auto textD = static_cast<TextDisplay *>(clientData);
	int newValue = ((XmScrollBarCallbackStruct *)callData)->value;
	int lineDelta = newValue - textD->topLineNum;

	if (lineDelta == 0)
		return;
	textD->setScroll(newValue, textD->horizOffset, False, true);
}

static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;

	auto textD = static_cast<TextDisplay *>(clientData);
	int newValue = ((XmScrollBarCallbackStruct *)callData)->value;

	if (newValue == textD->horizOffset)
		return;
	textD->setScroll(textD->topLineNum, newValue, False, false);
}

static void visibilityEH(Widget w, XtPointer data, XEvent *event, Boolean *continueDispatch) {
	(void)w;
	(void)continueDispatch;

	/* Record whether the window is fully visible or not.  This information
	   is used for choosing the scrolling methodology for optimal performance,
	   if the window is partially obscured, XCopyArea may not work */


	static_cast<TextDisplay *>(data)->visibility = reinterpret_cast<XVisibilityEvent *>(event)->state;
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
	int x, width, cursorX = this->cursorX, cursorY = this->cursorY;
	int fontWidth = this->fontStruct->max_bounds.width;
	int fontHeight = this->ascent + this->descent;
	int cursorWidth, left = this->left, right = left + this->width;

	cursorWidth = (fontWidth / 3) * 2;
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

	clipRect.x = this->left;
	clipRect.y = this->top;
	clipRect.width = this->width;
	clipRect.height = this->height - this->height % (this->ascent + this->descent);

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
					*modRangeStart = std::min<int>(pos, lineStarts[visLineNum + 1] - 1);
				else
					*modRangeStart = countFrom;
			} else
				*modRangeStart = std::min<int>(*modRangeStart, lineStart - 1);
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
		this->suppressResync = 0;
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
	this->suppressResync = 0;
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
	this->suppressResync = 1;
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
		wrapMargin = this->wrapMargin != 0 ? this->wrapMargin : this->width / this->fixedFontWidth;
		maxWidth = INT_MAX;
	} else {
		countPixels = true;
		wrapMargin = INT_MAX;
		maxWidth = this->width;
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
				newLineStart = std::max<int>(p, lineStart + 1);
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
	if (this->continuousWrap && (this->wrapMargin == 0 || this->wrapMargin * this->fontStruct->max_bounds.width < this->width))
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

	if (!TEXT_OF_TEXTD(this).continuousWrap || startPos == endPos) {
		return buf->BufGetRangeEx(startPos, endPos);
	}

	/* Create a text buffer with a good estimate of the size that adding
	   newlines will expand it to.  Since it's a text buffer, if we guess
	   wrong, it will fail softly, and simply expand the size */
	auto outBuf = new TextBuffer((endPos - startPos) + (endPos - startPos) / 5);
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
	delete outBuf;
	return outString;
}

void TextDisplay::TextCopyClipboard(Time time) {
	cancelDrag();

	if (!this->buffer->primary_.selected) {
		QApplication::beep();
		return;
	}

	CopyToClipboard(w, time);
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

	TakeMotifDestination(w, time);
	CopyToClipboard(w, time);
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

	StopHandlingXSelections(w);
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
	return this->width;
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
	if (!allowWrap || !tw->text.autoWrap || (chars[0] == '\n' && chars[1] == '\0')) {
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
	wrapMargin = tw->text.wrapMargin != 0 ? tw->text.wrapMargin : this->width / fontWidth;
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
	} else if (tw->text.overstrike) {
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
	} else if (TEXT_OF_TEXTD(this).overstrike) {

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

	return TEXT_OF_TEXTD(this).pendingDelete && sel->selected && pos >= sel->start && pos <= sel->end;
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
	int length;
	int column;
	
	/* Scan backward for whitespace or BOL.  If BOL, return False, no
	   whitespace in line at which to wrap */
	for (p = lineEndPos;; p--) {
		if (p < lineStartPos || p < limitPos) {
			return False;
		}

		char c = buf->BufGetCharacter(p);
		if (c == '\t' || c == ' ')
			break;
	}

	/* Create an auto-indent string to insert to do wrap.  If the auto
	   indent string reaches the wrap position, slice the auto-indent
	   back off and return to the left margin */
	std::string indentStr;
	if (tw->text.autoIndent || tw->text.smartIndent) {
		indentStr = createIndentStringEx(buf, bufOffset, lineStartPos, lineEndPos, &length, &column);
		if (column >= p - lineStartPos) {
			indentStr.resize(1);
		}
	} else {
		indentStr = "\n";
		length = 1;
	}

	/* Replace the whitespace character with the auto-indent string
	   and return the stats */
	buf->BufReplaceEx(p, p + 1, indentStr);

	*breakAt = p;
	*charsAdded = length - 1;
	return True;
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


	auto tw = reinterpret_cast<TextWidget>(w);
	int indent = -1, tabDist = this->buffer->tabDist_;
	int i, useTabs = this->buffer->useTabs_;
	smartIndentCBStruct smartIndent;

	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (tw->text.smartIndent && (lineStartPos == 0 || buf == this->buffer)) {
		smartIndent.reason = NEWLINE_INDENT_NEEDED;
		smartIndent.pos = lineEndPos + bufOffset;
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped = nullptr;
		XtCallCallbacks((Widget)tw, textNsmartIndentCallback, &smartIndent);
		indent = smartIndent.indentRequest;
	}

	// If smart indent wasn't used, measure the indent distance of the line
	if (indent == -1) {
		indent = 0;
		for (int pos = lineStartPos; pos < lineEndPos; pos++) {
			char c = buf->BufGetCharacter(pos);
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
		for (i = 0; i < indent / tabDist; i++)
			*indentPtr++ = '\t';
		for (i = 0; i < indent % tabDist; i++)
			*indentPtr++ = ' ';
	} else {
		for (i = 0; i < indent; i++)
			*indentPtr++ = ' ';
	}

	// Return any requested stats
	if(length)
		*length = indentStr.size();
	if(column)
		*column = indent;

	return indentStr;
}


/*
**  Sets the caret to on or off and restart the caret blink timer.
**  This could be used by other modules to modify the caret's blinking.
*/
void TextDisplay::ResetCursorBlink(bool startsBlanked) {

	auto tw = reinterpret_cast<TextWidget>(w);

	if (tw->text.cursorBlinkRate != 0) {
		if (tw->text.cursorBlinkProcID != 0) {
			XtRemoveTimeOut(tw->text.cursorBlinkProcID);
		}

		if (startsBlanked) {
			TextDBlankCursor();
		} else {
			TextDUnblankCursor();
		}

		tw->text.cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)tw), tw->text.cursorBlinkRate, cursorBlinkTimerProc, tw);
	}
}

/*
** Xt timer procedure for cursor blinking
*/
void TextDisplay::cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	TextWidget w = static_cast<TextWidget>(clientData);
	TextDisplay *textD = w->text.textD;

	// Blink the cursor
	if (textD->cursorOn)
		textD->TextDBlankCursor();
	else
		textD->TextDUnblankCursor();

	// re-establish the timer proc (this routine) to continue processing
	w->text.cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), w->text.cursorBlinkRate, cursorBlinkTimerProc, w);
}


void TextDisplay::ShowHidePointer(bool hidePointer) {

	auto tw = reinterpret_cast<TextWidget>(w);

	if (tw->text.hidePointer) {
		if (hidePointer != tw->text.textD->pointerHidden) {
			if (hidePointer) {
				// Don't listen for keypresses any more
				XtRemoveEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, False, handleHidePointer, nullptr);
				// Switch to empty cursor
				XDefineCursor(XtDisplay(w), XtWindow(w), empty_cursor);

				tw->text.textD->pointerHidden = true;

				// Listen to mouse movement, focus change, and button presses
				XtAddEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK, False, handleShowPointer, nullptr);
			} else {
				// Don't listen to mouse/focus events any more
				XtRemoveEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK, False, handleShowPointer, nullptr);
				// Switch to regular cursor
				XUndefineCursor(XtDisplay(w), XtWindow(w));

				tw->text.textD->pointerHidden = false;

				// Listen for keypresses now
				XtAddEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, False, handleHidePointer, nullptr);
			}
		}
	}
}

// Hide the pointer while the user is typing
void TextDisplay::handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	auto textD = reinterpret_cast<TextWidget>(w)->text.textD;
	textD->ShowHidePointer(true);
}


// Restore the pointer if the mouse moves or focus changes
void TextDisplay::handleShowPointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	auto textD = reinterpret_cast<TextWidget>(w)->text.textD;
	textD->ShowHidePointer(false);
}


void TextDisplay::TextPasteClipboard(Time time) {
	cancelDrag();
	if (checkReadOnly())
		return;
	TakeMotifDestination(w, time);
	InsertClipboard(w, false);
	callCursorMovementCBs(nullptr);
}

void TextDisplay::TextColPasteClipboard(Time time) {
	cancelDrag();
	if (checkReadOnly())
		return;
	TakeMotifDestination(w, time);
	InsertClipboard(w, true);
	callCursorMovementCBs(nullptr);
}


/*
** Set this widget to be the owner of selections made in it's attached
** buffer (text buffers may be shared among several text widgets).
*/
void TextDisplay::TextHandleXSelections() {
	HandleXSelections(w);
}


bool TextDisplay::checkReadOnly() const {
	if (TEXT_OF_TEXTD(this).readOnly) {
		QApplication::beep();
		return True;
	}
	return False;
}


/*
** Cancel any drag operation that might be in progress.  Should be included
** in nearly every key event to cleanly end any dragging before edits are made
** which might change the insert position or the content of the buffer during
** a drag operation)
*/
void TextDisplay::cancelDrag() {
	int dragState = TEXT_OF_TEXTD(this).dragState;

	if (TEXT_OF_TEXTD(this).autoScrollProcID != 0)
		XtRemoveTimeOut(TEXT_OF_TEXTD(this).autoScrollProcID);

	if (dragState == SECONDARY_DRAG || dragState == SECONDARY_RECT_DRAG)
		this->buffer->BufSecondaryUnselect();

	if (dragState == PRIMARY_BLOCK_DRAG)
		CancelBlockDrag(reinterpret_cast<TextWidget>(w));

	if (dragState == MOUSE_PAN)
		XUngrabPointer(XtDisplay(w), CurrentTime);
		
	if (dragState != NOT_CLICKED)
		TEXT_OF_TEXTD(this).dragState = DRAG_CANCELED;
}


void TextDisplay::checkAutoShowInsertPos() const {

	auto tw = reinterpret_cast<TextWidget>(w);

	if (tw->text.autoShowInsertPos) {
		tw->text.textD->TextDMakeInsertPosVisible();
	}
}

/*
** Do operations triggered by cursor movement: Call cursor movement callback
** procedure(s), and cancel marker indicating that the cursor is after one or
** more just-entered emulated tabs (spaces to be deleted as a unit).
*/
void TextDisplay::callCursorMovementCBs(XEvent *event) {
	TEXT_OF_TEXTD(this).emTabsBeforeCursor = 0;
	XtCallCallbacks((Widget)w, textNcursorMovementCallback, (XtPointer)event);
}

void TextDisplay::HandleAllPendingGraphicsExposeNoExposeEvents(XEvent *event) {
	XEvent foundEvent;
	int left;
	int top;
	int width;
	int height;
	bool invalidRect = true;

	if (event) {
		adjustRectForGraphicsExposeOrNoExposeEvent(event, &invalidRect, &left, &top, &width, &height);
	}
	while (XCheckIfEvent(XtDisplay(w), &foundEvent, findGraphicsExposeOrNoExposeEvent, (XPointer)w)) {
		adjustRectForGraphicsExposeOrNoExposeEvent(&foundEvent, &invalidRect, &left, &top, &width, &height);
	}
	if (!invalidRect) {
		TextDRedisplayRect(left, top, width, height);
	}
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

			*left   = std::min<int>(*left, x);
			*top    = std::min<int>(*top, y);
			*width  = std::max<int>(prev_left + *width, x + e->width) - *left;
			*height = std::max<int>(prev_top + *height, y + e->height) - *top;
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
			this->calltip.pos = this->width / 2;
			this->calltip.hAlign = TIP_CENTER;
			rel_y = this->height / 3;
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
