
#ifndef DRAG_END_EVENT_H_
#define DRAG_END_EVENT_H_

#include "TextCursor.h"
#include "Util/string_view.h"
#include <cstdint>

struct DragEndEvent {
    TextCursor        startPos;
    int64_t           nCharsDeleted;
    int64_t           nCharsInserted;
	view::string_view deletedText;
};

#endif
