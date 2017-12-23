
#ifndef DRAG_END_EVENT_H_
#define DRAG_END_EVENT_H_

#include "Util/string_view.h"

struct DragEndEvent {
	int               startPos;
	int               nCharsDeleted;
	int               nCharsInserted;
	view::string_view deletedText;
};

#endif
