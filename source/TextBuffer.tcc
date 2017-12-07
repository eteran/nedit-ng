
#ifndef TEXT_BUFFER_TCC_
#define TEXT_BUFFER_TCC_

#include <algorithm>
#include <cassert>
#include <QtDebug>
#include <QApplication>
#include <QClipboard>
#include "util/raise.h"
#include "util/algorithm.h"

/*
** Free a text buffer
*/
template <class Ch, class Tr>
BasicTextBuffer<Ch, Tr>::~BasicTextBuffer() noexcept {
    delete [] buf_;    
}

/*
** Get the entire contents of a text buffer.  Memory is allocated to contain
** the returned string, which the caller must free.
*/
template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::BufGetAllEx() const -> string_type {
    string_type text;
    text.reserve(static_cast<size_t>(length_));

    std::copy_n(buf_,           gapStart_,           std::back_inserter(text));
    std::copy_n(&buf_[gapEnd_], length_ - gapStart_, std::back_inserter(text));

    return text;
}

/*
** Get the entire contents of a text buffer as a single string.  The gap is
** moved so that the buffer data can be accessed as a single contiguous
** character array.
** NB DO NOT ALTER THE TEXT THROUGH THE RETURNED POINTER!
** (we make an exception in TextBuffer::BufSubstituteNullCharsEx() however)
** This function is intended ONLY to provide a searchable string without copying
** into a temporary buffer.
*/
template <class Ch, class Tr>
const Ch *BasicTextBuffer<Ch, Tr>::BufAsString() noexcept {

    const int bufLen   = length_;
    int leftLen        = gapStart_;
    const int rightLen = bufLen - leftLen;

    // find where best to put the gap to minimise memory movement
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        moveGap(leftLen);
    }

    // get the start position of the actual data
    Ch *const text = &buf_[(leftLen == 0) ? gapEnd_ : 0];

    // make sure it's null-terminated
    text[bufLen] = Ch();
    return text;
}

/*
** Get the entire contents of a text buffer as a single string.  The gap is
** moved so that the buffer data can be accessed as a single contiguous
** character array.
** NB DO NOT ALTER THE TEXT THROUGH THE RETURNED POINTER!
** (we make an exception in TextBuffer::BufSubstituteNullCharsEx() however)
** This function is intended ONLY to provide a searchable string without copying
** into a temporary buffer.
*/
template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::BufAsStringEx() noexcept -> view_type {

    const int bufLen   = length_;
    int leftLen        = gapStart_;
    const int rightLen = bufLen - leftLen;

    // find where best to put the gap to minimise memory movement
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        moveGap(leftLen);
    }

    // get the start position of the actual data
    Ch *const text = &buf_[(leftLen == 0) ? gapEnd_ : 0];

    return view_type(text, bufLen);
}

/*
** Replace the entire contents of the text buffer
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSetAllEx(view_type text) {

    const auto length = gsl::narrow<int>(text.size());

    callPreDeleteCBs(0, length_);

    // Save information for redisplay, and get rid of the old buffer
    const string_type deletedText = BufGetAllEx();

    delete [] buf_;

    // Start a new buffer with a gap of PreferredGapSize in the center
    buf_ = new Ch[length + PreferredGapSize + 1];
    buf_[length + PreferredGapSize] = Ch();

    length_   = length;
    gapStart_ = length / 2;
    gapEnd_   = gapStart_ + PreferredGapSize;

    std::copy_n(&text[0], gapStart_, buf_);
    std::copy_n(&text[gapStart_], length - gapStart_, &buf_[gapEnd_]);

#ifdef PURIFY
    std::fill(&buf_[gapStart_], &buf_[gapEnd_], Ch('.'));
#endif

    // Zero all of the existing selections
    updateSelections(0, deletedText.size(), 0);

    // Call the saved display routine(s) to update the screen
    callModifyCBs(0, deletedText.size(), length, 0, deletedText);
}

/*
** Return a copy of the text between "start" and "end" character positions
** from text buffer "buf".  Positions start at 0, and the range does not
** include the character pointed to by "end"
*/
template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::BufGetRangeEx(int start, int end) const -> string_type {

    string_type text;

    /* Make sure start and end are ok.
       If start is bad, return "", if end is bad, adjust it. */
    if (start < 0 || start > length_) {
        return text;
    }

    if (end < start) {
        std::swap(start, end);
    }

    if (end > length_) {
        end = length_;
    }

    const int length = end - start;
    text.reserve(length);

    // Copy the text from the buffer to the returned string
    if (end <= gapStart_) {
        std::copy_n(&buf_[start], length, std::back_inserter(text));
    } else if (start >= gapStart_) {
        std::copy_n(&buf_[start + (gapEnd_ - gapStart_)], length, std::back_inserter(text));
    } else {
        const int part1Length = gapStart_ - start;

        std::copy_n(&buf_[start],   part1Length,          std::back_inserter(text));
        std::copy_n(&buf_[gapEnd_], length - part1Length, std::back_inserter(text));
    }

    return text;
}

/*
** Return the character at buffer position "pos".  Positions start at 0.
*/
template <class Ch, class Tr>
Ch BasicTextBuffer<Ch, Tr>::BufGetCharacter(int pos) const noexcept {

    if (pos < 0 || pos >= length_)
        return Ch();

    if (pos < gapStart_)
        return buf_[pos];

    return buf_[pos + (gapEnd_ - gapStart_)];
}

/*
** Insert null-terminated string "text" at position "pos" in "buf"
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufInsertEx(int pos, view_type text) noexcept {

    // if pos is not contiguous to existing text, make it
    if (pos > length_) {
        pos = length_;
    }

    if (pos < 0) {
        pos = 0;
    }

    // Even if nothing is deleted, we must call these callbacks
    callPreDeleteCBs(pos, 0);

    // insert and redisplay
    const int nInserted = insertEx(pos, text);
    cursorPosHint_ = pos + nInserted;
    callModifyCBs(pos, 0, nInserted, 0, {});
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufInsertEx(int pos, Ch ch) noexcept {

    // if pos is not contiguous to existing text, make it
    if (pos > length_) {
        pos = length_;
    }

    if (pos < 0) {
        pos = 0;
    }

    // Even if nothing is deleted, we must call these callbacks
    callPreDeleteCBs(pos, 0);

    // insert and redisplay
    const int nInserted = insertEx(pos, ch);
    cursorPosHint_ = pos + nInserted;
    callModifyCBs(pos, 0, nInserted, 0, {});
}

/*
** Delete the characters between "start" and "end", and insert the
** null-terminated string "text" in their place in in "buf"
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufReplaceEx(int start, int end, view_type text) noexcept {

    // TODO(eteran): 2.0, do same type of parameter normalization as BufRemove does?

    const auto nInserted = gsl::narrow<int>(text.size());

    callPreDeleteCBs(start, end - start);
    const string_type deletedText = BufGetRangeEx(start, end);

    deleteRange(start, end);
    insertEx(start, text);
    cursorPosHint_ = start + nInserted;
    callModifyCBs(start, end - start, nInserted, 0, deletedText);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRemove(int start, int end) noexcept {

    // Make sure the arguments make sense
    if (start > end) {
        std::swap(start, end);
    }

    if (start > length_) {
        start = length_;
    }

    if (start < 0) {
        start = 0;
    }

    if (end > length_) {
        end = length_;
    }

    if (end < 0) {
        end = 0;
    }

    callPreDeleteCBs(start, end - start);

    // Remove and redisplay
    const string_type deletedText = BufGetRangeEx(start, end);

    deleteRange(start, end);
    cursorPosHint_ = start;
    callModifyCBs(start, end - start, 0, 0, deletedText);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufCopyFromBuf(BasicTextBuffer<Ch, Tr> *fromBuf, int fromStart, int fromEnd, int toPos) noexcept {

    const int length = fromEnd - fromStart;

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PreferredGapSize */
    if (length > gapEnd_ - gapStart_) {
        reallocateBuf(toPos, length + PreferredGapSize);
    } else if (toPos != gapStart_) {
        moveGap(toPos);
    }

    // Insert the new text (toPos now corresponds to the start of the gap)
    if (fromEnd <= fromBuf->gapStart_) {
        std::copy_n(&fromBuf->buf_[fromStart], length, &buf_[toPos]);
    } else if (fromStart >= fromBuf->gapStart_) {
        std::copy_n(&fromBuf->buf_[fromStart + (fromBuf->gapEnd_ - fromBuf->gapStart_)], length, &buf_[toPos]);
    } else {
        const int part1Length = fromBuf->gapStart_ - fromStart;
        std::copy_n(&fromBuf->buf_[fromStart], part1Length, &buf_[toPos]);
        std::copy_n(&fromBuf->buf_[fromBuf->gapEnd_], length - part1Length, &buf_[toPos + part1Length]);
    }

    gapStart_ += length;
    length_   += length;
    updateSelections(toPos, 0, length);
}

