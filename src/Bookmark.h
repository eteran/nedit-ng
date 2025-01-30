
#ifndef BOOKMARK_H_
#define BOOKMARK_H_

#include "TextBuffer.h"
#include "TextCursor.h"

#include <QChar>

// Element in bookmark table
struct Bookmark {
	QChar label;
	TextCursor cursorPos;
	TextBuffer::Selection sel;
};

#endif
