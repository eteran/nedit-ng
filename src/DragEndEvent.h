
#ifndef DRAG_END_EVENT_H_
#define DRAG_END_EVENT_H_

#include "TextCursor.h"

#include <cstdint>
#include <boost/utility/string_view.hpp>

struct DragEndEvent {
	TextCursor        startPos;
	int64_t           nCharsDeleted;
	int64_t           nCharsInserted;
	boost::string_view deletedText;
};

#endif
