
#include "PatternSet.h"

// NOTE(eteran): comparisons do NOT include the "languageMode"

/**
 * @brief PatternSet::operator !=
 * @param rhs
 * @return
 */
bool PatternSet::operator!=(const PatternSet &rhs) const {

	if (this->lineContext != rhs.lineContext) {
		return true;
	}

	if (this->charContext != rhs.charContext) {
		return true;
	}

	if(this->patterns != rhs.patterns) {
		return true;
	}

	return false;
}

/**
 * @brief PatternSet::operator ==
 * @param rhs
 * @return
 */
bool PatternSet::operator==(const PatternSet &rhs) const {
	return !(*this != rhs);
}
