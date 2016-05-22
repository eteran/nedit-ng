
#ifndef HIGHLIGHT_PATTERN_H_
#define HIGHLIGHT_PATTERN_H_

#include <QString>

/* Pattern specification structure */
class HighlightPattern {
public:
	HighlightPattern();
	HighlightPattern(const HighlightPattern &) = default;
	HighlightPattern &operator=(const HighlightPattern &) = default;
	HighlightPattern(HighlightPattern &&) = default;
	HighlightPattern &operator=(HighlightPattern &&) = default;	
	~HighlightPattern();
	
public:
	bool operator==(const HighlightPattern &rhs) const;
	bool operator!=(const HighlightPattern &rhs) const;
	
public:
	void swap(HighlightPattern &other);

public:
	QString name;
	QString startRE;
	QString endRE;
	QString errorRE;
	QString style;
	QString subPatternOf;
	int flags;
};


#endif
