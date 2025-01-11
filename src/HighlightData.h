
#ifndef HIGHLIGHT_DATA_H_X_
#define HIGHLIGHT_DATA_H_X_

#include "Regex.h"
#include <cstdint>
#include <memory>
#include <vector>

// "Compiled" version of pattern specification
struct HighlightData {
	std::unique_ptr<Regex> startRE;
	std::unique_ptr<Regex> endRE;
	std::unique_ptr<Regex> errorRE;
	std::unique_ptr<Regex> subPatternRE;
	std::vector<size_t> startSubexpressions;
	std::vector<size_t> endSubexpressions;
	std::unique_ptr<HighlightData *[]> subPatterns;
	size_t userStyleIndex;
	size_t nSubPatterns;
	size_t nSubBranches; // Number of top-level branches of subPatternRE
	int flags;
	bool colorOnly;
	uint8_t style;
};

#endif
