
#ifndef TEXT_BUFFER_H_
#define TEXT_BUFFER_H_

#include "TextBufferFwd.h"
#include "TextSelection.h"
#include "util/string_view.h"

#include <gsl/gsl_util>

#include <deque>
#include <string>

template <class Ch, class Tr>
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

    static constexpr int DefaultTabWidth = 8;

public:
    BasicTextBuffer();
    explicit BasicTextBuffer(int size);
    BasicTextBuffer(const BasicTextBuffer &)            = delete;
    BasicTextBuffer &operator=(const BasicTextBuffer &) = delete;
    ~BasicTextBuffer() noexcept;

public:
    static int BufCharWidth(Ch ch, int indent, int tabDist) noexcept;
    static int BufExpandCharacter(Ch ch, int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;
    static int BufExpandTab(int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;

public:
    bool BufGetUseTabs() const noexcept;
    bool BufIsEmpty() const noexcept;
    bool BufSearchBackwardEx(int startPos, view_type searchChars, int *foundPos) const noexcept;
    bool BufSearchForwardEx(int startPos, view_type searchChars, int *foundPos) const noexcept;
    Ch BufGetCharacter(int pos) const noexcept;
    view_type BufAsString() noexcept;
    int BufCmpEx(int pos, view_type cmpText) const noexcept;
    int BufCmpEx(int pos, Ch ch) const noexcept;
    int BufCountBackwardNLines(int startPos, int nLines) const noexcept;
    int BufCountDispChars(int lineStartPos, int targetPos) const noexcept;
    int BufCountForwardDispChars(int lineStartPos, int nChars) const noexcept;
    int BufCountForwardNLines(int startPos, int nLines) const noexcept;
    int BufCountLines(int startPos, int endPos) const noexcept;
    int BufCursorPosHint() const noexcept;
    int BufEndOfLine(int pos) const noexcept;
    bool BufGetEmptySelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    int BufGetExpandedChar(int pos, int indent, Ch outStr[MAX_EXP_CHAR_LEN]) const noexcept;
    int BufGetHighlightPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    int BufGetLength() const noexcept;
    bool BufGetSecSelectPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    bool BufGetSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept;
    int BufGetTabDistance() const noexcept;
    int BufGetTabDist() const noexcept;
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
    void BufAppendEx(Ch ch) noexcept;
    void BufAppendEx(view_type text) noexcept;
    void BufCheckDisplay(int start, int end) const noexcept;
    void BufClearRect(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufCopyFromBuf(BasicTextBuffer *fromBuf, int fromStart, int fromEnd, int toPos) noexcept;
    void BufHighlight(int start, int end) noexcept;
    void BufInsertColEx(int column, int startPos, view_type text, int *charsInserted, int *charsDeleted) noexcept;
    void BufInsertEx(int pos, Ch ch) noexcept;
    void BufInsertEx(int pos, view_type text) noexcept;
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
    void BufSecondarySelect(int start, int end) noexcept;
    void BufSecondaryUnselect() noexcept;
    void BufSecRectSelect(int start, int end, int rectStart, int rectEnd) noexcept;
    void BufSelect(int start, int end) noexcept;
    void BufSetAllEx(view_type text);
    void BufSetTabDistance(int tabDist) noexcept;
    void BufSetUseTabs(bool useTabs) noexcept;
    void BufUnhighlight() noexcept;
    void BufUnselect() noexcept;
    const TextSelection &BufGetPrimary() const;
    const TextSelection &BufGetSecondary() const;
    const TextSelection &BufGetHighlight() const;
    TextSelection &BufGetPrimary();
    TextSelection &BufGetSecondary();
    TextSelection &BufGetHighlight();
    bool BufGetSyncXSelection() const;
    bool BufSetSyncXSelection(bool sync);

public:
    /* unlike BufSetTabDistance, this version doesn't execute all of the
     * modification callbacks */
    void BufSetTabDist(int dist) noexcept;

public:
    bool GetSimpleSelection(int *left, int *right) const noexcept;

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
    static string_type realignTabsEx(view_type text, int origIndent, int newIndent, int tabDist, bool useTabs) noexcept;
    static void insertColInLineEx(view_type line, view_type insLine, int column, int insWidth, int tabDist, bool useTabs, string_type *outStr, int *endOffset) noexcept;
    static void deleteRectFromLine(view_type line, int rectStart, int rectEnd, int tabDist, bool useTabs, string_type *outStr, int *endOffset) noexcept;
    static int textWidthEx(view_type text, int tabDist) noexcept;
    static int countLinesEx(view_type string) noexcept;
    static void overlayRectInLineEx(view_type line, view_type insLine, int rectStart, int rectEnd, int tabDist, bool useTabs, string_type *outStr, int *endOffset) noexcept;
    static const Ch *controlCharacter(size_t index) noexcept;

private:
    template <class Out>
    static int addPaddingEx(Out out, int startIndent, int toIndent, int tabDist, bool useTabs) noexcept;

    template <class Ran>
    static string_type copyLineEx(Ran first, Ran last);

private:
    int gapStart_        = 0;                // points to the first character of the gap
    int gapEnd_          = PreferredGapSize; // points to the first char after the gap
    int length_          = 0;                // length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd gapStart + length)
    int tabDist_         = DefaultTabWidth;  // equiv. number of characters in a tab
    bool useTabs_        = true;             // true if buffer routines are allowed to use tabs for padding in rectangular operations
    int cursorPosHint_   = 0;                // hint for reasonable cursor position after a buffer modification operation
    bool syncXSelection_ = true;

private:
    Ch *buf_;                 // allocated memory where the text is stored
    TextSelection primary_;   // highlighted areas
    TextSelection secondary_;
    TextSelection highlight_;

private:
    std::deque<std::pair<bufPreDeleteCallbackProc, void *>> preDeleteProcs_; // procedure to call before text is deleted from the buffer; at most one is supported.
    std::deque<std::pair<bufModifyCallbackProc, void *>> modifyProcs_;       // procedures to call when buffer is modified to redisplay contents
};


#include "TextBuffer.tcc"

#endif
