
#ifndef WINDOW_HIGHLIGHT_DATA_H_
#define WINDOW_HIGHLIGHT_DATA_H_

#include "ReparseContext.h"

class HighlightData;
class StyleTableEntry;
class TextBuffer;
class PatternSet;

/* Data structure attached to window to hold all syntax highlighting
   information (for both drawing and incremental reparsing) */
class WindowHighlightData {
public:
	HighlightData *pass1Patterns;
	HighlightData *pass2Patterns;
	char *parentStyles;
	ReparseContext contextRequirements;
	StyleTableEntry *styleTable;
	int nStyles;
	TextBuffer *styleBuffer;
	PatternSet *patternSetForWindow;
};

#endif
