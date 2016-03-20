
#ifndef PATTERN_SET_H_
#define PATTERN_SET_H_

#include <QString>

struct HighlightPattern;

/* Header for a set of patterns */
struct PatternSet {
public:
	PatternSet();
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
