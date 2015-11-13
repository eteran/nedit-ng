
#include "highlightPattern.h"
#include "MotifHelper.h"
#include <algorithm>

highlightPattern::highlightPattern() : name(nullptr), startRE(nullptr), endRE(nullptr), errorRE(nullptr), style(nullptr), subPatternOf(nullptr), flags(0) {
}

highlightPattern::highlightPattern(const highlightPattern &other) {

	name         = XtNewStringEx(other.name);
	startRE      = XtNewStringEx(other.startRE);
	endRE        = XtNewStringEx(other.endRE);
	errorRE      = XtNewStringEx(other.errorRE);
	style        = XtNewStringEx(other.style);
	subPatternOf = XtNewStringEx(other.subPatternOf);
	flags        = other.flags;

}

highlightPattern &highlightPattern::operator=(const highlightPattern &rhs) {
	if(this != &rhs) {
		highlightPattern(rhs).swap(*this);
	}
	return *this;
}

highlightPattern::~highlightPattern() {
	// NOTE(eteran): this is a no-op FOR NOW. Ownership of these
	// strings is a mess and needs to be cleaned up first
}

void highlightPattern::swap(highlightPattern &other) {
	using std::swap;
	
	std::swap(name,         other.name);
	std::swap(startRE,      other.startRE);
	std::swap(endRE,        other.endRE);
	std::swap(errorRE,      other.errorRE);
	std::swap(style,        other.style);
	std::swap(subPatternOf, other.subPatternOf);
	std::swap(flags,        other.flags);
}

