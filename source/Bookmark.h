
#ifndef BOOKMARK_H_
#define BOOKMARK_H_

#include "TextBuffer.h"
#include <QChar>
#include <cstdint>

// max. # of bookmarks (one per letter & #)
constexpr int MAX_MARKS = 36;

// Element in bookmark table
struct Bookmark {
    QChar                 label;
    int64_t               cursorPos;
    TextBuffer::Selection sel;
};

#endif
