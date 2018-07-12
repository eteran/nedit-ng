
#include "shift.h"
#include "DocumentWidget.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "Util/utils.h"
#include <gsl/gsl_util>

namespace {

std::string makeIndentString(int64_t indent, int tabDist, bool allowTabs) {

    std::string indentString;
    indentString.reserve(static_cast<size_t>(indent));

    auto outPtr = std::back_inserter(indentString);

    if (allowTabs) {
        for (int i = 0; i < indent / tabDist; i++) {
            *outPtr++ = '\t';
        }

        for (int i = 0; i < indent % tabDist; i++) {
            *outPtr++ = ' ';
        }
    } else {
        for (int i = 0; i < indent; i++) {
            *outPtr++ = ' ';
        }
    }

    return indentString;
}

/*
** Trim leading space, and arrange text to fill between leftMargin and
** rightMargin (except for the first line which fills from firstLineIndent),
** re-creating whitespace to the left of the text using tabs (if allowTabs is
** true) calculated using tabDist, and spaces.
*/
std::string fillParagraphEx(view::string_view text, int64_t leftMargin, int64_t firstLineIndent, int64_t rightMargin, int tabDist, bool allowTabs) {

    size_t nLines = 1;

    // remove leading spaces, convert newlines to spaces
    std::string cleanedText;
    cleanedText.reserve(text.size());
    auto outPtr = std::back_inserter(cleanedText);

    bool inMargin = true;

    for(char ch : text) {
        if (ch == '\t' || ch == ' ') {
            if (!inMargin)
                *outPtr++ = ch;
        } else if (ch == '\n') {
            if (inMargin) {
                /* a newline before any text separates paragraphs, so leave
                   it in, back up, and convert the previous space back to \n */
                if (!cleanedText.empty() && cleanedText.back() == ' ') {
                    cleanedText.pop_back();
                    cleanedText.push_back('\n');
                }

                *outPtr++ = '\n';
                nLines += 2;
            } else
                *outPtr++ = ' ';
            inMargin = true;
        } else {
            *outPtr++ = ch;
            inMargin = false;
        }
    }

    /* Put back newlines breaking text at word boundaries within the margins.
       Algorithm: scan through characters, counting columns, and when the
       margin width is exceeded, search backward for beginning of the word
       and convert the last whitespace character into a newline */
    int64_t col = firstLineIndent;
    bool inWhitespace;

    for (auto it = cleanedText.begin(); it != cleanedText.end(); ++it) {
        if (*it == '\n') {
            col = leftMargin;
        } else {
            col += TextBuffer::BufCharWidth(*it, col, tabDist);
        }

        if (col - 1 > rightMargin) {
            inWhitespace = true;
            for (auto b = it; b >= cleanedText.begin() && *b != '\n'; b--) {
                if (*b == '\t' || *b == ' ') {
                    if (!inWhitespace) {
                        *b = '\n';
                        it = b;
                        col = leftMargin;
                        nLines++;
                        break;
                    }
                } else {
                    inWhitespace = false;
                }
            }
        }
    }

    nLines++;

    // produce a string to prepend to lines to indent them to the left margin
    std::string leadIndentStr = makeIndentString(firstLineIndent, tabDist, allowTabs);
    std::string indentString  = makeIndentString(leftMargin, tabDist, allowTabs);

    std::string outText;
    outText.reserve(cleanedText.size() + leadIndentStr.size() + indentString.size() * (nLines - 1));

    auto outPtr2 = std::back_inserter(outText);

    // prepend the indent string to each line of the filled text
    std::copy(leadIndentStr.begin(), leadIndentStr.end(), outPtr2);

    for(char ch : cleanedText) {
        *outPtr2++ = ch;
        if (ch == '\n') {
            std::copy(indentString.begin(), indentString.end(), outPtr2);
        }
    }

    // convert any trailing space to newline.
    if(!outText.empty() && outText.back() == ' ') {
        outText.pop_back();
        outText.push_back('\n');
    }

    return outText;
}

/*
** Find the boundaries of the paragraph containing pos
*/
TextCursor findParagraphEnd(TextBuffer *buf, TextCursor startPos) {

    static const char whiteChars[] = " \t";

    TextCursor pos = buf->BufEndOfLine(startPos) + 1;
    while (pos < buf->BufGetLength()) {
        char c = buf->BufGetCharacter(pos);
        if (c == '\n') {
            break;
        }

        if (::strchr(whiteChars, c) != nullptr) {
            pos++;
        } else {
            pos = buf->BufEndOfLine(pos) + 1;
        }
    }

    return pos < buf->BufGetLength() ? pos : buf->BufEndOfBuffer();
}

/*
** Find the implied left margin of a text string (the number of columns to the
** first non-whitespace character on any line) up to either the terminating
** null character at the end of the string, or "length" characters, whever
** comes first.
*/
template <class In, class Size>
int findLeftMarginEx(In first, In last, Size length, int tabDist) {

    int col         = 0;
    auto leftMargin = std::numeric_limits<int>::max();
    bool inMargin   = true;

    for (auto it = first; it != last && length--; ++it) {

        switch(*it) {
        case '\t':
            col += TextBuffer::BufCharWidth('\t', col, tabDist);
            break;
        case ' ':
            col++;
            break;
        case '\n':
            col = 0;
            inMargin = true;
            break;
        default:
            // non-whitespace
            if (col < leftMargin && inMargin) {
                leftMargin = col;
            }
            inMargin = false;
            break;
        }
    }

    // if no non-white text is found, the leftMargin will never be set
    if (leftMargin == std::numeric_limits<int>::max()) {
        return 0;
    }

    return leftMargin;
}

/*
** Fill multiple paragraphs between rightMargin and an implied left margin
** and first line indent determined by analyzing the text.  alignWithFirst
** aligns subsequent paragraphs with the margins of the first paragraph (a
** capability not currently used in NEdit, but carried over from code for
** previous versions which did all paragraphs together).
*/
std::string fillParagraphsEx(view::string_view text, int64_t rightMargin, int tabDist, int useTabs, bool alignWithFirst) {

    // Create a buffer to accumulate the filled paragraphs
    TextBuffer buf;
    buf.BufSetSyncXSelection(false);
    buf.BufSetAll(text);

    /*
    ** Loop over paragraphs, filling each one, and accumulating the results
    ** in buf
    */
    TextCursor paraStart = {};
    for (;;) {

        // Skip over white space
        while (paraStart < buf.BufGetLength()) {
            char ch = buf.BufGetCharacter(paraStart);
            if (ch != ' ' && ch != '\t' && ch != '\n') {
                break;
            }

            paraStart++;
        }

        if (paraStart >= buf.BufGetLength()) {
            break;
        }

        paraStart = buf.BufStartOfLine(paraStart);

        // Find the end of the paragraph
        TextCursor paraEnd = findParagraphEnd(&buf, paraStart);

        /* Operate on either the one paragraph, or to make them all identical,
           do all of them together (fill paragraph can format all the paragraphs
           it finds with identical specs if it gets passed more than one) */
        TextCursor fillEnd = alignWithFirst ? TextCursor(buf.BufGetLength()) : paraEnd;

        /* Get the paragraph in a text string (or all of the paragraphs if
           we're making them all the same) */
        std::string paraText = buf.BufGetRangeEx(paraStart, fillEnd);

        /* Find separate left margins for the first and for the first line of
           the paragraph, and for rest of the remainder of the paragraph */
        auto it = std::find(paraText.begin(), paraText.end(), '\n');

        long firstLineLen    = std::distance(paraText.begin(), it);
        auto secondLineStart = (it == paraText.end()) ? paraText.begin() : it + 1;
        int firstLineIndent  = findLeftMarginEx(paraText.begin(), paraText.end(), firstLineLen, tabDist);
        int leftMargin       = findLeftMarginEx(secondLineStart,  paraText.end(), paraEnd - paraStart - (secondLineStart - paraText.begin()), tabDist);

        // Fill the paragraph
        std::string filledText = fillParagraphEx(paraText, leftMargin, firstLineIndent, rightMargin, tabDist, useTabs);

        // Replace it in the buffer
        buf.BufReplaceEx(paraStart, fillEnd, filledText);

        // move on to the next paragraph
        paraStart += filledText.size();
    }

    // Free the buffer and return its contents
    return buf.BufGetAllEx();
}

TextCursor findParagraphStart(TextBuffer *buf, TextCursor startPos) {

    static const char whiteChars[] = " \t";

    if (startPos == buf->BufStartOfBuffer()) {
        return buf->BufStartOfBuffer();
    }

    TextCursor parStart = buf->BufStartOfLine(startPos);
    TextCursor pos      = parStart - 2;

    while (pos > 0) {
        const char c = buf->BufGetCharacter(pos);
        if (c == '\n') {
            break;
        }

        if (::strchr(whiteChars, c) != nullptr) {
            pos--;
        } else {
            parStart = buf->BufStartOfLine(pos);
            pos = parStart - 2;
        }
    }
    return parStart > buf->BufStartOfBuffer() ? parStart : buf->BufStartOfBuffer();
}

int countLinesEx(view::string_view text) {
    int count = 1;

    for(char ch: text) {
        if (ch == '\n') {
            count++;
        }
    }
    return count;
}

int countLinesEx(const QString &text) {
    int count = 1;

    for(QChar ch: text) {
        if (ch == QLatin1Char('\n')) {
            count++;
        }
    }
    return count;
}

bool atTabStop(int pos, int tabDist) {
    return (pos % tabDist == 0);
}

int nextTab(int pos, int tabDist) {
    return (pos / tabDist) * tabDist + tabDist;
}

QString shiftLineLeftEx(const QString &line, int64_t lineLen, int tabDist, int nChars) {
    auto lineInPtr = line.begin();

    QString out;
    out.reserve(gsl::narrow<int>(lineLen + tabDist));

    int whiteWidth = 0;
    int lastWhiteWidth = 0;

    while (true) {
        if (lineInPtr == line.end() || (lineInPtr - line.begin()) >= lineLen) {
            // nothing on line, wipe it out
            return QString();
        } else if (*lineInPtr == QLatin1Char(' ')) {
            // white space continues with space, advance one character
            whiteWidth++;
            out.append(*lineInPtr++);
        } else if (*lineInPtr == QLatin1Char('\t')) {
            // white space continues with tab, advance to next tab stop
            // save the position, though, in case we need to remove the tab
            lastWhiteWidth = whiteWidth;
            whiteWidth = nextTab(whiteWidth, tabDist);
            out.append(*lineInPtr++);
        } else {

            // end of white space, remove nChars characters
            for (int i = 1; i <= nChars; i++) {
                if(!out.isEmpty()) {
                    if (out.endsWith(QLatin1Char(' '))) {
                        // end of white space is a space, just remove it
                        out.chop(1);
                    } else {
                        /* end of white space is a tab, remove it and add
                           back spaces */
                        out.chop(1);

                        int whiteGoal = whiteWidth - i;
                        whiteWidth = lastWhiteWidth;

                        while (whiteWidth < whiteGoal) {
                            out.append(QLatin1Char(' '));
                            whiteWidth++;
                        }
                    }
                }
            }
            // move remainder of line
            while (lineInPtr != line.end() && (lineInPtr - line.begin()) < lineLen) {
                out.append(*lineInPtr++);
            }

            return out;
        }
    }
}

std::string shiftLineLeftEx(view::string_view line, int64_t lineLen, int tabDist, int nChars) {

    auto lineInPtr = line.begin();

    std::string out;
    out.reserve(static_cast<size_t>(lineLen + tabDist));

    int whiteWidth = 0;
    int lastWhiteWidth = 0;

    while (true) {
        if (lineInPtr == line.end() || (lineInPtr - line.begin()) >= lineLen) {
            // nothing on line, wipe it out
            return std::string();
        } else if (*lineInPtr == ' ') {
            // white space continues with space, advance one character
            whiteWidth++;
            out.append(1, *lineInPtr++);
        } else if (*lineInPtr == '\t') {
            // white space continues with tab, advance to next tab stop
            // save the position, though, in case we need to remove the tab
            lastWhiteWidth = whiteWidth;
            whiteWidth = nextTab(whiteWidth, tabDist);
            out.append(1, *lineInPtr++);
        } else {

            // end of white space, remove nChars characters
            for (int i = 1; i <= nChars; i++) {
                if(!out.empty()) {
                    if (out.back() == ' ') {
                        // end of white space is a space, just remove it
                        out.pop_back();
                    } else {
                        /* end of white space is a tab, remove it and add
                           back spaces */
                        out.pop_back();

                        int whiteGoal = whiteWidth - i;
                        whiteWidth = lastWhiteWidth;

                        while (whiteWidth < whiteGoal) {
                            out.append(1, ' ');
                            whiteWidth++;
                        }
                    }
                }
            }
            // move remainder of line
            while (lineInPtr != line.end() && (lineInPtr - line.begin()) < lineLen) {
                out.append(1, *lineInPtr++);
            }

            return out;
        }
    }
}

QString shiftLineRightEx(const QString &line, int64_t lineLen, int tabsAllowed, int tabDist, int nChars) {
    int whiteWidth, i;

    auto lineInPtr = line.begin();
    QString lineOut;
    lineOut.reserve(gsl::narrow<int>(lineLen + nChars));

    auto lineOutPtr = std::back_inserter(lineOut);
    whiteWidth = 0;
    while (true) {
        if (lineInPtr == line.end() || (lineInPtr - line.begin()) >= lineLen) {
            // nothing on line, wipe it out
            return lineOut;
        } else if (*lineInPtr == QLatin1Char(' ')) {
            // white space continues with tab, advance to next tab stop
            whiteWidth++;
            *lineOutPtr++ = *lineInPtr++;
        } else if (*lineInPtr == QLatin1Char('\t')) {
            // white space continues with tab, advance to next tab stop
            whiteWidth = nextTab(whiteWidth, tabDist);
            *lineOutPtr++ = *lineInPtr++;
        } else {
            // end of white space, add nChars of space
            for (i = 0; i < nChars; i++) {
                *lineOutPtr++ = QLatin1Char(' ');
                whiteWidth++;
                // if we're now at a tab stop, change last 8 spaces to a tab
                if (tabsAllowed && atTabStop(whiteWidth, tabDist)) {

                    lineOut.resize(lineOut.size() - tabDist);

                    //lineOutPtr -= tabDist;
                    *lineOutPtr++ = QLatin1Char('\t');
                }
            }

            // move remainder of line
            while (lineInPtr != line.end() && (lineInPtr - line.begin()) < lineLen) {
                *lineOutPtr++ = *lineInPtr++;
            }

            return lineOut;
        }
    }
}

std::string shiftLineRightEx(view::string_view line, int64_t lineLen, int tabsAllowed, int tabDist, int nChars) {
    int whiteWidth;

    auto lineInPtr = line.begin();
    std::string lineOut;
    lineOut.reserve(static_cast<size_t>(lineLen + nChars));

    auto lineOutPtr = std::back_inserter(lineOut);
    whiteWidth = 0;
    while (true) {
        if (lineInPtr == line.end() || (lineInPtr - line.begin()) >= lineLen) {
            // nothing on line, wipe it out
            return lineOut;
        } else if (*lineInPtr == ' ') {
            // white space continues with tab, advance to next tab stop
            whiteWidth++;
            *lineOutPtr++ = *lineInPtr++;
        } else if (*lineInPtr == '\t') {
            // white space continues with tab, advance to next tab stop
            whiteWidth = nextTab(whiteWidth, tabDist);
            *lineOutPtr++ = *lineInPtr++;
        } else {
            // end of white space, add nChars of space
            for (int i = 0; i < nChars; i++) {
                *lineOutPtr++ = ' ';
                whiteWidth++;
                // if we're now at a tab stop, change last 8 spaces to a tab
                if (tabsAllowed && atTabStop(whiteWidth, tabDist)) {

                    lineOut.resize(lineOut.size() - static_cast<size_t>(tabDist));

                    //lineOutPtr -= tabDist;
                    *lineOutPtr++ = '\t';
                }
            }

            // move remainder of line
            while (lineInPtr != line.end() && (lineInPtr - line.begin()) < lineLen) {
                *lineOutPtr++ = *lineInPtr++;
            }

            return lineOut;
        }
    }
}

std::string ShiftTextEx(view::string_view text, ShiftDirection direction, int tabsAllowed, int tabDist, int nChars) {
    size_t bufLen;

    /*
    ** Shift left adds a maximum of tabDist-2 characters per line
    ** (remove one tab, add tabDist-1 spaces). Shift right adds a maximum of
    ** nChars character per line.
    */
    if (direction == SHIFT_RIGHT) {
        bufLen = text.size() + static_cast<size_t>(countLinesEx(text) * nChars);
    } else {
        bufLen = text.size() + static_cast<size_t>(countLinesEx(text) * tabDist);
    }

    std::string shiftedText;
    shiftedText.reserve(bufLen);

    auto shiftedPtr = std::back_inserter(shiftedText);

    /*
    ** break into lines and call shiftLine(Left/Right) on each
    */
    auto lineStartPtr = text.begin();
    auto textPtr      = text.begin();

    while (true) {
        if (textPtr == text.end() || *textPtr == '\n') {

            auto segment = substr(lineStartPtr, text.end());

            std::string shiftedLineString = (direction == SHIFT_RIGHT) ?
                shiftLineRightEx(segment, textPtr - lineStartPtr, tabsAllowed, tabDist, nChars):
                shiftLineLeftEx (segment, textPtr - lineStartPtr, 			   tabDist, nChars);

            std::copy(shiftedLineString.begin(), shiftedLineString.end(), shiftedPtr);

            if (textPtr == text.end()) {
                // terminate string & exit loop at end of text
                break;
            } else {
                // move the newline from text to shifted text
                *shiftedPtr++ = *textPtr++;
            }
            // start line over
            lineStartPtr = textPtr;
        } else
            textPtr++;
    }

    return shiftedText;
}



/*
** Shift the selection left or right by a single character, or by one tab stop
** if "byTab" is true.  (The length of a tab stop is the size of an emulated
** tab if emulated tabs are turned on, or a hardware tab if not).
*/
void shiftRectEx(DocumentWidget *document, TextArea *area, int direction, bool byTab, TextCursor selStart, TextCursor selEnd, int64_t rectStart, int64_t rectEnd) {
    int64_t offset;
    TextBuffer *buf = document->buffer_;

    // Make sure selStart and SelEnd refer to whole lines
    selStart = buf->BufStartOfLine(selStart);
    selEnd   = buf->BufEndOfLine(selEnd);

    // Calculate the the left/right offset for the new rectangle
    if (byTab) {
        int emTabDist = area->getEmulateTabs();
        offset = (emTabDist == 0) ? buf->BufGetTabDist() : emTabDist;
    } else {
        offset = 1;
    }

    offset *= (direction == SHIFT_LEFT) ? -1 : 1;

    if (rectStart + offset < 0) {
        offset = -rectStart;
    }

    /* Create a temporary buffer for the lines containing the selection, to
       hide the intermediate steps from the display update routines */
    TextBuffer tempBuf;
    tempBuf.BufSetSyncXSelection(false);
    tempBuf.BufSetTabDistance(buf->BufGetTabDist(), false);
    tempBuf.BufSetUseTabs(buf->BufGetUseTabs());

    std::string text = buf->BufGetRangeEx(selStart, selEnd);
    tempBuf.BufSetAll(text);

    // Do the shift in the temporary buffer
    text = buf->BufGetTextInRectEx(selStart, selEnd, rectStart, rectEnd);
    tempBuf.BufRemoveRect(TextCursor(), TextCursor(selEnd - selStart), rectStart, rectEnd);
    tempBuf.BufInsertColEx(rectStart + offset, TextCursor(), text, nullptr, nullptr);

    // Make the change in the real buffer
    buf->BufReplaceEx(selStart, selEnd, tempBuf.BufAsStringEx());
    buf->BufRectSelect(selStart, selStart + tempBuf.BufGetLength(), rectStart + offset, rectEnd + offset);
}

}

