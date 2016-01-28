
#include "PatternSet.h"
#include "preferences.h" // for AllocatedStringsDiffer
#include "highlightPattern.h"

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
	
	if (this->nPatterns != rhs.nPatterns) {
		return true;
	}
		
	for (int i = 0; i < rhs.nPatterns; i++) {
		highlightPattern *pat1 = &this->patterns[i];
		highlightPattern *pat2 = &rhs.patterns[i];
		
		if (pat1->flags != pat2->flags) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->name, pat2->name)) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->startRE, pat2->startRE)) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->endRE, pat2->endRE)) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->errorRE, pat2->errorRE)) {
			return true;
		}
			
			
		if(pat1->style != pat2->style) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->subPatternOf, pat2->subPatternOf)) {
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool PatternSet::operator==(const PatternSet &rhs) const {
	return !(*this != rhs);
}
