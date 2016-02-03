
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
	HighlightPattern(HighlightPattern &&other);
	HighlightPattern &operator=(HighlightPattern &&rhs);
	~HighlightPattern();
	
public:
	void swap(HighlightPattern &other);

public:
	std::string name;
	char *startRE;
	char *endRE;
	char *errorRE;
	nullable_string style;
	nullable_string subPatternOf;
	int flags;
};


#endif
