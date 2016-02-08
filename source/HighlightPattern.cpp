
#include "HighlightPattern.h"
#include "MotifHelper.h"
#include <algorithm>

HighlightPattern::HighlightPattern() : startRE(nullptr), endRE(nullptr), errorRE(nullptr), flags(0) {
}

HighlightPattern::HighlightPattern(const HighlightPattern &other) : name(other.name), errorRE(other.errorRE), style(other.style), subPatternOf(other.subPatternOf), flags(other.flags) {
	startRE = XtNewStringEx(other.startRE);
	endRE   = XtNewStringEx(other.endRE);
	
}

HighlightPattern &HighlightPattern::operator=(const HighlightPattern &rhs) {
	if(this != &rhs) {
		HighlightPattern(rhs).swap(*this);
	}
	return *this;
}

HighlightPattern::HighlightPattern(HighlightPattern &&other) : name(std::move(other.name)), errorRE(std::move(other.errorRE)), style(std::move(other.style)), subPatternOf(std::move(other.subPatternOf)), flags(std::move(other.flags)) {
	startRE = other.startRE;
	endRE   = other.endRE;
	
	other.startRE = nullptr;
	other.endRE   = nullptr;
}


HighlightPattern &HighlightPattern::operator=(HighlightPattern &&rhs) {
	if(this != &rhs) {
		HighlightPattern(std::move(rhs)).swap(*this);
	}
	return *this;
}

HighlightPattern::~HighlightPattern() {
	XtFree(startRE);
	XtFree(endRE);
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

