
#include "PatternSet.h"
#include "HighlightPattern.h"

#include <algorithm>

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
PatternSet::PatternSet() : lineContext(0), charContext(0) {
}


//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void PatternSet::swap(PatternSet &other) {
	using std::swap;

	swap(languageMode, other.languageMode);
	swap(lineContext,  other.lineContext);
	swap(charContext,  other.charContext);
	swap(patterns,     other.patterns);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PatternSet::operator!=(const PatternSet &rhs) const {

	if (this->lineContext != rhs.lineContext) {
		return true;
	}

	if (this->charContext != rhs.charContext) {
		return true;
	}

	if(this->patterns != rhs.patterns) {
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PatternSet::operator==(const PatternSet &rhs) const {
	return !(*this != rhs);
}
