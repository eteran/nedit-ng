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
static int findRelativeLineStart(TextBuffer *buf, int referencePos, int referenceLineNum, int newLineNum);
static int min3(int i1, int i2, int i3);
static int max3(int i1, int i2, int i3);


/*
** Reposition the primary-selected text that is being dragged as a block
** for a new mouse position of (x, y)
*/
void BlockDragSelection(TextWidget tw, Point pos, int dragType) {
	TextDisplay *textD = textD_of(tw);
	TextBuffer *buf = textD->buffer;
	int fontHeight = textD->fontStruct->ascent + textD->fontStruct->descent;
	int fontWidth = textD->fontStruct->max_bounds.width;
	TextBuffer *origBuf = text_of(tw).dragOrigBuf;
	int dragXOffset = text_of(tw).dragXOffset;
	TextBuffer *tempBuf;
	TextSelection *origSel = &origBuf->primary_;
	int rectangular = origSel->rectangular;
	int overlay, oldDragType = text_of(tw).dragType;
	int nLines = text_of(tw).dragNLines;
	int insLineNum, insLineStart, insRectStart, insRectEnd, insStart;
	int modRangeStart = -1, tempModRangeEnd = -1, bufModRangeEnd = -1;
	int referenceLine, referencePos, tempStart, tempEnd, origSelLen;
	int insertInserted, insertDeleted, row, column;
	int origSelLineStart, origSelLineEnd;
	int sourceInserted, sourceDeleted, sourceDeletePos;

	if (text_of(tw).dragState != PRIMARY_BLOCK_DRAG)
		return;

	/* The operation of block dragging is simple in theory, but not so simple
	   in practice.  There is a backup buffer (text_of(tw).dragOrigBuf) which
	   holds a copy of the buffer as it existed before the drag.  When the
	   user drags the mouse to a new location, this routine is called, and
	   a temporary buffer is created and loaded with the local part of the
	   buffer (from the backup) which might be changed by the drag.  The
	   changes are all made to this temporary buffer, and the parts of this
	   buffer which then differ from the real (displayed) buffer are used to
	   replace those parts, thus one replace operation serves as both undo
	   and modify.  This double-buffering of the operation prevents excessive
	   redrawing (though there is still plenty of needless redrawing due to
	   re-selection and rectangular operations).

	   The hard part is keeping track of the changes such that a single replace
	   operation will do everyting.  This is done using a routine called
	   trackModifyRange which tracks expanding ranges of changes in the two
	   buffers in modRangeStart, tempModRangeEnd, and bufModRangeEnd. */

	/* Create a temporary buffer for accumulating changes which will
	   eventually be replaced in the real buffer.  Load the buffer with the
	   range of characters which might be modified in this drag step
	   (this could be tighter, but hopefully it's not too slow) */
	tempBuf = new TextBuffer;
	tempBuf->tabDist_ = buf->tabDist_;
	tempBuf->useTabs_ = buf->useTabs_;
	tempStart = min3(text_of(tw).dragInsertPos, origSel->start, buf->BufCountBackwardNLines(textD->firstChar, nLines + 2));
	tempEnd = buf->BufCountForwardNLines(max3(text_of(tw).dragInsertPos, origSel->start, textD->lastChar), nLines + 2) + origSel->end - origSel->start;
	std::string text = origBuf->BufGetRangeEx(tempStart, tempEnd);
	tempBuf->BufSetAllEx(text);

	// If the drag type is USE_LAST, use the last dragType applied 
	if (dragType == USE_LAST)
		dragType = text_of(tw).dragType;
	overlay = dragType == DRAG_OVERLAY_MOVE || dragType == DRAG_OVERLAY_COPY;

	/* Overlay mode uses rectangular selections whether or not the original
	   was rectangular.  To use a plain selection as if it were rectangular,
	   the start and end positions need to be moved to the line boundaries
	   and trailing newlines must be excluded */
	origSelLineStart = origBuf->BufStartOfLine(origSel->start);
	if (!rectangular && origBuf->BufGetCharacter(origSel->end - 1) == '\n')
		origSelLineEnd = origSel->end - 1;
	else
		origSelLineEnd = origBuf->BufEndOfLine(origSel->end);
	if (!rectangular && overlay && nLines != 0)
		dragXOffset -= fontWidth * (origSel->rectStart - (origSel->start - origSelLineStart));

	/* If the drag operation is of a different type than the last one, and the
	   operation is a move, expand the modified-range to include undoing the
	   text-removal at the site from which the text was dragged. */
	if (dragType != oldDragType && text_of(tw).dragSourceDeleted != 0)
		trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, text_of(tw).dragSourceDeletePos, text_of(tw).dragSourceInserted, text_of(tw).dragSourceDeleted);

	/* Do, or re-do the original text removal at the site where a move began.
	   If this part has not changed from the last call, do it silently to
	   bring the temporary buffer in sync with the real (displayed)
	   buffer.  If it's being re-done, track the changes to complete the
	   redo operation begun above */
	if (dragType == DRAG_MOVE || dragType == DRAG_OVERLAY_MOVE) {
		if (rectangular || overlay) {
			int prevLen = tempBuf->BufGetLength();
			origSelLen = origSelLineEnd - origSelLineStart;
			if (overlay)
				tempBuf->BufClearRect(origSelLineStart - tempStart, origSelLineEnd - tempStart, origSel->rectStart, origSel->rectEnd);
			else
				tempBuf->BufRemoveRect(origSelLineStart - tempStart, origSelLineEnd - tempStart, origSel->rectStart, origSel->rectEnd);
			sourceDeletePos = origSelLineStart;
			sourceInserted = origSelLen - prevLen + tempBuf->BufGetLength();
			sourceDeleted = origSelLen;
		} else {
			tempBuf->BufRemove(origSel->start - tempStart, origSel->end - tempStart);
			sourceDeletePos = origSel->start;
			sourceInserted = 0;
			sourceDeleted = origSel->end - origSel->start;
		}
		if (dragType != oldDragType)
			trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, sourceDeletePos, sourceInserted, sourceDeleted);
	} else {
		sourceDeletePos = 0;
		sourceInserted = 0;
		sourceDeleted = 0;
	}

	/* Expand the modified-range to include undoing the insert from the last
	   call. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, text_of(tw).dragInsertPos, text_of(tw).dragInserted, text_of(tw).dragDeleted);

	/* Find the line number and column of the insert position.  Note that in
	   continuous wrap mode, these must be calculated as if the text were
	   not wrapped */
	textD->TextDXYToUnconstrainedPosition(
		Point{
			std::max<int>(0, pos.x - dragXOffset), 
			std::max<int>(0, pos.y - (text_of(tw).dragYOffset % fontHeight))
		},
		&row, &column);
		
		
	column = textD->TextDOffsetWrappedColumn(row, column);
	row = textD->TextDOffsetWrappedRow(row);
	insLineNum = row + textD->topLineNum - text_of(tw).dragYOffset / fontHeight;

	/* find a common point of reference between the two buffers, from which
	   the insert position line number can be translated to a position */
	if (textD->firstChar > modRangeStart) {
		referenceLine = textD->topLineNum - buf->BufCountLines(modRangeStart, textD->firstChar);
		referencePos = modRangeStart;
	} else {
		referencePos = textD->firstChar;
		referenceLine = textD->topLineNum;
	}

	/* find the position associated with the start of the new line in the
	   temporary buffer */
	insLineStart = findRelativeLineStart(tempBuf, referencePos - tempStart, referenceLine, insLineNum) + tempStart;
	if (insLineStart - tempStart == tempBuf->BufGetLength())
		insLineStart = tempBuf->BufStartOfLine(insLineStart - tempStart) + tempStart;

	// Find the actual insert position 
	if (rectangular || overlay) {
		insStart = insLineStart;
		insRectStart = column;
	} else { // note, this will fail with proportional fonts 
		insStart = tempBuf->BufCountForwardDispChars(insLineStart - tempStart, column) + tempStart;
		insRectStart = 0;
	}

	/* If the position is the same as last time, don't bother drawing (it
	   would be nice if this decision could be made earlier) */
	if (insStart == text_of(tw).dragInsertPos && insRectStart == text_of(tw).dragRectStart && dragType == oldDragType) {
		delete tempBuf;
		return;
	}

	// Do the insert in the temporary buffer 
	if (rectangular || overlay) {
		std::string insText = origBuf->BufGetTextInRectEx(origSelLineStart, origSelLineEnd, origSel->rectStart, origSel->rectEnd);
		if (overlay)
			tempBuf->BufOverlayRectEx(insStart - tempStart, insRectStart, insRectStart + origSel->rectEnd - origSel->rectStart, insText, &insertInserted, &insertDeleted);
		else
			tempBuf->BufInsertColEx(insRectStart, insStart - tempStart, insText, &insertInserted, &insertDeleted);
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, insertInserted, insertDeleted);
	} else {
		std::string insText = origBuf->BufGetSelectionTextEx();
		tempBuf->BufInsertEx(insStart - tempStart, insText);
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, origSel->end - origSel->start, 0);
		insertInserted = origSel->end - origSel->start;
		insertDeleted = 0;
	}

	// Make the changes in the real buffer 
	std::string repText = tempBuf->BufGetRangeEx(modRangeStart - tempStart, tempModRangeEnd - tempStart);
	delete tempBuf;
	textD->TextDBlankCursor();
	buf->BufReplaceEx(modRangeStart, bufModRangeEnd, repText);

	// Store the necessary information for undoing this step 
	text_of(tw).dragInsertPos = insStart;
	text_of(tw).dragRectStart = insRectStart;
	text_of(tw).dragInserted = insertInserted;
	text_of(tw).dragDeleted = insertDeleted;
	text_of(tw).dragSourceDeletePos = sourceDeletePos;
	text_of(tw).dragSourceInserted = sourceInserted;
	text_of(tw).dragSourceDeleted = sourceDeleted;
	text_of(tw).dragType = dragType;

	// Reset the selection and cursor position 
	if (rectangular || overlay) {
		insRectEnd = insRectStart + origSel->rectEnd - origSel->rectStart;
		buf->BufRectSelect(insStart, insStart + insertInserted, insRectStart, insRectEnd);
		textD->TextDSetInsertPosition(buf->BufCountForwardDispChars(buf->BufCountForwardNLines(insStart, text_of(tw).dragNLines), insRectEnd));
	} else {
		buf->BufSelect(insStart, insStart + origSel->end - origSel->start);
		textD->TextDSetInsertPosition(insStart + origSel->end - origSel->start);
	}
	textD->TextDUnblankCursor();
	XtCallCallbacks((Widget)tw, textNcursorMovementCallback, (XtPointer) nullptr);
	text_of(tw).emTabsBeforeCursor = 0;
}

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

/*
** Find a text position in buffer "buf" by counting forward or backward
** from a reference position with known line number
*/
static int findRelativeLineStart(TextBuffer *buf, int referencePos, int referenceLineNum, int newLineNum) {
	if (newLineNum < referenceLineNum)
		return buf->BufCountBackwardNLines(referencePos, referenceLineNum - newLineNum);
	else if (newLineNum > referenceLineNum)
		return buf->BufCountForwardNLines(referencePos, newLineNum - referenceLineNum);
	return buf->BufStartOfLine(referencePos);
}

static int min3(int i1, int i2, int i3) {
	if (i1 <= i2 && i1 <= i3)
		return i1;
	return i2 <= i3 ? i2 : i3;
}

static int max3(int i1, int i2, int i3) {
	if (i1 >= i2 && i1 >= i3)
		return i1;
	return i2 >= i3 ? i2 : i3;
}