/*
** Shift the selection left or right by a single character, or by one tab stop
** if "byTab" is true.  (The length of a tab stop is the size of an emulated
** tab if emulated tabs are turned on, or a hardware tab if not).
*/
void ShiftSelectionEx(DocumentWidget *document, TextArea *area, ShiftDirection direction, bool byTab) {
    TextCursor selStart;
    TextCursor selEnd;
    bool isRect;
    int64_t rectStart;
    int64_t rectEnd;
    int shiftDist;
    TextBuffer *buf = document->buffer_;
    std::string text;

    // get selection, if no text selected, use current insert position
    if (!buf->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {

        const TextCursor cursorPos = area->TextGetCursorPos();
        selStart = buf->BufStartOfLine(cursorPos);
        selEnd = buf->BufEndOfLine(cursorPos);

        if (selEnd < buf->BufGetLength()) {
            selEnd++;
        }

        buf->BufSelect(selStart, selEnd);
        isRect = false;
        text = buf->BufGetRangeEx(selStart, selEnd);

    } else if (isRect) {
        const TextCursor cursorPos = area->TextGetCursorPos();
        int64_t origLength = buf->BufGetLength();
        shiftRectEx(document, area, direction, byTab, selStart, selEnd, rectStart, rectEnd);

        area->TextSetCursorPos((cursorPos < (selEnd + to_integer(selStart)) / 2) ? selStart : cursorPos + (buf->BufGetLength() - origLength));
        return;
    } else {
        selStart = buf->BufStartOfLine(selStart);
        if (selEnd != 0 && buf->BufGetCharacter(selEnd - 1) != '\n') {
            selEnd = buf->BufEndOfLine(selEnd);
            if (selEnd < buf->BufGetLength()) {
                selEnd++;
            }
        }
        buf->BufSelect(selStart, selEnd);
        text = buf->BufGetRangeEx(selStart, selEnd);
    }

    // shift the text by the appropriate distance
    if (byTab) {
        int emTabDist = area->getEmulateTabs();
        shiftDist = emTabDist == 0 ? buf->BufGetTabDist() : emTabDist;
    } else {
        shiftDist = 1;
    }

    std::string shiftedText = ShiftTextEx(text, direction, buf->BufGetUseTabs(), buf->BufGetTabDist(), shiftDist);

    buf->BufReplaceSelectedEx(shiftedText);

    const TextCursor newEndPos = selStart + static_cast<int64_t>(shiftedText.size());
    buf->BufSelect(selStart, newEndPos);
}


void FillSelectionEx(DocumentWidget *document, TextArea *area) {
    TextBuffer *buf = document->buffer_;
    TextCursor left;
    TextCursor right;
    int64_t rectStart = 0;
    int64_t rectEnd   = 0;
    bool isRect;
    int64_t rightMargin;

    TextCursor insertPos = area->TextGetCursorPos();
    int hasSelection = document->buffer_->primary.selected;
    std::string text;

    /* Find the range of characters and get the text to fill.  If there is a
       selection, use it but extend non-rectangular selections to encompass
       whole lines.  If there is no selection, find the paragraph containing
       the insertion cursor */
    if (!buf->BufGetSelectionPos(&left, &right, &isRect, &rectStart, &rectEnd)) {
        left = findParagraphStart(buf, insertPos);
        right = findParagraphEnd(buf, insertPos);
        if (left == right) {
            QApplication::beep();
            return;
        }
        text = buf->BufGetRangeEx(left, right);
    } else if (isRect) {
        left  = buf->BufStartOfLine(left);
        right = buf->BufEndOfLine(right);
        text  = buf->BufGetTextInRectEx(left, right, rectStart, INT_MAX);
    } else {
        left = buf->BufStartOfLine(left);
        if (right != 0 && buf->BufGetCharacter(right - 1) != '\n') {
            right = buf->BufEndOfLine(right);
            if (right < buf->BufGetLength())
                right++;
        }

        buf->BufSelect(left, right);
        text = buf->BufGetRangeEx(left, right);
    }

    /* Find right margin either as specified in the rectangular selection, or
       by measuring the text and querying the window's wrap margin (or width) */
    if (hasSelection && isRect) {
        rightMargin = rectEnd - rectStart;
    } else {

        int wrapMargin = area->getWrapMargin();
        int nCols      = area->getColumns();

        rightMargin = (wrapMargin == 0) ? nCols : wrapMargin;
    }

    // Fill the text
    std::string filledText = fillParagraphsEx(
                text,
                rightMargin,
                buf->BufGetTabDist(),
                buf->BufGetUseTabs(),
                false);

    // Replace the text in the window
    if (hasSelection && isRect) {
        buf->BufReplaceRectEx(left, right, rectStart, INT_MAX, filledText);
        buf->BufRectSelect(left, buf->BufEndOfLine(buf->BufCountForwardNLines(left, countLinesEx(filledText) - 1)), rectStart, rectEnd);
    } else {
        buf->BufReplaceEx(left, right, filledText);
        if (hasSelection) {
            buf->BufSelect(left, left + static_cast<int64_t>(filledText.size()));
        }
    }

    /* Find a reasonable cursor position.  Usually insertPos is best, but
       if the text was indented, positions can shift */
    if (hasSelection && isRect) {
        area->TextSetCursorPos(buf->BufCursorPosHint());
    } else {
        const auto len = static_cast<int64_t>(filledText.size());
        area->TextSetCursorPos(insertPos < left ? left : (insertPos > left + len ? left + len : insertPos));
    }
}

/*
** shift lines left and right in a multi-line text string.
*/
QString ShiftTextEx(const QString &text, ShiftDirection direction, bool tabsAllowed, int tabDist, int nChars) {
    int bufLen;

    /*
    ** Shift left adds a maximum of tabDist-2 characters per line
    ** (remove one tab, add tabDist-1 spaces). Shift right adds a maximum of
    ** nChars character per line.
    */
    if (direction == SHIFT_RIGHT) {
        bufLen = text.size() + countLinesEx(text) * nChars;
    } else {
        bufLen = text.size() + countLinesEx(text) * tabDist;
    }


    QString shiftedText;
    shiftedText.reserve(bufLen);

    auto shiftedPtr = std::back_inserter(shiftedText);

    /*
    ** break into lines and call shiftLine(Left/Right) on each
    */
    auto lineStartPtr = text.begin();
    auto textPtr      = text.begin();

    while (true) {
        if (textPtr == text.end() || *textPtr == QLatin1Char('\n')) {

            auto segment = text.mid(gsl::narrow<int>(lineStartPtr - text.data()));

            QString shiftedLineString = (direction == SHIFT_RIGHT) ?
                shiftLineRightEx(segment, textPtr - lineStartPtr, tabsAllowed, tabDist, nChars):
                shiftLineLeftEx (segment, textPtr - lineStartPtr, 			   tabDist, nChars);

            std::copy(shiftedLineString.begin(), shiftedLineString.end(), shiftedPtr);

            if (textPtr == text.end()) {
                // terminate string & exit loop at end of text
                break;
            } else {
                // move the newline from text to shifted text
                *shiftedPtr++ = *textPtr++;
            }
            // start line over
            lineStartPtr = textPtr;
        } else {
            textPtr++;
        }
    }

    return shiftedText;
}
