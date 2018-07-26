
#ifndef WINDOW_HIGHLIGHT_DATA_H_
#define WINDOW_HIGHLIGHT_DATA_H_

#include "ReparseContext.h"
#include "TextBufferFwd.h"
#include <vector>
#include <memory>

class HighlightData;
class PatternSet;
class StyleTableEntry;

// Data structure attached to window to hold all syntax highlighting
// information (for both drawing and incremental reparsing)
class WindowHighlightData {
public:
    ~WindowHighlightData();
public:
    std::vector<uint8_t>             parentStyles;
    std::vector<StyleTableEntry>     styleTable;
	std::shared_ptr<TextBuffer>      styleBuffer;
    HighlightData*                   pass1Patterns       = nullptr;
    HighlightData*                   pass2Patterns       = nullptr;    
    PatternSet*                      patternSetForWindow = nullptr;    
    ReparseContext                   contextRequirements = { 0, 0 };
};

#endif
