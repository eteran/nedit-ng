
#ifndef HIGHLIGHT_PATTERN_H_
#define HIGHLIGHT_PATTERN_H_

#include <string>
#include "nullable_string.h"

/* Pattern specification structure */
struct HighlightPattern {
public:
	HighlightPattern();
	HighlightPattern(const HighlightPattern &other);
	HighlightPattern &operator=(const HighlightPattern &rhs);
	~HighlightPattern();
	
public:
	void swap(HighlightPattern &other);

public:
	const char *name;
	char *startRE;
	char *endRE;
	char *errorRE;
	nullable_string style;
	char *subPatternOf;
	int flags;
};


#endif
