
#ifndef PATTERN_SET_H_
#define PATTERN_SET_H_

#include "HighlightPattern.h"
#include <QString>
#include <vector>

// Header for a set of patterns
class PatternSet {
public:
    static constexpr int DefaultLineContext = 1;
    static constexpr int DefaultCharContext = 0;
public:
	bool operator!=(const PatternSet &rhs) const;
	bool operator==(const PatternSet &rhs) const;

public:
    QString                       languageMode;
    int                           lineContext = DefaultLineContext;
    int                           charContext = DefaultCharContext;
    std::vector<HighlightPattern> patterns;
};

#endif
