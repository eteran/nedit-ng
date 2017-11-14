
#ifndef WINDOW_HIGHLIGHT_DATA_H_
#define WINDOW_HIGHLIGHT_DATA_H_

#include "ReparseContext.h"
#include <QByteArray>
#include <memory>

class HighlightData;
class PatternSet;
class StyleTableEntry;
class TextBuffer;

// Data structure attached to window to hold all syntax highlighting
// information (for both drawing and incremental reparsing)
class WindowHighlightData {
public:
    ~WindowHighlightData();
public:
    StyleTableEntry*                 styleTable          = nullptr;
    HighlightData*                   pass1Patterns       = nullptr;
    HighlightData*                   pass2Patterns       = nullptr;
    std::shared_ptr<TextBuffer>      styleBuffer         = nullptr;
    PatternSet*                      patternSetForWindow = nullptr;
    QByteArray                       parentStyles;
    ReparseContext                   contextRequirements = { 0, 0 };
    long                             nStyles             = 0;
};

#endif
