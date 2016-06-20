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

#include "BlockDragTypes.h"
#include "Point.h"
#include "Rect.h"
#include "util/string_view.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <Xm/Xm.h>

enum CursorStyles {
	NORMAL_CURSOR,
	CARET_CURSOR,
	DIM_CURSOR,
	BLOCK_CURSOR,
	HEAVY_CURSOR
};

#define NO_HINT -1

class StyleTableEntry;
class TextDisplay;
class TextPart;
class TextBuffer;

struct graphicExposeTranslationEntry {
	int horizontal;
	int vertical;
	graphicExposeTranslationEntry *next;
};

typedef void (*unfinishedStyleCBProc)(const TextDisplay *, int, const void *);

struct CallTip {
	int ID;           //  ID of displayed calltip.  Equals zero if none is displayed.
	bool anchored; //  Is it anchored to a position
	int pos;          //  Position tip is anchored to
	int hAlign;       //  horizontal alignment
	int vAlign;       //  vertical alignment
	int alignMode;    //  Strict or sloppy alignment
};


class TextDisplay {
public:
	TextDisplay(
		Widget widget,
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
		Pixel calltipBGPixel);

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
	void TextDImposeGraphicsExposeTranslation(int *xOffset, int *yOffset) const;
	void TextDInsertEx(view::string_view text);
	void TextDMaintainAbsLineNum(int state);
	void TextDMakeInsertPosVisible();
	void TextDOverstrikeEx(view::string_view text);
	void TextDRedisplayRect(int left, int top, int width, int height);
	void TextDRedisplayRect(const Rect &rect);
	void TextDResize(int width, int height);
	void TextDSetBuffer(TextBuffer *buffer);
	void TextDSetColors(Pixel textFgP, Pixel textBgP, Pixel selectFgP, Pixel selectBgP, Pixel hiliteFgP, Pixel hiliteBgP, Pixel lineNoFgP, Pixel cursorFgP);
	void TextDSetCursorStyle(CursorStyles style);
	void TextDSetFont(XFontStruct *fontStruct);
	void TextDSetInsertPosition(int newPos);
	void TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft);
	void TextDSetScroll(int topLineNum, int horizOffset);
	void TextDSetWrapMode(int wrap, int wrapMargin);
	void TextDTranlateGraphicExposeQueue(int xOffset, int yOffset, bool appendEntry);
	void TextDUnblankCursor();
	void TextDXYToUnconstrainedPosition(Point coord, int *row, int *column);
	void TextDKillCalltip(int calltipID);
	void TextDRedrawCalltip(int calltipID);
	
public:
	// block drag stuff
	void BeginBlockDrag();
	void BlockDragSelection(Point pos, int dragType);
	void FinishBlockDrag();
	void CancelBlockDrag();
	
public:
	// clipboard/selection
	void InsertClipboard(bool isColumnar);
	void CopyToClipboard(Time time);
	void HandleXSelections();
	void StopHandlingXSelections();
	void InsertPrimarySelection(Time time, bool isColumnar);
	void MovePrimarySelection(Time time, bool isColumnar);
	void ExchangeSelections(Time time);
	void SendSecondarySelection(Time time, bool removeAfter);
	void TakeMotifDestination(Time time);

public:
	TextBuffer *TextGetBuffer();
	bool checkReadOnly() const;
	int TextFirstVisibleLine() const;
	int TextFirstVisiblePos() const;
	int TextGetCursorPos() const;
	int TextLastVisiblePos() const;
	int TextNumVisibleLines() const;
	int TextVisibleWidth() const;
	std::string TextGetWrappedEx(int startPos, int endPos);
	void HandleAllPendingGraphicsExposeNoExposeEvents(XEvent *event);
	void ResetCursorBlink(bool startsBlanked);
	void ShowHidePointer(bool hidePointer);
	void TextColPasteClipboard(Time time);
	void TextCopyClipboard(Time time);
	void TextCutClipboard(Time time);
	void TextHandleXSelections();
	void TextInsertAtCursorEx(view::string_view chars, XEvent *event, bool allowPendingDelete, bool allowWrap);
	void TextPasteClipboard(Time time);
	void TextSetBuffer(TextBuffer *buffer);
	void TextSetCursorPos(int pos);
	void adjustRectForGraphicsExposeOrNoExposeEvent(XEvent *event, bool *first, Rect *rect);
	void adjustRectForGraphicsExposeOrNoExposeEvent(XEvent *event, bool *first, int *left, int *top, int *width, int *height);
	void callCursorMovementCBs(XEvent *event);
	void cancelDrag();
	void checkAutoShowInsertPos();
	void setScroll(int topLineNum, int horizOffset, int updateVScrollBar, int updateHScrollBar);
	
