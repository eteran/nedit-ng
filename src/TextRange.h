
#ifndef TEXT_RANGE_H_
#define TEXT_RANGE_H_

#include "TextCursor.h"

struct TextRange {
	TextCursor start;
	TextCursor end; /* range from [start-]end */
};

inline bool operator<(const TextRange &lhs, const TextRange &rhs) {
	return (lhs.start < rhs.start) || (lhs.start == rhs.start && lhs.end < rhs.end);
}

inline bool operator==(const TextRange &lhs, const TextRange &rhs) {
	return lhs.start == rhs.start && lhs.end == rhs.end;
}

#endif
