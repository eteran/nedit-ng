
#ifndef HIGHLIGHT_DATA_H_X_
#define HIGHLIGHT_DATA_H_X_

#include "regularExp.h"
#include <memory>
#include <cstdint>

// "Compiled" version of pattern specification 
class HighlightData {
public:
    std::shared_ptr<regexp> startRE;
    std::shared_ptr<regexp> endRE;
    std::shared_ptr<regexp> errorRE;
    std::shared_ptr<regexp> subPatternRE;
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
