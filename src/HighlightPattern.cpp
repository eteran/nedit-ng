
#include "HighlightPattern.h"

HighlightPattern::HighlightPattern(QString styleName)
	: style(std::move(styleName)) {
}

bool HighlightPattern::operator==(const HighlightPattern &rhs) const {

	return name == rhs.name &&
		   startRE == rhs.startRE &&
		   endRE == rhs.endRE &&
		   errorRE == rhs.errorRE &&
		   style == rhs.style &&
		   subPatternOf == rhs.subPatternOf &&
		   flags == rhs.flags;
}

bool HighlightPattern::operator!=(const HighlightPattern &rhs) const {
	return !(*this == rhs);
}
