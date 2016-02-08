
#include "PatternSet.h"
#include "preferences.h" // for AllocatedStringsDiffer
#include "HighlightPattern.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
PatternSet::PatternSet() : lineContext(0), charContext(0), nPatterns(0), patterns(nullptr) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
PatternSet::PatternSet(int patternCount) : lineContext(0), charContext(0), nPatterns(patternCount) {
	patterns = new HighlightPattern[patternCount];
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
PatternSet::~PatternSet() {
	delete [] patterns;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
PatternSet::PatternSet(const PatternSet &other) : languageMode(other.languageMode), lineContext(other.lineContext), charContext(other.charContext), nPatterns(other.nPatterns) {

	patterns = new HighlightPattern[nPatterns];
	std::copy_n(other.patterns, nPatterns, patterns);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
PatternSet& PatternSet::operator=(const PatternSet &rhs) {
	if(this != &rhs) {
		PatternSet(rhs).swap(*this);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void PatternSet::swap(PatternSet &other) {
	using std::swap;
	
	swap(languageMode, other.languageMode);
	swap(lineContext,  other.lineContext);
	swap(charContext,  other.charContext);
	swap(nPatterns,    other.nPatterns);
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
	
	if (this->nPatterns != rhs.nPatterns) {
		return true;
	}
		
	for (int i = 0; i < rhs.nPatterns; i++) {
		HighlightPattern *pat1 = &this->patterns[i];
		HighlightPattern *pat2 = &rhs.patterns[i];
		
		if (pat1->flags != pat2->flags) {
			return true;
		}
		
		if (pat1->name != pat2->name) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->startRE, pat2->startRE)) {
			return true;
		}
		
		if (AllocatedStringsDiffer(pat1->endRE, pat2->endRE)) {
			return true;
		}
		
		if (pat1->errorRE != pat2->errorRE) {
			return true;
		}
			
			
		if(pat1->style != pat2->style) {
			return true;
		}
		
		if (pat1->subPatternOf != pat2->subPatternOf) {
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
