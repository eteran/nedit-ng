
#ifndef PATTERN_SET_H_
#define PATTERN_SET_H_

#include "nullable_string.h"

struct highlightPattern;

/* Header for a set of patterns */
struct PatternSet {
public:
	bool operator!=(const PatternSet &rhs) const;
	bool operator==(const PatternSet &rhs) const;
	
public:
	nullable_string languageMode;
	int lineContext;
	int charContext;
	int nPatterns;
	highlightPattern *patterns;
};

#endif
