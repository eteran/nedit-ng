/*******************************************************************************
*                                                                              *
* TextDisplay.h -- Nirvana Editor Text Diplay Header File                      *
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

#ifndef TEXTDISP_H_
#define TEXTDISP_H_

#include "TextBuffer.h"
#include "Point.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <Xm/Xm.h>

enum CursorStyles { NORMAL_CURSOR, CARET_CURSOR, DIM_CURSOR, BLOCK_CURSOR, HEAVY_CURSOR };

#define NO_HINT -1

class StyleTableEntry;
class TextDisplay;

struct graphicExposeTranslationEntry {
	int horizontal;
	int vertical;
	graphicExposeTranslationEntry *next;
};

typedef void (*unfinishedStyleCBProc)(const TextDisplay *, int, const void *);

struct calltipStruct {
	int ID;           //  ID of displayed calltip.  Equals zero if none is displayed.
	Boolean anchored; //  Is it anchored to a position
	int pos;          //  Position tip is anchored to
	int hAlign;       //  horizontal alignment
	int vAlign;       //  vertical alignment
	int alignMode;    //  Strict or sloppy alignment
};

class TextDisplay {
public:
	TextDisplay(Widget widget, Widget hScrollBar, Widget vScrollBar, Position left, Position top, Position width, Position height, Position lineNumLeft, Position lineNumWidth, TextBuffer *buffer, XFontStruct *fontStruct,
                      Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel cursorFGPixel, Pixel lineNumFGPixel, int continuousWrap, int wrapMargin,
                      XmString bgClassString, Pixel calltipFGPixel, Pixel calltipBGPixel);

	~TextDisplay();

	TextDisplay(const TextDisplay &) = delete;
	TextDisplay& operator=(const TextDisplay &) = delete;

public:
	bool TextDPopGraphicExposeQueueEntry();
	int TextDCountBackwardNLines(int startPos, int nLines);
	int TextDCountForwardNLines(int startPos, const unsigned nLines, const Boolean startPosIsLineStart);
	int TextDCountLines(int startPos, int endPos, int startPosIsLineStart);
	int TextDEndOfLine(int pos, const Boolean startPosIsLineStart);
	int TextDGetInsertPosition() const;
	int TextDInSelection(Point coord);
	int TextDLineAndColToPos(int lineNum, int column);
	int TextDMaxFontWidth(Boolean considerStyles);
	int TextDMinFontWidth(Boolean considerStyles);
	int TextDMoveDown(bool absolute);
	int TextDMoveLeft();
	int TextDMoveRight();
	int TextDMoveUp(bool absolute);
	int TextDOffsetWrappedColumn(int row, int column);
	int TextDOffsetWrappedRow(int row) const;
	int TextDPosOfPreferredCol(int column, int lineStartPos);
	int TextDPosToLineAndCol(int pos, int *lineNum, int *column);
	int TextDPositionToXY(int pos, int *x, int *y);
	int TextDPreferredColumn(int *visLineNum, int *lineStartPos);
	int TextDStartOfLine(int pos) const;
	int TextDXYToCharPos(Point coord);
	int TextDXYToPosition(Point coord);
	void TextDAttachHighlightData(TextBuffer *styleBuffer, StyleTableEntry *styleTable, int nStyles, char unfinishedStyle, unfinishedStyleCBProc unfinishedHighlightCB, void *cbArg);
	void TextDBlankCursor();
	void TextDGetScroll(int *topLineNum, int *horizOffset);
	void TextDImposeGraphicsExposeTranslation(int *xOffset, int *yOffset);
	void TextDInsertEx(view::string_view text);
	void TextDMaintainAbsLineNum(int state);
	void TextDMakeInsertPosVisible();
	void TextDOverstrikeEx(view::string_view text);
	void TextDRedisplayRect(int left, int top, int width, int height);
	void TextDResize(int width, int height);
	void TextDSetBuffer(TextBuffer *buffer);
	void TextDSetColors(Pixel textFgP, Pixel textBgP, Pixel selectFgP, Pixel selectBgP, Pixel hiliteFgP, Pixel hiliteBgP, Pixel lineNoFgP, Pixel cursorFgP);
	void TextDSetCursorStyle(int style);
	void TextDSetFont(XFontStruct *fontStruct);
	void TextDSetInsertPosition(int newPos);
	void TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft);
	void TextDSetScroll(int topLineNum, int horizOffset);
	void TextDSetWrapMode(int wrap, int wrapMargin);
	void TextDTranlateGraphicExposeQueue(int xOffset, int yOffset, bool appendEntry);
	void TextDUnblankCursor();
	void TextDXYToUnconstrainedPosition(Point coord, int *row, int *column);

public:
	int TextFirstVisibleLine();
	int TextFirstVisiblePos();
	int TextGetCursorPos();
	int TextGetMaxFontWidth(Boolean considerStyles);
	int TextGetMinFontWidth(Boolean considerStyles);
	int TextLastVisiblePos();
	int TextLineAndColToPos(int lineNum, int column);
	int TextNumVisibleLines();
	std::string TextGetWrappedEx(int startPos, int endPos);
	void TextCopyClipboard(Time time);
	void TextCutClipboard(Time time);
	void TextSetBuffer(TextBuffer *buffer);
	void TextSetCursorPos(int pos);
	void TextSetScroll(int topLineNum, int horizOffset);

public:
	static void TextDSetupBGClasses(Widget w, XmString str, Pixel **pp_bgClassPixel, unsigned char **pp_bgClass, Pixel bgPixelDefault);

public:
	Widget w; // TextWidget
	int top;
	int left;
	int width;
	int height;
	int lineNumLeft;
	int lineNumWidth;
	int cursorPos;
	int cursorOn;
	int cursorX, cursorY;                        // X, Y pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
	int cursorToHint;                            // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
	int cursorStyle;                             // One of enum cursorStyles above
	int cursorPreferredCol;                      // Column for vert. cursor movement
	int nVisibleLines;                           // # of visible (displayed) lines
	int nBufferLines;                            // # of newlines in the buffer
	TextBuffer *buffer;                          // Contains text to be displayed
	TextBuffer *styleBuffer;                     // Optional parallel buffer containing color and font information
	int firstChar, lastChar;                     // Buffer positions of first and last displayed character (lastChar points either to a newline or one character beyond the end of the buffer)
	int continuousWrap;                          // Wrap long lines when displaying
	int wrapMargin;                              // Margin in # of char positions for wrapping in continuousWrap mode
	int *lineStarts;
	int topLineNum;                              // Line number of top displayed line of file (first line of file is 1)
	int absTopLineNum;                           // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
	int needAbsTopLineNum;                       // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
	int horizOffset;                             // Horizontal scroll pos. in pixels
	int visibility;                              // Window visibility (see XVisibility event)
	int nStyles;                                 // Number of entries in styleTable
	StyleTableEntry *styleTable;                 // Table of fonts and colors for coloring/syntax-highlighting
	char unfinishedStyle;                        // Style buffer entry which triggers on-the-fly reparsing of region
	unfinishedStyleCBProc unfinishedHighlightCB; // Callback to parse "unfinished" regions
	void *highlightCBArg;                        // Arg to unfinishedHighlightCB
	XFontStruct *fontStruct;                     // Font structure for primary font
	int ascent, descent;                         // Composite ascent and descent for primary font + all-highlight fonts
	int fixedFontWidth;                          // Font width if all current fonts are fixed and match in width, else -1
	Widget hScrollBar, vScrollBar;
	GC gc, selectGC, highlightGC;                // GCs for drawing text
	GC selectBGGC, highlightBGGC;                // GCs for erasing text
	GC cursorFGGC;                               // GC for drawing the cursor
	GC lineNumGC;                                // GC for drawing line numbers
	GC styleGC;                                  // GC with color and font unspecified for drawing colored/styled text
	Pixel fgPixel, bgPixel;                      // Foreground/Background colors
	Pixel selectFGPixel;                         // Foreground select color
	Pixel selectBGPixel;                         // Background select color
	Pixel highlightFGPixel;                      // Highlight colors are used when flashing matching parens
	Pixel highlightBGPixel;
	Pixel lineNumFGPixel;                        // Color for drawing line numbers
	Pixel cursorFGPixel;
	Pixel *bgClassPixel;                         // table of colors for each BG class
	unsigned char *bgClass;                      // obtains index into bgClassPixel[]

	Widget calltipW;                             // The Label widget for the calltip
	Widget calltipShell;                         // The Shell that holds the calltip
	calltipStruct calltip;                       // The info for the calltip itself
	Pixel calltipFGPixel;
	Pixel calltipBGPixel;
	int suppressResync;                          // Suppress resynchronization of line starts during buffer updates
	int nLinesDeleted;                           // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
	int modifyingTabDist;                        // Whether tab distance is being modified
	Boolean pointerHidden;                       // true if the mouse pointer is hidden
	graphicExposeTranslationEntry *graphicsExposeQueue_;
};

#endif
