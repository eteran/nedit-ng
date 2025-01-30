
#ifndef WINDOW_HIGHLIGHT_DATA_H_
#define WINDOW_HIGHLIGHT_DATA_H_

#include "HighlightData.h"
#include "ReparseContext.h"
#include "StyleTableEntry.h"
#include "TextBufferFwd.h"

#include <memory>
#include <vector>

class PatternSet;

// Data structure attached to window to hold all syntax highlighting
// information (for both drawing and incremental re-parsing)
struct WindowHighlightData {
	std::vector<uint8_t> parentStyles;
	std::vector<StyleTableEntry> styleTable;
	std::shared_ptr<UTextBuffer> styleBuffer;
	std::unique_ptr<HighlightData[]> pass1Patterns;
	std::unique_ptr<HighlightData[]> pass2Patterns;
	PatternSet *patternSetForWindow    = nullptr;
	ReparseContext contextRequirements = {0, 0};
};

#endif
