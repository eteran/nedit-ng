
#ifndef TEXT_RANGE_H_
#define TEXT_RANGE_H_

#include "TextCursor.h"

struct TextRange {
	TextCursor start;
	TextCursor end; /* range from [start-]end */
};

#endif
