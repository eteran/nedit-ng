/*******************************************************************************
*                                                                              *
* highlightData.h -- Nirvana Editor Highlight Data Header File                 *
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

#ifndef HIGHLIGHT_DATA_H_
#define HIGHLIGHT_DATA_H_

#include <QString>
#include "highlight.h"
#include "nedit.h"
#include "string_view.h"
#include <string>


struct PatternSet;

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

bool LoadHighlightStringEx(const std::string &inString, int convertOld);
bool LoadStylesStringEx(const std::string &string);
bool NamedStyleExists(view::string_view styleName);
int FontOfNamedStyleIsBold(view::string_view styleName);
int FontOfNamedStyleIsItalic(view::string_view styleName);
int IndexOfNamedStyle(view::string_view styleName);
bool LMHasHighlightPatterns(view::string_view languageMode);
PatternSet *FindPatternSet(view::string_view langModeName);
QString BgColorOfNamedStyleEx(view::string_view styleName);
QString ColorOfNamedStyleEx(view::string_view styleName);
QString WriteHighlightStringEx(void);
QString WriteStylesStringEx(void);
void EditHighlightPatterns(Document *window);
void EditHighlightStyles(const char *initialStyle);
void RenameHighlightPattern(view::string_view oldName, view::string_view newName);
void UpdateLanguageModeMenu(void);
XFontStruct *FontOfNamedStyle(Document *window, view::string_view styleName);

#endif