public:
	CursorStyles getCursorStyle() const;
	Pixel backgroundPixel() const;
	Pixel foregroundPixel() const;
	Rect getRect() const;
	TextBuffer *getStyleBuffer() const;
	XFontStruct *TextDGetFont() const;
	XtIntervalId getCursorBlinkProcID() const;
	int fontAscent() const;
	int fontDescent() const;
	int getAbsTopLineNum();
	int getBufferLinesCount() const;
	int getFirstChar() const;
	int getLastChar() const;
	int getLineNumLeft() const;
	int getLineNumWidth() const;
	void setModifyingTabDist(int tabDist);
	void setStyleBuffer(TextBuffer *buffer);
	Pixel getSelectFGPixel() const;
	Pixel getSelectBGPixel() const;
	Pixel getHighlightFGPixel() const;
	Pixel getHighlightBGPixel() const;
	Pixel getLineNumFGPixel() const;
	Pixel getCursorFGPixel() const;
	
public:
	int TextDShowCalltip(view::string_view text, bool anchored, int pos, int hAlign, int vAlign, int alignMode);
	int TextDGetCalltipID(int calltipID);

private:
	Pixel getRangesetColor(int ind, Pixel bground);
	int deleteEmulatedTab(XEvent *event);
	int deletePendingSelection(XEvent *event);
	int emptyLinesVisible() const;
	int endOfWord(int pos);
	int measurePropChar(const char c, const int colNum, const int pos) const;
	int measureVisLine(int visLineNum);
	int pendingSelection();
	int posToVisibleLineNum(int pos, int *lineNum);
	int startOfWord(int pos);
	int stringWidth(const char *string, const int length, const int style) const;
	int styleOfPos(int lineStartPos, int lineLen, int lineIndex, int dispIndex, int thisChar);
	int visLineLength(int visLineNum);
	int wrapLine(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int limitPos, int *breakAt, int *charsAdded);
	int wrapUsesCharacter(int lineEndPos);
	int xyToPos(int x, int y, int posType);
	std::string createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *column);
	std::string createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *length, int *column);
	std::string wrapTextEx(view::string_view startLine, view::string_view text, int bufOffset, int wrapMargin, int *breakBefore);
	void allocateFixedFontGCs(XFontStruct *fontStruct, Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel lineNumFGPixel);
	void calcLastChar();
	void calcLineStarts(int startLine, int endLine);
	void checkAutoScroll(const Point &coord);
	void checkMoveSelectionChange(XEvent *event, int startPos, String *args, Cardinal *nArgs);
	void clearRect(GC gc, const Rect &rect);
	void clearRect(GC gc, int x, int y, int width, int height);
	void drawCursor(int x, int y);
	void drawString(int style, int x, int y, int toX, char *string, int nChars);
	void endDrag();
	void findLineEnd(int startPos, int startPosIsLineStart, int *lineEnd, int *nextLineStart);
	void hideOrShowHScrollBar();
	void keyMoveExtendSelection(XEvent *event, int origPos, int rectangular);
	void offsetAbsLineNum(int oldFirstChar);
	void offsetLineStarts(int newTopLineNum);
	void redisplayLine(int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex);
	void resetClipRectangles();
	void selectLine();
	void selectWord(int pointerX);
	void simpleInsertAtCursorEx(view::string_view chars, XEvent *event, bool allowPendingDelete);
	void simpleInsertAtCursorEx(view::string_view chars, XEvent *event, int allowPendingDelete);
	void wrappedLineCounter(const TextBuffer *buf, const int startPos, const int maxPos, const int maxLines, const Boolean startPosIsLineStart, const int styleBufOffset, int *retPos, int *retLines, int *retLineStart, int *retLineEnd) const;
	void xyToUnconstrainedPos(int x, int y, int *row, int *column, int posType);

