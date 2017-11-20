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
#include "util/string_view.h"

#include <string>
#include <deque>

// Maximum length in characters of a tab or control character expansion
// of a single buffer character
constexpr int MAX_EXP_CHAR_LEN = 20;

#if 0
#define PURIFY
#endif

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
    /* Initial size for the buffer gap (empty space in the buffer where text might
     * be inserted if the user is typing sequential chars)
     */
    static constexpr int PreferredGapSize = 80;

public:
    BasicTextBuffer();
    explicit BasicTextBuffer(int requestedSize);
    BasicTextBuffer(const BasicTextBuffer &)            = delete;
    BasicTextBuffer &operator=(const BasicTextBuffer &) = delete;
    ~BasicTextBuffer();

public:
    static int BufCharWidth(Ch c, int indent, int tabDist, Ch nullSubsChar);
    static int BufExpandCharacter(Ch c, int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist, Ch nullSubsChar);

public:
    bool BufSubstituteNullChars(Ch *string, int length);

private:
    void BufUnsubstituteNullChars(Ch *string, int length) const;

public:
	bool BufIsEmpty() const;
    bool BufSearchBackwardEx(int startPos, view_type searchChars, int *foundPos) const;
    bool BufSearchForwardEx(int startPos, view_type searchChars, int *foundPos) const;
    bool BufSubstituteNullCharsEx(string_type &string);
    Ch BufGetCharacter(int pos) const;
    const Ch *BufAsString();
    int BufCmpEx(int pos, view_type cmpText);
	int BufCountBackwardNLines(int startPos, int nLines) const;
	int BufCountDispChars(int lineStartPos, int targetPos) const;
	int BufCountForwardDispChars(int lineStartPos, int nChars) const;
    int BufCountForwardNLines(int startPos, int nLines) const;
	int BufCountLines(int startPos, int endPos) const;
	int BufEndOfLine(int pos) const;
	int BufGetEmptySelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd);
    int BufGetExpandedChar(int pos, int indent, Ch *outStr) const;
	int BufGetHighlightPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd);
	int BufGetLength() const;
	int BufGetSecSelectPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd);
	int BufGetSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd);
	int BufGetTabDistance() const;
	int BufStartOfLine(int pos) const;
    string_type BufGetAllEx();
    string_type BufGetRangeEx(int start, int end);
    string_type BufGetSecSelectTextEx();
    string_type BufGetSelectionTextEx();
    string_type BufGetTextInRectEx(int start, int end, int rectStart, int rectEnd);
    view_type BufAsStringEx();
	void BufAddHighPriorityModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg);
	void BufAddModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg);
	void BufAddPreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg);
    void BufAppendEx(view_type text);
	void BufCheckDisplay(int start, int end) const;
	void BufClearRect(int start, int end, int rectStart, int rectEnd);
    void BufCopyFromBuf(BasicTextBuffer *fromBuf, int fromStart, int fromEnd, int toPos);
	void BufHighlight(int start, int end);
    void BufInsertColEx(int column, int startPos, view_type text, int *charsInserted, int *charsDeleted);
    void BufInsertEx(int pos, view_type text);
    void BufOverlayRectEx(int startPos, int rectStart, int rectEnd, view_type text, int *charsInserted, int *charsDeleted);
	void BufRectHighlight(int start, int end, int rectStart, int rectEnd);
	void BufRectSelect(int start, int end, int rectStart, int rectEnd);
	void BufRemove(int start, int end);
	void BufRemoveModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg);
	void BufRemovePreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg);
	void BufRemoveRect(int start, int end, int rectStart, int rectEnd);
	void BufRemoveSecSelect();
	void BufRemoveSelected();
    void BufReplaceEx(int start, int end, view_type text);
    void BufReplaceRectEx(int start, int end, int rectStart, int rectEnd, view_type text);
    void BufReplaceSecSelectEx(view_type text);
    void BufReplaceSelectedEx(view_type text);
	void BufSecRectSelect(int start, int end, int rectStart, int rectEnd);
	void BufSecondarySelect(int start, int end);
	void BufSecondaryUnselect();
	void BufSelect(int start, int end);
    void BufSetAllEx(view_type text);
	void BufSetTabDistance(int tabDist);
	void BufUnhighlight();
	void BufUnselect();
    void BufUnsubstituteNullCharsEx(string_type &string) const;

