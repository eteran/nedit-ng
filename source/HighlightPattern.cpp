
#include "HighlightPattern.h"
#include "MotifHelper.h"
#include <algorithm>

HighlightPattern::HighlightPattern() : startRE(nullptr), endRE(nullptr), errorRE(nullptr), flags(0) {
}

HighlightPattern::HighlightPattern(const HighlightPattern &other) {

	name         = other.name;
	startRE      = XtNewStringEx(other.startRE);
	endRE        = XtNewStringEx(other.endRE);
	errorRE      = XtNewStringEx(other.errorRE);
	style        = other.style;
	subPatternOf = other.subPatternOf;
	flags        = other.flags;
}

HighlightPattern &HighlightPattern::operator=(const HighlightPattern &rhs) {
	if(this != &rhs) {
		HighlightPattern(rhs).swap(*this);
	}
	return *this;
}

HighlightPattern::~HighlightPattern() {
	// NOTE(eteran): this is a no-op FOR NOW. Ownership of these
	// strings is a mess and needs to be cleaned up first
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

