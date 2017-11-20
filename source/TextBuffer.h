/*******************************************************************************
*                                                                              *
* TextBuffer.h -- Nirvana Editor Text Buffer Header File                       *
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

#ifndef TEXT_BUFFER_H_
#define TEXT_BUFFER_H_

#include "TextSelection.h"
#include "Rangeset.h"
#include "RangesetTable.h"
#include "util/string_view.h"

#include "gsl/gsl_util"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <deque>
#include <string>


class RangesetTable;

template <class Ch = char, class Tr = std::char_traits<Ch>>
class BasicTextBuffer {
public:
    using string_type = std::basic_string<Ch, Tr>;
    using view_type   = view::basic_string_view<Ch, Tr>;

public:
    using bufModifyCallbackProc    = void (*)(int pos, int nInserted, int nDeleted, int nRestyled, view_type deletedText, void *user);
    using bufPreDeleteCallbackProc = void (*)(int pos, int nDeleted, void *user);

private:
    /* Initial size for the buffer gap (empty space in the buffer where text
     * might be inserted if the user is typing sequential chars)
     */
    static constexpr int PreferredGapSize = 80;

public:
    /* Maximum length in characters of a tab or control character expansion
     * of a single buffer character
     */
    static constexpr int MAX_EXP_CHAR_LEN = 20;

public:
    BasicTextBuffer();
    explicit BasicTextBuffer(int requestedSize);
    BasicTextBuffer(const BasicTextBuffer &)            = delete;
    BasicTextBuffer &operator=(const BasicTextBuffer &) = delete;
    ~BasicTextBuffer() noexcept;

