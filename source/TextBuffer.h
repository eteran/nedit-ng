
#ifndef TEXT_BUFFER_H_
#define TEXT_BUFFER_H_

#include "gap_buffer.h"
#include "TextBufferFwd.h"
#include "TextSelection.h"
#include "Util/string_view.h"

#include <gsl/gsl_util>

#include <deque>
#include <string>

#include <boost/optional.hpp>

template <class Ch, class Tr>
class BasicTextBuffer {
public:
    using string_type = std::basic_string<Ch, Tr>;
    using view_type   = view::basic_string_view<Ch, Tr>;

public:
    using bufModifyCallbackProc    = void (*)(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view_type deletedText, void *user);
    using bufPreDeleteCallbackProc = void (*)(int64_t pos, int64_t nDeleted, void *user);

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
    explicit BasicTextBuffer(int64_t size);
    BasicTextBuffer(const BasicTextBuffer &)            = delete;
    BasicTextBuffer &operator=(const BasicTextBuffer &) = delete;
    ~BasicTextBuffer() noexcept                         = default;

public:
    static int64_t BufCharWidth(Ch ch, int64_t indent, int tabDist) noexcept;
    static int64_t BufExpandCharacter(Ch ch, int64_t indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;
    static int BufExpandTab(int64_t indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;

public:
    bool BufGetUseTabs() const noexcept;
    bool BufIsEmpty() const noexcept;
    boost::optional<int64_t> BufSearchBackwardEx(int64_t startPos, view_type searchChars) const noexcept;
    boost::optional<int64_t> BufSearchForwardEx(int64_t startPos, view_type searchChars) const noexcept;
    Ch BufGetCharacter(int64_t pos) const noexcept;
    int BufCmpEx(int64_t pos, view_type cmpText) const noexcept;
    int BufCmpEx(int64_t pos, Ch *cmpText, int64_t size) const noexcept;
    int BufCmpEx(int64_t pos, Ch ch) const noexcept;
    int64_t BufCountBackwardNLines(int64_t startPos, int64_t nLines) const noexcept;
    int BufCountDispChars(int64_t lineStartPos, int64_t targetPos) const noexcept;
    int64_t BufCountForwardDispChars(int64_t lineStartPos, int64_t nChars) const noexcept;
    int64_t BufCountForwardNLines(int64_t startPos, int64_t nLines) const noexcept;
    int64_t BufCountLines(int64_t startPos, int64_t endPos) const noexcept;
    int64_t BufCursorPosHint() const noexcept;
    int64_t BufEndOfLine(int64_t pos) const noexcept;
    bool BufGetEmptySelectionPos(int64_t *start, int64_t *end, bool *isRect, int64_t *rectStart, int64_t *rectEnd) const noexcept;
    int64_t BufGetExpandedChar(int64_t pos, int64_t indent, Ch outStr[MAX_EXP_CHAR_LEN]) const noexcept;
    int64_t BufGetLength() const noexcept;
    bool BufGetSelectionPos(int64_t *start, int64_t *end, bool *isRect, int64_t *rectStart, int64_t *rectEnd) const noexcept;
    int BufGetTabDistance() const noexcept;
    int BufGetTabDist() const noexcept;
    int64_t BufStartOfLine(int64_t pos) const noexcept;
    string_type BufGetAllEx() const;
    string_type BufGetRangeEx(int64_t start, int64_t end) const;
    string_type BufGetSecSelectTextEx() const;
    string_type BufGetSelectionTextEx() const;
    string_type BufGetTextInRectEx(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd) const;
    view_type BufAsStringEx() noexcept;
    void BufAddHighPriorityModifyCB(bufModifyCallbackProc bufModifiedCB, void *user);
    void BufAddModifyCB(bufModifyCallbackProc bufModifiedCB, void *user);
    void BufAddPreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *user);
    void BufAppendEx(Ch ch) noexcept;
    void BufAppendEx(view_type text) noexcept;
    void BufCheckDisplay(int64_t start, int64_t end) const noexcept;
    void BufClearRect(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd) noexcept;
    void BufCopyFromBuf(BasicTextBuffer *fromBuf, int64_t fromStart, int64_t fromEnd, int64_t toPos) noexcept;
    void BufHighlight(int64_t start, int64_t end) noexcept;
    void BufInsertColEx(int64_t column, int64_t startPos, view_type text, int64_t *charsInserted, int64_t *charsDeleted) noexcept;
    void BufInsertEx(int64_t pos, Ch ch) noexcept;
    void BufInsertEx(int64_t pos, view_type text) noexcept;
    void BufOverlayRectEx(int64_t startPos, int64_t rectStart, int64_t rectEnd, view_type text, int64_t *charsInserted, int64_t *charsDeleted) noexcept;
    void BufRectHighlight(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd) noexcept;
    void BufRectSelect(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd) noexcept;
    void BufRemove(int64_t start, int64_t end) noexcept;
    void BufRemoveModifyCB(bufModifyCallbackProc bufModifiedCB, void *user) noexcept;
    void BufRemovePreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *user) noexcept;
    void BufRemoveRect(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd) noexcept;
    void BufRemoveSecSelect() noexcept;
    void BufRemoveSelected() noexcept;
    void BufReplaceEx(int64_t start, int64_t end, view_type text) noexcept;
    void BufReplaceRectEx(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd, view_type text);
    void BufReplaceSecSelectEx(view_type text) noexcept;
    void BufReplaceSelectedEx(view_type text) noexcept;
    void BufSecondarySelect(int64_t start, int64_t end) noexcept;
    void BufSecondaryUnselect() noexcept;
    void BufSecRectSelect(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd) noexcept;
    void BufSelect(int64_t start, int64_t end) noexcept;
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
    bool GetSimpleSelection(int64_t *left, int64_t *right) const noexcept;

private:
    boost::optional<int64_t> searchBackward(int64_t startPos, Ch searchChar) const noexcept;
    boost::optional<int64_t> searchForward(int64_t startPos, Ch searchChar) const noexcept;
    int64_t insertEx(int64_t pos, view_type text) noexcept;
    int64_t insertEx(int64_t pos, Ch ch) noexcept;
    string_type getSelectionTextEx(const TextSelection *sel) const;
    void callModifyCBs(int64_t pos, int64_t nDeleted, int64_t nInserted, int64_t nRestyled, view_type deletedText) const noexcept;
    void callPreDeleteCBs(int64_t pos, int64_t nDeleted) const noexcept;
    void deleteRange(int64_t start, int64_t end) noexcept;
    void deleteRect(int64_t start, int64_t end, int64_t rectStart, int64_t rectEnd, int64_t *replaceLen, int64_t *endPos);
    void findRectSelBoundariesForCopy(int64_t lineStartPos, int64_t rectStart, int64_t rectEnd, int64_t *selStart, int64_t *selEnd) const noexcept;
    void insertColEx(int64_t column, int64_t startPos, view_type insText, int64_t *nDeleted, int64_t *nInserted, int64_t *endPos);
    void overlayRectEx(int64_t startPos, int64_t rectStart, int64_t rectEnd, view_type insText, int64_t *nDeleted, int64_t *nInserted, int64_t *endPos);
    void redisplaySelection(const TextSelection *oldSelection, TextSelection *newSelection) noexcept;
    void removeSelected(const TextSelection *sel) noexcept;
    void replaceSelectedEx(TextSelection *sel, view_type text) noexcept;
    void updateSelections(int64_t pos, int64_t nDeleted, int64_t nInserted) noexcept;

private:
    static string_type unexpandTabsEx(view_type text, int64_t startIndent, int tabDist);
    static string_type expandTabsEx(view_type text, int64_t startIndent, int tabDist);
    static string_type realignTabsEx(view_type text, int64_t origIndent, int64_t newIndent, int tabDist, bool useTabs) noexcept;
    static void insertColInLineEx(view_type line, view_type insLine, int64_t column, int insWidth, int tabDist, bool useTabs, string_type *outStr, int64_t *endOffset) noexcept;
    static void deleteRectFromLine(view_type line, int64_t rectStart, int64_t rectEnd, int tabDist, bool useTabs, string_type *outStr, int64_t *endOffset) noexcept;
    static int textWidthEx(view_type text, int tabDist) noexcept;
    static int64_t countLinesEx(view_type string) noexcept;
    static void overlayRectInLineEx(view_type line, view_type insLine, int64_t rectStart, int64_t rectEnd, int tabDist, bool useTabs, string_type *outStr, int64_t *endOffset) noexcept;
    static const Ch *controlCharacter(size_t index) noexcept;

private:
    template <class Out>
    static int addPaddingEx(Out out, int64_t startIndent, int64_t toIndent, int tabDist, bool useTabs) noexcept;

    template <class Ran>
    static string_type copyLineEx(Ran first, Ran last);

private:
    int tabDist_           = DefaultTabWidth;  // equiv. number of characters in a tab
    bool useTabs_          = true;             // true if buffer routines are allowed to use tabs for padding in rectangular operations
    int64_t cursorPosHint_ = 0;                // hint for reasonable cursor position after a buffer modification operation
    bool syncXSelection_   = true;

private:
    gap_buffer<char> buffer_;
    TextSelection    primary_;   // highlighted areas
    TextSelection    secondary_;
    TextSelection    highlight_;

private:
    std::deque<std::pair<bufPreDeleteCallbackProc, void *>> preDeleteProcs_; // procedure to call before text is deleted from the buffer; at most one is supported.
    std::deque<std::pair<bufModifyCallbackProc, void *>> modifyProcs_;       // procedures to call when buffer is modified to redisplay contents
};


#include "TextBuffer.tcc"

#endif
