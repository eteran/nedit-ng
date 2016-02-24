
#ifndef HIGHLIGHT_PATTERN_H_
#define HIGHLIGHT_PATTERN_H_

#include <string>
#include "nullable_string.h"
#include "XString.h"

/* Pattern specification structure */
class HighlightPattern {
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
	XString startRE;
	XString endRE;
	XString errorRE;
	nullable_string style;
	nullable_string subPatternOf;
	int flags;
};


#endif