public:
    static int BufCharWidth(Ch c, int indent, int tabDist) noexcept;
    static int BufExpandCharacter(Ch ch, int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;
    static int BufExpandTab(int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;

public:
    bool BufIsEmpty() const noexcept;
    bool BufSearchBackwardEx(int startPos, view_type searchChars, int *foundPos) const noexcept;
    bool BufSearchForwardEx(int startPos, view_type searchChars, int *foundPos) const noexcept;
    Ch BufGetCharacter(int pos) const noexcept;
    const Ch *BufAsString() noexcept;
    int BufCmpEx(int pos, view_type cmpText) noexcept;
    int BufCountBackwardNLines(int startPos, int nLines) const noexcept;
    int BufCountDispChars(int lineStartPos, int targetPos) const noexcept;
    int BufCountForwardDispChars(int lineStartPos, int nChars) const noexcept;
    int BufCountForwardNLines(int startPos, int nLines) const noexcept;
    int BufCountLines(int startPos, int endPos) const noexcept;
    int BufEndOfLine(int pos) const noexcept;
    int BufGetEmptySelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    int BufGetExpandedChar(int pos, int indent, Ch outStr[MAX_EXP_CHAR_LEN]) const noexcept;
    int BufGetHighlightPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) noexcept;
    int BufGetLength() const noexcept;
    int BufGetSecSelectPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    int BufGetSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    int BufGetTabDistance() const noexcept;
    int BufStartOfLine(int pos) const noexcept;
    string_type BufGetAllEx() const;
    string_type BufGetRangeEx(int start, int end) const;
    string_type BufGetSecSelectTextEx() const;
    string_type BufGetSelectionTextEx() const;
    string_type BufGetTextInRectEx(int start, int end, int rectStart, int rectEnd) const;
    view_type BufAsStringEx() noexcept;
    void BufAddHighPriorityModifyCB(bufModifyCallbackProc bufModifiedCB, void *user);
    void BufAddModifyCB(bufModifyCallbackProc bufModifiedCB, void *user);
    void BufAddPreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *user);
    void BufAppendEx(view_type text) noexcept;
    void BufAppendEx(Ch ch) noexcept;
    void BufCheckDisplay(int start, int end) const noexcept;
    void BufClearRect(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufCopyFromBuf(BasicTextBuffer *fromBuf, int fromStart, int fromEnd, int toPos) noexcept;
    void BufHighlight(int start, int end) noexcept;
    void BufInsertColEx(int column, int startPos, view_type text, int *charsInserted, int *charsDeleted) noexcept;
    void BufInsertEx(int pos, view_type text) noexcept;
    void BufInsertEx(int pos, Ch ch) noexcept;
    void BufOverlayRectEx(int startPos, int rectStart, int rectEnd, view_type text, int *charsInserted, int *charsDeleted) noexcept;
    void BufRectHighlight(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufRectSelect(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufRemove(int start, int end) noexcept;
    void BufRemoveModifyCB(bufModifyCallbackProc bufModifiedCB, void *user) noexcept;
    void BufRemovePreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *user) noexcept;
    void BufRemoveRect(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufRemoveSecSelect() noexcept;
    void BufRemoveSelected() noexcept;
    void BufReplaceEx(int start, int end, view_type text) noexcept;
    void BufReplaceRectEx(int start, int end, int rectStart, int rectEnd, view_type text);
    void BufReplaceSecSelectEx(view_type text) noexcept;
    void BufReplaceSelectedEx(view_type text) noexcept;
    void BufSecRectSelect(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufSecondarySelect(int start, int end) noexcept;
    void BufSecondaryUnselect() noexcept;
    void BufSelect(int start, int end) noexcept;
    void BufSetAllEx(view_type text);
    void BufSetTabDistance(int tabDist) noexcept;
    void BufUnhighlight() noexcept;
    void BufUnselect() noexcept;

public:
    // TODO(eteran): 2.0, implement an STL style interface.
    // Might allow some interesting use of algorithms

public:
    bool GetSimpleSelection(int *left, int *right) noexcept;

private:
    bool searchBackward(int startPos, Ch searchChar, int *foundPos) const noexcept;
    bool searchForward(int startPos, Ch searchChar, int *foundPos) const noexcept;
    int insertEx(int pos, view_type text) noexcept;
    int insertEx(int pos, Ch ch) noexcept;
    string_type getSelectionTextEx(const TextSelection *sel) const;
    void callModifyCBs(int pos, int nDeleted, int nInserted, int nRestyled, view_type deletedText) const noexcept;
    void callPreDeleteCBs(int pos, int nDeleted) const noexcept;
    void deleteRange(int start, int end) noexcept;
    void deleteRect(int start, int end, int rectStart, int rectEnd, int *replaceLen, int *endPos);
    void findRectSelBoundariesForCopy(int lineStartPos, int rectStart, int rectEnd, int *selStart, int *selEnd) const noexcept;
    void insertColEx(int column, int startPos, view_type insText, int *nDeleted, int *nInserted, int *endPos);
    void moveGap(int pos) noexcept;
    void overlayRectEx(int startPos, int rectStart, int rectEnd, view_type insText, int *nDeleted, int *nInserted, int *endPos);
    void reallocateBuf(int newGapStart, int newGapLen);
    void redisplaySelection(const TextSelection *oldSelection, TextSelection *newSelection) noexcept;
    void removeSelected(const TextSelection *sel) noexcept;
    void replaceSelectedEx(TextSelection *sel, view_type text) noexcept;
    void updateSelections(int pos, int nDeleted, int nInserted) noexcept;

private:
    static string_type unexpandTabsEx(view_type text, int startIndent, int tabDist);
    static string_type expandTabsEx(view_type text, int startIndent, int tabDist);
    static string_type realignTabsEx(view_type text, int origIndent, int newIndent, int tabDist, int useTabs) noexcept;
    static void subsChars(Ch *string, int length, Ch fromChar, Ch toChar) noexcept;
    static void subsCharsEx(string_type &string, Ch fromChar, Ch toChar) noexcept;
    static void insertColInLineEx(view_type line, view_type insLine, int column, int insWidth, int tabDist, int useTabs, string_type *outStr, int *endOffset) noexcept;
    static void deleteRectFromLine(view_type line, int rectStart, int rectEnd, int tabDist, int useTabs, string_type *outStr, int *endOffset) noexcept;
    static int textWidthEx(view_type text, int tabDist) noexcept;
    static int countLinesEx(view_type string) noexcept;
    static void overlayRectInLineEx(view_type line, view_type insLine, int rectStart, int rectEnd, int tabDist, int useTabs, string_type *outStr, int *endOffset) noexcept;
    static const Ch *controlCharacter(size_t index) noexcept;

private:
    template <class Out>
    static int addPaddingEx(Out out, int startIndent, int toIndent, int tabDist, int useTabs) noexcept;

    template <class Ran>
    static string_type copyLineEx(Ran first, Ran last) noexcept;

private:
    Ch *buf_;      // allocated memory where the text is stored
    int gapStart_; // points to the first character of the gap
    int gapEnd_;   // points to the first char after the gap
    int length_;   // length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd gapStart + length)
	
public:
	// TODO(eteran): accessors
	TextSelection primary_;                                                  // highlighted areas
	TextSelection secondary_;
	TextSelection highlight_;	
	int tabDist_;                                                            // equiv. number of characters in a tab
	bool useTabs_;                                                           // True if buffer routines are allowed to use tabs for padding in rectangular operations
	std::deque<std::pair<bufModifyCallbackProc, void *>> modifyProcs_;       // procedures to call when buffer is modified to redisplay contents
	std::deque<std::pair<bufPreDeleteCallbackProc, void *>> preDeleteProcs_; // procedure to call before text is deleted from the buffer; at most one is supported.
	int cursorPosHint_;                                                      // hint for reasonable cursor position after a buffer modification operation
	RangesetTable *rangesetTable_;                                           // current range sets
};

#include "TextBuffer.tcc"

using TextBuffer = BasicTextBuffer<char>;

#endif
