/*******************************************************************************
*                                                                              *
* TextDisplay.c - Display text from a text buffer                                 *
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <sys/param.h>
#include <algorithm>

#include <Xm/Xm.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/Label.h>
#include <X11/Shell.h>

namespace {

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

// Macro for getting the TextPart from a textD
#define TEXT_OF_TEXTD(t) (reinterpret_cast<TextWidget>((t)->w)->text)

enum positionTypes {
	CURSOR_POS,
	CHARACTER_POS
};

}

static void updateLineStarts(TextDisplay *textD, int pos, int charsInserted, int charsDeleted, int linesInserted, int linesDeleted, int *scrolled);
static void offsetLineStarts(TextDisplay *textD, int newTopLineNum);
static void calcLineStarts(TextDisplay *textD, int startLine, int endLine);
static void calcLastChar(TextDisplay *textD);
static int posToVisibleLineNum(TextDisplay *textD, int pos, int *lineNum);
static void redisplayLine(TextDisplay *textD, int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex);
static void drawString(TextDisplay *textD, int style, int x, int y, int toX, char *string, int nChars);
static void clearRect(TextDisplay *textD, GC gc, int x, int y, int width, int height);
static void drawCursor(TextDisplay *textD, int x, int y);
static int styleOfPos(TextDisplay *textD, int lineStartPos, int lineLen, int lineIndex, int dispIndex, int thisChar);
static int stringWidth(const TextDisplay *textD, const char *string, const int length, const int style);
static int inSelection(TextSelection *sel, int pos, int lineStartPos, int dispIndex);
static int xyToPos(TextDisplay *textD, int x, int y, int posType);
static void xyToUnconstrainedPos(TextDisplay *textD, int x, int y, int *row, int *column, int posType);
static void bufPreDeleteCB(int pos, int nDeleted, void *cbArg);
static void bufModifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
static void setScroll(TextDisplay *textD, int topLineNum, int horizOffset, int updateVScrollBar, int updateHScrollBar);
static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData);
static void vScrollCB(Widget w, XtPointer clientData, XtPointer callData);
static void visibilityEH(Widget w, XtPointer data, XEvent *event, Boolean *continueDispatch);
static void redrawLineNumbers(TextDisplay *textD, int clearAll);
static void updateVScrollBarRange(TextDisplay *textD);
static int updateHScrollBarRange(TextDisplay *textD);
static int countLinesEx(view::string_view string);
static int measureVisLine(TextDisplay *textD, int visLineNum);
static int emptyLinesVisible(TextDisplay *textD);
static void blankCursorProtrusions(TextDisplay *textD);
static void allocateFixedFontGCs(TextDisplay *textD, XFontStruct *fontStruct, Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel lineNumFGPixel);
static GC allocateGC(Widget w, unsigned long valueMask, unsigned long foreground, unsigned long background, Font font, unsigned long dynamicMask, unsigned long dontCareMask);
static void releaseGC(Widget w, GC gc);
static void resetClipRectangles(TextDisplay *textD);
static int visLineLength(TextDisplay *textD, int visLineNum);
static void measureDeletedLines(TextDisplay *textD, int pos, int nDeleted);
static void findWrapRangeEx(TextDisplay *textD, view::string_view deletedText, int pos, int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd, int *linesInserted, int *linesDeleted);
static void wrappedLineCounter(const TextDisplay *textD, const TextBuffer *buf, const int startPos, const int maxPos, const int maxLines, const Boolean startPosIsLineStart, const int styleBufOffset, int *retPos, int *retLines, int *retLineStart, int *retLineEnd);
static void findLineEnd(TextDisplay *textD, int startPos, int startPosIsLineStart, int *lineEnd, int *nextLineStart);
static int wrapUsesCharacter(TextDisplay *textD, int lineEndPos);
static void hideOrShowHScrollBar(TextDisplay *textD);
static int rangeTouchesRectSel(TextSelection *sel, int rangeStart, int rangeEnd);
static void extendRangeForStyleMods(TextDisplay *textD, int *start, int *end);
static int getAbsTopLineNum(TextDisplay *textD);
static void offsetAbsLineNum(TextDisplay *textD, int oldFirstChar);
static int maintainingAbsTopLineNum(TextDisplay *textD);
static void resetAbsLineNum(TextDisplay *textD);
static int measurePropChar(const TextDisplay *textD, const char c, const int colNum, const int pos);
static Pixel allocBGColor(Widget w, char *colorName, int *ok);
static Pixel getRangesetColor(TextDisplay *textD, int ind, Pixel bground);
static void textDRedisplayRange(TextDisplay *textD, int start, int end);

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
	allocateFixedFontGCs(this, fontStruct, bgPixel, fgPixel, selectFGPixel, selectBGPixel, highlightFGPixel, highlightBGPixel, lineNumFGPixel);
	this->styleGC = allocateGC(this->w, 0, 0, 0, fontStruct->fid, GCClipMask | GCForeground | GCBackground, GCArcMode);
	this->lineNumLeft = lineNumLeft;
	this->lineNumWidth = lineNumWidth;
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
	hideOrShowHScrollBar(this);
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

	while (this->TextDPopGraphicExposeQueueEntry()) {
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
	this->TextDSetFont(this->fontStruct);
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
	allocateFixedFontGCs(this, this->fontStruct, textBgP, textFgP, selectFgP, selectBgP, hiliteFgP, hiliteBgP, lineNoFgP);

	// Change the cursor GC (the cursor GC is not shared).
	values.foreground = cursorFgP;
	XChangeGC(d, this->cursorFGGC, GCForeground, &values);

	// Redisplay
	this->TextDRedisplayRect(this->left, this->top, this->width, this->height);
	redrawLineNumbers(this, True);
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
	blankCursorProtrusions(this);

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

	allocateFixedFontGCs(this, fontStruct, bgPixel, fgPixel, selectFGPixel, selectBGPixel, highlightFGPixel, highlightBGPixel, lineNumFGPixel);
	XSetFont(display, this->styleGC, fontStruct->fid);

	// Do a full resize to force recalculation of font related parameters
	width  = this->width;
	height = this->height;
	this->width  = 0;
	this->height = 0;
	this->TextDResize(width, height);

	/* if the shell window doesn't get resized, and the new fonts are
	   of smaller sizes, sometime we get some residual text on the
	   blank space at the bottom part of text area. Clear it here. */
	clearRect(this, this->gc, this->left, this->top + this->height - maxAscent - maxDescent, this->width, maxAscent + maxDescent);

	// Redisplay
	this->TextDRedisplayRect(this->left, this->top, this->width, this->height);

	// Clean up line number area in case spacing has changed
	redrawLineNumbers(this, True);
}

