
#ifndef BOOKMARK_H_
#define BOOKMARK_H_

#include "TextSelection.h"

#define MAX_MARKS 36        /* max. # of bookmarks (one per letter & #) */

/* Element in bookmark table */
struct Bookmark {
	char label;
	int cursorPos;
	TextSelection sel;
};

#endif
