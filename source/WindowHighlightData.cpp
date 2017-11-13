
#include "WindowHighlightData.h"
#include "TextBuffer.h"
#include "HighlightData.h"
#include "StyleTableEntry.h"
#include "highlight.h"

namespace {

/*
** Free a pattern list and all of its allocated components
*/
void freePatterns(HighlightData *patterns) {

    if(patterns) {
        for (int i = 0; patterns[i].style != 0; i++) {
            delete [] patterns[i].subPatterns;
        }

        delete [] patterns;
    }
}

}

/*
** Free allocated memory associated with highlight data, including compiled
** regular expressions, style buffer and style table.  Note: be sure to
** nullptr out the widget references to the objects in this structure before
** calling this.  Because of the slow, multi-phase destruction of
** widgets, this data can be referenced even AFTER destroying the widget.
*/
WindowHighlightData::~WindowHighlightData() {
    freePatterns(pass1Patterns);
    freePatterns(pass2Patterns);
    delete[] styleTable;
}
