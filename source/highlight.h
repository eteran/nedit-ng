/*******************************************************************************
*                                                                              *
* highlight.h -- Nirvana Editor Syntax Highlighting Header File                *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

#include "nedit.h"
#include "Color.h"

#include <X11/Intrinsic.h>
#include <string>

/* Pattern flags for modifying pattern matching behavior */
enum {
	PARSE_SUBPATS_FROM_START = 1,
	DEFER_PARSING            = 2,
	COLOR_ONLY               = 4
};

/* Don't use plain 'A' or 'B' for style indices, it causes problems
   with EBCDIC coding (possibly negative offsets when subtracting 'A'). */
#define ASCII_A static_cast<char>(65)

class PatternSet;
class HighlightPattern;

bool TestHighlightPatterns(PatternSet *patSet);
HighlightPattern *FindPatternOfWindow(Document *window, const char *name);
int HighlightCodeOfPos(Document *window, int pos);
int HighlightLengthOfCodeFromPos(Document *window, int pos, int *checkCode);
int StyleLengthOfCodeFromPos(Document *window, int pos);
Pixel AllocateColor(Widget w, const char *colorName);
Pixel AllocColor(Widget w, const char *colorName, Color *color);
Pixel AllocColor(Widget w, const char *colorName);
Pixel GetHighlightBGColorOfCode(Document *window, int hCode, Color *color);
Pixel HighlightColorValueOfCode(Document *window, int hCode, Color *color);
std::string HighlightNameOfCode(Document *window, int hCode);
std::string HighlightStyleOfCode(Document *window, int hCode);
void AttachHighlightToWidget(Widget widget, Document *window);
void FreeHighlightingData(Document *window);
void *GetHighlightInfo(Document *window, int pos);
void RemoveWidgetHighlight(Widget widget);
void StartHighlighting(Document *window, int warn);
void StopHighlighting(Document *window);
void SyntaxHighlightModifyCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
void UpdateHighlightStyles(Document *window);

#endif
