
#ifndef DRAG_END_EVENT_H_
#define DRAG_END_EVENT_H_

#include "TextCursor.h"
#include <cstdint>
#include <string_view>

struct DragEndEvent {
	TextCursor startPos;
	int64_t nCharsDeleted;
	int64_t nCharsInserted;
	std::string_view deletedText;
};

#endif
