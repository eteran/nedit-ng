
#include "HighlightPattern.h"

/**
 * @brief Constructor for the HighlightPattern class.
 *
 * @param styleName
 */
HighlightPattern::HighlightPattern(QString styleName)
	: style(std::move(styleName)) {
}

/**
 * @brief Equality operator for the HighlightPattern class.
 *
 * @param rhs The right-hand side HighlightPattern to compare with.
 * @return `true` if both HighlightPattern objects are equal, `false` otherwise.
 */
bool HighlightPattern::operator==(const HighlightPattern &rhs) const {

	return name == rhs.name &&
		   startRE == rhs.startRE &&
		   endRE == rhs.endRE &&
		   errorRE == rhs.errorRE &&
		   style == rhs.style &&
		   subPatternOf == rhs.subPatternOf &&
		   flags == rhs.flags;
}

/**
 * @brief Inequality operator for the HighlightPattern class.
 *
 * @param rhs The right-hand side HighlightPattern to compare with.
 * @return `true` if both HighlightPattern objects are not equal, `false` otherwise.
 */
bool HighlightPattern::operator!=(const HighlightPattern &rhs) const {
	return !(*this == rhs);
}
