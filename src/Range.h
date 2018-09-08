
#ifndef RANGE_H_
#define RANGE_H_

#include "TextCursor.h"

struct Range {
	TextCursor start;
	TextCursor end; /* range from [start-]end */
};

#endif
