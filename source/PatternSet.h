
#ifndef PATTERN_SET_H_
#define PATTERN_SET_H_

#include <QString>

#include "HighlightPattern.h"

/* Header for a set of patterns */
class PatternSet {
public:
	PatternSet();
	
	template <class In>
	PatternSet(In first, In last) : lineContext(0), charContext(0) {
	
		nPatterns = std::distance(first, last);
		patterns = new HighlightPattern[nPatterns];
		
		std::copy(first, last, patterns);	
	}
	
	explicit PatternSet(int patternCount);
	~PatternSet();
	PatternSet(const PatternSet &other);
	PatternSet& operator=(const PatternSet &rhs);

public:
	bool operator!=(const PatternSet &rhs) const;
	bool operator==(const PatternSet &rhs) const;
	
public:
	void swap(PatternSet &other);

public:
	QString           languageMode;
	int               lineContext;
	int               charContext;
	int               nPatterns;
	HighlightPattern *patterns;
};

#endif
