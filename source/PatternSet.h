
#ifndef PATTERN_SET_H_
#define PATTERN_SET_H_

#include "HighlightPattern.h"
#include <QString>
#include <QVector>

// Header for a set of patterns
class PatternSet {
public:
	PatternSet();
	PatternSet(const PatternSet &other)          = default;
	PatternSet& operator=(const PatternSet &rhs) = default;
	PatternSet(PatternSet &&other)               = default;
	PatternSet& operator=(PatternSet &&rhs)      = default;
	~PatternSet()                                = default;

public:
	bool operator!=(const PatternSet &rhs) const;
	bool operator==(const PatternSet &rhs) const;

public:
	void swap(PatternSet &other);

public:
	QString                   languageMode;
	int                       lineContext;
	int                       charContext;
	QVector<HighlightPattern> patterns;
};

inline void swap(PatternSet &lhs, PatternSet &rhs) {
    lhs.swap(rhs);
}

#endif
