
#ifndef SHIFT_H_
#define SHIFT_H_

#include "ShiftDirection.h"
#include "Util/string_view.h"

class DocumentWidget;
class TextArea;

class QString;

void ShiftSelectionEx(DocumentWidget *document, TextArea *area, ShiftDirection direction, bool byTab);
void UpcaseSelectionEx(DocumentWidget *document, TextArea *area);
void DowncaseSelectionEx(DocumentWidget *document, TextArea *area);
void FillSelectionEx(DocumentWidget *document, TextArea *area);
QString ShiftTextEx(const QString &text, ShiftDirection direction, int tabsAllowed, int tabDist, int nChars);

#endif
