
#include "TextSelection.h"
#include <algorithm>

/**
 * @brief TextSelection::setSelection
 * @param newStart
 * @param newEnd
 */
void TextSelection::setSelection(int newStart, int newEnd) {
	selected	= (newStart != newEnd);
	zeroWidth	= (newStart == newEnd);
	rectangular = false;
	start		= std::min(newStart, newEnd);
	end 		= std::max(newStart, newEnd);
}

/**
 * @brief TextSelection::setRectSelect
 * @param newStart
 * @param newEnd
 * @param newRectStart
 * @param newRectEnd
 */
void TextSelection::setRectSelect(int newStart, int newEnd, int newRectStart, int newRectEnd) {
	selected	= (newRectStart < newRectEnd);
	zeroWidth	= (newRectStart == newRectEnd);
	rectangular = true;
	start		= newStart;
	end 		= newEnd;
	rectStart	= newRectStart;
	rectEnd 	= newRectEnd;
}

/**
 * @brief TextSelection::getSelectionPos
 * @param start
 * @param end
 * @param isRect
 * @param rectStart
 * @param rectEnd
 * @return
 */
bool TextSelection::getSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const {
	// Always fill in the parameters (zero-width can be requested too). 
	*isRect = this->rectangular;
	*start  = this->start;
	*end    = this->end;
	if (this->rectangular) {
		*rectStart = this->rectStart;
		*rectEnd   = this->rectEnd;
	}
	return this->selected;
}

/**
 * @brief TextSelection::updateSelection
 * @param pos
 * @param nDeleted
 * @param nInserted
 *
 * Update an individual selection for changes in the corresponding text
 */
void TextSelection::updateSelection(int pos, int nDeleted, int nInserted) {
	if ((!selected && !zeroWidth) || pos > end) {
		return;
	}
	
	if (pos + nDeleted <= start) {
		start += nInserted - nDeleted;
		end   += nInserted - nDeleted;
	} else if (pos <= start && pos + nDeleted >= end) {
		start     = pos;
		end       = pos;
		selected  = false;
		zeroWidth = false;
	} else if (pos <= start && pos + nDeleted < end) {
		start = pos;
		end   = nInserted + end - nDeleted;
	} else if (pos < end) {
		end += nInserted - nDeleted;
		if (end <= start) {
			selected = false;
		}
	}
}

/*
** Return true if position "pos" with indentation "dispIndex" is in this
** selection
*/
bool TextSelection::inSelection(int pos, int lineStartPos, int dispIndex) const {
    return this->selected && ((!rectangular && pos >= start && pos < end) || (rectangular && pos >= start && lineStartPos <= end && dispIndex >= rectStart && dispIndex < rectEnd));
}

/*
** Return true if this selection is rectangular, and touches a buffer position
** within "rangeStart" to "rangeEnd"
*/
bool TextSelection::rangeTouchesRectSel(int rangeStart, int rangeEnd) const {
    return selected && rectangular && end >= rangeStart && start <= rangeEnd;
}