int TextDisplay::TextDMinFontWidth(Boolean considerStyles) {
	int fontWidth = this->fontStruct->max_bounds.width;
	int i;

	if (considerStyles) {
		for (i = 0; i < this->nStyles; ++i) {
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
	int i;

	if (considerStyles) {
		for (i = 0; i < this->nStyles; ++i) {
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
		this->nBufferLines = this->TextDCountLines(0, this->buffer->BufGetLength(), True);
		this->firstChar = this->TextDStartOfLine(this->firstChar);
		this->topLineNum = this->TextDCountLines(0, this->firstChar, True) + 1;
		redrawAll = true;
		offsetAbsLineNum(this, oldFirstChar);
	}

	/* reallocate and update the line starts array, which may have changed
	   size and/or contents. (contents can change in continuous wrap mode
	   when the width changes, even without a change in height) */
	if (oldVisibleLines < newVisibleLines) {
		delete[] this->lineStarts;
		this->lineStarts = new int[newVisibleLines];
	}

	this->nVisibleLines = newVisibleLines;
	calcLineStarts(this, 0, newVisibleLines);
	calcLastChar(this);

	/* if the window became shorter, there may be partially drawn
	   text left at the bottom edge, which must be cleaned up */
	if (canRedraw && oldVisibleLines > newVisibleLines && exactHeight != height)
		XClearArea(XtDisplay(this->w), XtWindow(this->w), this->left, this->top + exactHeight, this->width, height - exactHeight, False);

	/* if the window became taller, there may be an opportunity to display
	   more text by scrolling down */
	if (canRedraw && oldVisibleLines < newVisibleLines && this->topLineNum + this->nVisibleLines > this->nBufferLines)
		setScroll(this, std::max<int>(1, this->nBufferLines - this->nVisibleLines + 2 + TEXT_OF_TEXTD(this).cursorVPadding), this->horizOffset, False, False);

	/* Update the scroll bar page increment size (as well as other scroll
	   bar parameters.  If updating the horizontal range caused scrolling,
	   redraw */
	updateVScrollBarRange(this);
	if (updateHScrollBarRange(this))
		redrawAll = true;

	// If a full redraw is needed
	if (redrawAll && canRedraw)
		this->TextDRedisplayRect(this->left, this->top, this->width, this->height);

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar(this);

	/* Refresh the line number display to draw more line numbers, or
	   erase extras */
	redrawLineNumbers(this, True);

	// Redraw the calltip
	TextDRedrawCalltip(this, 0);
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
	resetClipRectangles(this);

	// draw the lines of text
	for (line = firstLine; line <= lastLine; line++)
		redisplayLine(this, line, left, left + width, 0, INT_MAX);

	// draw the line numbers if exposed area includes them
	if (this->lineNumWidth != 0 && left <= this->lineNumLeft + this->lineNumWidth)
		redrawLineNumbers(this, False);
}

/*
** Refresh all of the text between buffer positions "start" and "end"
** not including the character at the position "end".
** If end points beyond the end of the buffer, refresh the whole display
** after pos, including blank lines which are not technically part of
** any range of characters.
*/
static void textDRedisplayRange(TextDisplay *textD, int start, int end) {
	int i, startLine, lastLine, startIndex, endIndex;

	// If the range is outside of the displayed text, just return
	if (end < textD->firstChar || (start > textD->lastChar && !emptyLinesVisible(textD)))
		return;

	// Clean up the starting and ending values
	if (start < 0)
		start = 0;
	if (start > textD->buffer->BufGetLength())
		start = textD->buffer->BufGetLength();
	if (end < 0)
		end = 0;
	if (end > textD->buffer->BufGetLength())
		end = textD->buffer->BufGetLength();

	// Get the starting and ending lines
	if (start < textD->firstChar) {
		start = textD->firstChar;
	}

	if (!posToVisibleLineNum(textD, start, &startLine)) {
		startLine = textD->nVisibleLines - 1;
	}

	if (end >= textD->lastChar) {
		lastLine = textD->nVisibleLines - 1;
	} else {
		if (!posToVisibleLineNum(textD, end, &lastLine)) {
			// shouldn't happen
			lastLine = textD->nVisibleLines - 1;
		}
	}

	// Get the starting and ending positions within the lines
	startIndex = (textD->lineStarts[startLine] == -1) ? 0 : start - textD->lineStarts[startLine];
	if (end >= textD->lastChar) {
		/*  Request to redisplay beyond textD->lastChar, so tell
		    redisplayLine() to display everything to infy.  */
		endIndex = INT_MAX;
	} else if (textD->lineStarts[lastLine] == -1) {
		/*  Here, lastLine is determined by posToVisibleLineNum() (see
		    if/else above) but deemed to be out of display according to
		    textD->lineStarts. */
		endIndex = 0;
	} else {
		endIndex = end - textD->lineStarts[lastLine];
	}

	/* Reset the clipping rectangles for the drawing GCs which are shared
	   using XtAllocateGC, and may have changed since the last use */
	resetClipRectangles(textD);

	/* If the starting and ending lines are the same, redisplay the single
	   line between "start" and "end" */
	if (startLine == lastLine) {
		redisplayLine(textD, startLine, 0, INT_MAX, startIndex, endIndex);
		return;
	}

	// Redisplay the first line from "start"
	redisplayLine(textD, startLine, 0, INT_MAX, startIndex, INT_MAX);

	// Redisplay the lines in between at their full width
	for (i = startLine + 1; i < lastLine; i++)
		redisplayLine(textD, i, 0, INT_MAX, 0, INT_MAX);

	// Redisplay the last line to "end"
	redisplayLine(textD, lastLine, 0, INT_MAX, 0, endIndex);
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

	setScroll(this, topLineNum, horizOffset, True, True);
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
	this->TextDBlankCursor();

	// draw it at its new position
	this->cursorPos = newPos;
	this->cursorOn = true;
	textDRedisplayRange(this, this->cursorPos - 1, this->cursorPos + 1);
}

void TextDisplay::TextDBlankCursor() {
	if (!this->cursorOn)
		return;

	blankCursorProtrusions(this);
	this->cursorOn = false;
	textDRedisplayRange(this, this->cursorPos - 1, this->cursorPos + 1);
}

void TextDisplay::TextDUnblankCursor() {
	if (!this->cursorOn) {
		this->cursorOn = true;
		textDRedisplayRange(this, this->cursorPos - 1, this->cursorPos + 1);
	}
}

void TextDisplay::TextDSetCursorStyle(int style) {
	this->cursorStyle = style;
	blankCursorProtrusions(this);
	if (this->cursorOn) {
		textDRedisplayRange(this, this->cursorPos - 1, this->cursorPos + 1);
	}
}

void TextDisplay::TextDSetWrapMode(int wrap, int wrapMargin) {
	this->wrapMargin = wrapMargin;
	this->continuousWrap = wrap;

	// wrapping can change change the total number of lines, re-count
	this->nBufferLines = this->TextDCountLines(0, this->buffer->BufGetLength(), True);

	/* changing wrap margins wrap or changing from wrapped mode to non-wrapped
	   can leave the character at the top no longer at a line start, and/or
	   change the line number */
	this->firstChar = this->TextDStartOfLine(this->firstChar);
	this->topLineNum = this->TextDCountLines(0, this->firstChar, True) + 1;
	resetAbsLineNum(this);

	// update the line starts array
	calcLineStarts(this, 0, this->nVisibleLines);
	calcLastChar(this);

	/* Update the scroll bar page increment size (as well as other scroll
	   bar parameters) */
	updateVScrollBarRange(this);
	updateHScrollBarRange(this);

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar(this);

	// Do a full redraw
	this->TextDRedisplayRect(0, this->top, this->width + this->left, this->height);
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
	return xyToPos(this, coord.x, coord.y, CURSOR_POS);
}

/*
** Translate window coordinates to the nearest character cell.
*/
int TextDisplay::TextDXYToCharPos(Point coord) {
	return xyToPos(this, coord.x, coord.y, CHARACTER_POS);
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
void TextDisplay::TextDXYToUnconstrainedPosition(Point coord, int *row, int *column) {
	xyToUnconstrainedPos(this, coord.x, coord.y, row, column, CURSOR_POS);
}

/*
** Translate line and column to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
int TextDisplay::TextDLineAndColToPos(int lineNum, int column) {
	int i, lineEnd, charIndex, outIndex;
	int lineStart = 0, charLen = 0;
	char expandedChar[MAX_EXP_CHAR_LEN];

	// Count lines
	if (lineNum < 1)
		lineNum = 1;
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
	charIndex = 0;

	// Only have to count columns if column isn't zero (or negative)
	if (column > 0) {
		// Count columns, expanding each character
		std::string lineStr = this->buffer->BufGetRangeEx(lineStart, lineEnd);
		outIndex = 0;
		for (i = lineStart; i < lineEnd; i++, charIndex++) {
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
	int charIndex, lineStartPos, fontHeight, lineLen;
	int visLineNum, charLen, outIndex, xStep, charStyle;
	char expandedChar[MAX_EXP_CHAR_LEN];

	// If position is not displayed, return false
	if (pos < this->firstChar || (pos > this->lastChar && !emptyLinesVisible(this)))
		return false;

	// Calculate y coordinate
	if (!posToVisibleLineNum(this, pos, &visLineNum))
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
	lineLen = visLineLength(this, visLineNum);
	std::string lineStr = this->buffer->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);

	/* Step through character positions from the beginning of the line
	   to "pos" to calculate the x coordinate */
	xStep = this->left - this->horizOffset;
	outIndex = 0;
	for (charIndex = 0; charIndex < pos - lineStartPos; charIndex++) {
		charLen = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, this->buffer->tabDist_, this->buffer->nullSubsChar_);
		charStyle = styleOfPos(this, lineStartPos, lineLen, charIndex, outIndex, lineStr[charIndex]);
		xStep += stringWidth(this, expandedChar, charLen, charStyle);
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
		if (!maintainingAbsTopLineNum(this) || pos < this->firstChar || pos > this->lastChar)
			return false;
		*lineNum = this->absTopLineNum + buf->BufCountLines(this->firstChar, pos);
		*column = buf->BufCountDispChars(buf->BufStartOfLine(pos), pos);
		return true;
	}

	// Only return the data if pos is within the displayed text
	if (!posToVisibleLineNum(this, pos, lineNum))
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
	int pos = xyToPos(this, p.x, p.y, CHARACTER_POS);
	TextBuffer *buf = this->buffer;

	xyToUnconstrainedPos(this, p.x, p.y, &row, &column, CHARACTER_POS);
	if (rangeTouchesRectSel(&buf->primary_, this->firstChar, this->lastChar))
		column = this->TextDOffsetWrappedColumn(row, column);
	return inSelection(&buf->primary_, pos, buf->BufStartOfLine(pos), column);
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
	int linesFromTop = 0, do_padding = 1;
	int cursorVPadding = (int)TEXT_OF_TEXTD(this).cursorVPadding;

	hOffset = this->horizOffset;
	topLine = this->topLineNum;

	// Don't do padding if this is a mouse operation
	do_padding = ((TEXT_OF_TEXTD(this).dragState == NOT_CLICKED) && (cursorVPadding > 0));

	// Find the new top line number
	if (cursorPos < this->firstChar) {
		topLine -= this->TextDCountLines(cursorPos, this->firstChar, False);
		// linesFromTop = 0;
	} else if (cursorPos > this->lastChar && !emptyLinesVisible(this)) {
		topLine += this->TextDCountLines(this->lastChar - (wrapUsesCharacter(this, this->lastChar) ? 0 : 1), cursorPos, False);
		linesFromTop = this->nVisibleLines - 1;
	} else if (cursorPos == this->lastChar && !emptyLinesVisible(this) && !wrapUsesCharacter(this, this->lastChar)) {
		topLine++;
		linesFromTop = this->nVisibleLines - 1;
	} else {
		// Avoid extra counting if cursorVPadding is disabled
		if (do_padding)
			linesFromTop = this->TextDCountLines(this->firstChar, cursorPos, True);
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
	if (!this->TextDPositionToXY(cursorPos, &x, &y)) {
		setScroll(this, topLine, hOffset, True, True);
		if (!this->TextDPositionToXY(cursorPos, &x, &y))
			return; // Give up, it's not worth it (but why does it fail?)
	}
	if (x > this->left + this->width)
		hOffset += x - (this->left + this->width);
	else if (x < this->left)
		hOffset += x - this->left;

	// Do the scroll
	setScroll(this, topLine, hOffset, True, True);
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
	if (posToVisibleLineNum(this, this->cursorPos, visLineNum)) {
		*lineStartPos = this->lineStarts[*visLineNum];
	} else {
		*lineStartPos =this->TextDStartOfLine(this->cursorPos);
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
		newPos = std::min<int>(newPos, this->TextDEndOfLine(lineStartPos, True));
	}
	return (newPos);
}

/*
** Cursor movement functions
*/
int TextDisplay::TextDMoveRight() {
	if (this->cursorPos >= this->buffer->BufGetLength())
		return false;
	this->TextDSetInsertPosition(this->cursorPos + 1);
	return true;
}

int TextDisplay::TextDMoveLeft() {
	if (this->cursorPos <= 0)
		return false;
	this->TextDSetInsertPosition(this->cursorPos - 1);
	return true;
}

int TextDisplay::TextDMoveUp(bool absolute) {
	int lineStartPos, column, prevLineStartPos, newPos, visLineNum;

	/* Find the position of the start of the line.  Use the line starts array
	   if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (absolute) {
		lineStartPos = this->buffer->BufStartOfLine(this->cursorPos);
		visLineNum = -1;
	} else if (posToVisibleLineNum(this, this->cursorPos, &visLineNum))
		lineStartPos = this->lineStarts[visLineNum];
	else {
		lineStartPos =this->TextDStartOfLine(this->cursorPos);
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
		prevLineStartPos = this->TextDCountBackwardNLines(lineStartPos, 1);
	}

	newPos = this->buffer->BufCountForwardDispChars(prevLineStartPos, column);
	if (this->continuousWrap && !absolute)
		newPos = std::min<int>(newPos, this->TextDEndOfLine(prevLineStartPos, True));

	// move the cursor
	this->TextDSetInsertPosition(newPos);

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
	} else if (posToVisibleLineNum(this, this->cursorPos, &visLineNum)) {
		lineStartPos = this->lineStarts[visLineNum];
	} else {
		lineStartPos =this->TextDStartOfLine(this->cursorPos);
		visLineNum = -1;
	}

	column = this->cursorPreferredCol >= 0 ? this->cursorPreferredCol : this->buffer->BufCountDispChars(lineStartPos, this->cursorPos);

	if (absolute)
		nextLineStartPos = this->buffer->BufCountForwardNLines(lineStartPos, 1);
	else
		nextLineStartPos = this->TextDCountForwardNLines(lineStartPos, 1, True);

	newPos = this->buffer->BufCountForwardDispChars(nextLineStartPos, column);

	if (this->continuousWrap && !absolute) {
		newPos = std::min<int>(newPos, this->TextDEndOfLine(nextLineStartPos, True));
	}

	this->TextDSetInsertPosition(newPos);
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

	wrappedLineCounter(this, this->buffer, startPos, endPos, INT_MAX, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
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
	wrappedLineCounter(this, this->buffer, startPos, this->buffer->BufGetLength(), nLines, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
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
	wrappedLineCounter(this, this->buffer, pos, this->buffer->BufGetLength(), 1, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
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

	wrappedLineCounter(this, this->buffer, this->buffer->BufStartOfLine(pos), pos, INT_MAX, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLineStart;
}

/*
** Same as BufCountBackwardNLines, but takes in to account line breaks when
** wrapping is turned on.
*/
int TextDisplay::TextDCountBackwardNLines(int startPos, int nLines) {
	TextBuffer *buf = this->buffer;
	int pos, lineStart, retLines, retPos, retLineStart, retLineEnd;

	// If we're not wrapping, use the more efficient BufCountBackwardNLines
	if (!this->continuousWrap)
		return this->buffer->BufCountBackwardNLines(startPos, nLines);

	pos = startPos;
	while (true) {
		lineStart = buf->BufStartOfLine(pos);
		wrappedLineCounter(this, this->buffer, lineStart, pos, INT_MAX, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retLines > nLines)
			return this->TextDCountForwardNLines(lineStart, retLines - nLines, True);
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
		measureDeletedLines(textD, pos, nDeleted);
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
		findWrapRangeEx(textD, deletedText, pos, nInserted, nDeleted, &wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);
	} else {
		linesInserted = nInserted == 0 ? 0 : buf->BufCountLines(pos, pos + nInserted);
		linesDeleted = nDeleted == 0 ? 0 : countLinesEx(deletedText);
	}

	// Update the line starts and topLineNum
	if (nInserted != 0 || nDeleted != 0) {
		if (textD->continuousWrap) {
			updateLineStarts(textD, wrapModStart, wrapModEnd - wrapModStart, nDeleted + pos - wrapModStart + (wrapModEnd - (pos + nInserted)), linesInserted, linesDeleted, &scrolled);
		} else {
			updateLineStarts(textD, pos, nInserted, nDeleted, linesInserted, linesDeleted, &scrolled);
		}
	} else
		scrolled = false;

	/* If we're counting non-wrapped lines as well, maintain the absolute
	   (non-wrapped) line number of the text displayed */
	if (maintainingAbsTopLineNum(textD) && (nInserted != 0 || nDeleted != 0)) {
		if (pos + nDeleted < oldFirstChar)
			textD->absTopLineNum += buf->BufCountLines(pos, pos + nInserted) - countLinesEx(deletedText);
		else if (pos < oldFirstChar)
			resetAbsLineNum(textD);
	}

	// Update the line count for the whole buffer
	textD->nBufferLines += linesInserted - linesDeleted;

	/* Update the scroll bar ranges (and value if the value changed).  Note
	   that updating the horizontal scroll bar range requires scanning the
	   entire displayed text, however, it doesn't seem to hurt performance
	   much.  Note also, that the horizontal scroll bar update routine is
	   allowed to re-adjust horizOffset if there is blank space to the right
	   of all lines of text. */
	updateVScrollBarRange(textD);
	scrolled |= updateHScrollBarRange(textD);

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
		blankCursorProtrusions(textD);
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
				blankCursorProtrusions(textD);
		}
		/* If more than one line is inserted/deleted, a line break may have
		   been inserted or removed in between, and the line numbers may
		   have changed. If only one line is altered, line numbers cannot
		   be affected (the insertion or removal of a line break always
		   results in at least two lines being redrawn). */
		if (linesInserted > 1)
			redrawLineNumbers(textD, False);
	} else { // linesInserted != linesDeleted
		endDispPos = textD->lastChar + 1;
		if (origCursorPos >= pos)
			blankCursorProtrusions(textD);
		redrawLineNumbers(textD, False);
	}

	/* If there is a style buffer, check if the modification caused additional
	   changes that need to be redisplayed.  (Redisplaying separately would
	   cause double-redraw on almost every modification involving styled
	   text).  Extend the redraw range to incorporate style changes */
	if (textD->styleBuffer)
		extendRangeForStyleMods(textD, &startDispPos, &endDispPos);

	// Redisplay computed range
	textDRedisplayRange(textD, startDispPos, endDispPos);
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
	resetAbsLineNum(this);
}

/*
** Returns the absolute (non-wrapped) line number of the first line displayed.
** Returns 0 if the absolute top line number is not being maintained.
*/
static int getAbsTopLineNum(TextDisplay *textD) {
	if (!textD->continuousWrap)
		return textD->topLineNum;
	if (maintainingAbsTopLineNum(textD))
		return textD->absTopLineNum;
	return 0;
}

/*
** Re-calculate absolute top line number for a change in scroll position.
*/
static void offsetAbsLineNum(TextDisplay *textD, int oldFirstChar) {
	if (maintainingAbsTopLineNum(textD)) {
		if (textD->firstChar < oldFirstChar)
			textD->absTopLineNum -= textD->buffer->BufCountLines(textD->firstChar, oldFirstChar);
		else
			textD->absTopLineNum += textD->buffer->BufCountLines(oldFirstChar, textD->firstChar);
	}
}

/*
** Return true if a separate absolute top line number is being maintained
** (for displaying line numbers or showing in the statistics line).
*/
static int maintainingAbsTopLineNum(TextDisplay *textD) {
	return textD->continuousWrap && (textD->lineNumWidth != 0 || textD->needAbsTopLineNum);
}

/*
** Count lines from the beginning of the buffer to reestablish the
** absolute (non-wrapped) top line number.  If mode is not continuous wrap,
** or the number is not being maintained, does nothing.
*/
static void resetAbsLineNum(TextDisplay *textD) {
	textD->absTopLineNum = 1;
	offsetAbsLineNum(textD, 0);
}

/*
** Find the line number of position "pos" relative to the first line of
** displayed text. Returns False if the line is not displayed.
*/
static int posToVisibleLineNum(TextDisplay *textD, int pos, int *lineNum) {
	int i;

	if (pos < textD->firstChar)
		return false;
	if (pos > textD->lastChar) {
		if (emptyLinesVisible(textD)) {
			if (textD->lastChar < textD->buffer->BufGetLength()) {
				if (!posToVisibleLineNum(textD, textD->lastChar, lineNum)) {
					fprintf(stderr, "nedit: Consistency check ptvl failed\n");
					return false;
				}
				return ++(*lineNum) <= textD->nVisibleLines - 1;
			} else {
				posToVisibleLineNum(textD, std::max<int>(textD->lastChar - 1, 0), lineNum);
				return true;
			}
		}
		return false;
	}

	for (i = textD->nVisibleLines - 1; i >= 0; i--) {
		if (textD->lineStarts[i] != -1 && pos >= textD->lineStarts[i]) {
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
static void redisplayLine(TextDisplay *textD, int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex) {
	TextBuffer *buf = textD->buffer;
	int i, x, y, startX, charIndex, lineStartPos, lineLen, fontHeight;
	int stdCharWidth, charWidth, startIndex, charStyle, style;
	int charLen, outStartIndex, outIndex, cursorX = 0, hasCursor = false;
	int dispIndexOffset, cursorPos = textD->cursorPos, y_orig;
	char expandedChar[MAX_EXP_CHAR_LEN], outStr[MAX_DISP_LINE_LEN];
	char *outPtr;
	char baseChar;
	std::string lineStr;

	// If line is not displayed, skip it
	if (visLineNum < 0 || visLineNum >= textD->nVisibleLines)
		return;

	// Shrink the clipping range to the active display area
	leftClip = std::max<int>(textD->left, leftClip);
	rightClip = std::min<int>(rightClip, textD->left + textD->width);

	if (leftClip > rightClip) {
		return;
	}

	// Calculate y coordinate of the string to draw
	fontHeight = textD->ascent + textD->descent;
	y = textD->top + visLineNum * fontHeight;

	// Get the text, length, and  buffer position of the line to display
	lineStartPos = textD->lineStarts[visLineNum];
	if (lineStartPos == -1) {
		lineLen = 0;
	} else {
		lineLen = visLineLength(textD, visLineNum);
		lineStr = buf->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);
	}

	/* Space beyond the end of the line is still counted in units of characters
	   of a standardized character width (this is done mostly because style
	   changes based on character position can still occur in this region due
	   to rectangular selections).  stdCharWidth must be non-zero to prevent a
	   potential infinite loop if x does not advance */
	stdCharWidth = textD->fontStruct->max_bounds.width;
	if (stdCharWidth <= 0) {
		fprintf(stderr, "nedit: Internal Error, bad font measurement\n");
		return;
	}

	/* Rectangular selections are based on "real" line starts (after a newline
	   or start of buffer).  Calculate the difference between the last newline
	   position and the line start we're using.  Since scanning back to find a
	   newline is expensive, only do so if there's actually a rectangular
	   selection which needs it */
	if (textD->continuousWrap && (rangeTouchesRectSel(&buf->primary_, lineStartPos, lineStartPos + lineLen) || rangeTouchesRectSel(&buf->secondary_, lineStartPos, lineStartPos + lineLen) ||
	                              rangeTouchesRectSel(&buf->highlight_, lineStartPos, lineStartPos + lineLen))) {
		dispIndexOffset = buf->BufCountDispChars(buf->BufStartOfLine(lineStartPos), lineStartPos);
	} else
		dispIndexOffset = 0;

	/* Step through character positions from the beginning of the line (even if
	   that's off the left edge of the displayed area) to find the first
	   character position that's not clipped, and the x coordinate for drawing
	   that character */
	x = textD->left - textD->horizOffset;
	outIndex = 0;

	for (charIndex = 0;; charIndex++) {
		baseChar = '\0';
		charLen = charIndex >= lineLen ? 1 : TextBuffer::BufExpandCharacter(baseChar = lineStr[charIndex], outIndex, expandedChar, buf->tabDist_, buf->nullSubsChar_);
		style = styleOfPos(textD, lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, baseChar);
		charWidth = charIndex >= lineLen ? stdCharWidth : stringWidth(textD, expandedChar, charLen, style);

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
				if (wrapUsesCharacter(textD, cursorPos)) {
					hasCursor = true;
					cursorX = x - 1;
				}
			}
		}

		baseChar = '\0';
		charLen = charIndex >= lineLen ? 1 : TextBuffer::BufExpandCharacter(baseChar = lineStr[charIndex], outIndex, expandedChar, buf->tabDist_, buf->nullSubsChar_);
		charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, baseChar);
		for (i = 0; i < charLen; i++) {
			if (i != 0 && charIndex < lineLen && lineStr[charIndex] == '\t') {
				charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, '\t');
			}

			if (charStyle != style) {
				drawString(textD, style, startX, y, x, outStr, outPtr - outStr);
				outPtr = outStr;
				startX = x;
				style = charStyle;
			}

			if (charIndex < lineLen) {
				*outPtr = expandedChar[i];
				charWidth = stringWidth(textD, &expandedChar[i], 1, charStyle);
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
	drawString(textD, style, startX, y, x, outStr, outPtr - outStr);

	/* Draw the cursor if part of it appeared on the redisplayed part of
	   this line.  Also check for the cases which are not caught as the
	   line is scanned above: when the cursor appears at the very end
	   of the redisplayed section. */
	y_orig = textD->cursorY;
	if (textD->cursorOn) {
		if (hasCursor) {
			drawCursor(textD, cursorX, y);
		} else if (charIndex < lineLen && (lineStartPos + charIndex + 1 == cursorPos) && x == rightClip) {
			if (cursorPos >= buf->BufGetLength()) {
				drawCursor(textD, x - 1, y);
			} else {
				if (wrapUsesCharacter(textD, cursorPos)) {
					drawCursor(textD, x - 1, y);
				}
			}
		} else if ((lineStartPos + rightCharIndex) == cursorPos) {
			drawCursor(textD, x - 1, y);
		}
	}

	// If the y position of the cursor has changed, redraw the calltip
	if (hasCursor && (y_orig != textD->cursorY || y_orig != y))
		TextDRedrawCalltip(textD, 0);
}

/*
** Draw a string or blank area according to parameter "style", using the
** appropriate colors and drawing method for that style, with top left
** corner at x, y.  If style says to draw text, use "string" as source of
** characters, and draw "nChars", if style is FILL, erase
** rectangle where text would have drawn from x to toX and from y to
** the maximum y extent of the current font(s).
*/
static void drawString(TextDisplay *textD, int style, int x, int y, int toX, char *string, int nChars) {
	GC gc, bgGC;
	XGCValues gcValues;
	XFontStruct *fs = textD->fontStruct;
	Pixel bground = textD->bgPixel;
	Pixel fground = textD->fgPixel;
	int underlineStyle = FALSE;

	// Don't draw if widget isn't realized
	if (XtWindow(textD->w) == 0)
		return;

	// select a GC
	if (style & (STYLE_LOOKUP_MASK | BACKLIGHT_MASK | RANGESET_MASK)) {
		gc = bgGC = textD->styleGC;
	} else if (style & HIGHLIGHT_MASK) {
		gc = textD->highlightGC;
		bgGC = textD->highlightBGGC;
	} else if (style & PRIMARY_MASK) {
		gc = textD->selectGC;
		bgGC = textD->selectBGGC;
	} else {
		gc = bgGC = textD->gc;
	}

	if (gc == textD->styleGC) {

		// we have work to do
		StyleTableEntry *styleRec;
		/* Set font, color, and gc depending on style.  For normal text, GCs
		   for normal drawing, or drawing within a selection or highlight are
		   pre-allocated and pre-configured.  For syntax highlighting, GCs are
		   configured here, on the fly. */
		if (style & STYLE_LOOKUP_MASK) {
			styleRec = &textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A];
			underlineStyle = styleRec->underline;
			fs = styleRec->font;
			gcValues.font = fs->fid;
			fground = styleRec->color;
			// here you could pick up specific select and highlight fground
		} else {
			styleRec = nullptr;
			gcValues.font = fs->fid;
			fground = textD->fgPixel;
		}

		/* Background color priority order is:
		   1 Primary(Selection), 2 Highlight(Parens),
		   3 Rangeset, 4 SyntaxHighlightStyle,
		   5 Backlight (if NOT fill), 6 DefaultBackground */
		bground = (style & PRIMARY_MASK)                           ? textD->selectBGPixel :
		          (style & HIGHLIGHT_MASK)                         ? textD->highlightBGPixel :
				  (style & RANGESET_MASK)                          ? getRangesetColor(textD, (style & RANGESET_MASK) >> RANGESET_SHIFT, bground) :
				  (styleRec && !styleRec->bgColorName.isNull())    ? styleRec->bgColor :
				  (style & BACKLIGHT_MASK) && !(style & FILL_MASK) ? textD->bgClassPixel[(style >> BACKLIGHT_SHIFT) & 0xff] :
				  textD->bgPixel;


		if (fground == bground) { // B&W kludge
			fground = textD->bgPixel;
		}

		// set up gc for clearing using the foreground color entry
		gcValues.foreground = bground;
		gcValues.background = bground;
		XChangeGC(XtDisplay(textD->w), gc, GCFont | GCForeground | GCBackground, &gcValues);
	}

	// Draw blank area rather than text, if that was the request
	if (style & FILL_MASK) {
		// wipes out to right hand edge of widget
		if (toX >= textD->left) {
			clearRect(textD, bgGC, std::max<int>(x, textD->left), y, toX - std::max<int>(x, textD->left), textD->ascent + textD->descent);
		}
		return;
	}

	/* If any space around the character remains unfilled (due to use of
	   different sized fonts for highlighting), fill in above or below
	   to erase previously drawn characters */
	if (fs->ascent < textD->ascent)
		clearRect(textD, bgGC, x, y, toX - x, textD->ascent - fs->ascent);
	if (fs->descent < textD->descent)
		clearRect(textD, bgGC, x, y + textD->ascent + fs->descent, toX - x, textD->descent - fs->descent);

	// set up gc for writing text (set foreground properly)
	if (bgGC == textD->styleGC) {
		gcValues.foreground = fground;
		XChangeGC(XtDisplay(textD->w), gc, GCForeground, &gcValues);
	}

	// Draw the string using gc and font set above
	XDrawImageString(XtDisplay(textD->w), XtWindow(textD->w), gc, x, y + textD->ascent, string, nChars);

	// Underline if style is secondary selection
	if (style & SECONDARY_MASK || underlineStyle) {
		// restore foreground in GC (was set to background by clearRect())
		gcValues.foreground = fground;
		XChangeGC(XtDisplay(textD->w), gc, GCForeground, &gcValues);
		// draw underline
		XDrawLine(XtDisplay(textD->w), XtWindow(textD->w), gc, x, y + textD->ascent, toX - 1, y + textD->ascent);
	}
}

/*
** Clear a rectangle with the appropriate background color for "style"
*/
static void clearRect(TextDisplay *textD, GC gc, int x, int y, int width, int height) {
	// A width of zero means "clear to end of window" to XClearArea
	if (width == 0 || XtWindow(textD->w) == 0)
		return;

	if (gc == textD->gc) {
		XClearArea(XtDisplay(textD->w), XtWindow(textD->w), x, y, width, height, False);
	} else {
		XFillRectangle(XtDisplay(textD->w), XtWindow(textD->w), gc, x, y, width, height);
	}
}

/*
** Draw a cursor with top center at x, y.
*/
static void drawCursor(TextDisplay *textD, int x, int y) {
	XSegment segs[5];
	int left, right, cursorWidth, midY;
	int fontWidth = textD->fontStruct->min_bounds.width, nSegs = 0;
	int fontHeight = textD->ascent + textD->descent;
	int bot = y + fontHeight - 1;

	if (XtWindow(textD->w) == 0 || x < textD->left - 1 || x > textD->left + textD->width)
		return;

	/* For cursors other than the block, make them around 2/3 of a character
	   width, rounded to an even number of pixels so that X will draw an
	   odd number centered on the stem at x. */
	cursorWidth = (fontWidth / 3) * 2;
	left = x - cursorWidth / 2;
	right = left + cursorWidth;

	// Create segments and draw cursor
	if (textD->cursorStyle == CARET_CURSOR) {
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
	} else if (textD->cursorStyle == NORMAL_CURSOR) {
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
	} else if (textD->cursorStyle == HEAVY_CURSOR) {
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
	} else if (textD->cursorStyle == DIM_CURSOR) {
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
	} else if (textD->cursorStyle == BLOCK_CURSOR) {
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
	XDrawSegments(XtDisplay(textD->w), XtWindow(textD->w), textD->cursorFGGC, segs, nSegs);

	// Save the last position drawn
	textD->cursorX = x;
	textD->cursorY = y;
}

/*
** Determine the drawing method to use to draw a specific character from "buf".
** "lineStartPos" gives the character index where the line begins, "lineIndex",
** the number of characters past the beginning of the line, and "dispIndex",
** the number of displayed characters past the beginning of the line.  Passing
** lineStartPos of -1 returns the drawing style for "no text".
**
** Why not just: styleOfPos(textD, pos)?  Because style applies to blank areas
** of the window beyond the text boundaries, and because this routine must also
** decide whether a position is inside of a rectangular selection, and do so
** efficiently, without re-counting character positions from the start of the
** line.
**
** Note that style is a somewhat incorrect name, drawing method would
** be more appropriate.
*/
static int styleOfPos(TextDisplay *textD, int lineStartPos, int lineLen, int lineIndex, int dispIndex, int thisChar) {
	TextBuffer *buf = textD->buffer;
	TextBuffer *styleBuf = textD->styleBuffer;
	int pos, style = 0;

	if (lineStartPos == -1 || buf == nullptr)
		return FILL_MASK;

	pos = lineStartPos + std::min<int>(lineIndex, lineLen);

	if (lineIndex >= lineLen)
		style = FILL_MASK;
	else if (styleBuf) {
		style = (unsigned char)styleBuf->BufGetCharacter(pos);
		if (style == textD->unfinishedStyle) {
			// encountered "unfinished" style, trigger parsing
			(textD->unfinishedHighlightCB)(textD, pos, textD->highlightCBArg);
			style = (unsigned char)styleBuf->BufGetCharacter(pos);
		}
	}
	if (inSelection(&buf->primary_, pos, lineStartPos, dispIndex))
		style |= PRIMARY_MASK;
	if (inSelection(&buf->highlight_, pos, lineStartPos, dispIndex))
		style |= HIGHLIGHT_MASK;
	if (inSelection(&buf->secondary_, pos, lineStartPos, dispIndex))
		style |= SECONDARY_MASK;
	// store in the RANGESET_MASK portion of style the rangeset index for pos
	if (buf->rangesetTable_) {
		int rangesetIndex = buf->rangesetTable_->RangesetIndex1ofPos(pos, True);
		style |= ((rangesetIndex << RANGESET_SHIFT) & RANGESET_MASK);
	}
	/* store in the BACKLIGHT_MASK portion of style the background color class
	   of the character thisChar */
	if (textD->bgClass) {
		style |= (textD->bgClass[(unsigned char)thisChar] << BACKLIGHT_SHIFT);
	}
	return style;
}

/*
** Find the width of a string in the font of a particular style
*/
static int stringWidth(const TextDisplay *textD, const char *string, const int length, const int style) {
	XFontStruct *fs;

	if (style & STYLE_LOOKUP_MASK)
		fs = textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A].font;
	else
		fs = textD->fontStruct;
	return XTextWidth(fs, (char *)string, (int)length);
}

/*
** Return true if position "pos" with indentation "dispIndex" is in
** selection "sel"
*/
static int inSelection(TextSelection *sel, int pos, int lineStartPos, int dispIndex) {
	return sel->selected && ((!sel->rectangular && pos >= sel->start && pos < sel->end) || (sel->rectangular && pos >= sel->start && lineStartPos <= sel->end && dispIndex >= sel->rectStart && dispIndex < sel->rectEnd));
}

/*
** Translate window coordinates to the nearest (insert cursor or character
** cell) text position.  The parameter posType specifies how to interpret the
** position: CURSOR_POS means translate the coordinates to the nearest cursor
** position, and CHARACTER_POS means return the position of the character
** closest to (x, y).
*/
static int xyToPos(TextDisplay *textD, int x, int y, int posType) {
	int charIndex, lineStart, lineLen, fontHeight;
	int charWidth, charLen, charStyle, visLineNum, xStep, outIndex;
	char expandedChar[MAX_EXP_CHAR_LEN];

	// Find the visible line number corresponding to the y coordinate
	fontHeight = textD->ascent + textD->descent;
	visLineNum = (y - textD->top) / fontHeight;
	if (visLineNum < 0)
		return textD->firstChar;
	if (visLineNum >= textD->nVisibleLines)
		visLineNum = textD->nVisibleLines - 1;

	// Find the position at the start of the line
	lineStart = textD->lineStarts[visLineNum];

	// If the line start was empty, return the last position in the buffer
	if (lineStart == -1)
		return textD->buffer->BufGetLength();

	// Get the line text and its length
	lineLen = visLineLength(textD, visLineNum);
	std::string lineStr = textD->buffer->BufGetRangeEx(lineStart, lineStart + lineLen);

	/* Step through character positions from the beginning of the line
	   to find the character position corresponding to the x coordinate */
	xStep = textD->left - textD->horizOffset;
	outIndex = 0;
	for (charIndex = 0; charIndex < lineLen; charIndex++) {
		charLen = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, textD->buffer->tabDist_, textD->buffer->nullSubsChar_);
		charStyle = styleOfPos(textD, lineStart, lineLen, charIndex, outIndex, lineStr[charIndex]);
		charWidth = stringWidth(textD, expandedChar, charLen, charStyle);
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
static void xyToUnconstrainedPos(TextDisplay *textD, int x, int y, int *row, int *column, int posType) {
	int fontHeight = textD->ascent + textD->descent;
	int fontWidth = textD->fontStruct->max_bounds.width;

	// Find the visible line number corresponding to the y coordinate
	*row = (y - textD->top) / fontHeight;
	if (*row < 0)
		*row = 0;
	if (*row >= textD->nVisibleLines)
		*row = textD->nVisibleLines - 1;
	*column = ((x - textD->left) + textD->horizOffset + (posType == CURSOR_POS ? fontWidth / 2 : 0)) / fontWidth;
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
static void offsetLineStarts(TextDisplay *textD, int newTopLineNum) {
	int oldTopLineNum = textD->topLineNum;
	int oldFirstChar = textD->firstChar;
	int lineDelta = newTopLineNum - oldTopLineNum;
	int nVisLines = textD->nVisibleLines;
	int *lineStarts = textD->lineStarts;
	int i, lastLineNum;
	TextBuffer *buf = textD->buffer;

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
		textD->firstChar = textD->TextDCountForwardNLines(0, newTopLineNum - 1, True);
		// printf("counting forward %d lines from start\n", newTopLineNum-1);
	} else if (newTopLineNum < oldTopLineNum) {
		textD->firstChar = textD->TextDCountBackwardNLines(textD->firstChar, -lineDelta);
		// printf("counting backward %d lines from firstChar\n", -lineDelta);
	} else if (newTopLineNum < lastLineNum) {
		textD->firstChar = lineStarts[newTopLineNum - oldTopLineNum];
		/* printf("taking new start from lineStarts[%d]\n",
		    newTopLineNum - oldTopLineNum); */
	} else if (newTopLineNum - lastLineNum < textD->nBufferLines - newTopLineNum) {
		textD->firstChar = textD->TextDCountForwardNLines(lineStarts[nVisLines - 1], newTopLineNum - lastLineNum, True);
		/* printf("counting forward %d lines from start of last line\n",
		    newTopLineNum - lastLineNum); */
	} else {
		textD->firstChar = textD->TextDCountBackwardNLines(buf->BufGetLength(), textD->nBufferLines - newTopLineNum + 1);
		/* printf("counting backward %d lines from end\n",
		        textD->nBufferLines - newTopLineNum + 1); */
	}

	// Fill in the line starts array
	if (lineDelta < 0 && -lineDelta < nVisLines) {
		for (i = nVisLines - 1; i >= -lineDelta; i--)
			lineStarts[i] = lineStarts[i + lineDelta];
		calcLineStarts(textD, 0, -lineDelta);
	} else if (lineDelta > 0 && lineDelta < nVisLines) {
		for (i = 0; i < nVisLines - lineDelta; i++)
			lineStarts[i] = lineStarts[i + lineDelta];
		calcLineStarts(textD, nVisLines - lineDelta, nVisLines - 1);
	} else
		calcLineStarts(textD, 0, nVisLines);

	// Set lastChar and topLineNum
	calcLastChar(textD);
	textD->topLineNum = newTopLineNum;

	/* If we're numbering lines or being asked to maintain an absolute line
	   number, re-calculate the absolute line number */
	offsetAbsLineNum(textD, oldFirstChar);

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
static void updateLineStarts(TextDisplay *textD, int pos, int charsInserted, int charsDeleted, int linesInserted, int linesDeleted, int *scrolled) {
	int *lineStarts = textD->lineStarts;
	int i, lineOfPos, lineOfEnd, nVisLines = textD->nVisibleLines;
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
	if (pos + charsDeleted < textD->firstChar) {
		textD->topLineNum += lineDelta;
		for (i = 0; i < nVisLines && lineStarts[i] != -1; i++)
			lineStarts[i] += charDelta;
		/* {   int i;
		    printf("lineStarts after delete doesn't touch: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		textD->firstChar += charDelta;
		textD->lastChar += charDelta;
		*scrolled = false;
		return;
	}

	/* The change began before the beginning of the displayed text, but
	   part or all of the displayed text was deleted */
	if (pos < textD->firstChar) {
		// If some text remains in the window, anchor on that
		if (posToVisibleLineNum(textD, pos + charsDeleted, &lineOfEnd) && ++lineOfEnd < nVisLines && lineStarts[lineOfEnd] != -1) {
			textD->topLineNum = std::max<int>(1, textD->topLineNum + lineDelta);
			textD->firstChar = textD->TextDCountBackwardNLines(lineStarts[lineOfEnd] + charDelta, lineOfEnd);
			// Otherwise anchor on original line number and recount everything
		} else {
			if (textD->topLineNum > textD->nBufferLines + lineDelta) {
				textD->topLineNum = 1;
				textD->firstChar = 0;
			} else
				textD->firstChar = textD->TextDCountForwardNLines(0, textD->topLineNum - 1, True);
		}
		calcLineStarts(textD, 0, nVisLines - 1);
		/* {   int i;
		    printf("lineStarts after delete encroaches: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		// calculate lastChar by finding the end of the last displayed line
		calcLastChar(textD);
		*scrolled = true;
		return;
	}

	/* If the change was in the middle of the displayed text (it usually is),
	   salvage as much of the line starts array as possible by moving and
	   offsetting the entries after the changed area, and re-counting the
	   added lines or the lines beyond the salvaged part of the line starts
	   array */
	if (pos <= textD->lastChar) {
		// find line on which the change began
		posToVisibleLineNum(textD, pos, &lineOfPos);
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
			calcLineStarts(textD, lineOfPos + 1, lineOfPos + linesInserted);
		if (lineDelta < 0)
			calcLineStarts(textD, nVisLines + lineDelta, nVisLines);
		/* {   int i;
		    printf("lineStarts after recalculation: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
		    printf("\n");
		} */
		// calculate lastChar by finding the end of the last displayed line
		calcLastChar(textD);
		*scrolled = false;
		return;
	}

	/* Change was past the end of the displayed text, but displayable by virtue
	   of being an insert at the end of the buffer into visible blank lines */
	if (emptyLinesVisible(textD)) {
		posToVisibleLineNum(textD, pos, &lineOfPos);
		calcLineStarts(textD, lineOfPos, lineOfPos + linesInserted);
		calcLastChar(textD);
		/* {   int i;
		    printf("lineStarts after insert at end: ");
		    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
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
static void calcLineStarts(TextDisplay *textD, int startLine, int endLine) {
	int startPos, bufLen = textD->buffer->BufGetLength();
	int line, lineEnd, nextLineStart, nVis = textD->nVisibleLines;
	int *lineStarts = textD->lineStarts;

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
		lineStarts[0] = textD->firstChar;
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
		findLineEnd(textD, startPos, True, &lineEnd, &nextLineStart);
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
static void calcLastChar(TextDisplay *textD) {
	int i;

	for (i = textD->nVisibleLines - 1; i > 0 && textD->lineStarts[i] == -1; i--) {
		;
	}
	textD->lastChar = i < 0 ? 0 : textD->TextDEndOfLine(textD->lineStarts[i], True);
}

void TextDisplay::TextDImposeGraphicsExposeTranslation(int *xOffset, int *yOffset) {

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

static void setScroll(TextDisplay *textD, int topLineNum, int horizOffset, int updateVScrollBar, int updateHScrollBar) {
	int fontHeight = textD->ascent + textD->descent;
	int origHOffset = textD->horizOffset;
	int lineDelta = textD->topLineNum - topLineNum;
	int xOffset, yOffset, srcX, srcY, dstX, dstY, width, height;
	int exactHeight = textD->height - textD->height % (textD->ascent + textD->descent);

	/* Do nothing if scroll position hasn't actually changed or there's no
	   window to draw in yet */
	if (XtWindow(textD->w) == 0 || (textD->horizOffset == horizOffset && textD->topLineNum == topLineNum))
		return;

	/* If part of the cursor is protruding beyond the text clipping region,
	   clear it off */
	blankCursorProtrusions(textD);

	/* If the vertical scroll position has changed, update the line
	   starts array and related counters in the text display */
	offsetLineStarts(textD, topLineNum);

	// Just setting textD->horizOffset is enough information for redisplay
	textD->horizOffset = horizOffset;

	/* Update the scroll bar positions if requested, note: updating the
	   horizontal scroll bars can have the further side-effect of changing
	   the horizontal scroll position, textD->horizOffset */
	if (updateVScrollBar && textD->vScrollBar) {
		updateVScrollBarRange(textD);
	}
	if (updateHScrollBar && textD->hScrollBar) {
		updateHScrollBarRange(textD);
	}

	/* Redisplay everything if the window is partially obscured (since
	   it's too hard to tell what displayed areas are salvageable) or
	   if there's nothing to recover because the scroll distance is large */
	xOffset = origHOffset - textD->horizOffset;
	yOffset = lineDelta * fontHeight;
	if (textD->visibility != VisibilityUnobscured || abs(xOffset) > textD->width || abs(yOffset) > exactHeight) {
		textD->TextDTranlateGraphicExposeQueue(xOffset, yOffset, false);
		textD->TextDRedisplayRect(textD->left, textD->top, textD->width, textD->height);
	} else {
		/* If the window is not obscured, paint most of the window using XCopyArea
		   from existing displayed text, and redraw only what's necessary */
		// Recover the useable window areas by moving to the proper location
		srcX = textD->left + (xOffset >= 0 ? 0 : -xOffset);
		dstX = textD->left + (xOffset >= 0 ? xOffset : 0);
		width = textD->width - abs(xOffset);
		srcY = textD->top + (yOffset >= 0 ? 0 : -yOffset);
		dstY = textD->top + (yOffset >= 0 ? yOffset : 0);
		height = exactHeight - abs(yOffset);
		resetClipRectangles(textD);
		textD->TextDTranlateGraphicExposeQueue(xOffset, yOffset, true);
		XCopyArea(XtDisplay(textD->w), XtWindow(textD->w), XtWindow(textD->w), textD->gc, srcX, srcY, width, height, dstX, dstY);
		// redraw the un-recoverable parts
		if (yOffset > 0) {
			textD->TextDRedisplayRect(textD->left, textD->top, textD->width, yOffset);
		} else if (yOffset < 0) {
			textD->TextDRedisplayRect(textD->left, textD->top + textD->height + yOffset, textD->width, -yOffset);
		}
		if (xOffset > 0) {
			textD->TextDRedisplayRect(textD->left, textD->top, xOffset, textD->height);
		} else if (xOffset < 0) {
			textD->TextDRedisplayRect(textD->left + textD->width + xOffset, textD->top, -xOffset, textD->height);
		}
		// Restore protruding parts of the cursor
		textDRedisplayRange(textD, textD->cursorPos - 1, textD->cursorPos + 1);
	}

	/* Refresh line number/calltip display if its up and we've scrolled
	    vertically */
	if (lineDelta != 0) {
		redrawLineNumbers(textD, False);
		TextDRedrawCalltip(textD, 0);
	}

	HandleAllPendingGraphicsExposeNoExposeEvents(reinterpret_cast<TextWidget>(textD->w), nullptr);
}

/*
** Update the minimum, maximum, slider size, page increment, and value
** for vertical scroll bar.
*/
static void updateVScrollBarRange(TextDisplay *textD) {
	int sliderSize, sliderMax, sliderValue;

	if (!textD->vScrollBar)
		return;

	/* The Vert. scroll bar value and slider size directly represent the top
	   line number, and the number of visible lines respectively.  The scroll
	   bar maximum value is chosen to generally represent the size of the whole
	   buffer, with minor adjustments to keep the scroll bar widget happy */
	sliderSize = std::max<int>(textD->nVisibleLines, 1); // Avoid X warning (size < 1)
	sliderValue = textD->topLineNum;
	sliderMax = std::max<int>(textD->nBufferLines + 2 + TEXT_OF_TEXTD(textD).cursorVPadding, sliderSize + sliderValue);
	XtVaSetValues(textD->vScrollBar, XmNmaximum, sliderMax, XmNsliderSize, sliderSize, XmNpageIncrement, std::max<int>(1, textD->nVisibleLines - 1), XmNvalue, sliderValue, nullptr);
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
static int updateHScrollBarRange(TextDisplay *textD) {
	int i, maxWidth = 0, sliderMax, sliderWidth;
	int origHOffset = textD->horizOffset;

	if (textD->hScrollBar == nullptr || !XtIsManaged(textD->hScrollBar))
		return false;

	// Scan all the displayed lines to find the width of the longest line
	for (i = 0; i < textD->nVisibleLines && textD->lineStarts[i] != -1; i++)
		maxWidth = std::max<int>(measureVisLine(textD, i), maxWidth);

	/* If the scroll position is beyond what's necessary to keep all lines
	   in view, scroll to the left to bring the end of the longest line to
	   the right margin */
	if (maxWidth < textD->width + textD->horizOffset && textD->horizOffset > 0)
		textD->horizOffset = std::max<int>(0, maxWidth - textD->width);

	// Readjust the scroll bar
	sliderWidth = textD->width;
	sliderMax = std::max<int>(maxWidth, sliderWidth + textD->horizOffset);
	XtVaSetValues(textD->hScrollBar, XmNmaximum, sliderMax, XmNsliderSize, sliderWidth, XmNpageIncrement, std::max<int>(textD->width - 100, 10), XmNvalue, textD->horizOffset, nullptr);

	// Return True if scroll position was changed
	return origHOffset != textD->horizOffset;
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
	resetAbsLineNum(this);
	this->TextDResize(newWidth, this->height);
	this->TextDRedisplayRect(0, this->top, INT_MAX, this->height);
}

/*
** Refresh the line number area.  If clearAll is False, writes only over
** the character cell areas.  Setting clearAll to True will clear out any
** stray marks outside of the character cell area, which might have been
** left from before a resize or font change.
*/
static void redrawLineNumbers(TextDisplay *textD, int clearAll) {
	int y, line, visLine, nCols, lineStart;
	char lineNumString[12];
	int lineHeight = textD->ascent + textD->descent;
	int charWidth = textD->fontStruct->max_bounds.width;
	XRectangle clipRect;
	Display *display = XtDisplay(textD->w);

	/* Don't draw if lineNumWidth == 0 (line numbers are hidden), or widget is
	   not yet realized */
	if (textD->lineNumWidth == 0 || XtWindow(textD->w) == 0)
		return;

	/* Make sure we reset the clipping range for the line numbers GC, because
	   the GC may be shared (eg, if the line numbers and text have the same
	   color) and therefore the clipping ranges may be invalid. */
	clipRect.x = textD->lineNumLeft;
	clipRect.y = textD->top;
	clipRect.width = textD->lineNumWidth;
	clipRect.height = textD->height;
	XSetClipRectangles(display, textD->lineNumGC, 0, 0, &clipRect, 1, Unsorted);

	// Erase the previous contents of the line number area, if requested
	if (clearAll)
		XClearArea(XtDisplay(textD->w), XtWindow(textD->w), textD->lineNumLeft, textD->top, textD->lineNumWidth, textD->height, False);

	// Draw the line numbers, aligned to the text
	nCols = std::min<int>(11, textD->lineNumWidth / charWidth);
	y = textD->top;
	line = getAbsTopLineNum(textD);
	for (visLine = 0; visLine < textD->nVisibleLines; visLine++) {
		lineStart = textD->lineStarts[visLine];
		if (lineStart != -1 && (lineStart == 0 || textD->buffer->BufGetCharacter(lineStart - 1) == '\n')) {
			sprintf(lineNumString, "%*d", nCols, line);
			XDrawImageString(XtDisplay(textD->w), XtWindow(textD->w), textD->lineNumGC, textD->lineNumLeft, y + textD->ascent, lineNumString, strlen(lineNumString));
			line++;
		} else {
			XClearArea(XtDisplay(textD->w), XtWindow(textD->w), textD->lineNumLeft, y, textD->lineNumWidth, textD->ascent + textD->descent, False);
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
	setScroll(textD, newValue, textD->horizOffset, False, True);
}
static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;

	auto textD = static_cast<TextDisplay *>(clientData);
	int newValue = ((XmScrollBarCallbackStruct *)callData)->value;

	if (newValue == textD->horizOffset)
		return;
	setScroll(textD, textD->topLineNum, newValue, False, False);
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
static int measureVisLine(TextDisplay *textD, int visLineNum) {
	int i, width = 0, len, style, lineLen = visLineLength(textD, visLineNum);
	int charCount = 0, lineStartPos = textD->lineStarts[visLineNum];
	char expandedChar[MAX_EXP_CHAR_LEN];

	if (!textD->styleBuffer) {
		for (i = 0; i < lineLen; i++) {
			len = textD->buffer->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
			width += XTextWidth(textD->fontStruct, expandedChar, len);
			charCount += len;
		}
	} else {
		for (i = 0; i < lineLen; i++) {
			len = textD->buffer->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
			style = (unsigned char)textD->styleBuffer->BufGetCharacter(lineStartPos + i) - ASCII_A;
			width += XTextWidth(textD->styleTable[style].font, expandedChar, len);
			charCount += len;
		}
	}
	return width;
}

/*
** Return true if there are lines visible with no corresponding buffer text
*/
static int emptyLinesVisible(TextDisplay *textD) {
	return textD->nVisibleLines > 0 && textD->lineStarts[textD->nVisibleLines - 1] == -1;
}

/*
** When the cursor is at the left or right edge of the text, part of it
** sticks off into the clipped region beyond the text.  Normal redrawing
** can not overwrite this protruding part of the cursor, so it must be
** erased independently by calling this routine.
*/
static void blankCursorProtrusions(TextDisplay *textD) {
	int x, width, cursorX = textD->cursorX, cursorY = textD->cursorY;
	int fontWidth = textD->fontStruct->max_bounds.width;
	int fontHeight = textD->ascent + textD->descent;
	int cursorWidth, left = textD->left, right = left + textD->width;

	cursorWidth = (fontWidth / 3) * 2;
	if (cursorX >= left - 1 && cursorX <= left + cursorWidth / 2 - 1) {
		x = cursorX - cursorWidth / 2;
		width = left - x;
	} else if (cursorX >= right - cursorWidth / 2 && cursorX <= right) {
		x = right;
		width = cursorX + cursorWidth / 2 + 2 - right;
	} else
		return;

	XClearArea(XtDisplay(textD->w), XtWindow(textD->w), x, cursorY, width, fontHeight, False);
}

/*
** Allocate shared graphics contexts used by the widget, which must be
** re-allocated on a font change.
*/
static void allocateFixedFontGCs(TextDisplay *textD, XFontStruct *fontStruct, Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel lineNumFGPixel) {
	textD->gc = allocateGC(textD->w, GCFont | GCForeground | GCBackground, fgPixel, bgPixel, fontStruct->fid, GCClipMask, GCArcMode);
	textD->selectGC = allocateGC(textD->w, GCFont | GCForeground | GCBackground, selectFGPixel, selectBGPixel, fontStruct->fid, GCClipMask, GCArcMode);
	textD->selectBGGC = allocateGC(textD->w, GCForeground, selectBGPixel, 0, fontStruct->fid, GCClipMask, GCArcMode);
	textD->highlightGC = allocateGC(textD->w, GCFont | GCForeground | GCBackground, highlightFGPixel, highlightBGPixel, fontStruct->fid, GCClipMask, GCArcMode);
	textD->highlightBGGC = allocateGC(textD->w, GCForeground, highlightBGPixel, 0, fontStruct->fid, GCClipMask, GCArcMode);
	textD->lineNumGC = allocateGC(textD->w, GCFont | GCForeground | GCBackground, lineNumFGPixel, bgPixel, fontStruct->fid, GCClipMask, GCArcMode);
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
static void resetClipRectangles(TextDisplay *textD) {
	XRectangle clipRect;
	Display *display = XtDisplay(textD->w);

	clipRect.x = textD->left;
	clipRect.y = textD->top;
	clipRect.width = textD->width;
	clipRect.height = textD->height - textD->height % (textD->ascent + textD->descent);

	XSetClipRectangles(display, textD->gc, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, textD->selectGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, textD->highlightGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, textD->selectBGGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, textD->highlightBGGC, 0, 0, &clipRect, 1, Unsorted);
	XSetClipRectangles(display, textD->styleGC, 0, 0, &clipRect, 1, Unsorted);
}

/*
** Return the length of a line (number of displayable characters) by examining
** entries in the line starts array rather than by scanning for newlines
*/
static int visLineLength(TextDisplay *textD, int visLineNum) {
	int nextLineStart, lineStartPos = textD->lineStarts[visLineNum];

	if (lineStartPos == -1)
		return 0;
	if (visLineNum + 1 >= textD->nVisibleLines)
		return textD->lastChar - lineStartPos;
	nextLineStart = textD->lineStarts[visLineNum + 1];
	if (nextLineStart == -1)
		return textD->lastChar - lineStartPos;
	if (wrapUsesCharacter(textD, nextLineStart - 1))
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
static void findWrapRangeEx(TextDisplay *textD, view::string_view deletedText, int pos, int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd, int *linesInserted, int *linesDeleted) {
	int length;
	int retPos;
	int retLines;
	int retLineStart;
	int retLineEnd;
	TextBuffer *buf = textD->buffer;
	int nVisLines   = textD->nVisibleLines;
	int *lineStarts = textD->lineStarts;
	int countFrom;
	int countTo;
	int lineStart;
	int adjLineStart, i;
	int visLineNum = 0;
	int nLines = 0;

	/*
	** Determine where to begin searching: either the previous newline, or
	** if possible, limit to the start of the (original) previous displayed
	** line, using information from the existing line starts array
	*/
	if (pos >= textD->firstChar && pos <= textD->lastChar) {
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
		wrappedLineCounter(textD, buf, lineStart, buf->BufGetLength(), 1, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
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
		if (textD->suppressResync) {
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
				countTo = textD->TextDEndOfLine(lineStart, True);
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
	if (textD->suppressResync) {
		*linesDeleted = textD->nLinesDeleted;
		textD->suppressResync = 0;
		return;
	}

	length = (pos - countFrom) + nDeleted + (countTo - (pos + nInserted));
	auto deletedTextBuf = new TextBuffer(length);
	if (pos > countFrom)
		deletedTextBuf->BufCopyFromBuf(textD->buffer, countFrom, pos, 0);
	if (nDeleted != 0)
		deletedTextBuf->BufInsertEx(pos - countFrom, deletedText);
	if (countTo > pos + nInserted)
		deletedTextBuf->BufCopyFromBuf(textD->buffer, pos + nInserted, countTo, pos - countFrom + nDeleted);
	/* Note that we need to take into account an offset for the style buffer:
	   the deletedTextBuf can be out of sync with the style buffer. */
	wrappedLineCounter(textD, deletedTextBuf, 0, length, INT_MAX, True, countFrom, &retPos, &retLines, &retLineStart, &retLineEnd);
	delete deletedTextBuf;
	*linesDeleted = retLines;
	textD->suppressResync = 0;
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
static void measureDeletedLines(TextDisplay *textD, int pos, int nDeleted) {
	int retPos, retLines, retLineStart, retLineEnd;
	TextBuffer *buf = textD->buffer;
	int nVisLines = textD->nVisibleLines;
	int *lineStarts = textD->lineStarts;
	int countFrom, lineStart;
	int nLines = 0, i;
	/*
	** Determine where to begin searching: either the previous newline, or
	** if possible, limit to the start of the (original) previous displayed
	** line, using information from the existing line starts array
	*/
	if (pos >= textD->firstChar && pos <= textD->lastChar) {
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
		wrappedLineCounter(textD, buf, lineStart, buf->BufGetLength(), 1, True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
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
	textD->nLinesDeleted = nLines;
	textD->suppressResync = 1;
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
static void wrappedLineCounter(const TextDisplay *textD, const TextBuffer *buf, const int startPos, const int maxPos, const int maxLines, const Boolean startPosIsLineStart, const int styleBufOffset, int *retPos, int *retLines,
                               int *retLineStart, int *retLineEnd) {
	int lineStart, newLineStart = 0, b, p, colNum, wrapMargin;
	int maxWidth, width, countPixels, i, foundBreak;
	int nLines = 0, tabDist = textD->buffer->tabDist_;
	unsigned char c;
	char nullSubsChar = textD->buffer->nullSubsChar_;

	/* If the font is fixed, or there's a wrap margin set, it's more efficient
	   to measure in columns, than to count pixels.  Determine if we can count
	   in columns (countPixels == False) or must count pixels (countPixels ==
	   True), and set the wrap target for either pixels or columns */
	if (textD->fixedFontWidth != -1 || textD->wrapMargin != 0) {
		countPixels = false;
		wrapMargin = textD->wrapMargin != 0 ? textD->wrapMargin : textD->width / textD->fixedFontWidth;
		maxWidth = INT_MAX;
	} else {
		countPixels = true;
		wrapMargin = INT_MAX;
		maxWidth = textD->width;
	}

	/* Find the start of the line if the start pos is not marked as a
	   line start. */
	if (startPosIsLineStart)
		lineStart = startPos;
	else
		lineStart = textD->TextDStartOfLine(startPos);

	/*
	** Loop until position exceeds maxPos or line count exceeds maxLines.
	** (actually, contines beyond maxPos to end of line containing maxPos,
	** in case later characters cause a word wrap back before maxPos)
	*/
	colNum = 0;
	width = 0;
	for (p = lineStart; p < buf->BufGetLength(); p++) {
		c = buf->BufGetCharacter(p);

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
				width += measurePropChar(textD, c, colNum, p + styleBufOffset);
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
							width += measurePropChar(textD, buf->BufGetCharacter(i), colNum, i + styleBufOffset);
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
					width = measurePropChar(textD, c, colNum, p + styleBufOffset);
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
	*retPos = buf->BufGetLength();
	*retLines = nLines;
	*retLineStart = lineStart;
	*retLineEnd = buf->BufGetLength();
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
static int measurePropChar(const TextDisplay *textD, const char c, const int colNum, const int pos) {
	int charLen, style;
	char expChar[MAX_EXP_CHAR_LEN];
	TextBuffer *styleBuf = textD->styleBuffer;

	charLen = TextBuffer::BufExpandCharacter(c, colNum, expChar, textD->buffer->tabDist_, textD->buffer->nullSubsChar_);
	if(!styleBuf) {
		style = 0;
	} else {
		style = (unsigned char)styleBuf->BufGetCharacter(pos);
		if (style == textD->unfinishedStyle) {
			// encountered "unfinished" style, trigger parsing
			(textD->unfinishedHighlightCB)(textD, pos, textD->highlightCBArg);
			style = (unsigned char)styleBuf->BufGetCharacter(pos);
		}
	}
	return stringWidth(textD, expChar, charLen, style);
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
static void findLineEnd(TextDisplay *textD, int startPos, int startPosIsLineStart, int *lineEnd, int *nextLineStart) {
	int retLines, retLineStart;

	// if we're not wrapping use more efficient BufEndOfLine
	if (!textD->continuousWrap) {
		*lineEnd = textD->buffer->BufEndOfLine(startPos);
		*nextLineStart = std::min<int>(textD->buffer->BufGetLength(), *lineEnd + 1);
		return;
	}

	// use the wrapped line counter routine to count forward one line
	wrappedLineCounter(textD, textD->buffer, startPos, textD->buffer->BufGetLength(), 1, startPosIsLineStart, 0, nextLineStart, &retLines, &retLineStart, lineEnd);
	return;
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
static int wrapUsesCharacter(TextDisplay *textD, int lineEndPos) {
	char c;

	if (!textD->continuousWrap || lineEndPos == textD->buffer->BufGetLength())
		return true;

	c = textD->buffer->BufGetCharacter(lineEndPos);
	return c == '\n' || ((c == '\t' || c == ' ') && lineEndPos + 1 != textD->buffer->BufGetLength());
}

/*
** Decide whether the user needs (or may need) a horizontal scroll bar,
** and manage or unmanage the scroll bar widget accordingly.  The H.
** scroll bar is only hidden in continuous wrap mode when it's absolutely
** certain that the user will not need it: when wrapping is set
** to the window edge, or when the wrap margin is strictly less than
** the longest possible line.
*/
static void hideOrShowHScrollBar(TextDisplay *textD) {
	if (textD->continuousWrap && (textD->wrapMargin == 0 || textD->wrapMargin * textD->fontStruct->max_bounds.width < textD->width))
		XtUnmanageChild(textD->hScrollBar);
	else
		XtManageChild(textD->hScrollBar);
}

/*
** Return true if the selection "sel" is rectangular, and touches a
** buffer position withing "rangeStart" to "rangeEnd"
*/
static int rangeTouchesRectSel(TextSelection *sel, int rangeStart, int rangeEnd) {
	return sel->selected && sel->rectangular && sel->end >= rangeStart && sel->start <= rangeEnd;
}

/*
** Extend the range of a redraw request (from *start to *end) with additional
** redraw requests resulting from changes to the attached style buffer (which
** contains auxiliary information for coloring or styling text).
*/
static void extendRangeForStyleMods(TextDisplay *textD, int *start, int *end) {
	TextSelection *sel = &textD->styleBuffer->primary_;
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
	if (textD->fixedFontWidth == -1 && extended)
		*end = textD->buffer->BufEndOfLine(*end) + 1;
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

static Pixel getRangesetColor(TextDisplay *textD, int ind, Pixel bground) {
	TextBuffer *buf;
	RangesetTable *tab;
	Pixel color;
	char *color_name;
	int valid;

	if (ind > 0) {
		ind--;
		buf = textD->buffer;
		tab = buf->rangesetTable_;

		valid = tab->RangesetTableGetColorValid(ind, &color);
		if (valid == 0) {
			color_name = tab->RangesetTableGetColorName(ind);
			if (color_name)
				color = allocBGColor(textD->w, color_name, &valid);
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
void TextDisplay::TextDSetupBGClasses(Widget w, XmString str, Pixel **pp_bgClassPixel, unsigned char **pp_bgClass, Pixel bgPixelDefault) {
	unsigned char bgClass[256];
	Pixel bgClassPixel[256];
	int class_no = 0;
	char *semicol;
	auto s = (char *)str;
	size_t was_semicol;
	int lo, hi, dummy;
	char *pos;
	bool is_good = true;

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
		was_semicol = 0;
		is_good = true;
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
					bgClass[lo++] = (unsigned char)class_no;
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
	*pp_bgClass      = new unsigned char[256];
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

	if (!reinterpret_cast<TextWidget>(w)->text.continuousWrap || startPos == endPos) {
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
	int toPos = this->TextDCountForwardNLines(startPos, 1, False);
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
		toPos = this->TextDCountForwardNLines(fromPos, 1, True);
	}
	outBuf->BufCopyFromBuf(buf, fromPos, endPos, outPos);

	// return the contents of the output buffer as a string
	std::string outString = outBuf->BufGetAllEx();
	delete outBuf;
	return outString;
}

void TextDisplay::TextCopyClipboard(Time time) {
	cancelDrag(w);

	if (!this->buffer->primary_.selected) {
		XBell(XtDisplay(w), 0);
		return;
	}

	CopyToClipboard(w, time);
}

void TextDisplay::TextCutClipboard(Time time) {

	cancelDrag(w);
	if (checkReadOnly(w)) {
		return;
	}

	if (!this->buffer->primary_.selected) {
		XBell(XtDisplay(w), 0);
		return;
	}

	TakeMotifDestination(w, time);
	CopyToClipboard(w, time);
	this->buffer->BufRemoveSelected();
	this->TextDSetInsertPosition(this->buffer->cursorPosHint_);
	checkAutoShowInsertPos(w);
}

int TextDisplay::TextFirstVisibleLine() {
	return this->topLineNum;
}

int TextDisplay::TextFirstVisiblePos() {
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
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, nullptr);
}

/*
** Set the horizontal and vertical scroll positions of the widget
*/
void TextDisplay::TextSetScroll(int topLineNum, int horizOffset) {
	TextDSetScroll(topLineNum, horizOffset);
}


int TextDisplay::TextGetMinFontWidth(Boolean considerStyles) {
	return TextDMinFontWidth(considerStyles);
}

int TextDisplay::TextGetMaxFontWidth(Boolean considerStyles) {
	return TextDMaxFontWidth(considerStyles);
}

/*
** Return the cursor position
*/
int TextDisplay::TextGetCursorPos() {
	return TextDGetInsertPosition();
}

int TextDisplay::TextLastVisiblePos() {
	return lastChar;
}

/*
** Translate a line number and column into a position
*/
int TextDisplay::TextLineAndColToPos(int lineNum, int column) {
	return TextDLineAndColToPos(lineNum, column);
}

int TextDisplay::TextNumVisibleLines() {
	return this->nVisibleLines;
}

/*
** Return the horizontal and vertical scroll positions of the widget
*/
void TextDisplay::TextGetScroll(int *topLineNum, int *horizOffset) {
	TextDGetScroll(topLineNum, horizOffset);
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** False if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
int TextDisplay::TextPosToXY(int pos, int *x, int *y) {
	return TextDPositionToXY(pos, x, y);
}


/*
** Translate a position into a line number (if the position is visible,
** if it's not, return False
*/
int TextDisplay::TextPosToLineAndCol(int pos, int *lineNum, int *column) {
	return TextDPosToLineAndCol(pos, lineNum, column);
}