/*
** Insert "text" columnwise into buffer starting at displayed character
** position "column" on the line beginning at "startPos".  Opens a rectangular
** space the width and height of "text", by moving all text to the right of
** "column" right.  If charsInserted and charsDeleted are not nullptr, the
** number of characters inserted and deleted in the operation (beginning
** at startPos) are returned in these arguments
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufInsertColEx(int column, int startPos, view_type text, int *charsInserted, int *charsDeleted) noexcept {

    const int nLines       = countLinesEx(text);
    const int lineStartPos = BufStartOfLine(startPos);
    const int nDeleted     = BufEndOfLine(BufCountForwardNLines(startPos, nLines)) - lineStartPos;

    callPreDeleteCBs(lineStartPos, nDeleted);
    const string_type deletedText = BufGetRangeEx(lineStartPos, lineStartPos + nDeleted);

    int insertDeleted;
    int nInserted;
    insertColEx(column, lineStartPos, text, &insertDeleted, &nInserted, &cursorPosHint_);

    if (nDeleted != insertDeleted) {
        qCritical("NEdit: internal consistency check ins1 failed");
    }

    callModifyCBs(lineStartPos, nDeleted, nInserted, 0, deletedText);

    if (charsInserted) {
        *charsInserted = nInserted;
    }

    if (charsDeleted) {
        *charsDeleted = nDeleted;
    }
}

/*
** Overlay a rectangular area of text without calling the modify callbacks.
** "nDeleted" and "nInserted" return the number of characters deleted and
** inserted beginning at the start of the line containing "startPos".
** "endPos" returns buffer position of the lower left edge of the inserted
** column (as a hint for routines which need to set a cursor position).
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::overlayRectEx(int startPos, int rectStart, int rectEnd, view_type insText, int *nDeleted, int *nInserted, int *endPos) {
    int len;
    int endOffset;

    /* Allocate a buffer for the replacement string large enough to hold
       possibly expanded tabs in the inserted text, as well as per line: 1)
       an additional 2*MAX_EXP_CHAR_LEN characters for padding where tabs
       and control characters cross the column of the selection, 2) up to
       "column" additional spaces per line for padding out to the position
       of "column", 3) padding up to the width of the inserted text if that
       must be padded to align the text beyond the inserted column.  (Space
       for additional newlines if the inserted text extends beyond the end
       of the buffer is counted with the length of insText) */
    const int start  = BufStartOfLine(startPos);
    const int nLines = countLinesEx(insText) + 1;
    const int end    = BufEndOfLine(BufCountForwardNLines(start, nLines - 1));

    const string_type expIns = expandTabsEx(insText, 0, tabDist_);

    string_type outStr;
    outStr.reserve(static_cast<size_t>(end - start) + expIns.size() + static_cast<size_t>(nLines * (rectEnd + MAX_EXP_CHAR_LEN)));

    /* Loop over all lines in the buffer between start and end overlaying the
       text between rectStart and rectEnd and padding appropriately.  Trim
       trailing space from line (whitespace at the ends of lines otherwise
       tends to multiply, since additional padding is added to maintain it */
    auto outPtr = std::back_inserter(outStr);
    int lineStart = start;
    auto insPtr = insText.begin();

    while (true) {
        const int lineEnd = BufEndOfLine(lineStart);
        const string_type line    = BufGetRangeEx(lineStart, lineEnd);
        const string_type insLine = copyLineEx(insPtr, insText.end());
        insPtr += insLine.size();

        // TODO(eteran): 2.0, remove the need for this temp
        string_type temp;
        overlayRectInLineEx(line, insLine, rectStart, rectEnd, tabDist_, useTabs_, &temp, &endOffset);
        len = gsl::narrow<int>(temp.size());

        for(auto it = outStr.rbegin(); it != outStr.rend() && (*it == Ch(' ') || *it == Ch('\t')); ++it) {
            --len;
        }

        std::copy_n(temp.begin(), len, outPtr);

        *outPtr++ = Ch('\n');
        lineStart = (lineEnd < length_) ? lineEnd + 1 : length_;

        if (insPtr == insText.end()) {
            break;
        }
        insPtr++;
    }

    // trim back off extra newline
    if(!outStr.empty()) {
        outStr.pop_back();
    }

    // replace the text between start and end with the new stuff
    deleteRange(start, end);
    insertEx(start, outStr);

    *nInserted = gsl::narrow<int>(outStr.size());
    *nDeleted  = end - start;
    *endPos    = start + gsl::narrow<int>(outStr.size()) - len + endOffset;
}

/*
** Overlay "text" between displayed character positions "rectStart" and
** "rectEnd" on the line beginning at "startPos".  If charsInserted and
** charsDeleted are not nullptr, the number of characters inserted and deleted
** in the operation (beginning at startPos) are returned in these arguments.
** If rectEnd equals -1, the width of the inserted text is measured first.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufOverlayRectEx(int startPos, int rectStart, int rectEnd, view_type text, int *charsInserted, int *charsDeleted) noexcept {

    int insertDeleted;
    int nInserted;
    int nLines = countLinesEx(text);
    int lineStartPos = BufStartOfLine(startPos);

    if (rectEnd == -1) {
        rectEnd = rectStart + textWidthEx(text, tabDist_);
    }

#if 0
    // NOTE(eteran): this call seems entirely redundant
    lineStartPos = BufStartOfLine(startPos);
#endif

    const int nDeleted = BufEndOfLine(BufCountForwardNLines(startPos, nLines)) - lineStartPos;
    callPreDeleteCBs(lineStartPos, nDeleted);

    const string_type deletedText = BufGetRangeEx(lineStartPos, lineStartPos + nDeleted);
    overlayRectEx(lineStartPos, rectStart, rectEnd, text, &insertDeleted, &nInserted, &cursorPosHint_);

    if (nDeleted != insertDeleted) {
        qCritical("NEdit: internal consistency check ovly1 failed");
    }

    callModifyCBs(lineStartPos, nDeleted, nInserted, 0, deletedText);

    if (charsInserted) {
        *charsInserted = nInserted;
    }

    if (charsDeleted) {
        *charsDeleted = nDeleted;
    }

}

/*
** Replace a rectangular area in buf, given by "start", "end", "rectStart",
** and "rectEnd", with "text".  If "text" is vertically longer than the
** rectangle, add extra lines to make room for it.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufReplaceRectEx(int start, int end, int rectStart, int rectEnd, view_type text) {

    string_type insText;
    int linesPadded = 0;

    /* Make sure start and end refer to complete lines, since the
       columnar delete and insert operations will replace whole lines */
    start = BufStartOfLine(start);
    end   = BufEndOfLine(end);

    callPreDeleteCBs(start, end - start);

    /* If more lines will be deleted than inserted, pad the inserted text
       with newlines to make it as long as the number of deleted lines.  This
       will indent all of the text to the right of the rectangle to the same
       column.  If more lines will be inserted than deleted, insert extra
       lines in the buffer at the end of the rectangle to make room for the
       additional lines in "text" */
    const int nInsertedLines = countLinesEx(text);
    const int nDeletedLines  = BufCountLines(start, end);

    if (nInsertedLines < nDeletedLines) {

        insText.reserve(text.size() + nDeletedLines - nInsertedLines);
        insText.assign(text.begin(), text.end());
        insText.append(nDeletedLines - nInsertedLines, Ch('\n'));

        // NOTE(etreran): use insText instead of the passed in buffer
        text = insText;

    } else if (nDeletedLines < nInsertedLines) {
        linesPadded = nInsertedLines - nDeletedLines;
        for (int i = 0; i < linesPadded; i++) {
            insertEx(end, Ch('\n'));
        }

    } else /* nDeletedLines == nInsertedLines */ {
    }

    // Save a copy of the text which will be modified for the modify CBs
    const string_type deletedText = BufGetRangeEx(start, end);

    // Delete then insert
    int hint;
    int deleteInserted;
    deleteRect(start, end, rectStart, rectEnd, &deleteInserted, &hint);

    int insertDeleted;
    int insertInserted;
    insertColEx(rectStart, start, text, &insertDeleted, &insertInserted, &cursorPosHint_);

    // Figure out how many chars were inserted and call modify callbacks
    if (insertDeleted != deleteInserted + linesPadded) {
        qCritical("NEdit: internal consistency check repl1 failed");
    }

    callModifyCBs(start, end - start, insertInserted, 0, deletedText);
}

