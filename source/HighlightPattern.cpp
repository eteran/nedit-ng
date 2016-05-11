
#include "HighlightPattern.h"
#include <algorithm>

HighlightPattern::HighlightPattern() : flags(0) {
}

HighlightPattern::~HighlightPattern() {
}

void HighlightPattern::swap(HighlightPattern &other) {
	using std::swap;
	
	std::swap(name,         other.name);
	std::swap(startRE,      other.startRE);
	std::swap(endRE,        other.endRE);
	std::swap(errorRE,      other.errorRE);
	std::swap(style,        other.style);
	std::swap(subPatternOf, other.subPatternOf);
	std::swap(flags,        other.flags);
}


bool HighlightPattern::operator==(const HighlightPattern &rhs) const {


	if (this->flags != rhs.flags) {
		return false;
	}

	if (this->name != rhs.name) {
		return false;
	}

	if (this->startRE != rhs.startRE) {
		return false;
	}

	if (this->endRE != rhs.endRE) {
		return false;
	}

	if (this->errorRE != rhs.errorRE) {
		return false;
	}

	if(this->style != rhs.style) {
		return false;
	}

	if (this->subPatternOf != rhs.subPatternOf) {
		return false;
	}
	
	return true;

}

bool HighlightPattern::operator!=(const HighlightPattern &rhs) const {
	return !(*this == rhs);
}
