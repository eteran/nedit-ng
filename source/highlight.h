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

#include "util/string_view.h"

#include <gsl/span>
#include <vector>

class HighlightData;
class HighlightPattern;
class PatternSet;
class TextArea;
class WindowHighlightData;
struct HighlightStyle;
struct ReparseContext;
class Style;

class QColor;
class QString;
class QWidget;

template <class Ch, class Tr>
class BasicTextBuffer;

using TextBuffer = BasicTextBuffer<char, std::char_traits<char>>;

// Pattern flags for modifying pattern matching behavior
enum {
	PARSE_SUBPATS_FROM_START = 1,
	DEFER_PARSING            = 2,
	COLOR_ONLY               = 4
};

// How much re-parsing to do when an unfinished style is encountered
constexpr int PASS_2_REPARSE_CHUNK_SIZE = 1000;

// Don't use plain 'A' or 'B' for style indices, it causes problems
// with EBCDIC coding (possibly negative offsets when subtracting 'A').
constexpr auto ASCII_A = static_cast<char>(65);

/* Meanings of style buffer characters (styles). Don't use plain 'A' or 'B';
   it causes problems with EBCDIC coding (possibly negative offsets when
   subtracting 'A'). */
constexpr uint8_t UNFINISHED_STYLE = ASCII_A;

constexpr uint8_t PLAIN_STYLE = (ASCII_A + 1);

constexpr auto STYLE_NOT_FOUND = static_cast<size_t>(-1);

bool LoadHighlightStringEx(const QString &string);
bool LoadStylesStringEx(const QString &string);
bool NamedStyleExists(const QString &styleName);
bool parseString(HighlightData *pattern, const char **string, char **styleString, long length, char *prevChar, bool anchored, const QString &delimiters, const char *lookBehindTo, const char *match_till);
char getPrevChar(TextBuffer *buf, int pos);
HighlightData *patternOfStyle(HighlightData *patterns, int style);
int backwardOneContext(TextBuffer *buf, ReparseContext *context, int fromPos);
int findTopLevelParentIndex(const gsl::span<HighlightPattern> &patList, int index);
int FontOfNamedStyleIsBold(const QString &styleName);
int FontOfNamedStyleIsItalic(const QString &styleName);
int forwardOneContext(TextBuffer *buf, ReparseContext *context, int fromPos);
int indexOfNamedPattern(const gsl::span<HighlightPattern> &patList, const QString &patName);
PatternSet *FindPatternSet(const QString &langModeName);
QString BgColorOfNamedStyleEx(const QString &styleName);
QString ColorOfNamedStyleEx(const QString &styleName);
QString WriteHighlightStringEx();
QString WriteStylesStringEx();
size_t IndexOfNamedStyle(const QString &styleName);
std::unique_ptr<PatternSet> readDefaultPatternSet(const QString &langModeName);
void handleUnparsedRegionCBEx(const TextArea *area, int pos, const void *user);
void RemoveWidgetHighlightEx(TextArea *area);
void RenameHighlightPattern(const QString &oldName, const QString &newName);
void SyntaxHighlightModifyCBEx(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *user);


// list of available highlight styles
extern std::vector<HighlightStyle> HighlightStyles;
extern std::vector<PatternSet> PatternSets;
#endif
