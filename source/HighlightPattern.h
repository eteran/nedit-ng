
#ifndef HIGHLIGHT_PATTERN_H_
#define HIGHLIGHT_PATTERN_H_

#include <QString>

// Pattern specification structure
class HighlightPattern {
public:
    bool operator==(const HighlightPattern &rhs) const;
    bool operator!=(const HighlightPattern &rhs) const;

public:
    QString name;
	QString startRE;
	QString endRE;
	QString errorRE;
	QString style;
	QString subPatternOf;
    int flags = 0;
};

#endif
