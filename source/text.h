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

#include "string_view.h"
#include <X11/Intrinsic.h>
#include <X11/X.h>
#include <X11/Xlib.h>

/* Resource strings */
#define textNfont const_cast<char *>("font")
#define textCFont const_cast<char *>("Font")
#define textNrows const_cast<char *>("rows")
#define textCRows const_cast<char *>("Rows")
#define textNcolumns const_cast<char *>("columns")
#define textCColumns const_cast<char *>("Columns")
#define textNmarginWidth const_cast<char *>("marginWidth")
#define textCMarginWidth const_cast<char *>("MarginWidth")
#define textNmarginHeight const_cast<char *>("marginHeight")
#define textCMarginHeight const_cast<char *>("MarginHeight")
#define textNselectForeground const_cast<char *>("selectForeground")
#define textCSelectForeground const_cast<char *>("SelectForeground")
#define textNselectBackground const_cast<char *>("selectBackground")
#define textCSelectBackground const_cast<char *>("SelectBackground")
#define textNhighlightForeground const_cast<char *>("highlightForeground")
#define textCHighlightForeground const_cast<char *>("HighlightForeground")
#define textNhighlightBackground const_cast<char *>("highlightBackground")
#define textCHighlightBackground const_cast<char *>("HighlightBackground")
#define textNcursorForeground const_cast<char *>("cursorForeground")
#define textCCursorForeground const_cast<char *>("CursorForeground")
#define textNlineNumForeground const_cast<char *>("lineNumForeground")
#define textCLineNumForeground const_cast<char *>("LineNumForeground")
#define textNcalltipForeground const_cast<char *>("calltipForeground")
#define textCcalltipForeground const_cast<char *>("CalltipForeground")
#define textNcalltipBackground const_cast<char *>("calltipBackground")
#define textCcalltipBackground const_cast<char *>("CalltipBackground")
#define textNpendingDelete const_cast<char *>("pendingDelete")
#define textCPendingDelete const_cast<char *>("PendingDelete")
#define textNhScrollBar const_cast<char *>("hScrollBar")
#define textCHScrollBar const_cast<char *>("HScrollBar")
#define textNvScrollBar const_cast<char *>("vScrollBar")
#define textCVScrollBar const_cast<char *>("VScrollBar")
#define textNlineNumCols const_cast<char *>("lineNumCols")
#define textCLineNumCols const_cast<char *>("LineNumCols")
#define textNautoShowInsertPos const_cast<char *>("autoShowInsertPos")
#define textCAutoShowInsertPos const_cast<char *>("AutoShowInsertPos")
#define textNautoWrapPastedText const_cast<char *>("autoWrapPastedText")
#define textCAutoWrapPastedText const_cast<char *>("AutoWrapPastedText")
#define textNwordDelimiters const_cast<char *>("wordDelimiters")
#define textCWordDelimiters const_cast<char *>("WordDelimiters")
#define textNblinkRate const_cast<char *>("blinkRate")
#define textCBlinkRate const_cast<char *>("BlinkRate")
#define textNfocusCallback const_cast<char *>("focusCallback")
#define textCFocusCallback const_cast<char *>("FocusCallback")
#define textNlosingFocusCallback const_cast<char *>("losingFocusCallback")
#define textCLosingFocusCallback const_cast<char *>("LosingFocusCallback")
#define textNcursorMovementCallback const_cast<char *>("cursorMovementCallback")
#define textCCursorMovementCallback const_cast<char *>("CursorMovementCallback")
#define textNdragStartCallback const_cast<char *>("dragStartCallback")
#define textCDragStartCallback const_cast<char *>("DragStartCallback")
#define textNdragEndCallback const_cast<char *>("dragEndCallback")
#define textCDragEndCallback const_cast<char *>("DragEndCallback")
#define textNsmartIndentCallback const_cast<char *>("smartIndentCallback")
#define textCSmartIndentCallback const_cast<char *>("SmartIndentCallback")
#define textNautoWrap const_cast<char *>("autoWrap")
#define textCAutoWrap const_cast<char *>("AutoWrap")
#define textNcontinuousWrap const_cast<char *>("continuousWrap")
#define textCContinuousWrap const_cast<char *>("ContinuousWrap")
#define textNwrapMargin const_cast<char *>("wrapMargin")
#define textCWrapMargin const_cast<char *>("WrapMargin")
#define textNautoIndent const_cast<char *>("autoIndent")
#define textCAutoIndent const_cast<char *>("AutoIndent")
#define textNsmartIndent const_cast<char *>("smartIndent")
#define textCSmartIndent const_cast<char *>("SmartIndent")
#define textNoverstrike const_cast<char *>("overstrike")
#define textCOverstrike const_cast<char *>("Overstrike")
#define textNheavyCursor const_cast<char *>("heavyCursor")
#define textCHeavyCursor const_cast<char *>("HeavyCursor")
#define textNreadOnly const_cast<char *>("readOnly")
#define textCReadOnly const_cast<char *>("ReadOnly")
#define textNhidePointer const_cast<char *>("hidePointer")
#define textCHidePointer const_cast<char *>("HidePointer")
#define textNemulateTabs const_cast<char *>("emulateTabs")
#define textCEmulateTabs const_cast<char *>("EmulateTabs")
#define textNcursorVPadding const_cast<char *>("cursorVPadding")
#define textCCursorVPadding const_cast<char *>("CursorVPadding")
#define textNbacklightCharTypes const_cast<char *>("backlightCharTypes")
#define textCBacklightCharTypes const_cast<char *>("BacklightCharTypes")

extern WidgetClass textWidgetClass;

struct TextClassRec;
struct TextRec;

typedef TextRec *TextWidget;

struct dragEndCBStruct {
	int               startPos;
	int               nCharsDeleted;
	int               nCharsInserted;
	view::string_view deletedText;
};

enum SmartIndentCallbackReasons {
	NEWLINE_INDENT_NEEDED, 
	CHAR_TYPED
};

struct smartIndentCBStruct {
	SmartIndentCallbackReasons reason;
	int pos;
	int indentRequest;
	char *charsTyped;
};

/* User callable routines */
XtActionsRec *TextGetActions(int *nActions);

#endif
