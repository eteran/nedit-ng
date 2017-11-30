
#ifndef HIGHLIGHT_DATA_H_X_
#define HIGHLIGHT_DATA_H_X_

#include "Regex/Regex.h"
#include <memory>
#include <cstdint>

// "Compiled" version of pattern specification 
class HighlightData {
public:
    std::shared_ptr<Regex> startRE;
    std::shared_ptr<Regex> endRE;
    std::shared_ptr<Regex> errorRE;
    std::shared_ptr<Regex> subPatternRE;
    uint8_t style;
	int colorOnly;
	int startSubexprs[NSUBEXP + 1];
	int endSubexprs[NSUBEXP + 1];
	int flags;
	int nSubPatterns;
	int nSubBranches; // Number of top-level branches of subPatternRE 
    size_t userStyleIndex;
	HighlightData **subPatterns;
};

#endif
