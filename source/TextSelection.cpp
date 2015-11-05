
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
int TextSelection::getSelectionPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd) {
	/* Always fill in the parameters (zero-width can be requested too). */
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
