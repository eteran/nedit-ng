
#ifndef HIGHLIGHT_PATTERN_H_
#define HIGHLIGHT_PATTERN_H_

#include <QString>
#include <string>

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
	QString startRE;
	QString endRE;
	QString errorRE;
	QString style;
	QString subPatternOf;
	int flags;
};


#endif
