
#ifndef SMART_INDENT_EVENT_H_
#define SMART_INDENT_EVENT_H_

#include "Ext/string_view.h"
#include "TextCursor.h"

enum SmartIndentReason {
	NEWLINE_INDENT_NEEDED,
	CHAR_TYPED
};

struct SmartIndentEvent {
	SmartIndentReason reason;
	TextCursor pos;
	int request;
	ext::string_view charsTyped;
};

#endif
