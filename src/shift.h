
#ifndef SHIFT_H_
#define SHIFT_H_

#include "ShiftDirection.h"

class DocumentWidget;
class TextArea;
class QString;

void ShiftSelection(DocumentWidget *document, TextArea *area, ShiftDirection direction, bool byTab);
void FillSelection(DocumentWidget *document, TextArea *area);
QString ShiftTextEx(const QString &text, ShiftDirection direction, bool tabsAllowed, int tabDist, int nChars);

#endif
