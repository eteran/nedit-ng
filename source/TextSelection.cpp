
#include "TextSelection.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: TextSelection
//------------------------------------------------------------------------------
TextSelection::TextSelection() : selected(false), rectangular(false), zeroWidth(false), start(0), end(0), rectStart(0), rectEnd(0) {

}

//------------------------------------------------------------------------------
// Name: setSelection
//------------------------------------------------------------------------------
void TextSelection::setSelection(int newStart, int newEnd) {
	selected	= (newStart != newEnd);
	zeroWidth	= (newStart == newEnd);
	rectangular = false;
	start		= std::min<int>(newStart, newEnd);
	end 		= std::max<int>(newStart, newEnd);
}

//------------------------------------------------------------------------------
// Name: setRectSelect
//------------------------------------------------------------------------------
void TextSelection::setRectSelect(int newStart, int newEnd, int newRectStart, int newRectEnd) {
	selected	= (newRectStart < newRectEnd);
	zeroWidth	= (newRectStart == newRectEnd);
	rectangular = true;
	start		= newStart;
	end 		= newEnd;
	rectStart	= newRectStart;
	rectEnd 	= newRectEnd;
}

//------------------------------------------------------------------------------
// Name: getSelectionPos
//------------------------------------------------------------------------------
int TextSelection::getSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) {
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

//------------------------------------------------------------------------------
// Name: 
// Desc: Update an individual selection for changes in the corresponding text
//------------------------------------------------------------------------------
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
** Return true if position "pos" with indentation "dispIndex" is in
** selection "this"
*/
int TextSelection::inSelection(int pos, int lineStartPos, int dispIndex) const {
	return this->selected && ((!this->rectangular && pos >= this->start && pos < this->end) || (this->rectangular && pos >= this->start && lineStartPos <= this->end && dispIndex >= this->rectStart && dispIndex < this->rectEnd));
}


/*
** Return true if the selection "this" is rectangular, and touches a
** buffer position withing "rangeStart" to "rangeEnd"
*/
int TextSelection::rangeTouchesRectSel(int rangeStart, int rangeEnd) const {
	return this->selected && this->rectangular && this->end >= rangeStart && this->start <= rangeEnd;
}
