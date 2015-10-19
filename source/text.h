/*******************************************************************************
*                                                                              *
* text.h -- Nirvana Editor Text Widget Header File                            *
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

#ifndef NEDIT_TEXT_H_INCLUDED
#define NEDIT_TEXT_H_INCLUDED

#include "textBuf.h"

#include <X11/Intrinsic.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

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

struct _TextClassRec;
struct _TextRec;

typedef struct _TextRec *TextWidget;

typedef struct {
    int startPos;
    int nCharsDeleted;
    int nCharsInserted;
    char *deletedText;
} dragEndCBStruct;

enum smartIndentCallbackReasons {NEWLINE_INDENT_NEEDED, CHAR_TYPED};
typedef struct {
    int reason;
    int pos;
    int indentRequest;
    char *charsTyped;
} smartIndentCBStruct;

/* User callable routines */
void TextSetBuffer(Widget w, textBuffer *buffer);
textBuffer *TextGetBuffer(Widget w);
int TextLineAndColToPos(Widget w, int lineNum, int column);
int TextPosToLineAndCol(Widget w, int pos, int *lineNum, int *column);
int TextPosToXY(Widget w, int pos, int *x, int *y);
int TextGetCursorPos(Widget w);
void TextSetCursorPos(Widget w, int pos);
void TextGetScroll(Widget w, int *topLineNum, int *horizOffset);
void TextSetScroll(Widget w, int topLineNum, int horizOffset);
int TextGetMinFontWidth(Widget w, Boolean considerStyles);
int TextGetMaxFontWidth(Widget w, Boolean considerStyles);
void TextHandleXSelections(Widget w);
void TextPasteClipboard(Widget w, Time time);
void TextColPasteClipboard(Widget w, Time time);
void TextCopyClipboard(Widget w, Time time);
void TextCutClipboard(Widget w, Time time);
int TextFirstVisibleLine(Widget w);
int TextNumVisibleLines(Widget w);
int TextVisibleWidth(Widget w);
void TextInsertAtCursor(Widget w, const char *chars, XEvent *event,
    	int allowPendingDelete, int allowWrap);
int TextFirstVisiblePos(Widget w);
int TextLastVisiblePos(Widget w);
char *TextGetWrapped(Widget w, int startPos, int endPos, int *length);
XtActionsRec *TextGetActions(int *nActions);
void ShowHidePointer(TextWidget w, Boolean hidePointer);
void ResetCursorBlink(TextWidget textWidget, Boolean startsBlanked);

void HandleAllPendingGraphicsExposeNoExposeEvents(TextWidget w, XEvent *event);

#ifdef __cplusplus
}
#endif

#endif /* NEDIT_TEXT_H_INCLUDED */
