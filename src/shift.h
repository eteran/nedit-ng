
#ifndef SHIFT_H_
#define SHIFT_H_

#include "ShiftDirection.h"

class DocumentWidget;
class TextArea;
class QString;

void shiftSelection(DocumentWidget *document, TextArea *area, ShiftDirection direction, bool byTab);
void fillSelection(DocumentWidget *document, TextArea *area);
QString shiftText(const QString &text, ShiftDirection direction, bool tabsAllowed, int tabDist, int nChars);

#endif
