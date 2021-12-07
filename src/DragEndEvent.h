
#ifndef DRAG_END_EVENT_H_
#define DRAG_END_EVENT_H_

#include "Ext/string_view.h"
#include "TextCursor.h"
#include <cstdint>

struct DragEndEvent {
	TextCursor startPos;
	int64_t nCharsDeleted;
	int64_t nCharsInserted;
	ext::string_view deletedText;
};

#endif
