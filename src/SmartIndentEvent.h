
#ifndef SMART_INDENT_EVENT_H_
#define SMART_INDENT_EVENT_H_

#include "TextCursor.h"
#include <boost/utility/string_view.hpp>

enum SmartIndentReason {
	NEWLINE_INDENT_NEEDED,
	CHAR_TYPED
};

struct SmartIndentEvent {
	SmartIndentReason reason;
	TextCursor        pos;
	int               request;
	boost::string_view charsTyped;
};

#endif