public:
	// Callbacks
	void selectNotifyTimerProcEx(XtPointer clientData, XtIntervalId *id);
	void selectNotifyEHEx(XtPointer data, XEvent *event, Boolean *continueDispatch);
	Boolean convertMotifDestCallback(Atom *selType, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format);
	Boolean convertSecondaryCallback(Atom *selType, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format);
	void getExchSelCallback(XtPointer clientData, Atom *selType, Atom *type, XtPointer value, unsigned long *length, int *format);
	void getInsertSelectionCallback(XtPointer clientData, Atom *selType, Atom *type, XtPointer value, unsigned long *length, int *format);
	void cursorBlinkTimerProcEx(XtPointer clientData, XtIntervalId *id);
	void hScrollCallback(XtPointer clientData, XtPointer callData);
	void vScrollCallback(XtPointer clientData, XtPointer callData);
	void visibilityEventHandler(XtPointer data, XEvent *event, Boolean *continueDispatch);
	void bufPreDeleteCallback(int pos, int nDeleted);
	void bufModifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText);
	void autoScrollTimerProcEx(XtPointer clientData, XtIntervalId *id);
	void getSelectionCallback(XtPointer clientData, Atom *selType, Atom *type, XtPointer value, unsigned long *length, int *format);
	void loseMotifDestCallback(Atom *selType);
	void modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
	Boolean convertSelectionCallback(Atom *selType, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format);
	void loseSelectionCallback(Atom *selType);

public:
	void addFocusCallback(XtCallbackProc callback, XtPointer client_data);
	void addCursorMovementCallback(XtCallbackProc callback, XtPointer client_data);
	void addDragStartCallback(XtCallbackProc callback, XtPointer client_data);
	void addDragEndCallback(XtCallbackProc callback, XtPointer client_data);
	void addsmartIndentCallback(XtCallbackProc callback, XtPointer client_data);

public:
	// Events
	void backwardCharacterAP(XEvent *event, String *args, Cardinal *nArgs);
	void backwardParagraphAP(XEvent *event, String *args, Cardinal *nArgs);
	void backwardWordAP(XEvent *event, String *args, Cardinal *nArgs);
	void beginningOfFileAP(XEvent *event, String *args, Cardinal *nArgs);
	void beginningOfLineAP(XEvent *event, String *args, Cardinal *nArgs);
	void copyClipboardAP(XEvent *event, String *args, Cardinal *nArgs);
	void copyPrimaryAP(XEvent *event, String *args, Cardinal *nArgs);
	void copyToAP(XEvent *event, String *args, Cardinal *nArgs);
	void copyToOrEndDragAP(XEvent *event, String *args, Cardinal *nArgs);
	void cutClipboardAP(XEvent *event, String *args, Cardinal *nArgs);
	void cutPrimaryAP(XEvent *event, String *args, Cardinal *nArgs);
	void deleteNextCharacterAP(XEvent *event, String *args, Cardinal *nArgs);
	void deleteNextWordAP(XEvent *event, String *args, Cardinal *nArgs);
	void deletePreviousCharacterAP(XEvent *event, String *args, Cardinal *nArgs);
	void deletePreviousWordAP(XEvent *event, String *args, Cardinal *nArgs);
	void deleteSelectionAP(XEvent *event, String *args, Cardinal *nArgs);
	void deleteToEndOfLineAP(XEvent *event, String *args, Cardinal *nArgs);
	void deleteToStartOfLineAP(XEvent *event, String *args, Cardinal *nArgs);
	void deselectAllAP(XEvent *event, String *args, Cardinal *nArgs);
	void endDragAP(XEvent *event, String *args, Cardinal *nArgs);
	void endOfFileAP(XEvent *event, String *args, Cardinal *nArgs);	
	void endOfLineAP(XEvent *event, String *args, Cardinal *nArgs);	
	void exchangeAP(XEvent *event, String *args, Cardinal *nArgs);
	void extendAdjustAP(XEvent *event, String *args, Cardinal *nArgs);
	void extendEndAP(XEvent *event, String *args, Cardinal *nArgs);
	void extendStartAP(XEvent *event, String *args, Cardinal *nArgs);
	void focusInAP(XEvent *event, String *args, Cardinal *nArgs);
	void focusOutAP(XEvent *event, String *args, Cardinal *nArgs);
	void forwardCharacterAP(XEvent *event, String *args, Cardinal *nArgs);
	void forwardParagraphAP(XEvent *event, String *args, Cardinal *nArgs);
	void forwardWordAP(XEvent *event, String *args, Cardinal *nArgs);
	void grabFocusAP(XEvent *event, String *args, Cardinal *n_args);
	void insertStringAP(XEvent *event, String *args, Cardinal *nArgs);
	void keySelectAP(XEvent *event, String *args, Cardinal *nArgs);	
	void mousePanAP(XEvent *event, String *args, Cardinal *nArgs);
	void moveDestinationAP(XEvent *event, String *args, Cardinal *nArgs);
	void moveToAP(XEvent *event, String *args, Cardinal *nArgs);
	void moveToOrEndDragAP(XEvent *event, String *args, Cardinal *nArgs);
	void newlineAP(XEvent *event, String *args, Cardinal *nArgs);
	void newlineAndIndentAP(XEvent *event, String *args, Cardinal *nArgs);
	void newlineNoIndentAP(XEvent *event, String *args, Cardinal *nArgs);
	void nextPageAP(XEvent *event, String *args, Cardinal *nArgs);
	void pageLeftAP(XEvent *event, String *args, Cardinal *nArgs);
	void pageRightAP(XEvent *event, String *args, Cardinal *nArgs);	
	void pasteClipboardAP(XEvent *event, String *args, Cardinal *nArgs);
	void previousPageAP(XEvent *event, String *args, Cardinal *nArgs);
	void processCancelAP(XEvent *event, String *args, Cardinal *nArgs);
	void processDownAP(XEvent *event, String *args, Cardinal *nArgs);
	void processShiftDownAP(XEvent *event, String *args, Cardinal *nArgs);
	void processShiftUpAP(XEvent *event, String *args, Cardinal *nArgs);
	void processTabAP(XEvent *event, String *args, Cardinal *nArgs);
	void processUpAP(XEvent *event, String *args, Cardinal *nArgs);
	void scrollDownAP(XEvent *event, String *args, Cardinal *nArgs);
	void scrollLeftAP(XEvent *event, String *args, Cardinal *nArgs);
	void scrollRightAP(XEvent *event, String *args, Cardinal *nArgs);
	void scrollToLineAP(XEvent *event, String *args, Cardinal *nArgs);
	void scrollUpAP(XEvent *event, String *args, Cardinal *nArgs);
	void secondaryAdjustAP(XEvent *event, String *args, Cardinal *nArgs);
	void secondaryOrDragAdjustAP(XEvent *event, String *args, Cardinal *nArgs);
	void secondaryOrDragStartAP(XEvent *event, String *args, Cardinal *nArgs);
	void secondaryStartAP(XEvent *event, String *args, Cardinal *nArgs);
	void selectAllAP(XEvent *event, String *args, Cardinal *nArgs);
	void selfInsertAP(XEvent *event, String *args, Cardinal *n_args);
	void toggleOverstrikeAP(XEvent *event, String *args, Cardinal *nArgs);

