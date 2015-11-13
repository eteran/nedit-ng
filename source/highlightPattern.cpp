
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
	if(&rhs != this) {
		highlightPattern(rhs).swap(*this);
	}
	return *this;
}

highlightPattern::~highlightPattern() {
#if 0
	// Grr, ownership issues are annoying!
	XtFree(const_cast<char *>(name));
	XtFree(startRE);
	XtFree(endRE);
	XtFree(errorRE);
	XtFree(style);
	XtFree(subPatternOf);
#endif
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
