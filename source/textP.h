/*******************************************************************************
*                                                                              *
* textP.h -- Nirvana Editor Text Editing Widget private include file           *
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

#ifndef TEXTP_H_
#define TEXTP_H_

#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/CoreP.h>

class TextDisplay;

struct TextClassPart {
	int ignore;
};

struct TextClassRec {
	CoreClassPart        core_class;
	XmPrimitiveClassPart primitive_class;
	TextClassPart        text_class;
};

extern TextClassRec nTextClassRec;

class TextPart {
public:
	// resources
	Pixel          P_selectFGPixel;
	Pixel          P_selectBGPixel;
	Pixel          P_highlightFGPixel;
	Pixel          P_highlightBGPixel;
	Pixel          P_cursorFGPixel;
	Pixel          P_lineNumFGPixel;
	Pixel          P_calltipFGPixel;
	Pixel          P_calltipBGPixel;
	XFontStruct *  P_fontStruct;
	Boolean        P_pendingDelete;
	Boolean        P_autoShowInsertPos;
	Boolean        P_autoWrap;
	Boolean        P_autoWrapPastedText;
	Boolean        P_continuousWrap;
	Boolean        P_autoIndent;
	Boolean        P_smartIndent;
	Boolean        P_overstrike;
	Boolean        P_heavyCursor;
	Boolean        P_readOnly;
	Boolean        P_hidePointer;
	int            P_rows;
	int            P_columns;
	int            P_marginWidth;
	int            P_marginHeight;
	int            P_cursorBlinkRate;
	int            P_wrapMargin;
	int            P_emulateTabs;
	int            P_lineNumCols;
	char *         P_delimiters;
	Cardinal       P_cursorVPadding;
	Widget         P_hScrollBar;
	Widget         P_vScrollBar;

	/* NOTE(eteran): as "private", but looks like a resource */
	XmString P_backlightCharTypes;   // background class string to parse
	
	// these are set indirectly in Document::createTextArea
	XtCallbackList P_focusInCB;		
	XtCallbackList P_focusOutCB;		
	XtCallbackList P_cursorCB;		
	XtCallbackList P_dragStartCB;		
	XtCallbackList P_dragEndCB;		
	XtCallbackList P_smartIndentCB;	


	/* private state */
	TextDisplay *textD;             // Pointer to display information
};

struct TextRec {
	CorePart        core;
	XmPrimitivePart primitive;
	TextPart        text;
};

#endif