public:
	int maintainingAbsTopLineNum() const;
	int updateHScrollBarRange();
	void adjustSecondarySelection(const Point &coord);
	void adjustSelection(const Point &coord);
	void blankCursorProtrusions();
	void extendRangeForStyleMods(int *start, int *end);
	void findWrapRangeEx(view::string_view deletedText, int pos, int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd, int *linesInserted, int *linesDeleted);
	void measureDeletedLines(int pos, int nDeleted);
	void redrawLineNumbers(int clearAll);
	void resetAbsLineNum();
	void sendSecondary(Time time, Atom sel, int action, char *actionText, int actionTextLen);
	void textDRedisplayRange(int start, int end);
	void updateLineStarts(int pos, int charsInserted, int charsDeleted, int linesInserted, int linesDeleted, int *scrolled);
	void updateVScrollBarRange();
	
public:
	int spanBackward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos);
	int spanForward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos);
	void TextDSetupBGClassesEx(Widget w, XmString str);
	
public:
	static void TextDSetupBGClasses(Widget w, XmString str, Pixel **pp_bgClassPixel, uint8_t **pp_bgClass, Pixel bgPixelDefault);
	static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id);
	static void handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch);
	static void handleShowPointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch);
	static Bool findGraphicsExposeOrNoExposeEvent(Display *theDisplay, XEvent *event, XPointer arg);
	static bool offscreenV(XWindowAttributes *screenAttr, int top, int height);

