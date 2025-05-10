
#include "PatternSet.h"

// comparisons do NOT include the "languageMode"

/**
 * @brief Compares two PatternSet objects for inequality.
 *
 * @param rhs The PatternSet object to compare against.
 * @return `true` if the two PatternSet objects are not equal, `false` otherwise.
 */
bool PatternSet::operator!=(const PatternSet &rhs) const {

	if (lineContext != rhs.lineContext) {
		return true;
	}

	if (charContext != rhs.charContext) {
		return true;
	}

	if (patterns != rhs.patterns) {
		return true;
	}

	return false;
}

/**
 * @brief Compares two PatternSet objects for equality.
 *
 * @param rhs The PatternSet object to compare against.
 * @return `true` if the two PatternSet objects are equal, `false` otherwise.
 */
bool PatternSet::operator==(const PatternSet &rhs) const {
	return !(*this != rhs);
}