/*
** Remove a rectangular swath of characters between character positions start
** and end and horizontal displayed-character offsets rectStart and rectEnd.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRemoveRect(int start, int end, int rectStart, int rectEnd) noexcept {

    start = BufStartOfLine(start);
    end   = BufEndOfLine(end);

    callPreDeleteCBs(start, end - start);

    const string_type deletedText = BufGetRangeEx(start, end);

    int nInserted;
    deleteRect(start, end, rectStart, rectEnd, &nInserted, &cursorPosHint_);

    callModifyCBs(start, end - start, nInserted, 0, deletedText);
}

/*
** Clear a rectangular "hole" out of the buffer between character positions
** start and end and horizontal displayed-character offsets rectStart and
** rectEnd.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufClearRect(int start, int end, int rectStart, int rectEnd) noexcept {

    const int nLines = BufCountLines(start, end);
    const string_type newlineString(nLines, Ch('\n'));

    BufOverlayRectEx(start, rectStart, rectEnd, newlineString, nullptr, nullptr);
}

template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::BufGetTextInRectEx(int start, int end, int rectStart, int rectEnd) const -> string_type {
    int selLeft;
    int selRight;

    start = BufStartOfLine(start);
    end   = BufEndOfLine(end);

    assert(end >= start);

    string_type textOut;
    textOut.reserve(end - start);

    int lineStart = start;
    auto outPtr = std::back_inserter(textOut);

    while (lineStart <= end) {
        findRectSelBoundariesForCopy(lineStart, rectStart, rectEnd, &selLeft, &selRight);
        const string_type textIn = BufGetRangeEx(selLeft, selRight);
        const int len = selRight - selLeft;

        std::copy_n(textIn.begin(), len, outPtr);
        lineStart = BufEndOfLine(selRight) + 1;
        *outPtr++ = Ch('\n');
    }

    // don't leave trailing newline
    if(!textOut.empty()) {
        textOut.pop_back();
    }

    /* If necessary, realign the tabs in the selection as if the text were
       positioned at the left margin */
    return realignTabsEx(textOut, rectStart, 0, tabDist_, useTabs_);
}