private:
	Widget w_; // TextWidget
	Widget calltipW_;                             // The Label widget for the calltip
	Widget calltipShell_;                         // The Shell that holds the calltip
	Pixel fgPixel_;                               // Foreground color
	Pixel bgPixel_;                               // Background color	
	Pixel *bgClassPixel_;                         // table of colors for each BG class
	uint8_t *bgClass_;                            // obtains index into bgClassPixel[]		
	Rect rect_;
	int lineNumLeft_;
	int lineNumWidth_;
	int cursorPos_;
	int cursorOn_;
	Point cursor_;                               // X pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
	int cursorToHint_;                            // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
	CursorStyles cursorStyle_;                    // One of enum cursorStyles above
	int cursorPreferredCol_;                      // Column for vert. cursor movement
	int nVisibleLines_;                           // # of visible (displayed) lines
	int nBufferLines_;                            // # of newlines in the buffer
	TextBuffer *buffer_;                          // Contains text to be displayed
	TextBuffer *styleBuffer_;                     // Optional parallel buffer containing color and font information
	int firstChar_;                               // Buffer positions of first and last displayed character (lastChar points either to a newline or one character beyond the end of the buffer)
	int lastChar_;
	int *lineStarts_;
	int topLineNum_;                              // Line number of top displayed line of file (first line of file is 1)
	int absTopLineNum_;                           // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
	int needAbsTopLineNum_;                       // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
	int horizOffset_;                             // Horizontal scroll pos. in pixels
	int visibility_;                              // Window visibility (see XVisibility event)
	int nStyles_;                                 // Number of entries in styleTable
	StyleTableEntry *styleTable_;                 // Table of fonts and colors for coloring/syntax-highlighting
	char unfinishedStyle_;                        // Style buffer entry which triggers on-the-fly reparsing of region
	unfinishedStyleCBProc unfinishedHighlightCB_; // Callback to parse "unfinished" regions
	void *highlightCBArg_;                        // Arg to unfinishedHighlightCB
	int ascent_;                                  // Composite ascent and descent for primary font + all-highlight fonts
	int descent_;
	int fixedFontWidth_;                          // Font width if all current fonts are fixed and match in width, else -1

	// GCs for drawing text
	GC gc_;
	GC selectGC_;
	GC highlightGC_;
	// GCs for erasing text
	GC selectBGGC_;
	GC highlightBGGC_;
	// GC for drawing the cursor
	GC cursorFGGC_;
	// GC for drawing line numbers
	GC lineNumGC_;
	// GC with color and font unspecified for drawing colored/styled text
	GC styleGC_;

	CallTip calltip_;                       // The info for the calltip itself
	bool suppressResync_;                          // Suppress resynchronization of line starts during buffer updates
	int nLinesDeleted_;                           // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
	int modifyingTabDist_;                        // Whether tab distance is being modified
	bool pointerHidden_;                          // true if the mouse pointer is hidden
	graphicExposeTranslationEntry *graphicsExposeQueue_;
	
	// NOTE(eteran): moved from textP
	TextBuffer *dragOrigBuf_;        // backup buffer copy used during block dragging of selections
	int dragXOffset_;                // offsets between cursor location and actual insertion point in drag
	int dragYOffset_;                // offsets between cursor location and actual insertion point in drag
	int dragType_;                   // style of block drag operation
	int dragInsertPos_;              // location where text being block dragged was last inserted
	int dragRectStart_;              // rect. offset ""
	int dragInserted_;               // # of characters inserted at drag destination in last drag position
	int dragDeleted_;                // # of characters deleted ""
	int dragSourceDeletePos_;        // location from which move source text was removed at start of drag
	int dragSourceInserted_;         // # of chars. inserted when move source text was deleted
	int dragSourceDeleted_;          // # of chars. deleted ""
	int dragNLines_;                 // # of newlines in text being drag'd
	
	int anchor_;                     // Anchor for drag operations
	int rectAnchor_;                 // Anchor for rectangular drag operations
	int dragState_;                  // Why is the mouse being dragged and what is being acquired
	int multiClickState_;            // How long is this multi-click sequence so far	
	Point btnDownCoord_;             // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
	Time lastBtnDown_;               // Timestamp of last button down event for multi-click recognition
	Point mouseCoord_;               // Last known mouse position in drag operation (for autoscroll)
	bool selectionOwner_;            // True if widget owns the selection
	bool motifDestOwner_;            // " "            owns the motif destination
	XtIntervalId autoScrollProcID_;  // id of Xt timer proc for autoscroll
	XtIntervalId cursorBlinkProcID_; // id of timer proc for cursor blink
	
private:
	// COPY OF RESOURCE?
	Pixel          selectFGPixel_;      // Foreground select color
	Pixel          selectBGPixel_;      // Background select color
	Pixel          highlightFGPixel_;   // Highlight colors are used when flashing matching parens
	Pixel          highlightBGPixel_;
	Pixel          lineNumFGPixel_;     // Color for drawing line numbers
	Pixel          cursorFGPixel_;
	Pixel          calltipFGPixel_;
	Pixel          calltipBGPixel_;	
	XFontStruct *  fontStruct_;         // Font structure for primary font	
	Widget         hScrollBar_; 
	Widget         vScrollBar_;
	bool           continuousWrap_;     // Wrap long lines when displaying
	int            wrapMargin_;         // Margin in # of char positions for wrapping in continuousWrap mode	
	int            emTabsBeforeCursor_; // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
};

#endif
