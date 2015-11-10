
#include "highlightStyleRec.h"
#include <Xm/Xm.h>

highlightStyleRec::highlightStyleRec() : color(nullptr), bgColor(nullptr), font(0) {
	XtFree(color);
	XtFree(bgColor); // NOTE(eteran): there were code paths before where this didn't get free'd!
}

highlightStyleRec::~highlightStyleRec() {

}
