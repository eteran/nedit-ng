
#include "PatternSet.h"

// comparisons do NOT include the "languageMode"

/**
 * @brief
 *
 * @param rhs
 * @return
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
 * @brief
 *
 * @param rhs
 * @return
 */
bool PatternSet::operator==(const PatternSet &rhs) const {
	return !(*this != rhs);
}
