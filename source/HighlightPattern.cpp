
#include "HighlightPattern.h"
#include "MotifHelper.h"
#include <algorithm>

HighlightPattern::HighlightPattern() : endRE(nullptr), errorRE(nullptr), flags(0) {
}

HighlightPattern::HighlightPattern(const HighlightPattern &other) : name(other.name), startRE(other.startRE), endRE(other.endRE), errorRE(other.errorRE), style(other.style), subPatternOf(other.subPatternOf), flags(other.flags) {
}

HighlightPattern &HighlightPattern::operator=(const HighlightPattern &rhs) {
	if(this != &rhs) {
		HighlightPattern(rhs).swap(*this);
	}
	return *this;
}

HighlightPattern::HighlightPattern(HighlightPattern &&other) : name(std::move(other.name)), startRE(std::move(other.startRE)), errorRE(std::move(other.errorRE)), style(std::move(other.style)), subPatternOf(std::move(other.subPatternOf)), flags(std::move(other.flags)) {
}


HighlightPattern &HighlightPattern::operator=(HighlightPattern &&rhs) {
	if(this != &rhs) {
		HighlightPattern(std::move(rhs)).swap(*this);
	}
	return *this;
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

