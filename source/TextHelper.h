
#ifndef TEXT_HELPER_H_
#define TEXT_HELPER_H_

#include "textP.h"

class TextPart;
class TextDisplay;
struct TextRec;

typedef TextRec *TextWidget;

template <class T>
TextPart &text_of(T w) {
	return reinterpret_cast<TextWidget>(w)->text;
}

inline TextDisplay *&textD_of(TextPart &w) {
	return w.textD;
}

template <class T>
TextDisplay *&textD_of(T w) {
	return text_of(w).textD;
}


#endif
