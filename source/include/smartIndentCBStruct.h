
#ifndef SMARTINDENTCBSTRUCT_H_
#define SMARTINDENTCBSTRUCT_H_

enum SmartIndentCallbackReasons {
	NEWLINE_INDENT_NEEDED, 
	CHAR_TYPED
};

struct smartIndentCBStruct {
	SmartIndentCallbackReasons reason;
	int pos;
	int indentRequest;
	const char *charsTyped;
};

#endif
