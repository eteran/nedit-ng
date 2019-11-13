
#ifndef SMART_INDENT_EVENT_H_
#define SMART_INDENT_EVENT_H_

#include "TextCursor.h"
#include "Util/string_view.h"

enum SmartIndentReason {
	NEWLINE_INDENT_NEEDED,
	CHAR_TYPED
};

struct SmartIndentEvent {
	SmartIndentReason reason;
	TextCursor pos;
	int request;
	view::string_view charsTyped;
};

#endif
