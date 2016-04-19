
#ifndef SMART_INDENT_H_
#define SMART_INDENT_H_

class SmartIndent {
public:
	const char *lmName;
	const char *initMacro;
	const char *newlineMacro;
	const char *modMacro;
};

SmartIndent *copyIndentSpec(SmartIndent *is);
void freeIndentSpec(SmartIndent *is);

#endif
