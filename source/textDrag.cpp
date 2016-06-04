/*******************************************************************************
*                                                                              *
* textDrag.c - Text Dragging routines for NEdit text widget                    *
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
* Dec. 15, 1995                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "textDrag.h"
#include "TextBuffer.h"
#include "TextDisplay.h"
#include "textP.h"
#include "TextHelper.h"

#include <climits>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>

static void trackModifyRange(int *rangeStart, int *modRangeEnd, int *unmodRangeEnd, int modPos, int nInserted, int nDeleted);


/*
** Complete a block text drag operation
*/
void FinishBlockDrag(TextWidget tw) {
	
	int modRangeStart = -1;
	int origModRangeEnd;
	int bufModRangeEnd;

	/* Find the changed region of the buffer, covering both the deletion
	   of the selected text at the drag start position, and insertion at
	   the drag destination */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, text_of(tw).dragSourceDeletePos, text_of(tw).dragSourceInserted, text_of(tw).dragSourceDeleted);
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, text_of(tw).dragInsertPos, text_of(tw).dragInserted, text_of(tw).dragDeleted);

	// Get the original (pre-modified) range of text from saved backup buffer 
	view::string_view deletedText = text_of(tw).dragOrigBuf->BufGetRangeEx(modRangeStart, origModRangeEnd);

	// Free the backup buffer 
	delete text_of(tw).dragOrigBuf;

	// Return to normal drag state 
	text_of(tw).dragState = NOT_CLICKED;

	// Call finish-drag calback 
	dragEndCBStruct endStruct;
	endStruct.startPos       = modRangeStart;
	endStruct.nCharsDeleted  = origModRangeEnd - modRangeStart;
	endStruct.nCharsInserted = bufModRangeEnd  - modRangeStart;
	endStruct.deletedText    = deletedText;

	XtCallCallbacks((Widget)tw, textNdragEndCallback, &endStruct);
}

/*
** Cancel a block drag operation
*/
void CancelBlockDrag(TextWidget tw) {
	TextBuffer *buf = textD_of(tw)->buffer;
	TextBuffer *origBuf = text_of(tw).dragOrigBuf;
	TextSelection *origSel = &origBuf->primary_;
	int modRangeStart = -1, origModRangeEnd, bufModRangeEnd;

	/* If the operation was a move, make the modify range reflect the
	   removal of the text from the starting position */
	if (text_of(tw).dragSourceDeleted != 0)
		trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, text_of(tw).dragSourceDeletePos, text_of(tw).dragSourceInserted, text_of(tw).dragSourceDeleted);

	/* Include the insert being undone from the last step in the modified
	   range. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, text_of(tw).dragInsertPos, text_of(tw).dragInserted, text_of(tw).dragDeleted);

	// Make the changes in the buffer 
	std::string repText = origBuf->BufGetRangeEx(modRangeStart, origModRangeEnd);
	buf->BufReplaceEx(modRangeStart, bufModRangeEnd, repText);

	// Reset the selection and cursor position 
	if (origSel->rectangular)
		buf->BufRectSelect(origSel->start, origSel->end, origSel->rectStart, origSel->rectEnd);
	else
		buf->BufSelect(origSel->start, origSel->end);
	textD_of(tw)->TextDSetInsertPosition(buf->cursorPosHint_);
	XtCallCallbacks((Widget)tw, textNcursorMovementCallback, nullptr);
	text_of(tw).emTabsBeforeCursor = 0;

	// Free the backup buffer 
	delete origBuf;

	// Indicate end of drag 
	text_of(tw).dragState = DRAG_CANCELED;

	// Call finish-drag calback 
	dragEndCBStruct endStruct;	
	endStruct.startPos       = 0;
	endStruct.nCharsDeleted  = 0;
	endStruct.nCharsInserted = 0;
	XtCallCallbacks((Widget)tw, textNdragEndCallback, &endStruct);
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
static void trackModifyRange(int *rangeStart, int *modRangeEnd, int *unmodRangeEnd, int modPos, int nInserted, int nDeleted) {
	if (*rangeStart == -1) {
		*rangeStart = modPos;
		*modRangeEnd = modPos + nInserted;
		*unmodRangeEnd = modPos + nDeleted;
	} else {
		if (modPos < *rangeStart)
			*rangeStart = modPos;
		if (modPos + nDeleted > *modRangeEnd) {
			*unmodRangeEnd += modPos + nDeleted - *modRangeEnd;
			*modRangeEnd = modPos + nInserted;
		} else
			*modRangeEnd += nInserted - nDeleted;
	}
}
