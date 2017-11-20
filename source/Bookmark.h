
#ifndef BOOKMARK_H_
#define BOOKMARK_H_

#include "TextSelection.h"
#include <QChar>

// max. # of bookmarks (one per letter & #)
constexpr int MAX_MARKS = 36;

// Element in bookmark table
struct Bookmark {
    QChar         label;
    int           cursorPos;
	TextSelection sel;
};

#endif