/*
** Get the hardware tab distance used by all displays for this buffer,
** and used in computing offsets for rectangular selection operations.
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetTabDistance() const noexcept {
    return tabDist_;
}

/*
** Set the hardware tab distance used by all displays for this buffer,
** and used in computing offsets for rectangular selection operations.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSetTabDistance(int tabDist) noexcept {

    /* First call the pre-delete callbacks with the previous tab setting
       still active. */
    callPreDeleteCBs(0, length_);

    // Change the tab setting
    tabDist_ = tabDist;

    // Force any display routines to redisplay everything
    auto deletedText = BufAsStringEx();
    callModifyCBs(0, length_, length_, 0, deletedText);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufCheckDisplay(int start, int end) const noexcept {
    // just to make sure colors in the selected region are up to date
    callModifyCBs(start, 0, 0, end - start, {});
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSelect(int start, int end) noexcept {
    const TextSelection oldSelection = primary_;

    primary_.setSelection(start, end);
    redisplaySelection(&oldSelection, &primary_);

#ifdef Q_OS_UNIX
    if(syncXSelection_) {
        std::string text = BufGetSelectionTextEx();
        QApplication::clipboard()->setText(QString::fromStdString(text), QClipboard::Selection);
    }
#endif
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufUnselect() noexcept {
    const TextSelection oldSelection = primary_;

    primary_.selected = false;
    primary_.zeroWidth = false;
    redisplaySelection(&oldSelection, &primary_);

#ifdef Q_OS_UNIX
    if(syncXSelection_) {
        QApplication::clipboard()->setText(QString(), QClipboard::Selection);
    }
#endif
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRectSelect(int start, int end, int rectStart, int rectEnd) noexcept {
    const TextSelection oldSelection = primary_;

    primary_.setRectSelect(start, end, rectStart, rectEnd);
    redisplaySelection(&oldSelection, &primary_);

#ifdef Q_OS_UNIX
    if(syncXSelection_) {
        std::string text = BufGetSelectionTextEx();
        QApplication::clipboard()->setText(QString::fromStdString(text), QClipboard::Selection);
    }
#endif
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept {
    return primary_.getSelectionPos(start, end, isRect, rectStart, rectEnd);
}

// Same as above, but also returns TRUE for empty selections
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetEmptySelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept {
    return primary_.getSelectionPos(start, end, isRect, rectStart, rectEnd) || primary_.zeroWidth;
}

template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::BufGetSelectionTextEx() const -> string_type {
    return getSelectionTextEx(&primary_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRemoveSelected() noexcept {
    removeSelected(&primary_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufReplaceSelectedEx(view_type text) noexcept {
    replaceSelectedEx(&primary_, text);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSecondarySelect(int start, int end) noexcept {
    const TextSelection oldSelection = secondary_;

    secondary_.setSelection(start, end);
    redisplaySelection(&oldSelection, &secondary_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSecondaryUnselect() noexcept {
    const TextSelection oldSelection = secondary_;

    secondary_.selected = false;
    secondary_.zeroWidth = false;
    redisplaySelection(&oldSelection, &secondary_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSecRectSelect(int start, int end, int rectStart, int rectEnd) noexcept {
    const TextSelection oldSelection = secondary_;

    secondary_.setRectSelect(start, end, rectStart, rectEnd);
    redisplaySelection(&oldSelection, &secondary_);
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetSecSelectPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept {
    return secondary_.getSelectionPos(start, end, isRect, rectStart, rectEnd);
}

template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::BufGetSecSelectTextEx() const -> string_type {
    return getSelectionTextEx(&secondary_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRemoveSecSelect() noexcept {
    removeSelected(&secondary_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufReplaceSecSelectEx(view_type text) noexcept {
    replaceSelectedEx(&secondary_, text);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufHighlight(int start, int end) noexcept {
    const TextSelection oldSelection = highlight_;

    highlight_.setSelection(start, end);
    redisplaySelection(&oldSelection, &highlight_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufUnhighlight() noexcept {
    const TextSelection oldSelection = highlight_;

    highlight_.selected = false;
    highlight_.zeroWidth = false;
    redisplaySelection(&oldSelection, &highlight_);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRectHighlight(int start, int end, int rectStart, int rectEnd) noexcept {
    const TextSelection oldSelection = highlight_;

    highlight_.setRectSelect(start, end, rectStart, rectEnd);
    redisplaySelection(&oldSelection, &highlight_);
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetHighlightPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const noexcept {
    return highlight_.getSelectionPos(start, end, isRect, rectStart, rectEnd);
}

/*
** Add a callback routine to be called when the buffer is modified
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufAddModifyCB(bufModifyCallbackProc bufModifiedCB, void *user) {
    modifyProcs_.emplace_back(bufModifiedCB, user);
}

/*
** Similar to the above, but makes sure that the callback is called before
** normal priority callbacks.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufAddHighPriorityModifyCB(bufModifyCallbackProc bufModifiedCB, void *user) {
    modifyProcs_.emplace_front(bufModifiedCB, user);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRemoveModifyCB(bufModifyCallbackProc bufModifiedCB, void *user) noexcept {

    for (auto it = modifyProcs_.begin(); it != modifyProcs_.end(); ++it) {
        auto &pair = *it;
        if (pair.first == bufModifiedCB && pair.second == user) {
            modifyProcs_.erase(it);
            return;
        }
    }

    qCritical("NEdit: Internal Error: Can't find modify CB to remove");
}

/*
** Add a callback routine to be called before text is deleted from the buffer.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufAddPreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *user) {
    preDeleteProcs_.emplace_back(bufPreDeleteCB, user);
}

/**
 *
 */
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufRemovePreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *user) noexcept {

    for (auto it = preDeleteProcs_.begin(); it != preDeleteProcs_.end(); ++it) {
        auto &pair = *it;
        if (pair.first == bufPreDeleteCB && pair.second == user) {
            preDeleteProcs_.erase(it);
            return;
        }
    }

    qCritical("NEdit: Internal Error: Can't find pre-delete CB to remove");
}

/*
** Find the position of the start of the line containing position "pos"
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufStartOfLine(int pos) const noexcept {

    int startPos;
    if (!searchBackward(pos, Ch('\n'), &startPos)) {
        return 0;
    }

    return startPos + 1;
}

/*
** Find the position of the end of the line containing position "pos"
** (which is either a pointer to the newline character ending the line,
** or a pointer to one character beyond the end of the buffer)
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufEndOfLine(int pos) const noexcept {

    int endPos;
    if (!searchForward(pos, Ch('\n'), &endPos)) {
        return length_;
    }

    return endPos;
}

/*
** Get a character from the text buffer expanded into it's screen
** representation (which may be several characters for a tab or a
** control code).  Returns the number of characters written to "outStr".
** "indent" is the number of characters from the start of the line
** for figuring tabs.
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetExpandedChar(int pos, int indent, Ch outStr[MAX_EXP_CHAR_LEN]) const noexcept {
    return BufExpandCharacter(BufGetCharacter(pos), indent, outStr, tabDist_);
}

/*
** Expand a single character from the text buffer into it's screen
** representation (which may be several characters for a tab or a
** control code).  Returns the number of characters added to "outStr".
** "indent" is the number of characters from the start of the line
** for figuring tabs.  Output string is guranteed to be shorter or
** equal in length to MAX_EXP_CHAR_LEN
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufExpandTab(int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept {
    const int nSpaces = tabDist - (indent % tabDist);

    for (int i = 0; i < nSpaces; i++) {
        outStr[i] = Ch(' ');
    }

    return nSpaces;
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufExpandCharacter(Ch ch, int indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept {

    // Convert tabs to spaces
    if (ch == Ch('\t')) {
        return BufExpandTab(indent, outStr, tabDist);
    }

#if defined(VISUAL_CTRL_CHARS)
    // Convert ASCII control codes to readable character sequences
    if ((static_cast<size_t>(ch)) < 32) {
        return snprintf(outStr, MAX_EXP_CHAR_LEN, "<%s>", controlCharacter(ch));
    }

    if (ch == 127) {
        return snprintf(outStr, MAX_EXP_CHAR_LEN, "<del>");
    }
#endif
    // Otherwise, just return the character
    *outStr = ch;
    return 1;
}

/*
** Return the length in displayed characters of character "ch" expanded
** for display
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCharWidth(Ch ch, int indent, int tabDist) noexcept {

    if (ch == Ch('\t')) {
        return tabDist - (indent % tabDist);
    }

#if defined(VISUAL_CTRL_CHARS)
    if (static_cast<size_t>(ch) < 32) {
        return Tr::length(controlCharacter(ch)) + 2;
    }

    if (ch == 127) {
        return 5; // Tr::length("<del>")
    }
#endif
    return 1;
}

/*
** Count the number of displayed characters between buffer position
** "lineStartPos" and "targetPos". (displayed characters are the characters
** shown on the screen to represent characters in the buffer, where tabs and
** control characters are expanded)
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCountDispChars(int lineStartPos, int targetPos) const noexcept {

    int charCount = 0;
    Ch expandedChar[MAX_EXP_CHAR_LEN];

    int pos = lineStartPos;
    while (pos < targetPos && pos < length_) {
        charCount += BufGetExpandedChar(pos++, charCount, expandedChar);
    }

    return charCount;
}

/*
** Count forward from buffer position "startPos" in displayed characters
** (displayed characters are the characters shown on the screen to represent
** characters in the buffer, where tabs and control characters are expanded)
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCountForwardDispChars(int lineStartPos, int nChars) const noexcept {

    int charCount = 0;

    int pos = lineStartPos;
    while (charCount < nChars && pos < length_) {
        const Ch ch = BufGetCharacter(pos);
        if (ch == Ch('\n')) {
            return pos;
        }

        charCount += BufCharWidth(ch, charCount, tabDist_);
        pos++;
    }

    return pos;
}

/*
** Count the number of newlines between startPos and endPos in buffer "buf".
** The character at position "endPos" is not counted.
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCountLines(int startPos, int endPos) const noexcept {

    const int gapLen = gapEnd_ - gapStart_;
    int lineCount = 0;

    int pos = startPos;
    while (pos < gapStart_) {
        if (pos == endPos) {
            return lineCount;
        }

        if (buf_[pos++] == Ch('\n')) {
            lineCount++;
        }
    }

    while (pos < length_) {
        if (pos == endPos) {
            return lineCount;
        }

        if (buf_[pos++ + gapLen] == Ch('\n')) {
            lineCount++;
        }
    }
    return lineCount;
}

/*
** Find the first character of the line "nLines" forward from "startPos"
** in "buf" and return its position
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCountForwardNLines(int startPos, int nLines) const noexcept {

    int gapLen = gapEnd_ - gapStart_;
    int lineCount = 0;

    if (nLines == 0) {
        return startPos;
    }

    int pos = startPos;
    while (pos < gapStart_) {
        if (buf_[pos++] == Ch('\n')) {
            lineCount++;
            if (lineCount == nLines) {
                return pos;
            }
        }
    }

    while (pos < length_) {
        if (buf_[pos++ + gapLen] == Ch('\n')) {
            lineCount++;
            if (lineCount >= nLines) {
                return pos;
            }
        }
    }
    return pos;
}

/*
** Find the position of the first character of the line "nLines" backwards
** from "startPos" (not counting the character pointed to by "startpos" if
** that is a newline) in "buf".  nLines == 0 means find the beginning of
** the line
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCountBackwardNLines(int startPos, int nLines) const noexcept {
    if(startPos == 0) {
        return 0;
    }

    const int gapLen = gapEnd_ - gapStart_;
    int pos          = startPos - 1;
    int lineCount    = -1;

    while (pos >= gapStart_) {
        if (buf_[pos + gapLen] == Ch('\n')) {
            if (++lineCount >= nLines)
                return pos + 1;
        }
        pos--;
    }

    while (pos >= 0) {
        if (buf_[pos] == Ch('\n')) {
            if (++lineCount >= nLines)
                return pos + 1;
        }
        pos--;
    }

    return 0;
}

/*
** Search forwards in buffer "buf" for characters in "searchChars", starting
** with the character "startPos", and returning the result in "foundPos"
** returns true if found, false if not.
*/
template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::BufSearchForwardEx(int startPos, view_type searchChars, int *foundPos) const noexcept {

    const int gapLen = gapEnd_ - gapStart_;
    int pos          = startPos;

    while (pos < gapStart_) {
        for (Ch ch : searchChars) {
            if (buf_[pos] == ch) {
                *foundPos = pos;
                return true;
            }
        }
        pos++;
    }

    while (pos < length_) {
        for (Ch ch : searchChars) {
            if (buf_[pos + gapLen] == ch) {
                *foundPos = pos;
                return true;
            }
        }
        pos++;
    }

    *foundPos = length_;
    return false;
}

/*
** Search backwards in buffer "buf" for characters in "searchChars", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns true if found, false if not.
*/
template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::BufSearchBackwardEx(int startPos, view_type searchChars, int *foundPos) const noexcept {

    const int gapLen = gapEnd_ - gapStart_;

    if (startPos == 0) {
        *foundPos = 0;
        return false;
    }

    int pos = (startPos == 0) ? 0 : startPos - 1;
    while (pos >= gapStart_) {
        for (Ch ch : searchChars) {
            if (buf_[pos + gapLen] == ch) {
                *foundPos = pos;
                return true;
            }
        }
        pos--;
    }

    while (pos >= 0) {
        for (Ch ch : searchChars) {
            if (buf_[pos] == ch) {
                *foundPos = pos;
                return true;
            }
        }
        pos--;
    }

    *foundPos = 0;
    return false;
}

/*
** Compares len Bytes contained in buf starting at Position pos with
** the contens of cmpText. Returns 0 if there are no differences,
** != 0 otherwise.
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCmpEx(int pos, view_type cmpText) const noexcept {

    auto posEnd = pos + gsl::narrow<int>(cmpText.size());
    if (posEnd > length_) {
        return 1;
    }

    if (pos < 0) {
        return -1;
    }

    if (posEnd <= gapStart_) {
        return Tr::compare(&buf_[pos], cmpText.data(), cmpText.size());
    } else if (pos >= gapStart_) {
        return Tr::compare(&buf_[pos + (gapEnd_ - gapStart_)], cmpText.data(), cmpText.size());
    } else {
        const int part1Length = gapStart_ - pos;
        const int result = Tr::compare(&buf_[pos], cmpText.data(), gsl::narrow<size_t>(part1Length));
        if (result) {
            return result;
        }

        return Tr::compare(&buf_[gapEnd_], &cmpText[part1Length], cmpText.size() - gsl::narrow<size_t>(part1Length));
    }
}

/*
** Internal (non-redisplaying) version of BufInsertEx.  Returns the length of
** text inserted (this is just (text.size()), however this calculation can be
** expensive and the length will be required by any caller who will continue
** on to call redisplay).  pos must be contiguous with the existing text in
** the buffer (i.e. not past the end).
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::insertEx(int pos, view_type text) noexcept {
    const auto length = gsl::narrow<int>(text.size());

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PreferredGapSize */
    if (length > gapEnd_ - gapStart_) {
        reallocateBuf(pos, length + PreferredGapSize);
    } else if (pos != gapStart_) {
        moveGap(pos);
    }

    // Insert the new text (pos now corresponds to the start of the gap)
    std::copy(text.begin(), text.end(), &buf_[pos]);

    gapStart_ += length;
    length_   += length;
    updateSelections(pos, 0, length);

    return length;
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::insertEx(int pos, Ch ch) noexcept {

    const auto length = 1;

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PreferredGapSize */
    if (length > gapEnd_ - gapStart_) {
        reallocateBuf(pos, length + PreferredGapSize);
    } else if (pos != gapStart_) {
        moveGap(pos);
    }

    // Insert the new text (pos now corresponds to the start of the gap)
    buf_[pos] = ch;

    gapStart_ += length;
    length_   += length;
    updateSelections(pos, 0, length);

    return length;
}

/*
** Search forwards in buffer "buf" for character "searchChar", starting
** with the character "startPos", and returning the result in "foundPos"
** returns true if found, false if not.  (The difference between this and
** BufSearchForwardEx is that it's optimized for single characters.  The
** overall performance of the text widget is dependent on its ability to
** count lines quickly, hence searching for a single character: newline)
*/
template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::searchForward(int startPos, Ch searchChar, int *foundPos) const noexcept {

    const int gapLen = gapEnd_ - gapStart_;

    int pos = startPos;
    while (pos < gapStart_) {


        if (buf_[pos] == searchChar) {
            *foundPos = pos;
            return true;
        }
        pos++;
    }

    while (pos < length_) {
        if (buf_[pos + gapLen] == searchChar) {
            *foundPos = pos;
            return true;
        }
        pos++;
    }

    *foundPos = length_;
    return false;
}

/*
** Search backwards in buffer "buf" for character "searchChar", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns true if found, false if not.  (The difference between this and
** BufSearchBackwardEx is that it's optimized for single characters.  The
** overall performance of the text widget is dependent on its ability to
** count lines quickly, hence searching for a single character: newline)
*/
template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::searchBackward(int startPos, Ch searchChar, int *foundPos) const noexcept {

    const int gapLen = gapEnd_ - gapStart_;

    assert(foundPos);

    if (startPos == 0) {
        *foundPos = 0;
        return false;
    }

    int pos = (startPos == 0) ? 0 : startPos - 1;

    while (pos >= gapStart_) {
        if (buf_[pos + gapLen] == searchChar) {
            *foundPos = pos;
            return true;
        }
        pos--;
    }

    while (pos >= 0) {
        if (buf_[pos] == searchChar) {
            *foundPos = pos;
            return true;
        }
        pos--;
    }

    *foundPos = 0;
    return false;
}

template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::getSelectionTextEx(const TextSelection *sel) const -> string_type {

    // If there's no selection, return an allocated empty string
    if (!*sel) {
        return string_type();
    }

    // If the selection is not rectangular, return the selected range
    if (sel->rectangular) {
        return BufGetTextInRectEx(sel->start, sel->end, sel->rectStart, sel->rectEnd);
    } else {
        return BufGetRangeEx(sel->start, sel->end);
    }
}

/*
** Call the stored modify callback procedure(s) for this buffer to update the
** changed area(s) on the screen and any other listeners.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::callModifyCBs(int pos, int nDeleted, int nInserted, int nRestyled, view_type deletedText) const noexcept {

    for (const auto &pair : modifyProcs_) {
        (pair.first)(pos, nInserted, nDeleted, nRestyled, deletedText, pair.second);
    }
}

/*
** Call the stored pre-delete callback procedure(s) for this buffer to update
** the changed area(s) on the screen and any other listeners.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::callPreDeleteCBs(int pos, int nDeleted) const noexcept {

    for (const auto &pair : preDeleteProcs_) {
        (pair.first)(pos, nDeleted, pair.second);
    }
}

/*
** Internal (non-redisplaying) version of BufRemove.  Removes the contents
** of the buffer between start and end (and moves the gap to the site of
** the delete).
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::deleteRange(int start, int end) noexcept {

    // if the gap is not contiguous to the area to remove, move it there
    if (start > gapStart_) {
        moveGap(start);
    } else if (end < gapStart_) {
        moveGap(end);
    }

    // expand the gap to encompass the deleted characters
    gapEnd_   += (end - gapStart_);
    gapStart_ -= (gapStart_ - start);

    // update the length
    length_ -= (end - start);

    // fix up any selections which might be affected by the change
    updateSelections(start, end - start, 0);
}

/*
** Delete a rectangle of text without calling the modify callbacks.  Returns
** the number of characters replacing those between start and end.  Note that
** in some pathological cases, deleting can actually increase the size of
** the buffer because of tab expansions.  "endPos" returns the buffer position
** of the point in the last line where the text was removed (as a hint for
** routines which need to position the cursor after a delete operation)
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::deleteRect(int start, int end, int rectStart, int rectEnd, int *replaceLen, int *endPos) {

    int endOffset = 0;

    /* allocate a buffer for the replacement string large enough to hold
       possibly expanded tabs as well as an additional  MAX_EXP_CHAR_LEN * 2
       characters per line for padding where tabs and control characters cross
       the edges of the selection */
    start = BufStartOfLine(start);
    end   = BufEndOfLine(end);
    const int nLines = BufCountLines(start, end) + 1;

    const string_type text = BufGetRangeEx(start, end);
    const string_type expText = expandTabsEx(text, 0, tabDist_);
    auto len = gsl::narrow<int>(expText.size());

    string_type outStr;
    outStr.reserve(expText.size() + static_cast<size_t>(nLines * MAX_EXP_CHAR_LEN * 2));

    /* loop over all lines in the buffer between start and end removing
       the text between rectStart and rectEnd and padding appropriately */
    int lineStart = start;
    auto outPtr = std::back_inserter(outStr);

    while (lineStart <= length_ && lineStart <= end) {
        const int lineEnd = BufEndOfLine(lineStart);
        const string_type line = BufGetRangeEx(lineStart, lineEnd);

        // TODO(eteran): 2.0, remove the need for this temp
        string_type temp;
        deleteRectFromLine(line, rectStart, rectEnd, tabDist_, useTabs_, &temp, &endOffset);
        len = gsl::narrow<int>(temp.size());

        std::copy_n(temp.begin(), len, outPtr);

        *outPtr++ = Ch('\n');
        lineStart = lineEnd + 1;
    }

    // trim back off extra newline
    if (!outStr.empty()) {
        outStr.pop_back();
    }

    // replace the text between start and end with the newly created string
    deleteRange(start, end);
    insertEx(start, outStr);

    *replaceLen = gsl::narrow<int>(outStr.size());
    *endPos     = start + gsl::narrow<int>(outStr.size()) - len + endOffset;
}

/*
** Find the first and last character position in a line withing a rectangular
** selection (for copying).  Includes tabs which cross rectStart, but not
** control characters which do so.  Leaves off tabs which cross rectEnd.
**
** Technically, the calling routine should convert tab characters which
** cross the right boundary of the selection to spaces which line up with
** the edge of the selection.  Unfortunately, the additional memory
** management required in the parent routine to allow for the changes
** in string size is not worth all the extra work just for a couple of
** shifted characters, so if a tab protrudes, just lop it off and hope
** that there are other characters in the selection to establish the right
** margin for subsequent columnar pastes of this data.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::findRectSelBoundariesForCopy(int lineStartPos, int rectStart, int rectEnd, int *selStart, int *selEnd) const noexcept {

    int pos    = lineStartPos;
    int indent = 0;

    // find the start of the selection
    for (; pos < length_; pos++) {
        const Ch c = BufGetCharacter(pos);
        if (c == Ch('\n')) {
            break;
        }

        const int width = BufCharWidth(c, indent, tabDist_);
        if (indent + width > rectStart) {
            if (indent != rectStart && c != Ch('\t')) {
                pos++;
                indent += width;
            }
            break;
        }
        indent += width;
    }

    *selStart = pos;

    // find the end
    for (; pos < length_; pos++) {
        const Ch c = BufGetCharacter(pos);
        if (c == Ch('\n')) {
            break;
        }

        const int width = BufCharWidth(c, indent, tabDist_);
        indent += width;
        if (indent > rectEnd) {
            if (indent - width != rectEnd && c != Ch('\t')) {
                pos++;
            }
            break;
        }
    }

    *selEnd = pos;
}

/*
** Insert a column of text without calling the modify callbacks.  Note that
** in some pathological cases, inserting can actually decrease the size of
** the buffer because of spaces being coalesced into tabs.  "nDeleted" and
** "nInserted" return the number of characters deleted and inserted beginning
** at the start of the line containing "startPos".  "endPos" returns buffer
** position of the lower left edge of the inserted column (as a hint for
** routines which need to set a cursor position).
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::insertColEx(int column, int startPos, view_type insText, int *nDeleted, int *nInserted, int *endPos) {

    int len;
    int endOffset;

    if (column < 0) {
        column = 0;
    }

    /* Allocate a buffer for the replacement string large enough to hold
       possibly expanded tabs in both the inserted text and the replaced
       area, as well as per line: 1) an additional 2*MAX_EXP_CHAR_LEN
       characters for padding where tabs and control characters cross the
       column of the selection, 2) up to "column" additional spaces per
       line for padding out to the position of "column", 3) padding up
       to the width of the inserted text if that must be padded to align
       the text beyond the inserted column.  (Space for additional
       newlines if the inserted text extends beyond the end of the buffer
       is counted with the length of insText) */
    const int start  = BufStartOfLine(startPos);
    const int nLines = countLinesEx(insText) + 1;

    const int insWidth = textWidthEx(insText, tabDist_);
    const int end      = BufEndOfLine(BufCountForwardNLines(start, nLines - 1));
    const string_type replText = BufGetRangeEx(start, end);

    const string_type expRep = expandTabsEx(replText, 0, tabDist_);
    const string_type expIns = expandTabsEx(insText, 0, tabDist_);

    string_type outStr;
    outStr.reserve(expRep.size() + expIns.size() + static_cast<size_t>(nLines * (column + insWidth + MAX_EXP_CHAR_LEN)));

    /* Loop over all lines in the buffer between start and end inserting
       text at column, splitting tabs and adding padding appropriately */
    auto outPtr   = std::back_inserter(outStr);
    int lineStart = start;
    auto insPtr   = insText.begin();

    while (true) {
        const int lineEnd = BufEndOfLine(lineStart);
        const string_type line    = BufGetRangeEx(lineStart, lineEnd);
        const string_type insLine = copyLineEx(insPtr, insText.end());
        insPtr += insLine.size();

        // TODO(eteran): 2.0, remove the need for this temp
        string_type temp;
        insertColInLineEx(line, insLine, column, insWidth, tabDist_, useTabs_, &temp, &endOffset);
        len = gsl::narrow<int>(temp.size());

#if 0
        /* Earlier comments claimed that trailing whitespace could multiply on                                                                                                                                                                   \
           the ends of lines, but insertColInLine looks like it should never                                                                                                                                                                        \
           add space unnecessarily, and this trimming interfered with                                                                                                                                                                               \
           paragraph filling, so lets see if it works without it. MWE */
        for(auto it = temp.rbegin(); it != temp.rend() && (*it == Ch(' ') || *it == Ch('\t')); ++it) {
            --len;
        }
#endif

        std::copy_n(temp.begin(), len, outPtr);

        *outPtr++ = Ch('\n');
        lineStart = lineEnd < length_ ? lineEnd + 1 : length_;
        if (insPtr == insText.end()) {
            break;
        }

        insPtr++;
    }

    // trim back off extra newline
    if (!outStr.empty()) {
        outStr.pop_back();
    }

    // replace the text between start and end with the new stuff
    deleteRange(start, end);
    insertEx(start, outStr);

    *nInserted = gsl::narrow<int>(outStr.size());
    *nDeleted  = end - start;
    *endPos    = start + gsl::narrow<int>(outStr.size()) - len + endOffset;
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::moveGap(int pos) noexcept {

    const int gapLen = gapEnd_ - gapStart_;

    if (pos > gapStart_) {
        Tr::move(&buf_[gapStart_], &buf_[gapEnd_], gsl::narrow<size_t>(pos - gapStart_));
    } else {
        Tr::move(&buf_[pos + gapLen], &buf_[pos], gsl::narrow<size_t>(gapStart_ - pos));
    }

    gapEnd_ += pos - gapStart_;
    gapStart_ += pos - gapStart_;
}

/*
** reallocate the text storage in "buf" to have a gap starting at "newGapStart"
** and a gap size of "newGapLen", preserving the buffer's current contents.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::reallocateBuf(int newGapStart, int newGapLen) {

    auto newBuf = new Ch[length_ + newGapLen + 1];
    newBuf[length_ + PreferredGapSize] = Ch();
    int newGapEnd = newGapStart + newGapLen;

    if (newGapStart <= gapStart_) {
        std::copy_n(buf_, newGapStart, newBuf);
        std::copy_n(&buf_[newGapStart], gapStart_ - newGapStart, &newBuf[newGapEnd]);
        std::copy_n(&buf_[gapEnd_], length_ - gapStart_, &newBuf[newGapEnd + gapStart_ - newGapStart]);
    } else { // newGapStart > gapStart_
        std::copy_n(buf_, gapStart_, newBuf);
        std::copy_n(&buf_[gapEnd_], newGapStart - gapStart_, &newBuf[gapStart_]);
        std::copy_n(&buf_[gapEnd_ + newGapStart - gapStart_], length_ - newGapStart, &newBuf[newGapEnd]);
    }

    delete [] buf_;

    buf_      = newBuf;
    gapStart_ = newGapStart;
    gapEnd_   = newGapEnd;

#ifdef PURIFY
    std::fill(&buf_[gapStart_], &buf_[gapEnd_], Ch('.'));
#endif
}

/*
** Call the stored redisplay procedure(s) for this buffer to update the
** screen for a change in a selection.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::redisplaySelection(const TextSelection *oldSelection, TextSelection *newSelection) noexcept {

    /* If either selection is rectangular, add an additional character to
       the end of the selection to request the redraw routines to wipe out
       the parts of the selection beyond the end of the line */
    int oldStart = oldSelection->start;
    int newStart = newSelection->start;
    int oldEnd   = oldSelection->end;
    int newEnd   = newSelection->end;

    if (oldSelection->rectangular) {
        oldEnd++;
    }

    if (newSelection->rectangular) {
        newEnd++;
    }

    /* If the old or new selection is unselected, just redisplay the
       single area that is (was) selected and return */
    if (!oldSelection->selected && !newSelection->selected) {
        return;
    }

    if (!oldSelection->selected) {
        callModifyCBs(newStart, 0, 0, newEnd - newStart, {});
        return;
    }

    if (!newSelection->selected) {
        callModifyCBs(oldStart, 0, 0, oldEnd - oldStart, {});
        return;
    }

    /* If the selection changed from normal to rectangular or visa versa, or
       if a rectangular selection changed boundaries, redisplay everything */
    if ((oldSelection->rectangular && !newSelection->rectangular) || (!oldSelection->rectangular && newSelection->rectangular) ||
        (oldSelection->rectangular && ((oldSelection->rectStart != newSelection->rectStart) || (oldSelection->rectEnd != newSelection->rectEnd)))) {
        callModifyCBs(std::min<int>(oldStart, newStart), 0, 0, std::max<int>(oldEnd, newEnd) - std::min<int>(oldStart, newStart), {});
        return;
    }

    /* If the selections are non-contiguous, do two separate updates
       and return */
    if (oldEnd < newStart || newEnd < oldStart) {
        callModifyCBs(oldStart, 0, 0, oldEnd - oldStart, {});
        callModifyCBs(newStart, 0, 0, newEnd - newStart, {});
        return;
    }

    /* Otherwise, separate into 3 separate regions: ch1, and ch2 (the two
       changed areas), and the unchanged area of their intersection,
       and update only the changed area(s) */
    const int ch1Start = std::min(oldStart, newStart);
    const int ch2End   = std::max(oldEnd, newEnd);
    const int ch1End   = std::max(oldStart, newStart);
    const int ch2Start = std::min(oldEnd, newEnd);

    if (ch1Start != ch1End) {
        callModifyCBs(ch1Start, 0, 0, ch1End - ch1Start, {});
    }

    if (ch2Start != ch2End) {
        callModifyCBs(ch2Start, 0, 0, ch2End - ch2Start, {});
    }
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::removeSelected(const TextSelection *sel) noexcept {

    assert(sel);

    if(!*sel) {
        return;
    }

    if (sel->rectangular) {
        BufRemoveRect(sel->start, sel->end, sel->rectStart, sel->rectEnd);
    } else {
        BufRemove(sel->start, sel->end);
    }
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::replaceSelectedEx(TextSelection *sel, view_type text) noexcept {

    assert(sel);

    const TextSelection oldSelection = *sel;

    // If there's no selection, return
    if (!*sel) {
        return;
    }

    // Do the appropriate type of replace
    if (sel->rectangular) {
        BufReplaceRectEx(sel->start, sel->end, sel->rectStart, sel->rectEnd, text);
    } else {
        BufReplaceEx(sel->start, sel->end, text);
    }


    /* Unselect (happens automatically in BufReplaceEx, but BufReplaceRectEx
       can't detect when the contents of a selection goes away) */
    sel->selected = false;
    redisplaySelection(&oldSelection, sel);
}

/*
** Update all of the selections in "buf" for changes in the buffer's text
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::updateSelections(int pos, int nDeleted, int nInserted) noexcept {
    primary_  .updateSelection(pos, nDeleted, nInserted);
    secondary_.updateSelection(pos, nDeleted, nInserted);
    highlight_.updateSelection(pos, nDeleted, nInserted);
}


template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetLength() const noexcept {
    return length_;
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufAppendEx(view_type text) noexcept {
    BufInsertEx(BufGetLength(), text);
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufAppendEx(Ch ch) noexcept {
    BufInsertEx(BufGetLength(), ch);
}

template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::BufIsEmpty() const noexcept {
    return BufGetLength() == 0;
}

/*
** Find the start and end of a single line selection.  Hides rectangular
** selection issues for older routines which use selections that won't
** span lines.
*/
template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::GetSimpleSelection(int *left, int *right) const noexcept {
    int selStart;
    int selEnd;
    int rectStart;
    int rectEnd;
    bool isRect;

    /* get the character to match and its position from the selection, or
       the character before the insert point if nothing is selected.
       Give up if too many characters are selected */
    if (!BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
        return false;
    }

    if (isRect) {
        const int lineStart = BufStartOfLine(selStart);
        selStart  = BufCountForwardDispChars(lineStart, rectStart);
        selEnd    = BufCountForwardDispChars(lineStart, rectEnd);
    }

    *left  = selStart;
    *right = selEnd;
    return true;
}

/*
** Convert sequences of spaces into tabs.  The threshold for conversion is
** when 3 or more spaces can be converted into a single tab, this avoids
** converting double spaces after a period withing a block of text.
*/
template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::unexpandTabsEx(view_type text, int startIndent, int tabDist) -> string_type {

    string_type outStr;
    outStr.reserve(text.size());

    auto outPtr = std::back_inserter(outStr);
    int indent = startIndent;

    for (size_t pos = 0; pos != text.size(); ) {
        if (text[pos] == Ch(' ')) {

            Ch expandedChar[MAX_EXP_CHAR_LEN];
            const int len = BufExpandTab(indent, expandedChar, tabDist);

            const auto cmp = text.compare(pos, len, expandedChar, len);

            if (len >= 3 && !cmp) {
                pos += len;
                *outPtr++ = Ch('\t');
                indent += len;
            } else {
                *outPtr++ = text[pos++];
                indent++;
            }
        } else if (text[pos]== Ch('\n')) {
            indent = startIndent;
            *outPtr++ = text[pos++];
        } else {
            *outPtr++ = text[pos++];
            indent++;
        }
    }

    return outStr;
}

/*
** Expand tabs to spaces for a block of text.  The additional parameter
** "startIndent" if nonzero, indicates that the text is a rectangular selection
** beginning at column "startIndent"
*/
template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::expandTabsEx(view_type text, int startIndent, int tabDist) -> string_type {

    size_t outLen = 0;

    // rehearse the expansion to figure out length for output string
    int indent = startIndent;
    for (Ch ch : text) {
        if (ch == Ch('\t')) {
            const int len = BufCharWidth(ch, indent, tabDist);
            outLen += len;
            indent += len;
        } else if (ch == Ch('\n')) {
            indent = startIndent;
            outLen++;
        } else {
            indent += BufCharWidth(ch, indent, tabDist);
            outLen++;
        }
    }

    // do the expansion
    string_type outStr;
    outStr.reserve(outLen);

    auto outPtr = std::back_inserter(outStr);

    indent = startIndent;
    for (Ch ch : text) {
        if (ch == Ch('\t')) {

            Ch temp[MAX_EXP_CHAR_LEN];
            const int len = BufExpandTab(indent, temp, tabDist);
            std::copy_n(temp, len, outPtr);

            indent += len;
        } else if (ch == Ch('\n')) {
            indent = startIndent;
            *outPtr++ = ch;
        } else {
            indent += BufCharWidth(ch, indent, tabDist);
            *outPtr++ = ch;
        }
    }

    return outStr;
}

/*
** Adjust the space and tab characters from string "text" so that non-white
** characters remain stationary when the text is shifted from starting at
** "origIndent" to starting at "newIndent".
*/
template <class Ch, class Tr>
auto BasicTextBuffer<Ch, Tr>::realignTabsEx(view_type text, int origIndent, int newIndent, int tabDist, bool useTabs) noexcept -> string_type {

    // If the tabs settings are the same, retain original tabs
    if (origIndent % tabDist == newIndent % tabDist) {
        return string_type(text);
    }

    /* If the tab settings are not the same, brutally convert tabs to
       spaces, then back to tabs in the new position */
    string_type expStr = expandTabsEx(text, origIndent, tabDist);

    if (!useTabs) {
        return expStr;
    }

    return unexpandTabsEx(expStr, newIndent, tabDist);
}

template <class Ch, class Tr>
template <class Out>
int BasicTextBuffer<Ch, Tr>::addPaddingEx(Out out, int startIndent, int toIndent, int tabDist, bool useTabs) noexcept {

    int indent = startIndent;
    int count  = 0;

    if (useTabs) {
        while (indent < toIndent) {
            int len = BufCharWidth(Ch('\t'), indent, tabDist);
            if (len > 1 && indent + len <= toIndent) {
                *out++ = Ch('\t');
                ++count;
                indent += len;
            } else {
                *out++ = Ch(' ');
                ++count;
                indent++;
            }
        }
    } else {
        while (indent < toIndent) {
            *out++ = Ch(' ');
            ++count;
            indent++;
        }
    }
    return count;
}

/*
** Insert characters from single-line string "insLine" in single-line string
** "line" at "column", leaving "insWidth" space before continuing line.
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::insertColInLineEx(view_type line, view_type insLine, int column, int insWidth, int tabDist, bool useTabs, string_type *outStr, int *endOffset) noexcept {

    int len = 0;
    int postColIndent;

    // copy the line up to "column"
    auto outPtr = std::back_inserter(*outStr);
    int indent = 0;

    auto linePtr = line.begin();
    for (; linePtr != line.end(); ++linePtr) {
        len = BufCharWidth(*linePtr, indent, tabDist);
        if (indent + len > column) {
            break;
        }

        indent += len;
        *outPtr++ = *linePtr;
    }

    /* If "column" falls in the middle of a character, and the character is a
       tab, leave it off and leave the indent short and it will get padded
       later.  If it's a control character, insert it and adjust indent
       accordingly. */
    if (indent < column && linePtr != line.end()) {
        postColIndent = indent + len;
        if (*linePtr == Ch('\t')) {
            linePtr++;
        } else {
            *outPtr++ = *linePtr++;
            indent += len;
        }
    } else {
        postColIndent = indent;
    }

    // If there's no text after the column and no text to insert, that's all
    if (insLine.empty() && linePtr == line.end()) {
        *endOffset = gsl::narrow<int>(outStr->size());
        return;
    }

    // pad out to column if text is too short
    if (indent < column) {
        len = addPaddingEx(outPtr, indent, column, tabDist, useTabs);
        indent = column;
    }

    /* Copy the text from "insLine" (if any), recalculating the tabs as if
       the inserted string began at column 0 to its new column destination */
    if (!insLine.empty()) {
        string_type retabbedStr = realignTabsEx(insLine, 0, indent, tabDist, useTabs);
        len = gsl::narrow<int>(retabbedStr.size());

        for (Ch ch : retabbedStr) {
            *outPtr++ = ch;
            len = BufCharWidth(ch, indent, tabDist);
            indent += len;
        }
    }

    // If the original line did not extend past "column", that's all
    if (linePtr == line.end()) {
        *endOffset = gsl::narrow<int>(outStr->size());
        return;
    }

    /* Pad out to column + width of inserted text + (additional original
       offset due to non-breaking character at column) */
    int toIndent = column + insWidth + postColIndent - column;
    len = addPaddingEx(outPtr, indent, toIndent, tabDist, useTabs);
    indent = toIndent;

    // realign tabs for text beyond "column" and write it out
    string_type retabbedStr = realignTabsEx(substr(linePtr, line.end()), postColIndent, indent, tabDist, useTabs);

    *endOffset = gsl::narrow<int>(outStr->size());

    std::copy(retabbedStr.begin(), retabbedStr.end(), outPtr);
}

/*
** Remove characters in single-line string "line" between displayed positions
** "rectStart" and "rectEnd", and write the result to "outStr", which is
** assumed to be large enough to hold the returned string.  Note that in
** certain cases, it is possible for the string to get longer due to
** expansion of tabs.  "endOffset" returns the number of characters from
** the beginning of the string to the point where the characters were
** deleted (as a hint for routines which need to position the cursor).
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::deleteRectFromLine(view_type line, int rectStart, int rectEnd, int tabDist, bool useTabs, string_type *outStr, int *endOffset) noexcept {

    int len;

    // copy the line up to rectStart
    auto outPtr = std::back_inserter(*outStr);
    int indent = 0;
    auto c = line.begin();
    for (; c != line.end(); c++) {
        if (indent > rectStart)
            break;
        len = BufCharWidth(*c, indent, tabDist);
        if (indent + len > rectStart && (indent == rectStart || *c == Ch('\t'))) {
            break;
        }

        indent += len;
        *outPtr++ = *c;
    }

    const int preRectIndent = indent;

    // skip the characters between rectStart and rectEnd
    for (; c != line.end() && indent < rectEnd; c++) {
        indent += BufCharWidth(*c, indent, tabDist);
    }

    const int postRectIndent = indent;

    // If the line ended before rectEnd, there's nothing more to do
    if (c == line.end()) {
        *endOffset = gsl::narrow<int>(outStr->size());
        return;
    }

    /* fill in any space left by removed tabs or control characters
       which straddled the boundaries */
    indent = std::max(rectStart + postRectIndent - rectEnd, preRectIndent);
    len = addPaddingEx(outPtr, preRectIndent, indent, tabDist, useTabs);

    /* Copy the rest of the line.  If the indentation has changed, preserve
       the position of non-whitespace characters by converting tabs to
       spaces, then back to tabs with the correct offset */
    string_type retabbedStr = realignTabsEx(substr(c, line.end()), postRectIndent, indent, tabDist, useTabs);

    *endOffset = gsl::narrow<int>(outStr->size());

    std::copy(retabbedStr.begin(), retabbedStr.end(), outPtr);
}

/*
** Copy from "text" to end up to but not including newline (or end of "text")
** and return the copy
*/
template <class Ch, class Tr>
template <class Ran>
auto BasicTextBuffer<Ch, Tr>::copyLineEx(Ran first, Ran last) -> string_type {
    auto it = std::find(first, last, Ch('\n'));
    return string_type(first, it);
}

/*
** Count the number of newlines in a null-terminated text string;
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::countLinesEx(view_type string) noexcept {
    return gsl::narrow<int>(std::count(string.begin(), string.end(), Ch('\n')));
}

/*
** Measure the width in displayed characters of string "text"
*/
template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::textWidthEx(view_type text, int tabDist) noexcept {
    int width    = 0;
    int maxWidth = 0;

    for (Ch ch : text) {
        if (ch == Ch('\n')) {
            maxWidth = std::max(maxWidth, width);
            width    = 0;
        } else {
            width += BufCharWidth(ch, width, tabDist);
        }
    }

    return std::max(width, maxWidth);
}

/*
** Overlay characters from single-line string "insLine" on single-line string
** "line" between displayed character offsets "rectStart" and "rectEnd".
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
**
** This code does not handle control characters very well, but oh well.
*/
template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::overlayRectInLineEx(view_type line, view_type insLine, int rectStart, int rectEnd, int tabDist, bool useTabs, string_type *outStr, int *endOffset) noexcept {

    int postRectIndent;

    /* copy the line up to "rectStart" or just before the char that contains it*/
    auto outPtr   = std::back_inserter(*outStr);
    int inIndent  = 0;
    int outIndent = 0;
    int len       = 0;

    auto linePtr = line.begin();

    for (; linePtr != line.end(); ++linePtr) {
        len = BufCharWidth(*linePtr, inIndent, tabDist);
        if (inIndent + len > rectStart) {
            break;
        }
        inIndent  += len;
        outIndent += len;
        *outPtr++ = *linePtr;
    }

    /* If "rectStart" falls in the middle of a character, and the character
       is a tab, leave it off and leave the outIndent short and it will get
       padded later.  If it's a control character, insert it and adjust
       outIndent accordingly. */
    if (inIndent < rectStart && linePtr != line.end()) {
        if (*linePtr == Ch('\t')) {
            /* Skip past the tab */
            linePtr++;
            inIndent += len;
        } else {
            *outPtr++ = *linePtr++;
            outIndent += len;
            inIndent += len;
        }
    }

    /* skip the characters between rectStart and rectEnd */
    for(; linePtr != line.end() && inIndent < rectEnd; linePtr++) {
        inIndent += BufCharWidth(*linePtr, inIndent, tabDist);
    }

    postRectIndent = inIndent;

    /* After this inIndent is dead and linePtr is supposed to point at the
        character just past the last character that will be altered by
        the overlay, whether that's a \t or otherwise.  postRectIndent is
        the position at which that character is supposed to appear */

    /* If there's no text after rectStart and no text to insert, that's all */
    if (insLine.empty() && linePtr == line.end()) {
        *endOffset = gsl::narrow<int>(outStr->size());
        return;
    }

    /* pad out to rectStart if text is too short */
    if (outIndent < rectStart) {
        addPaddingEx(outPtr, outIndent, rectStart, tabDist, useTabs);
    }

    outIndent = rectStart;

    /* Copy the text from "insLine" (if any), recalculating the tabs as if
       the inserted string began at column 0 to its new column destination */
    if (!insLine.empty()) {
        string_type retabbedStr = realignTabsEx(insLine, 0, rectStart, tabDist, useTabs);
        len = gsl::narrow<int>(retabbedStr.size());

        for (Ch c : retabbedStr) {
            *outPtr++ = c;
            len = BufCharWidth(c, outIndent, tabDist);
            outIndent += len;
        }
    }

    /* If the original line did not extend past "rectStart", that's all */
    if (linePtr == line.end()) {
        *endOffset = gsl::narrow<int>(outStr->size());
        return;
    }

    /* Pad out to rectEnd + (additional original offset
       due to non-breaking character at right boundary) */
    addPaddingEx(outPtr, outIndent, postRectIndent, tabDist, useTabs);
    outIndent = postRectIndent;

    *endOffset = gsl::narrow<int>(outStr->size());

    /* copy the text beyond "rectEnd" */
    std::copy(linePtr, line.end(), outPtr);
}

template <class Ch, class Tr>
const Ch * BasicTextBuffer<Ch, Tr>::controlCharacter(size_t index) noexcept {
    static const char *const ControlCodeTable[32] = {
        "nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
        "bs",  "ht",  "nl",  "vt",  "np",  "cr",  "so",  "si",
        "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
        "can", "em",  "sub", "esc", "fs",  "gs",  "rs",  "us"
    };

    assert(index < 32);
    return ControlCodeTable[index];
}

/*
** Create an empty text buffer
*/
template <class Ch, class Tr>
BasicTextBuffer<Ch, Tr>::BasicTextBuffer() : BasicTextBuffer(0) {
}

/*
** Create an empty text buffer of a pre-determined size (use this to
** avoid unnecessary re-allocation if you know exactly how much the buffer
** will need to hold
*/
template <class Ch, class Tr>
BasicTextBuffer<Ch, Tr>::BasicTextBuffer(int size) {

    buf_ = new Ch[size + PreferredGapSize + 1];
    buf_[size + PreferredGapSize] = Ch();

#ifdef PURIFY
    std::fill(&buf_[gapStart_], &buf_[gapEnd_], Ch('.'));
#endif
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufCursorPosHint() const noexcept {
    return cursorPosHint_;
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSetUseTabs(bool useTabs) noexcept {
    useTabs_ = useTabs;
}

template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::BufGetUseTabs() const noexcept {
    return useTabs_;
}

template <class Ch, class Tr>
void BasicTextBuffer<Ch, Tr>::BufSetTabDist(int dist) noexcept {
    tabDist_ = dist;
}

template <class Ch, class Tr>
int BasicTextBuffer<Ch, Tr>::BufGetTabDist() const noexcept {
    return tabDist_;
}

template <class Ch, class Tr>
const TextSelection &BasicTextBuffer<Ch, Tr>::BufGetPrimary() const {
    return primary_;
}

template <class Ch, class Tr>
const TextSelection &BasicTextBuffer<Ch, Tr>::BufGetSecondary() const {
    return secondary_;
}

template <class Ch, class Tr>
const TextSelection &BasicTextBuffer<Ch, Tr>::BufGetHighlight() const {
    return highlight_;
}

template <class Ch, class Tr>
TextSelection &BasicTextBuffer<Ch, Tr>::BufGetPrimary() {
    return primary_;
}

template <class Ch, class Tr>
TextSelection &BasicTextBuffer<Ch, Tr>::BufGetSecondary() {
    return secondary_;
}

template <class Ch, class Tr>
TextSelection &BasicTextBuffer<Ch, Tr>::BufGetHighlight() {
    return highlight_;
}

template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::BufGetSyncXSelection() const {
    return syncXSelection_;
}

template <class Ch, class Tr>
bool BasicTextBuffer<Ch, Tr>::BufSetSyncXSelection(bool sync) {
    return std::exchange(syncXSelection_, sync);
}

#endif
