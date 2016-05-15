
#ifndef HIGHLIGHT_DATA_H_X_
#define HIGHLIGHT_DATA_H_X_

#include "regularExp.h"

// "Compiled" version of pattern specification 
class HighlightData {
public:
	regexp *startRE;
	regexp *endRE;
	regexp *errorRE;
	regexp *subPatternRE;
	char style;
	int colorOnly;
	int startSubexprs[NSUBEXP + 1];
	int endSubexprs[NSUBEXP + 1];
	int flags;
	int nSubPatterns;
	int nSubBranches; // Number of top-level branches of subPatternRE 
	long userStyleIndex;
	HighlightData **subPatterns;
};

#endif
