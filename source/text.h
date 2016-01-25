/*******************************************************************************
*                                                                              *
* text.h -- Nirvana Editor Text Widget Header File                             *
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

#ifndef TEXT_H_
#define TEXT_H_

#include "TextBuffer.h"
#include <string>

#include <X11/Intrinsic.h>
#include <X11/X.h>
#include <X11/Xlib.h>

/* Resource strings */
#define textNfont (String)"font"
#define textCFont (String)"Font"
#define textNrows (String)"rows"
#define textCRows (String)"Rows"
#define textNcolumns (String)"columns"
#define textCColumns (String)"Columns"
#define textNmarginWidth (String)"marginWidth"
#define textCMarginWidth (String)"MarginWidth"
#define textNmarginHeight (String)"marginHeight"
#define textCMarginHeight (String)"MarginHeight"
#define textNselectForeground (String)"selectForeground"
#define textCSelectForeground (String)"SelectForeground"
#define textNselectBackground (String)"selectBackground"
#define textCSelectBackground (String)"SelectBackground"
#define textNhighlightForeground (String)"highlightForeground"
#define textCHighlightForeground (String)"HighlightForeground"
#define textNhighlightBackground (String)"highlightBackground"
#define textCHighlightBackground (String)"HighlightBackground"
#define textNcursorForeground (String)"cursorForeground"
#define textCCursorForeground (String)"CursorForeground"
#define textNlineNumForeground (String)"lineNumForeground"
#define textCLineNumForeground (String)"LineNumForeground"
#define textNcalltipForeground (String)"calltipForeground"
#define textCcalltipForeground (String)"CalltipForeground"
#define textNcalltipBackground (String)"calltipBackground"
#define textCcalltipBackground (String)"CalltipBackground"
#define textNpendingDelete (String)"pendingDelete"
#define textCPendingDelete (String)"PendingDelete"
#define textNhScrollBar (String)"hScrollBar"
#define textCHScrollBar (String)"HScrollBar"
#define textNvScrollBar (String)"vScrollBar"
#define textCVScrollBar (String)"VScrollBar"
#define textNlineNumCols (String)"lineNumCols"
#define textCLineNumCols (String)"LineNumCols"
#define textNautoShowInsertPos (String)"autoShowInsertPos"
#define textCAutoShowInsertPos (String)"AutoShowInsertPos"
#define textNautoWrapPastedText (String)"autoWrapPastedText"
#define textCAutoWrapPastedText (String)"AutoWrapPastedText"
#define textNwordDelimiters (String)"wordDelimiters"
#define textCWordDelimiters (String)"WordDelimiters"
#define textNblinkRate (String)"blinkRate"
#define textCBlinkRate (String)"BlinkRate"
#define textNfocusCallback (String)"focusCallback"
#define textCFocusCallback (String)"FocusCallback"
#define textNlosingFocusCallback (String)"losingFocusCallback"
#define textCLosingFocusCallback (String)"LosingFocusCallback"
#define textNcursorMovementCallback (String)"cursorMovementCallback"
#define textCCursorMovementCallback (String)"CursorMovementCallback"
#define textNdragStartCallback (String)"dragStartCallback"
#define textCDragStartCallback (String)"DragStartCallback"
#define textNdragEndCallback (String)"dragEndCallback"
#define textCDragEndCallback (String)"DragEndCallback"
#define textNsmartIndentCallback (String)"smartIndentCallback"
#define textCSmartIndentCallback (String)"SmartIndentCallback"
#define textNautoWrap (String)"autoWrap"
#define textCAutoWrap (String)"AutoWrap"
#define textNcontinuousWrap (String)"continuousWrap"
#define textCContinuousWrap (String)"ContinuousWrap"
#define textNwrapMargin (String)"wrapMargin"
#define textCWrapMargin (String)"WrapMargin"
#define textNautoIndent (String)"autoIndent"
#define textCAutoIndent (String)"AutoIndent"
#define textNsmartIndent (String)"smartIndent"
#define textCSmartIndent (String)"SmartIndent"
#define textNoverstrike (String)"overstrike"
#define textCOverstrike (String)"Overstrike"
#define textNheavyCursor (String)"heavyCursor"
#define textCHeavyCursor (String)"HeavyCursor"
#define textNreadOnly (String)"readOnly"
#define textCReadOnly (String)"ReadOnly"
#define textNhidePointer (String)"hidePointer"
#define textCHidePointer (String)"HidePointer"
#define textNemulateTabs (String)"emulateTabs"
#define textCEmulateTabs (String)"EmulateTabs"
#define textNcursorVPadding (String)"cursorVPadding"
#define textCCursorVPadding (String)"CursorVPadding"
#define textNbacklightCharTypes (String)"backlightCharTypes"
#define textCBacklightCharTypes (String)"BacklightCharTypes"

extern WidgetClass textWidgetClass;

struct TextClassRec;
struct TextRec;

typedef struct TextRec *TextWidget;

struct dragEndCBStruct {
	int               startPos;
	int               nCharsDeleted;
	int               nCharsInserted;
	view::string_view deletedText;
};

enum smartIndentCallbackReasons { NEWLINE_INDENT_NEEDED, CHAR_TYPED };
struct smartIndentCBStruct {
	int reason;
	int pos;
	int indentRequest;
	char *charsTyped;
};

/* User callable routines */

XtActionsRec *TextGetActions(int *nActions);
int TextFirstVisibleLine(Widget w);
int TextFirstVisiblePos(Widget w);
int TextGetCursorPos(Widget w);
int TextGetMaxFontWidth(Widget w, Boolean considerStyles);
int TextGetMinFontWidth(Widget w, Boolean considerStyles);
int TextLastVisiblePos(Widget w);
int TextLineAndColToPos(Widget w, int lineNum, int column);
int TextNumVisibleLines(Widget w);
int TextPosToLineAndCol(Widget w, int pos, int *lineNum, int *column);
int TextPosToXY(Widget w, int pos, int *x, int *y);
int TextVisibleWidth(Widget w);
std::string TextGetWrappedEx(Widget w, int startPos, int endPos);
TextBuffer *TextGetBuffer(Widget w);
void HandleAllPendingGraphicsExposeNoExposeEvents(TextWidget w, XEvent *event);
void ResetCursorBlink(TextWidget textWidget, Boolean startsBlanked);
void ShowHidePointer(TextWidget w, Boolean hidePointer);
void TextColPasteClipboard(Widget w, Time time);
void TextCopyClipboard(Widget w, Time time);
void TextCutClipboard(Widget w, Time time);
void TextGetScroll(Widget w, int *topLineNum, int *horizOffset);
void TextHandleXSelections(Widget w);
void TextInsertAtCursorEx(Widget w, view::string_view chars, XEvent *event, int allowPendingDelete, int allowWrap);
void TextPasteClipboard(Widget w, Time time);
void TextSetBuffer(Widget w, TextBuffer *buffer);
void TextSetCursorPos(Widget w, int pos);
void TextSetScroll(Widget w, int topLineNum, int horizOffset);

#endif