public:
    // TODO(eteran): 2.0, implement an STL style interface.
    // Might allow some interesting use of algorithms

public:
	bool GetSimpleSelection(int *left, int *right);

private:
    bool searchBackward(int startPos, Ch searchChar, int *foundPos) const;
    bool searchForward(int startPos, Ch searchChar, int *foundPos) const;
    int insertEx(int pos, view_type text);
    int insertEx(int pos, Ch ch);
    string_type getSelectionTextEx(TextSelection *sel);
    void callModifyCBs(int pos, int nDeleted, int nInserted, int nRestyled, view_type deletedText) const;
	void callPreDeleteCBs(int pos, int nDeleted) const;
	void deleteRange(int start, int end);
	void deleteRect(int start, int end, int rectStart, int rectEnd, int *replaceLen, int *endPos);
	void findRectSelBoundariesForCopy(int lineStartPos, int rectStart, int rectEnd, int *selStart, int *selEnd);
    void insertColEx(int column, int startPos, view_type insText, int *nDeleted, int *nInserted, int *endPos);
	void moveGap(int pos);
    void overlayRectEx(int startPos, int rectStart, int rectEnd, view_type insText, int *nDeleted, int *nInserted, int *endPos);
	void reallocateBuf(int newGapStart, int newGapLen);
	void redisplaySelection(TextSelection *oldSelection, TextSelection *newSelection);
	void removeSelected(TextSelection *sel);
    void replaceSelectedEx(TextSelection *sel, view_type text);
	void updateSelections(int pos, int nDeleted, int nInserted);

public:
    static string_type unexpandTabsEx(view_type text, int startIndent, int tabDist, Ch nullSubsChar);
    static string_type expandTabsEx(view_type text, int startIndent, int tabDist, Ch nullSubsChar);
    static string_type realignTabsEx(view_type text, int origIndent, int newIndent, int tabDist, int useTabs, Ch nullSubsChar);
    static void histogramCharactersEx(view_type string, bool hist[256], bool init);
    static void subsChars(Ch *string, int length, Ch fromChar, Ch toChar);
    static void subsCharsEx(string_type &string, Ch fromChar, Ch toChar);
    static Ch chooseNullSubsChar(bool hist[256]);
    static void insertColInLineEx(view_type line, view_type insLine, int column, int insWidth, int tabDist, int useTabs, Ch nullSubsChar, string_type *outStr, int *endOffset);
    static void deleteRectFromLine(view_type line, int rectStart, int rectEnd, int tabDist, int useTabs, Ch nullSubsChar, string_type *outStr, int *endOffset);
    static int textWidthEx(view_type text, int tabDist, Ch nullSubsChar);
    static int countLinesEx(view_type string);
    static void overlayRectInLineEx(view_type line, view_type insLine, int rectStart, int rectEnd, int tabDist, int useTabs, Ch nullSubsChar, string_type *outStr, int *endOffset);
    static const Ch *controlCharacter(size_t index);

public:
    template <class Out>
    static int addPaddingEx(Out out, int startIndent, int toIndent, int tabDist, int useTabs, Ch nullSubsChar);

    template <class Ran>
    static string_type copyLineEx(Ran first, Ran last);

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
    Ch nullSubsChar_;                                                        // NEdit is based on C null-terminated strings, so ascii-nul characters must be substituted with something else.  This is the else, but of course, things get quite messy when you use it
	RangesetTable *rangesetTable_;                                           // current range sets
};

#include "TextBuffer.tcc"

using TextBuffer = BasicTextBuffer<char>;

#endif
