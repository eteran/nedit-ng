
#ifndef SMART_INDENT_EVENT_H_
#define SMART_INDENT_EVENT_H_

#include "TextCursor.h"
#include "Util/string_view.h"

enum SmartIndentCallbackReasons {
	NEWLINE_INDENT_NEEDED,
	CHAR_TYPED
};

struct SmartIndentEvent {
	SmartIndentCallbackReasons reason;
	TextCursor                 pos;
	int                        indentRequest;
	view::string_view          charsTyped;
};

#endif
