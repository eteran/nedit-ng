
#include "SmartIndent.h"
#include "MotifHelper.h"

SmartIndent *copyIndentSpec(SmartIndent *is) {

	auto ris = new SmartIndent;
	ris->lmName       = XtNewStringEx(is->lmName);
	ris->initMacro    = XtNewStringEx(is->initMacro);
	ris->newlineMacro = XtNewStringEx(is->newlineMacro);
	ris->modMacro     = XtNewStringEx(is->modMacro);
	return ris;
}

void freeIndentSpec(SmartIndent *is) {

	XtFree((char *)is->lmName);
	XtFree((char *)is->initMacro);
	XtFree((char *)is->newlineMacro);
	XtFree((char *)is->modMacro);	
	delete is;
}

