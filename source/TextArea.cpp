
#include "TextArea.h"
#include "BlockDragTypes.h"
#include "CallTipWidget.h"
#include "DragStates.h"
#include "MultiClickStates.h"
#include "RangesetTable.h"
#include "DocumentWidget.h"
#include "TextBuffer.h"
#include "DragEndEvent.h"
#include "highlight.h"
#include "preferences.h"
#include "X11Colors.h"
#include "SmartIndentEvent.h"
#include "LanguageMode.h"
#include "TextEditEvent.h"
#include <gsl/gsl_util>
#include <QApplication>
#include <QClipboard>
#include <QDesktopWidget>
#include <QFocusEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QPoint>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QtDebug>
#include <QTextCodec>
#include <QTimer>
#include <QtGlobal>
#include <memory>
#include "Font.h"

#define EMIT_EVENT_0(name)                                       \
    do {                                                         \
        if(!(flags & SupressRecording)) {                        \
            TextEditEvent textEvent(QLatin1String(name), flags); \
            QApplication::sendEvent(this, &textEvent);           \
        }                                                        \
    } while(0)

#define EMIT_EVENT_1(name, arg)                                       \
    do {                                                              \
        if(!(flags & SupressRecording)) {                             \
            TextEditEvent textEvent(QLatin1String(name), arg, flags); \
            QApplication::sendEvent(this, &textEvent);                \
        }                                                             \
    } while(0)


namespace {

QString asciiToUnicode(const char *chars, int64_t len) {

    QString s;
    s.reserve(gsl::narrow<int>(len));

    for(int i = 0; i < len; ++i) {
        quint32 ch = chars[i] & 0xff;
        if(ch < 0x80 || ch >= 0xa0) {
            s.append(QChar(ch));
        } else {
            s.append(QChar::ReplacementCharacter);
        }
    }
    return s;
}

constexpr int CALLTIP_EDGE_GUARD = 5;

// Length of delay in milliseconds for vertical autoscrolling
constexpr int VERTICAL_SCROLL_DELAY = 50;

/* Masks for text drawing methods.  These are or'd together to form an
   integer which describes what drawing calls to use to draw a string */
constexpr int FILL_SHIFT         = 8;
constexpr int SECONDARY_SHIFT    = 9;
constexpr int PRIMARY_SHIFT      = 10;
constexpr int HIGHLIGHT_SHIFT    = 11;
constexpr int STYLE_LOOKUP_SHIFT = 0;
constexpr int BACKLIGHT_SHIFT    = 12;

constexpr int FILL_MASK         = (1 << FILL_SHIFT);
constexpr int SECONDARY_MASK    = (1 << SECONDARY_SHIFT);
constexpr int PRIMARY_MASK      = (1 << PRIMARY_SHIFT);
constexpr int HIGHLIGHT_MASK    = (1 << HIGHLIGHT_SHIFT);
constexpr int STYLE_LOOKUP_MASK = (0xff << STYLE_LOOKUP_SHIFT);
constexpr int BACKLIGHT_MASK    = (0xff << BACKLIGHT_SHIFT);

constexpr int RANGESET_SHIFT = 20;
constexpr int RANGESET_MASK  = (0x3F << RANGESET_SHIFT);

/* If you use both 32-Bit Style mask layout:
   Bits +----------------+----------------+----------------+----------------+
    hex |1F1E1D1C1B1A1918|1716151413121110| F E D C B A 9 8| 7 6 5 4 3 2 1 0|
    dec |3130292827262524|2322212019181716|151413121110 9 8| 7 6 5 4 3 2 1 0|
        +----------------+----------------+----------------+----------------+
   Type |             r r| r r r r b b b b| b b b b H 1 2 F| s s s s s s s s|
        +----------------+----------------+----------------+----------------+
   where:
        s - style lookup value (8 bits)
        F - fill (1 bit)
        2 - secondary selection  (1 bit)
        1 - primary selection (1 bit)
        H - highlight (1 bit)
        b - backlighting index (8 bits)
        r - rangeset index (6 bits)
   This leaves 6 "unused" bits */

/* Maximum displayable line length (how many characters will fit across the
   widest window).  This amount of memory is temporarily allocated from the
   stack in the redisplayLine routine for drawing strings */
constexpr int MAX_DISP_LINE_LEN = 1000;

bool offscreenV(QDesktopWidget *desktop, int top, int height) {
    return (top < CALLTIP_EDGE_GUARD || top + height >= desktop->height() - CALLTIP_EDGE_GUARD);
}

/*
** Returns a new string with each \t replaced with tab_width spaces or
** a pointer to text if there were no tabs.
** Note that this is dumb replacement, not smart tab-like behavior!  The goal
** is to prevent tabs from turning into squares in calltips, not to get the
** formatting just right.
*/
QString expandAllTabsEx(const QString &text, int tab_width) {

    // First count 'em
    const int nTabs = static_cast<int>(std::count(text.begin(), text.end(), QLatin1Char('\t')));

    if (nTabs == 0) {
        return text;
    }

    // Allocate the new string
	int len = text.size() + (tab_width - 1) * nTabs;

    QString textCpy;
    textCpy.reserve(len);

	auto it = std::back_inserter(textCpy);

    // Now replace 'em
    for(QChar ch : text) {
        if (ch == QLatin1Char('\t')) {
            for (int i = 0; i < tab_width; ++i) {
				*it++ = QLatin1Char(' ');
            }
        } else {
			*it++ = ch;
        }
    }

    return textCpy;
}

/*
** Find the left and right margins of text between "start" and "end" in
** buffer "buf".  Note that "start is assumed to be at the start of a line.
*/
void findTextMargins(TextBuffer *buf, int64_t start, int64_t end, int64_t *leftMargin, int64_t *rightMargin) {

	int width = 0;
	int maxWidth = 0;
	int minWhite = INT_MAX;
	bool inWhite = true;

    for (int64_t pos = start; pos < end; pos++) {
		char c = buf->BufGetCharacter(pos);
		if (inWhite && c != ' ' && c != '\t') {
            inWhite = false;
			if (width < minWhite)
				minWhite = width;
		}
		if (c == '\n') {
			if (width > maxWidth) {
				maxWidth = width;
			}

			width = 0;
			inWhite = true;
		} else
            width += TextBuffer::BufCharWidth(c, width, buf->BufGetTabDist());
	}

	if (width > maxWidth) {
		maxWidth = width;
	}

	*leftMargin = (minWhite == INT_MAX) ? 0 : minWhite;
	*rightMargin = maxWidth;
}

/*
** Find a text position in buffer "buf" by counting forward or backward
** from a reference position with known line number
*/
int64_t findRelativeLineStart(const TextBuffer *buf, int64_t referencePos, int64_t referenceLineNum, int64_t newLineNum) {

	if (newLineNum < referenceLineNum) {
		return buf->BufCountBackwardNLines(referencePos, referenceLineNum - newLineNum);
	} else if (newLineNum > referenceLineNum) {
		return buf->BufCountForwardNLines(referencePos, newLineNum - referenceLineNum);
	}

	return buf->BufStartOfLine(referencePos);
}

/*
** Callback attached to the text buffer to receive delete information before
** the modifications are actually made.
*/
void bufPreDeleteCB(int64_t pos, int64_t nDeleted, void *arg) {
    auto area = static_cast<TextArea *>(arg);
    area->bufPreDeleteCallback(pos, nDeleted);
}

/*
** Callback attached to the text buffer to receive modification information
*/
void bufModifiedCB(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *arg) {

    auto area = static_cast<TextArea *>(arg);
    area->bufModifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText);
}

/*
** Count the number of newlines in a null-terminated text string;
*/
int countLinesEx(view::string_view string) {
	return static_cast<int>(std::count(string.begin(), string.end(), '\n'));
}

void ringIfNecessary(bool silent) {
	if (!silent) {
		QApplication::beep();
	}
}

/*
** Maintain boundaries of changed region between two buffers which
** start out with identical contents, but diverge through insertion,
** deletion, and replacement, such that the buffers can be reconciled
** by replacing the changed region of either buffer with the changed
** region of the other.
**
** rangeStart is the beginning of the modification region in the shared
** coordinates of both buffers (which are identical up to rangeStart).
** modRangeEnd is the end of the changed region for the buffer being
** modified, unmodRangeEnd is the end of the region for the buffer NOT
** being modified.  A value of -1 in rangeStart indicates that there
** have been no modifications so far.
*/
void trackModifyRange(int64_t *rangeStart, int64_t *modRangeEnd, int64_t *unmodRangeEnd, int64_t modPos, int64_t nInserted, int64_t nDeleted) {
	if (*rangeStart == -1) {
		*rangeStart    = modPos;
		*modRangeEnd   = modPos + nInserted;
		*unmodRangeEnd = modPos + nDeleted;
	} else {
		if (modPos < *rangeStart) {
			*rangeStart = modPos;
		}

		if (modPos + nDeleted > *modRangeEnd) {
			*unmodRangeEnd += modPos + nDeleted - *modRangeEnd;
			*modRangeEnd = modPos + nInserted;
		} else {
			*modRangeEnd += nInserted - nDeleted;
		}
	}
}

bool isModifier(QKeyEvent *e) {
    if (!e) {
        return false;
    }

    switch (e->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
        return true;
    default:
        return false;
    }
}

}

/**
 * @brief TextArea::TextArea
 * @param document
 * @param buffer
 * @param fontStruct
 */
TextArea::TextArea(DocumentWidget *document, TextBuffer *buffer, QFont fontStruct) : QAbstractScrollArea(document) {

	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setMouseTracking(true);
	setFocusPolicy(Qt::WheelFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(verticalScrollBar(),   &QScrollBar::valueChanged, this, &TextArea::verticalScrollBar_valueChanged);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &TextArea::horizontalScrollBar_valueChanged);

    document_         = document;
	autoScrollTimer_  = new QTimer(this);
	cursorBlinkTimer_ = new QTimer(this);
	clickTimer_       = new QTimer(this);

	autoScrollTimer_->setSingleShot(true);
    connect(autoScrollTimer_,  &QTimer::timeout, this, &TextArea::autoScrollTimerTimeout);
    connect(cursorBlinkTimer_, &QTimer::timeout, this, &TextArea::cursorBlinkTimerTimeout);

    clickTimer_->setSingleShot(true);
    connect(clickTimer_, &QTimer::timeout, this, &TextArea::clickTimeout);

    // defaults for the "resources"
    P_cursorBlinkRate = QApplication::cursorFlashTime() / 2;

    /* Add mandatory delimiters blank, tab, and newline to the list of
     * delimiters. */
    setWordDelimiters(GetPrefDelimiters().toStdString());

    P_colorizeHighlightedText = GetPrefColorizeHighlightedText();
    P_autoWrapPastedText      = GetPrefAutoWrapPastedText();
    P_heavyCursor             = GetPrefHeavyCursor();
    P_rows                    = GetPrefRows();
    P_columns                 = GetPrefCols();
    P_readOnly                = document->lockReasons().isAnyLocked();
    P_wrapMargin              = GetPrefWrapMargin();
    P_autoIndent              = document->indentStyle_ == IndentStyle::Auto;
    P_smartIndent             = document->indentStyle_ == IndentStyle::Smart;
    P_autoWrap                = document->wrapMode_ == WrapStyle::Newline;
    P_continuousWrap          = document->wrapMode_ == WrapStyle::Continuous;
    P_overstrike              = document->overstrike_;
    P_hidePointer             = GetPrefTypingHidesPointer();
    P_cursorVPadding          = GetVerticalAutoScroll();
    P_emulateTabs             = GetPrefEmTabDist(PLAIN_LANGUAGE_MODE);

    QFontMetrics fm(fontStruct);
	QFontInfo    fi(fontStruct);

	ascent_  = fm.ascent();
	descent_ = fm.descent();

    font_               = fontStruct;
    buffer_             = buffer;
	fixedFontWidth_     = fi.fixedPitch() ? fm.maxWidth() : -1;
    calltip_.ID         = 0;

    // this will set the rect_ correctly
    rect_.setLeft(P_marginWidth);

    /* Attach the callback to the text buffer for receiving modification
     * information */
	if (buffer) {
		buffer->BufAddModifyCB(bufModifiedCB, this);
		buffer->BufAddPreDeleteCB(bufPreDeleteCB, this);
	}

	// Update the display to reflect the contents of the buffer
	if(buffer) {
        bufModifiedCB(0, buffer->BufGetLength(), 0, 0, std::string(), this);
	}

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();

#if 0
	// NOTE(eteran): this seems to be a viable approach for shortcuts
    new QShortcut(QKeySequence(tr("Ctrl+K")), this, SLOT(DebugSlot()), nullptr, Qt::WidgetShortcut);
#endif

    // track when we lose ownership of the selection
    if(QApplication::clipboard()->supportsSelection()) {
        connect(QApplication::clipboard(), &QClipboard::selectionChanged, this, [this]() {
            if(!QApplication::clipboard()->ownsSelection()) {
                buffer_->BufUnselect();
            }
        });
    }
}

/**
 * @brief TextArea::pasteClipboardAP
 * @param flags
 */
void TextArea::pasteClipboardAP(EventFlags flags) {

    EMIT_EVENT_0("paste_clipboard");

	if (flags & RectFlag) {
		TextColPasteClipboard();
	} else {
		TextPasteClipboard();
	}
}

void TextArea::cutClipboardAP(EventFlags flags) {

    EMIT_EVENT_0("cut_clipboard");
    TextCutClipboard();
}

void TextArea::toggleOverstrikeAP(EventFlags flags) {

    EMIT_EVENT_0("toggle_overstrike");

	if (P_overstrike) {
		P_overstrike = false;
        TextDSetCursorStyle(P_heavyCursor ? CursorStyles::Heavy : CursorStyles::Normal);
	} else {
		P_overstrike = true;
        if (cursorStyle_ == CursorStyles::Normal || cursorStyle_ == CursorStyles::Heavy)
            TextDSetCursorStyle(CursorStyles::Block);
	}
}

void TextArea::endOfLineAP(EventFlags flags) {

    EMIT_EVENT_0("end_of_line");

    int64_t insertPos = cursorPos_;

	cancelDrag();

    if (flags & AbsoluteFlag) {
		TextDSetInsertPosition(buffer_->BufEndOfLine(insertPos));
	} else {
		TextDSetInsertPosition(TextDEndOfLine(insertPos, false));
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
	cursorPreferredCol_ = -1;
}

void TextArea::deleteNextCharacterAP(EventFlags flags) {

    EMIT_EVENT_0("delete_next_character");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (deletePendingSelection()) {
		return;
	}

	if (insertPos == buffer_->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}

	buffer_->BufRemove(insertPos, insertPos + 1);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::copyClipboardAP(EventFlags flags) {

    EMIT_EVENT_0("copy_clipboard");

    cancelDrag();
    if (!buffer_->BufGetPrimary().selected) {
        QApplication::beep();
        return;
    }

    CopyToClipboard();
}

void TextArea::deletePreviousWordAP(EventFlags flags) {

    EMIT_EVENT_0("delete_previous_word");

    int64_t insertPos = cursorPos_;
    int64_t lineStart = buffer_->BufStartOfLine(insertPos);

    bool silent = flags & NoBellFlag;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (deletePendingSelection()) {
		return;
	}

	if (insertPos == lineStart) {
		ringIfNecessary(silent);
		return;
	}

    int64_t pos = std::max<int64_t>(insertPos - 1, 0);

    while (P_delimiters.find(buffer_->BufGetCharacter(pos)) != std::string::npos && pos != lineStart) {
		pos--;
	}

	pos = startOfWord(pos);
	buffer_->BufRemove(pos, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::beginningOfLineAP(EventFlags flags) {

    EMIT_EVENT_0("beginning_of_line");

    int64_t insertPos = cursorPos_;

	cancelDrag();

    if (flags & AbsoluteFlag) {
		TextDSetInsertPosition(buffer_->BufStartOfLine(insertPos));
	} else {
		TextDSetInsertPosition(TextDStartOfLine(insertPos));
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
	cursorPreferredCol_ = 0;
}

void TextArea::processCancelAP(EventFlags flags) {

    EMIT_EVENT_0("process_cancel");

	DragStates dragState = dragState_;

	// If there's a calltip displayed, kill it.
	TextDKillCalltip(0);

	if (dragState == PRIMARY_DRAG || dragState == PRIMARY_RECT_DRAG) {
        buffer_->BufUnselect();
	}

	cancelDrag();
}

void TextArea::deletePreviousCharacterAP(EventFlags flags) {

    EMIT_EVENT_0("delete_previous_character");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
    if (checkReadOnly()) {
		return;
    }

    if (deletePendingSelection()) {
		return;
    }

	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}

    if (deleteEmulatedTab()) {
		return;
    }

	if (P_overstrike) {
		char c = buffer_->BufGetCharacter(insertPos - 1);
        if (c == '\n') {
			buffer_->BufRemove(insertPos - 1, insertPos);
        } else if (c != '\t') {
			buffer_->BufReplaceEx(insertPos - 1, insertPos, " ");
        }
	} else {
		buffer_->BufRemove(insertPos - 1, insertPos);
	}

	TextDSetInsertPosition(insertPos - 1);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::newlineAP
 * @param flags
 */
void TextArea::newlineAP(EventFlags flags) {

    EMIT_EVENT_0("newline");

	if (P_autoIndent || P_smartIndent) {
		newlineAndIndentAP(flags | SupressRecording);
	} else {
		newlineNoIndentAP(flags | SupressRecording);
	}
}

/**
 * @brief TextArea::processUpAP
 * @param flags
 */
void TextArea::processUpAP(EventFlags flags) {

    EMIT_EVENT_0("process_up");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;
	bool abs    = flags & AbsoluteFlag;

	cancelDrag();
	if (!TextDMoveUp(abs)) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::processDownAP
 * @param flags
 */
void TextArea::processDownAP(EventFlags flags) {

    EMIT_EVENT_0("process_down");

    int64_t insertPos = cursorPos_;

	bool silent = flags & NoBellFlag;
	bool abs    = flags & AbsoluteFlag;

	cancelDrag();
	if (!TextDMoveDown(abs)) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::forwardCharacterAP
 * @param flags
 */
void TextArea::forwardCharacterAP(EventFlags flags) {

    EMIT_EVENT_0("forward_character");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (!TextDMoveRight()) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::backwardCharacterAP
 * @param flags
 */
void TextArea::backwardCharacterAP(EventFlags flags) {

    EMIT_EVENT_0("backward_character");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (!TextDMoveLeft()) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::selfInsertAP
 * @param string
 * @param flags
 */
void TextArea::selfInsertAP(const QString &string, EventFlags flags) {

    EMIT_EVENT_1("insert_string", string);

    cancelDrag();
    if (checkReadOnly()) {
        return;
    }

    std::string s = string.toStdString();

    /* If smart indent is on, call the smart indent callback to check the
       inserted character */
    if (P_smartIndent) {
		SmartIndentEvent smartIndent;
        smartIndent.reason        = CHAR_TYPED;
        smartIndent.pos           = cursorPos_;
        smartIndent.indentRequest = 0;
        smartIndent.charsTyped    = view::string_view(s);

        for(auto &c : smartIndentCallbacks_) {
            c.first(this, &smartIndent, c.second);
        }
    }
    TextInsertAtCursorEx(s, /*allowPendingDelete=*/true, /*allowWrap=*/true);
    buffer_->BufUnselect();
}

/**
 * @brief TextArea::~TextArea
 */
TextArea::~TextArea() noexcept {
    buffer_->BufRemoveModifyCB(bufModifiedCB, this);
    buffer_->BufRemovePreDeleteCB(bufPreDeleteCB, this);
}

/**
 * @brief TextArea::verticalScrollBar_valueChanged
 * @param value
 */
void TextArea::verticalScrollBar_valueChanged(int value) {
	TextDSetScroll(value, horizOffset_);
}

/**
 * @brief TextArea::horizontalScrollBar_valueChanged
 * @param value
 */
void TextArea::horizontalScrollBar_valueChanged(int value) {
	TextDSetScroll(topLineNum_, value);
}

/**
 * @brief TextArea::cursorBlinkTimerTimeout
 */
void TextArea::cursorBlinkTimerTimeout() {

	// Blink the cursor
	if (cursorOn_) {
		TextDBlankCursor();
	} else {
		TextDUnblankCursor();
	}
}

/**
 * @brief TextArea::autoScrollTimerTimeout
 */
void TextArea::autoScrollTimerTimeout() {

    QFontMetrics fm(font_);
    int64_t topLineNum;
    int horizOffset;
    int cursorX;
    int y;
    int fontWidth    = fm.maxWidth();
    int fontHeight   = ascent_ + descent_;
    QPoint mouseCoord = mouseCoord_;

	/* For vertical autoscrolling just dragging the mouse outside of the top
	   or bottom of the window is sufficient, for horizontal (non-rectangular)
	   scrolling, see if the position where the CURSOR would go is outside */
    int64_t newPos = TextDXYToPosition(mouseCoord);

	if (dragState_ == PRIMARY_RECT_DRAG) {
        cursorX = mouseCoord.x();
	} else if (!TextDPositionToXY(newPos, &cursorX, &y)) {
        cursorX = mouseCoord.x();
	}

	/* Scroll away from the pointer, 1 character (horizontal), or 1 character
	   for each fontHeight distance from the mouse to the text (vertical) */
	TextDGetScroll(&topLineNum, &horizOffset);

	if (cursorX >= viewport()->width() - P_marginWidth) {
		horizOffset += fontWidth;
    } else if (mouseCoord.x() < rect_.left()) {
		horizOffset -= fontWidth;
	}

    if (mouseCoord.y() >= viewport()->height() - P_marginHeight) {
        topLineNum += 1 + ((mouseCoord.y() - viewport()->height() - P_marginHeight) / fontHeight) + 1;
    } else if (mouseCoord.y() < P_marginHeight) {
        topLineNum -= 1 + ((P_marginHeight - mouseCoord.y()) / fontHeight);
	}

	TextDSetScroll(topLineNum, horizOffset);

	/* Continue the drag operation in progress.  If none is in progress
	   (safety check) don't continue to re-establish the timer proc */
	switch(dragState_) {
	case PRIMARY_DRAG:
		adjustSelection(mouseCoord);
		break;
	case PRIMARY_RECT_DRAG:
		adjustSelection(mouseCoord);
		break;
	case SECONDARY_DRAG:
		adjustSecondarySelection(mouseCoord);
		break;
	case SECONDARY_RECT_DRAG:
		adjustSecondarySelection(mouseCoord);
		break;
	case PRIMARY_BLOCK_DRAG:
		BlockDragSelection(mouseCoord, USE_LAST);
		break;
	default:
		autoScrollTimer_->stop();
		return;
	}

	// re-establish the timer proc (this routine) to continue processing
    autoScrollTimer_->start(mouseCoord.y() >= P_marginHeight && mouseCoord.y() < viewport()->height() - P_marginHeight ? (VERTICAL_SCROLL_DELAY * fontWidth) / fontHeight : VERTICAL_SCROLL_DELAY);
}

/**
 * @brief TextArea::focusInEvent
 * @param event
 */
void TextArea::focusInEvent(QFocusEvent *event) {

	// If the timer is not already started, start it
	if(!cursorBlinkTimer_->isActive()) {
		cursorBlinkTimer_->start(P_cursorBlinkRate);
	}

	// Change the cursor to active style
	if (P_overstrike) {
        TextDSetCursorStyle(CursorStyles::Block);
	} else {
        TextDSetCursorStyle(P_heavyCursor ? CursorStyles::Heavy : CursorStyles::Normal);
	}

	TextDUnblankCursor();
    QAbstractScrollArea::focusInEvent(event);
}

void TextArea::focusOutEvent(QFocusEvent *event) {

	cursorBlinkTimer_->stop();

    TextDSetCursorStyle(CursorStyles::Caret);
	TextDUnblankCursor();

	// If there's a calltip displayed, kill it.
	TextDKillCalltip(0);
    QAbstractScrollArea::focusOutEvent(event);
}

/**
 * @brief TextArea::contextMenuEvent
 * @param event
 */
void TextArea::contextMenuEvent(QContextMenuEvent *event) {
	if(event->modifiers() != Qt::ControlModifier) {
		Q_EMIT customContextMenuRequested(mapToGlobal(event->pos()));
    }
}

/**
 * @brief TextArea::dragEnterEvent
 * @param event
 */
void TextArea::dragEnterEvent(QDragEnterEvent *event) {
	Q_UNUSED(event);
}

/**
 * @brief TextArea::dragLeaveEvent
 * @param event
 */
void TextArea::dragLeaveEvent(QDragLeaveEvent *event) {
	Q_UNUSED(event);
}

/**
 * @brief TextArea::dragMoveEvent
 * @param event
 */
void TextArea::dragMoveEvent(QDragMoveEvent *event) {
	Q_UNUSED(event);
}

/**
 * @brief TextArea::dropEvent
 * @param event
 */
void TextArea::dropEvent(QDropEvent *event) {
	Q_UNUSED(event);
}

/**
 * @brief TextArea::keyPressEvent
 * @param event
 */
void TextArea::keyPressEvent(QKeyEvent *event) {

    if (isModifier(event)) {
        return;
    }

    /* PageLeft and PageRight are placed later than the PageUp/PageDown
       bindings.  Some systems map osfPageLeft to Ctrl-PageUp.
       Overloading this single key gives problems, and we want to give
       priority to the normal version. */
    if ((event->key() == Qt::Key_Z) && (event->modifiers() == (Qt::ControlModifier))) {
        // higher lever should capture this, so let's avoid inserting chars from this event
        QApplication::beep();
        return;
    } else if ((event->key() == Qt::Key_Z) && (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier))) {
        // higher lever should capture this, so let's avoid inserting chars from this event
        QApplication::beep();
        return;
    } else if ((event->key() == Qt::Key_X) && (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier))) {
        // higher lever should capture this, so let's avoid inserting chars from this event
        QApplication::beep();
        return;
    } else if ((event->key() == Qt::Key_C) && (event->modifiers() == (Qt::ControlModifier))) {
        // higher lever should capture this, so let's avoid inserting chars from this event
        QApplication::beep();
        return;
    } else if ((event->key() == Qt::Key_B) && (event->modifiers() == (Qt::ControlModifier))) {
        // higher lever should capture this, so let's avoid inserting chars from this event
        QApplication::beep();
		return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ControlModifier))) {
#if 0
        previousDocumentAP(); // "previous-document"
        return;
#endif
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ShiftModifier | Qt::AltModifier))) {
        previousPageAP(ExtendFlag | RectFlag); // "previous-page(extend, rect)"
        return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier))) {
        previousPageAP(ExtendFlag | RectFlag); // "previous-page(extend, rect)"
        return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ShiftModifier))) {
        previousPageAP(ExtendFlag); // "previous-page(extend)"
        return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::NoModifier))) {
        previousPageAP(); // "previous-page()"
        return;
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ControlModifier))) {
#if 0
        nextDocumentAP(); // "next-document"
        return;
#endif
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ShiftModifier | Qt::AltModifier))) {
        nextPageAP(ExtendFlag | RectFlag); // "next-page(extend, rect)"
        return;
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier))) {
        nextPageAP(ExtendFlag | RectFlag); // "next-page(extend, rect)"
        return;
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ShiftModifier))) {
        nextPageAP(ExtendFlag); // "next-page(extend)"
        return;
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::NoModifier))) {
        nextPageAP(); // "next-page()"
        return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))) {
        pageLeftAP(ExtendFlag | RectFlag); // page-left(extent, rect);
        return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier))) {
        pageLeftAP(ExtendFlag | RectFlag); // page-left(extent, rect);
        return;
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        pageLeftAP(ExtendFlag); // page-left(extent);
        return;
#if 0
    } else if ((event->key() == Qt::Key_PageUp) && (event->modifiers() == (Qt::ControlModifier))) {
        // NOTE(eteran): no page left/right on most keyboards, so conflict here :-/
        pageLeftAP(); // page-left()
        return;
#endif
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))) {
        pageRightAP(ExtendFlag | RectFlag); // page-right(extent, rect);
        return;
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier))) {
        pageRightAP(ExtendFlag | RectFlag); // page-right(extent, rect);
        return;
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        pageRightAP(ExtendFlag); // page-right(extent);
        return;
#if 0
    } else if ((event->key() == Qt::Key_PageDown) && (event->modifiers() == (Qt::ControlModifier))) {
        // NOTE(eteran): no page left/right on most keyboards, so conflict here :-/
        pageRightAP(); // page-right()
        return;
#endif
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::AltModifier | Qt::ShiftModifier))) {
        processShiftUpAP(RectFlag); // process-shift-up(rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::MetaModifier | Qt::ShiftModifier))) {
        processShiftUpAP(RectFlag); // process-shift-up(rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::AltModifier | Qt::ShiftModifier))) {
        processShiftDownAP(RectFlag); // process-shift-down(rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::MetaModifier | Qt::ShiftModifier))) {
        processShiftDownAP(RectFlag); // process-shift-down(rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        backwardParagraphAP(ExtendFlag | RectFlag); // backward-paragraph(extent, rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        backwardParagraphAP(ExtendFlag | RectFlag); // backward-paragraph(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier  | Qt::ShiftModifier))) {
        backwardParagraphAP(ExtendFlag); // backward-paragraph(extend)
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        forwardParagraphAP(ExtendFlag | RectFlag); // forward-paragraph(extent, rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        forwardParagraphAP(ExtendFlag | RectFlag); // forward-paragraph(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier  | Qt::ShiftModifier))) {
        forwardParagraphAP(ExtendFlag); // forward-paragraph(extend)
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier  | Qt::AltModifier | Qt::ShiftModifier))) {
        forwardWordAP(ExtendFlag | RectFlag); // forward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier  | Qt::MetaModifier | Qt::ShiftModifier))) {
        forwardWordAP(ExtendFlag | RectFlag); // forward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::AltModifier | Qt::ShiftModifier))) {
        keySelectAP(RightFlag | RectFlag); // key-select(right, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::MetaModifier | Qt::ShiftModifier))) {
        keySelectAP(RightFlag | RectFlag); // key-select(right, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier | Qt::ShiftModifier))) {
        forwardWordAP(ExtendFlag); // forward-word(extend)
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier  | Qt::AltModifier | Qt::ShiftModifier))) {
        backwardWordAP(ExtendFlag | RectFlag); // backward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier  | Qt::MetaModifier | Qt::ShiftModifier))) {
        backwardWordAP(ExtendFlag | RectFlag); // backward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::AltModifier | Qt::ShiftModifier))) {
        keySelectAP(LeftFlag | RectFlag); // key-select(left, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::MetaModifier | Qt::ShiftModifier))) {
        keySelectAP(LeftFlag | RectFlag); // key-select(left, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier | Qt::ShiftModifier))) {
        backwardWordAP(ExtendFlag); // backward-word(extend)
        return;
#ifdef Q_OS_MACOS
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        processShiftUpAP(RectFlag); // process-shift-up(rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        processShiftUpAP(RectFlag); // process-shift-up(rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        processShiftDownAP(RectFlag); // process-shift-down(rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        processShiftDownAP(RectFlag); // process-shift-down(rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        backwardParagraphAP(ExtendFlag | RectFlag); // backward-paragraph(extent, rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        backwardParagraphAP(ExtendFlag | RectFlag); // backward-paragraph(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::ControlModifier  | Qt::ShiftModifier | Qt::KeypadModifier))) {
        backwardParagraphAP(ExtendFlag); // backward-paragraph(extend)
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        forwardParagraphAP(ExtendFlag | RectFlag); // forward-paragraph(extent, rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        forwardParagraphAP(ExtendFlag | RectFlag); // forward-paragraph(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::ControlModifier  | Qt::ShiftModifier | Qt::KeypadModifier))) {
        forwardParagraphAP(ExtendFlag); // forward-paragraph(extend)
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::ControlModifier  | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        forwardWordAP(ExtendFlag | RectFlag); // forward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::ControlModifier  | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        forwardWordAP(ExtendFlag | RectFlag); // forward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        keySelectAP(RightFlag | RectFlag); // key-select(right, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        keySelectAP(RightFlag | RectFlag); // key-select(right, rect)
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        forwardWordAP(ExtendFlag); // forward-word(extend)
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::ControlModifier  | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        backwardWordAP(ExtendFlag | RectFlag); // backward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::ControlModifier  | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        backwardWordAP(ExtendFlag | RectFlag); // backward-word(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        keySelectAP(LeftFlag | RectFlag); // key-select(left, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        keySelectAP(LeftFlag | RectFlag); // key-select(left, rect)
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        backwardWordAP(ExtendFlag); // backward-word(extend)
        return;
#endif
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))) {
        endOfLineAP(ExtendFlag | RectFlag); // end-of-line(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::MetaModifier | Qt::ShiftModifier))) {
        endOfLineAP(ExtendFlag | RectFlag); // end-of-line(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::ShiftModifier))) {
        endOfLineAP(ExtendFlag); // end-of-line(extend)
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        endOfFileAP(ExtendFlag | RectFlag); // end-of-file(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        endOfFileAP(ExtendFlag | RectFlag); // end-of-file(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        endOfFileAP(ExtendFlag); // end-of-file(extend)
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))) {
        beginningOfLineAP(ExtendFlag | RectFlag); // beginning-of-line(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::MetaModifier | Qt::ShiftModifier))) {
        beginningOfLineAP(ExtendFlag | RectFlag); // beginning-of-line(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::ShiftModifier))) {
        beginningOfLineAP(ExtendFlag); // beginning-of-line(extend)
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        beginningOfFileAP(ExtendFlag | RectFlag); // beginning-of-file(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        beginningOfFileAP(ExtendFlag | RectFlag); // beginning-of-file(extend, rect)
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        beginningOfFileAP(ExtendFlag); // beginning-of-file(extend)
        return;
    } else if ((event->key() == Qt::Key_Insert) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        copyPrimaryAP(RectFlag); // copy-primary(rect)
        return;
    } else if ((event->key() == Qt::Key_Insert) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        copyPrimaryAP(RectFlag); // copy-primary(rect)
        return;
    } else if ((event->key() == Qt::Key_Delete) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        cutPrimaryAP(RectFlag); // cut-primary(rect)
        return;
    } else if ((event->key() == Qt::Key_Delete) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        cutPrimaryAP(RectFlag); // cut-primary(rect)
        return;
    } else if ((event->key() == Qt::Key_Space) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        keySelectAP(); // key-select()
        return;
    } else if ((event->key() == Qt::Key_Space) && (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        keySelectAP(RectFlag); // key-select(rect)
        return;
    } else if ((event->key() == Qt::Key_Space) && (event->modifiers() == (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
        keySelectAP(RectFlag); // key-select(rect)
        return;
    } else if ((event->key() == Qt::Key_Delete) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        cutPrimaryAP(); // cut-primary()
        return;
    } else if ((event->key() == Qt::Key_Delete) && (event->modifiers() == (Qt::ControlModifier))) {
        deleteToEndOfLineAP(); // delete-to-end-of-line()
        return;
    } else if ((event->key() == Qt::Key_U) && (event->modifiers() == (Qt::ControlModifier))) {
        deleteToStartOfLineAP(); // delete-to-start-of-line()
        return;
    } else if ((event->key() == Qt::Key_Slash) && (event->modifiers() == (Qt::ControlModifier))) {
        selectAllAP(); // select-all()
        return;
    } else if ((event->key() == Qt::Key_Backslash) && (event->modifiers() == (Qt::ControlModifier))) {
        deselectAllAP(); // deselect-all()
        return;
    } else if ((event->key() == Qt::Key_Tab) && (event->modifiers() == (Qt::NoModifier))) {
        processTabAP(); // process-tab()
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::NoModifier))) {
        forwardCharacterAP(); // forward-character()
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::NoModifier))) {
        backwardCharacterAP(); // backward-character()
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::NoModifier))) {
        processUpAP(); // process-up()
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::NoModifier))) {
        processDownAP(); // process-down()
        return;
#if Q_OS_MACOS
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::KeypadModifier))) {
        forwardCharacterAP(); // forward-character()
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::KeypadModifier))) {
        backwardCharacterAP(); // backward-character()
        return;
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::KeypadModifier))) {
        processUpAP(); // process-up()
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::KeypadModifier))) {
        processDownAP(); // process-down()
        return;
#endif
    } else if ((event->key() == Qt::Key_Return) && (event->modifiers() == (Qt::NoModifier))) {
        newlineAP(); // newline()
        return;
    } else if ((event->key() == Qt::Key_Enter) && (event->modifiers() == (Qt::KeypadModifier))) {
        newlineAP(); // newline()
        return;
    } else if ((event->key() == Qt::Key_Backspace) && (event->modifiers() == (Qt::NoModifier))) {
        deletePreviousCharacterAP(); // delete-previous-character()
        return;
	} else if ((event->key() == Qt::Key_Backspace) && (event->modifiers() == (Qt::ShiftModifier))) {
		deletePreviousCharacterAP(); // delete-previous-character()
		return;
	} else if ((event->key() == Qt::Key_Backspace) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
		deletePreviousWordAP(); // delete-previous-word()
		return;
	} else if ((event->key() == Qt::Key_Escape) && (event->modifiers() == (Qt::NoModifier))) {
        processCancelAP(); // process-cancel()
        return;
    } else if ((event->key() == Qt::Key_Return) && (event->modifiers() == (Qt::ControlModifier))) {
        newlineAndIndentAP(); // newline-and-indent()
        return;
    } else if ((event->key() == Qt::Key_Enter) && (event->modifiers() == (Qt::KeypadModifier | Qt::ControlModifier))) {
        newlineAndIndentAP(); // newline-and-indent()
        return;
    } else if ((event->key() == Qt::Key_Return) && (event->modifiers() == (Qt::ShiftModifier))) {
        newlineNoIndentAP(); // newline-no-indent()
        return;
    } else if ((event->key() == Qt::Key_Enter) && (event->modifiers() == (Qt::KeypadModifier | Qt::ShiftModifier))) {
        newlineNoIndentAP(); // newline-no-indent()
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::NoModifier))) {
        beginningOfLineAP(); // process-home()
        return;
    } else if ((event->key() == Qt::Key_Backspace) && (event->modifiers() == (Qt::ControlModifier))) {
        deletePreviousWordAP(); // delete-previous-word()
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::NoModifier))) {
        endOfLineAP(); // end-of-line()
        return;
    } else if ((event->key() == Qt::Key_Delete) && (event->modifiers() == (Qt::NoModifier))) {
        deleteNextCharacterAP(); // delete-next-character()
        return;
    } else if ((event->key() == Qt::Key_Insert) && (event->modifiers() == (Qt::NoModifier))) {
        toggleOverstrikeAP(); // toggle-overstrike()
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ShiftModifier))) {
        processShiftUpAP(); // process-shift-up()
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ShiftModifier))) {
        processShiftDownAP(); // process-shift-down()
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ShiftModifier))) {
        keySelectAP(LeftFlag); // key-select(left)
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ShiftModifier))) {
        keySelectAP(RightFlag); // key-select(right)
        return;

#if Q_OS_MACOS
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::ShiftModifier | Qt::KeypadModifier))) {
        processShiftUpAP(); // process-shift-up()
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::ShiftModifier | Qt::KeypadModifier))) {
        processShiftDownAP(); // process-shift-down()
        return;
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::ShiftModifier | Qt::KeypadModifier))) {
        keySelectAP(LeftFlag); // key-select(left)
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::ShiftModifier | Qt::KeypadModifier))) {
        keySelectAP(RightFlag); // key-select(right)
        return;
#endif

    } else if ((event->key() == Qt::Key_Delete) && (event->modifiers() == (Qt::ShiftModifier))) {
        cutClipboardAP(); // cut-clipboard()
        return;
    } else if ((event->key() == Qt::Key_Insert) && (event->modifiers() == (Qt::ControlModifier))) {
        copyClipboardAP(); // copy-clipboard()
        return;
    } else if ((event->key() == Qt::Key_Insert) && (event->modifiers() == (Qt::ShiftModifier))) {
        pasteClipboardAP(); // paste-clipboard()
        return;
    } else if ((event->key() == Qt::Key_Insert) && (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) {
        copyPrimaryAP(); // copy-primary()
        return;
    } else if ((event->key() == Qt::Key_Home) && (event->modifiers() == (Qt::ControlModifier))) {
        beginningOfFileAP(); // begining-of-file()
        return;
    } else if ((event->key() == Qt::Key_End) && (event->modifiers() == (Qt::ControlModifier))) {
        endOfFileAP(); // end-of-file()
        return;
    } else if ((event->key() == Qt::Key_Left) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier))) {
        backwardWordAP(); // backward-word()
        return;
    } else if ((event->key() == Qt::Key_Right) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier))) {
        forwardWordAP(); // forward-word()
        return;
    } else if ((event->key() == Qt::Key_Up) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier))) {
        backwardParagraphAP(); // backward-paragraph()
        return;
    } else if ((event->key() == Qt::Key_Down) && ((event->modifiers() &~Qt::KeypadModifier) == (Qt::ControlModifier))) {
        forwardParagraphAP(); // forward-paragraph()
        return;
#if Q_OS_MACOS
    } else if ((event->key() == Qt::Key_Left) && (event->modifiers() == (Qt::ControlModifier | Qt::KeypadModifier))) {
        backwardWordAP(); // backward-word()
        return;
    } else if ((event->key() == Qt::Key_Right) && (event->modifiers() == (Qt::ControlModifier | Qt::KeypadModifier))) {
        forwardWordAP(); // forward-word()
        return;
    } else if ((event->key() == Qt::Key_Up) && (event->modifiers() == (Qt::ControlModifier | Qt::KeypadModifier))) {
        backwardParagraphAP(); // backward-paragraph()
        return;
    } else if ((event->key() == Qt::Key_Down) && (event->modifiers() == (Qt::ControlModifier | Qt::KeypadModifier))) {
        forwardParagraphAP(); // forward-paragraph()
        return;
#endif
    }

    // NOTE(eteran): these were added because Qt handles disabled shortcuts differently from Motif
    // In Motif, they are apparently still caught, but just do nothing, in Qt, it acts like
    // they don't exist, so they get sent to widgets lower in the chain (such as this one)
    // resulting in the suprising ability to type in some funny characters
    if(event->modifiers() == Qt::ControlModifier) {
        switch(event->key()) {
        case Qt::Key_W:
        case Qt::Key_X:
        case Qt::Key_D:
        case Qt::Key_Apostrophe:
        case Qt::Key_Period:
            QApplication::beep();
            return;
        }
    }

    // if the user prefers, we can hide the pointer during typing
    if(P_hidePointer) {
        setCursor(Qt::BlankCursor);
    }

    QString text = event->text();
    if(text.isEmpty()) {
        return;
    }

    selfInsertAP(text); // self-insert()
}

/**
 * @brief TextArea::mouseDoubleClickEvent
 * @param event
 */
void TextArea::mouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        if (!clickTracker(event, true)) {
            return;
        }

        selectWord(event->x());
        callCursorMovementCBs();
    }
}

/**
 * @brief TextArea::mouseTripleClickEvent
 * @param event
 */
void TextArea::mouseTripleClickEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		selectLine();
		callCursorMovementCBs();
	}
}

/**
 * @brief TextArea::mouseQuadrupleClickEvent
 * @param event
 */
void TextArea::mouseQuadrupleClickEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		buffer_->BufSelect(0, buffer_->BufGetLength());
	}
}

/**
 * "extend_adjust", "extend_adjust('rect')", "mouse_pan"
 *
 * @brief TextArea::mouseMoveEvent
 * @param event
 */
void TextArea::mouseMoveEvent(QMouseEvent *event) {

    if(P_hidePointer) {
        unsetCursor();
    }

	if(event->buttons() == Qt::LeftButton) {
		if(event->modifiers() & Qt::ControlModifier) {
			extendAdjustAP(event, RectFlag);
		} else {
			extendAdjustAP(event);
		}
	} else if(event->buttons() == Qt::RightButton) {
		mousePanAP(event);
	} else if(event->buttons() == Qt::MiddleButton) {

		if(event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
			secondaryOrDragAdjustAP(event, RectFlag | OverlayFlag | CopyFlag);
		} else if(event->modifiers() == (Qt::ShiftModifier)) {
			secondaryOrDragAdjustAP(event, CopyFlag);
		} else if(event->modifiers() == (Qt::ControlModifier)) {
			secondaryOrDragAdjustAP(event, RectFlag | OverlayFlag);
		} else {
			secondaryOrDragAdjustAP(event);
		}
	}
}

/**
 * "grab_focus", "extend_start", "extend_start('rect')", "mouse_pan"
 * @brief TextArea::mousePressEvent
 * @param event
 */
void TextArea::mousePressEvent(QMouseEvent *event) {

	if(event->button() == Qt::LeftButton) {
		switch(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
		case Qt::ShiftModifier | Qt::ControlModifier:
			extendStartAP(event, RectFlag);
            return;
		case Qt::ShiftModifier:
			extendStartAP(event);
            return;
		case Qt::ControlModifier | Qt::AltModifier:
        case Qt::ControlModifier | Qt::MetaModifier:
			moveDestinationAP(event);
            return;
		default:
			break;
		}

		/* Indicate state for future events, PRIMARY_CLICKED indicates that
		   the proper initialization has been done for primary dragging and/or
		   multi-clicking.  Also record the timestamp for multi-click processing */
		dragState_ = PRIMARY_CLICKED;

		// Check for possible multi-click sequence in progress
		if(!clickTracker(event, false)) {
			return;
		}

		// Clear any existing selections
        buffer_->BufUnselect();

		// Move the cursor to the pointer location
		moveDestinationAP(event);

		/* Record the site of the initial button press and the initial character
		   position so subsequent motion events and clicking can decide when and
		   where to begin a primary selection */
        btnDownCoord_ = event->pos();
        anchor_       = cursorPos_;

        int64_t row;
        int64_t column;
        TextDXYToUnconstrainedPosition(event->pos(), &row, &column);

        column      = TextDOffsetWrappedColumn(row, column);
		rectAnchor_ = column;

	} else if(event->button() == Qt::RightButton) {
		if(event->modifiers() == Qt::ControlModifier) {
			mousePanAP(event);
		}
	} else if(event->button() == Qt::MiddleButton) {
		secondaryOrDragStartAP(event);
	}
}

/**
 * "extend_end", "copy_to_or_end_drag", "end_drag"
 *
 * @brief TextArea::mouseReleaseEvent
 * @param event
 */
void TextArea::mouseReleaseEvent(QMouseEvent *event) {

	if(event->button() == Qt::LeftButton) {
		endDrag();
    } else if(event->button() == Qt::RightButton) {
		endDrag();
	} else if(event->button() == Qt::MiddleButton) {

		switch(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
		case Qt::ControlModifier:
			copyToOrEndDragAP(event, OverlayFlag);
            break;
		case Qt::MetaModifier:
        case Qt::AltModifier:
            exchangeAP(event);
            break;
		case Qt::ShiftModifier:
			moveToOrEndDragAP(event, CopyFlag);
            break;
		case Qt::ShiftModifier | Qt::ControlModifier:
			moveToOrEndDragAP(event, CopyFlag | OverlayFlag);
            break;
		default:
            copyToOrEndDragAP(event);
			break;
        }
	}
}

/**
 * @brief TextArea::paintEvent
 * @param event
 */
void TextArea::paintEvent(QPaintEvent *event) {

	QRect rect = event->rect();
	const int top    = rect.top();
	const int left   = rect.left();
	const int width  = rect.width();
	const int height = rect.height();

	// find the line number range of the display
    const int fontHeight = ascent_ + descent_;
    const int firstLine  = (top - rect_.top() - fontHeight + 1) / fontHeight;
    const int lastLine   = (top + height - rect_.top()) / fontHeight;

    QPainter painter(viewport());
    {
        painter.save();
        painter.setClipRect(QRect(rect_.left(), rect_.top(), rect_.width(), rect_.height() - rect_.height() % (ascent_ + descent_)));

        // draw the lines of text
        for (int line = firstLine; line <= lastLine; line++) {
            redisplayLine(&painter, line, left, left + width, 0, INT_MAX);
        }

        painter.restore();
    }

    {
        // Make sure we reset the clipping range for the line numbers
        painter.save();
        painter.setClipRect(QRect(lineNumLeft_, rect_.top(), lineNumWidth_, rect_.height()));

        // draw the line numbers if exposed area includes them
        if (lineNumWidth_ != 0 && left <= lineNumLeft_ + lineNumWidth_) {
            redrawLineNumbers(&painter);
        }

        painter.restore();
    }
}

/**
 * @brief TextArea::resizeEvent
 * @param event
 */
void TextArea::resizeEvent(QResizeEvent *event) {

    QFontMetrics fm(font_);
	int height           = event->size().height();
	int width            = event->size().width();
	int marginWidth      = P_marginWidth;
	int marginHeight     = P_marginHeight;
    int lineNumAreaWidth = (P_lineNumCols == 0) ? 0 : P_marginWidth + fm.maxWidth() * P_lineNumCols;

	P_columns = (width - marginWidth * 2 - lineNumAreaWidth) / fm.maxWidth();
    P_rows    = (height - marginHeight * 2) / (ascent_ + descent_);

	// Resize the text display that the widget uses to render text
	TextDResize(width - marginWidth * 2 - lineNumAreaWidth, height - marginHeight * 2);
}

/**
 * @brief TextArea::bufPreDeleteCallback
 * @param pos
 * @param nDeleted
 */
void TextArea::bufPreDeleteCallback(int64_t pos, int64_t nDeleted) {
	if (P_continuousWrap && (fixedFontWidth_ == -1 || modifyingTabDist_)) {
		/* Note: we must perform this measurement, even if there is not a
		   single character deleted; the number of "deleted" lines is the
		   number of visual lines spanned by the real line in which the
		   modification takes place.
		   Also, a modification of the tab distance requires the same
		   kind of calculations in advance, even if the font width is "fixed",
		   because when the width of the tab characters changes, the layout
		   of the text may be completely different. */
		measureDeletedLines(pos, nDeleted);
	} else {
		suppressResync_ = false; // Probably not needed, but just in case
	}
}

/**
 * @brief TextArea::bufModifiedCallback
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param nRestyled
 * @param deletedText
 */
void TextArea::bufModifiedCallback(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText) {
    int64_t linesInserted;
    int64_t linesDeleted;
    int64_t startDispPos;
    int64_t endDispPos;
    int64_t oldFirstChar = firstChar_;
	int scrolled;
    int64_t origCursorPos = cursorPos_;
    int64_t wrapModStart = 0;
    int64_t wrapModEnd   = 0;

	// buffer modification cancels vertical cursor motion column
    if (nInserted != 0 || nDeleted != 0) {
		cursorPreferredCol_ = -1;
    }

	/* Count the number of lines inserted and deleted, and in the case
	   of continuous wrap mode, how much has changed */
	if (P_continuousWrap) {
		findWrapRangeEx(deletedText, pos, nInserted, nDeleted, &wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);
	} else {
        linesInserted = (nInserted == 0) ? 0 : buffer_->BufCountLines(pos, pos + nInserted);
        linesDeleted  = (nDeleted  == 0) ? 0 : countLinesEx(deletedText);
	}

	// Update the line starts and topLineNum
	if (nInserted != 0 || nDeleted != 0) {
		if (P_continuousWrap) {
            updateLineStarts(wrapModStart, wrapModEnd - wrapModStart, nDeleted + pos - wrapModStart + (wrapModEnd - (pos + nInserted)), linesInserted, linesDeleted, &scrolled);
		} else {
            updateLineStarts(pos, nInserted, nDeleted, linesInserted, linesDeleted, &scrolled);
		}
	} else {
		scrolled = false;
	}

	/* If we're counting non-wrapped lines as well, maintain the absolute
	   (non-wrapped) line number of the text displayed */
	if (maintainingAbsTopLineNum() && (nInserted != 0 || nDeleted != 0)) {
        if (pos + nDeleted < oldFirstChar) {
			absTopLineNum_ = absTopLineNum_ + buffer_->BufCountLines(pos, pos + nInserted) - countLinesEx(deletedText);
        } else if (pos < oldFirstChar) {
			resetAbsLineNum();
        }
	}

	// Update the line count for the whole buffer
	nBufferLines_ = (nBufferLines_ + linesInserted - linesDeleted);

	/* Update the scroll bar ranges (and value if the value changed).  Note
	   that updating the horizontal scroll bar range requires scanning the
	   entire displayed text, however, it doesn't seem to hurt performance
	   much.  Note also, that the horizontal scroll bar update routine is
	   allowed to re-adjust horizOffset if there is blank space to the right
	   of all lines of text. */
	updateVScrollBarRange();
	scrolled |= updateHScrollBarRange();

	// Update the cursor position
	if (cursorToHint_ != NO_HINT) {
		cursorPos_ = cursorToHint_;
		cursorToHint_ = NO_HINT;
	} else if (cursorPos_ > pos) {
		if (cursorPos_ < pos + nDeleted) {
			cursorPos_ = pos;
		} else {
			cursorPos_ = (cursorPos_ + nInserted - nDeleted);
		}
	}

	// If the changes caused scrolling, re-paint everything and we're done.
	if (scrolled) {
		blankCursorProtrusions();
        TextDRedisplayRect(0, rect_.top(), rect_.width() + rect_.left(), rect_.height());
		if (styleBuffer_) { // See comments in extendRangeForStyleMods
            styleBuffer_->BufGetPrimary().selected = false;
            styleBuffer_->BufGetPrimary().zeroWidth = false;
		}
		return;
	}

	/* If the changes didn't cause scrolling, decide the range of characters
	   that need to be re-painted.  Also if the cursor position moved, be
	   sure that the redisplay range covers the old cursor position so the
	   old cursor gets erased, and erase the bits of the cursor which extend
	   beyond the left and right edges of the text. */
	startDispPos = P_continuousWrap ? wrapModStart : pos;
	if (origCursorPos == startDispPos && cursorPos_ != startDispPos)
		startDispPos = std::min(startDispPos, origCursorPos - 1);
	if (linesInserted == linesDeleted) {
		if (nInserted == 0 && nDeleted == 0) {
			endDispPos = pos + nRestyled;
		} else {
			endDispPos = P_continuousWrap ? wrapModEnd : buffer_->BufEndOfLine(pos + nInserted) + 1;
			if (origCursorPos >= startDispPos && (origCursorPos <= endDispPos || endDispPos == buffer_->BufGetLength())) {
				blankCursorProtrusions();
			}
		}
		/* If more than one line is inserted/deleted, a line break may have
		   been inserted or removed in between, and the line numbers may
		   have changed. If only one line is altered, line numbers cannot
		   be affected (the insertion or removal of a line break always
		   results in at least two lines being redrawn). */
		if (linesInserted > 1) {
            redrawLineNumbersEx();
		}
	} else { // linesInserted != linesDeleted
		endDispPos = lastChar_ + 1;
		if (origCursorPos >= pos) {
			blankCursorProtrusions();
		}

        redrawLineNumbersEx();
	}

	/* If there is a style buffer, check if the modification caused additional
	   changes that need to be redisplayed.  (Redisplaying separately would
	   cause double-redraw on almost every modification involving styled
	   text).  Extend the redraw range to incorporate style changes */
	if (styleBuffer_) {
		extendRangeForStyleMods(&startDispPos, &endDispPos);
	}

	// Redisplay computed range
	textDRedisplayRange(startDispPos, endDispPos);
}

/**
 * @brief TextArea::setBacklightCharTypes
 * @param charTypes
 */
void TextArea::setBacklightCharTypes(const QString &charTypes) {
	TextDSetupBGClassesEx(charTypes);
	viewport()->update();
}

/**
 * @brief TextArea::hideOrShowHScrollBar
 */
void TextArea::hideOrShowHScrollBar() {
    QFontMetrics fm(font_);
    if (P_continuousWrap && (P_wrapMargin == 0 || P_wrapMargin * fm.maxWidth() < rect_.width())) {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	} else {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	}
}


/**
 * This is a stripped-down version of the findWrapRange() function above,
 * intended to be used to calculate the number of "deleted" lines during a
 * buffer modification. It is called _before_ the modification takes place.
 *
 * This function should only be called in continuous wrap mode with a non-fixed
 * font width. In that case, it is impossible to calculate the number of
 * deleted lines, because the necessary style information is no longer
 * available _after_ the modification. In other cases, we can still perform the
 * calculation afterwards (possibly even more efficiently).
 *
 * @brief TextArea::measureDeletedLines
 * @param pos
 * @param nDeleted
 */
void TextArea::measureDeletedLines(int64_t pos, int64_t nDeleted) {
    int64_t retPos;
    int64_t retLines;
    int64_t retLineStart;
    int64_t retLineEnd;
    int nVisLines = nVisibleLines_;
    int64_t countFrom;
	int nLines = 0;
	/*
	** Determine where to begin searching: either the previous newline, or
	** if possible, limit to the start of the (original) previous displayed
	** line, using information from the existing line starts array
	*/
	if (pos >= firstChar_ && pos <= lastChar_) {
		int i;
        for (i = nVisLines - 1; i > 0; i--) {
            if (lineStarts_[i] != -1 && pos >= lineStarts_[i]) {
				break;
            }
        }

		if (i > 0) {
            countFrom = lineStarts_[i - 1];
        } else {
			countFrom = buffer_->BufStartOfLine(pos);
        }
    } else {
		countFrom = buffer_->BufStartOfLine(pos);
    }

	/*
	** Move forward through the (new) text one line at a time, counting
	** displayed lines, and looking for either a real newline, or for the
	** line starts to re-sync with the original line starts array
	*/
    int64_t lineStart = countFrom;
	while (true) {
		/* advance to the next line.  If the line ended in a real newline
           or the end of the buffer, that's far enough */
		wrappedLineCounter(buffer_, lineStart, buffer_->BufGetLength(), 1, true, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retPos >= buffer_->BufGetLength()) {
            if (retPos != retLineEnd) {
				nLines++;
            }
			break;
        } else {
			lineStart = retPos;
        }

		nLines++;
		if (lineStart > pos + nDeleted && buffer_->BufGetCharacter(lineStart - 1) == '\n') {
			break;
		}

		/* Unlike in the findWrapRange() function above, we don't try to
		   resync with the line starts, because we don't know the length
		   of the inserted text yet, nor the updated style information.

		   Because of that, we also shouldn't resync with the line starts
		   after the modification either, because we must perform the
		   calculations for the deleted and inserted lines in the same way.

		   This can result in some unnecessary recalculation and redrawing
		   overhead, and therefore we should only use this two-phase mode
		   of calculation when it's really needed (continuous wrap + variable
		   font width). */
	}

	nLinesDeleted_ = nLines;
	suppressResync_ = true;
}

/*
** Count forward from startPos to either maxPos or maxLines (whichever is
** reached first), and return all relevant positions and line count.
** The provided TextBuffer may differ from the actual text buffer of the
** widget. In that case it must be a (partial) copy of the actual text buffer
** and the styleBufOffset argument must indicate the starting position of the
** copy, to take into account the correct style information.
**
** Returned values:
**
**   retPos:	    Position where counting ended.  When counting lines, the
**  	    	    position returned is the start of the line "maxLines"
**  	    	    lines beyond "startPos".
**   retLines:	    Number of line breaks counted
**   retLineStart:  Start of the line where counting ended
**   retLineEnd:    End position of the last line traversed
*/
void TextArea::wrappedLineCounter(const TextBuffer *buf, int64_t startPos, int64_t maxPos, int64_t maxLines, bool startPosIsLineStart, int64_t styleBufOffset, int64_t *retPos, int64_t *retLines, int64_t *retLineStart, int64_t *retLineEnd) const {
    int64_t lineStart;
    int64_t newLineStart = 0;
    int64_t b;
	int wrapMargin;
	int maxWidth;
	int countPixels;
    int64_t i;
	int foundBreak;
	int nLines = 0;
    int tabDist = buffer_->BufGetTabDist();


	/* If the font is fixed, or there's a wrap margin set, it's more efficient
	   to measure in columns, than to count pixels.  Determine if we can count
       in columns (countPixels == false) or must count pixels (countPixels ==
       true), and set the wrap target for either pixels or columns */
	if (fixedFontWidth_ != -1 || P_wrapMargin != 0) {
		countPixels = false;
        wrapMargin = P_wrapMargin != 0 ? P_wrapMargin : rect_.width() / fixedFontWidth_;
		maxWidth = INT_MAX;
	} else {
		countPixels = true;
		wrapMargin = INT_MAX;
        maxWidth = rect_.width();
	}

	/* Find the start of the line if the start pos is not marked as a
	   line start. */
    if (startPosIsLineStart) {
		lineStart = startPos;
    } else {
		lineStart = TextDStartOfLine(startPos);
    }

	/*
	** Loop until position exceeds maxPos or line count exceeds maxLines.
	** (actually, contines beyond maxPos to end of line containing maxPos,
	** in case later characters cause a word wrap back before maxPos)
	*/
    int64_t colNum = 0;
    int64_t width = 0;
    for (int64_t p = lineStart; p < buf->BufGetLength(); p++) {
        const char c = buf->BufGetCharacter(p);

		/* If the character was a newline, count the line and start over,
		   otherwise, add it to the width and column counts */
		if (c == '\n') {
			if (p >= maxPos) {
				*retPos = maxPos;
				*retLines = nLines;
				*retLineStart = lineStart;
				*retLineEnd = maxPos;
				return;
			}
			nLines++;
			if (nLines >= maxLines) {
				*retPos = p + 1;
				*retLines = nLines;
				*retLineStart = p + 1;
				*retLineEnd = p;
				return;
			}
			lineStart = p + 1;
			colNum = 0;
			width = 0;
		} else {
            colNum += TextBuffer::BufCharWidth(c, colNum, tabDist);
			if (countPixels)
				width += measurePropChar(c, colNum, p + styleBufOffset);
		}

		/* If character exceeded wrap margin, find the break point
		   and wrap there */
		if (colNum > wrapMargin || width > maxWidth) {
			foundBreak = false;
			for (b = p; b >= lineStart; b--) {
                const char c = buf->BufGetCharacter(b);
				if (c == '\t' || c == ' ') {
					newLineStart = b + 1;
					if (countPixels) {
						colNum = 0;
						width = 0;
						for (i = b + 1; i < p + 1; i++) {
							width += measurePropChar(buf->BufGetCharacter(i), colNum, i + styleBufOffset);
							colNum++;
						}
					} else
						colNum = buf->BufCountDispChars(b + 1, p + 1);
					foundBreak = true;
					break;
				}
			}

			if (!foundBreak) { // no whitespace, just break at margin
				newLineStart = std::max(p, lineStart + 1);
                colNum = TextBuffer::BufCharWidth(c, colNum, tabDist);
				if (countPixels)
					width = measurePropChar(c, colNum, p + styleBufOffset);
			}

			if (p >= maxPos) {
				*retPos = maxPos;
				*retLines = maxPos < newLineStart ? nLines : nLines + 1;
				*retLineStart = maxPos < newLineStart ? lineStart : newLineStart;
				*retLineEnd = maxPos;
				return;
			}

			nLines++;
			if (nLines >= maxLines) {
				*retPos = foundBreak ? b + 1 : std::max(p, lineStart + 1);
				*retLines = nLines;
				*retLineStart = lineStart;
				*retLineEnd = foundBreak ? b : p;
				return;
			}

			lineStart = newLineStart;
		}
	}

	// reached end of buffer before reaching pos or line target
	*retPos       = buf->BufGetLength();
	*retLines     = nLines;
	*retLineStart = lineStart;
	*retLineEnd   = buf->BufGetLength();
}

/*
** Measure the width in pixels of a character "c" at a particular column
** "colNum" and buffer position "pos".  This is for measuring characters in
** proportional or mixed-width highlighting fonts.
**
** A note about proportional and mixed-width fonts: the mixed width and
** proportional font code in nedit does not get much use in general editing,
** because nedit doesn't allow per-language-mode fonts, and editing programs
** in a proportional font is usually a bad idea, so very few users would
** choose a proportional font as a default.  There are still probably mixed-
** width syntax highlighting cases where things don't redraw properly for
** insertion/deletion, though static display and wrapping and resizing
** should now be solid because they are now used for online help display.
*/
int64_t TextArea::measurePropChar(char ch, int64_t colNum, int64_t pos) const {
    int style;
    char expChar[TextBuffer::MAX_EXP_CHAR_LEN];
    const std::shared_ptr<TextBuffer> &styleBuf = styleBuffer_;

    int64_t charLen = TextBuffer::BufExpandCharacter(ch, colNum, expChar, buffer_->BufGetTabDist());

	if(!styleBuf) {
		style = 0;
	} else {
        style = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
		if (style == unfinishedStyle_) {
			// encountered "unfinished" style, trigger parsing
			(unfinishedHighlightCB_)(this, pos, highlightCBArg_);
            style = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
		}
	}

	return stringWidth(expChar, charLen, style);
}

/*
** Find the width of a string in the font of a particular style
*/
int64_t TextArea::stringWidth(const char *string, int64_t length, int style) const {

    QString str = asciiToUnicode(string, length);

	if (style & STYLE_LOOKUP_MASK) {
        QFontMetrics fm(styleTable_[(style & STYLE_LOOKUP_MASK) - ASCII_A].font);
        return fm.width(str);
	} else {
        QFontMetrics fm(font_);
        return fm.width(str);
	}
}

/*
** Same as BufStartOfLine, but returns the character after last wrap point
** rather than the last newline.
*/
int64_t TextArea::TextDStartOfLine(int64_t pos) const {
    int64_t retLines;
    int64_t retPos;
    int64_t retLineStart;
    int64_t retLineEnd;

	// If we're not wrapping, use the more efficient BufStartOfLine
	if (!P_continuousWrap) {
		return buffer_->BufStartOfLine(pos);
	}

    wrappedLineCounter(buffer_, buffer_->BufStartOfLine(pos), pos, INT_MAX, true, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLineStart;
}

/**
 * When continuous wrap is on, and the user inserts or deletes characters,
 * wrapping can happen before and beyond the changed position.  This routine
 * finds the extent of the changes, and counts the deleted and inserted lines
 * over that range.  It also attempts to minimize the size of the range to what
 * has to be counted and re-displayed, so the results can be useful both for
 * delimiting where the line starts need to be recalculated, and for deciding
 * what part of the text to redisplay.
 *
 * @brief TextArea::findWrapRangeEx
 * @param deletedText
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param modRangeStart
 * @param modRangeEnd
 * @param linesInserted
 * @param linesDeleted
 */
void TextArea::findWrapRangeEx(view::string_view deletedText, int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t *modRangeStart, int64_t *modRangeEnd, int64_t *linesInserted, int64_t *linesDeleted) {

    int64_t retPos;
    int64_t retLines;
    int64_t retLineStart;
    int64_t retLineEnd;	
    int64_t countFrom;
    int64_t countTo;
    int64_t lineStart;
    int64_t adjLineStart;
	int visLineNum = 0;
    int nLines     = 0;
    int nVisLines  = nVisibleLines_;

	/*
	** Determine where to begin searching: either the previous newline, or
	** if possible, limit to the start of the (original) previous displayed
	** line, using information from the existing line starts array
	*/
	if (pos >= firstChar_ && pos <= lastChar_) {
		int i;
        for (i = nVisLines - 1; i > 0; i--) {
            if (lineStarts_[i] != -1 && pos >= lineStarts_[i]) {
				break;
            }
        }

		if (i > 0) {
            countFrom = lineStarts_[i - 1];
			visLineNum = i - 1;
        } else {
			countFrom = buffer_->BufStartOfLine(pos);
        }
    } else {
		countFrom = buffer_->BufStartOfLine(pos);
    }

	/*
	** Move forward through the (new) text one line at a time, counting
	** displayed lines, and looking for either a real newline, or for the
	** line starts to re-sync with the original line starts array
	*/
	lineStart = countFrom;
	*modRangeStart = countFrom;
	while (true) {

		/* advance to the next line.  If the line ended in a real newline
		   or the end of the buffer, that's far enough */
		wrappedLineCounter(buffer_, lineStart, buffer_->BufGetLength(), 1, true, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retPos >= buffer_->BufGetLength()) {
			countTo = buffer_->BufGetLength();
			*modRangeEnd = countTo;
            if (retPos != retLineEnd) {
				nLines++;
            }
			break;
        } else {
			lineStart = retPos;
        }

		nLines++;
		if (lineStart > pos + nInserted && buffer_->BufGetCharacter(lineStart - 1) == '\n') {
			countTo = lineStart;
			*modRangeEnd = lineStart;
			break;
		}

        /*
         * Don't try to resync in continuous wrap mode with non-fixed font
         * sizes; it would result in a chicken-and-egg dependency between the
         * calculations for the inserted and the deleted lines. If we're in
         * that mode, the number of deleted lines is calculated in advance,
         * without resynchronization, so we shouldn't resynchronize for the
         * inserted lines either. */
		if (suppressResync_) {
			continue;
		}

		/* check for synchronization with the original line starts array
		   before pos, if so, the modified range can begin later */
		if (lineStart <= pos) {
            while (visLineNum < nVisLines && lineStarts_[visLineNum] < lineStart) {
				visLineNum++;
            }

            if (visLineNum < nVisLines && lineStarts_[visLineNum] == lineStart) {
				countFrom = lineStart;
				nLines = 0;
                if (visLineNum + 1 < nVisLines && lineStarts_[visLineNum + 1] != -1) {
                    *modRangeStart = std::min(pos, lineStarts_[visLineNum + 1] - 1);
                } else {
					*modRangeStart = countFrom;
                }
            } else {
				*modRangeStart = std::min(*modRangeStart, lineStart - 1);
            }
		}

        /* check for synchronization with the original line starts array after
         * pos, if so, the modified range can end early */
		else if (lineStart > pos + nInserted) {
			adjLineStart = lineStart - nInserted + nDeleted;
            while (visLineNum < nVisLines && lineStarts_[visLineNum] < adjLineStart) {
				visLineNum++;
            }

            if (visLineNum < nVisLines && lineStarts_[visLineNum] != -1 && lineStarts_[visLineNum] == adjLineStart) {
				countTo = TextDEndOfLine(lineStart, true);
				*modRangeEnd = lineStart;
				break;
			}
		}
	}
	*linesInserted = nLines;

	/* Count deleted lines between countFrom and countTo as the text existed
	   before the modification (that is, as if the text between pos and
	   pos+nInserted were replaced by "deletedText").  This extra context is
	   necessary because wrapping can occur outside of the modified region
	   as a result of adding or deleting text in the region. This is done by
	   creating a TextBuffer containing the deleted text and the necessary
	   additional context, and calling the wrappedLineCounter on it.

	   NOTE: This must not be done in continuous wrap mode when the font
		 width is not fixed. In that case, the calculation would try
		 to access style information that is no longer available (deleted
		 text), or out of date (updated highlighting), possibly leading
		 to completely wrong calculations and/or even crashes eventually.
		 (This is not theoretical; it really happened.)

		 In that case, the calculation of the number of deleted lines
		 has happened before the buffer was modified (only in that case,
		 because resynchronization of the line starts is impossible
		 in that case, which makes the whole calculation less efficient).
	*/
	if (suppressResync_) {
		*linesDeleted = nLinesDeleted_;
		suppressResync_ = false;
		return;
	}

    int64_t length = (pos - countFrom) + nDeleted + (countTo - (pos + nInserted));
    TextBuffer deletedTextBuf;
    if (pos > countFrom) {
        deletedTextBuf.BufCopyFromBuf(buffer_, countFrom, pos, 0);
    }

    if (nDeleted != 0) {
        deletedTextBuf.BufInsertEx(pos - countFrom, deletedText);
    }

    if (countTo > pos + nInserted) {
        deletedTextBuf.BufCopyFromBuf(buffer_, pos + nInserted, countTo, pos - countFrom + nDeleted);
    }

    /* Note that we need to take into account an offset for the style buffer:
     * the deletedTextBuf can be out of sync with the style buffer. */
    wrappedLineCounter(&deletedTextBuf, 0, length, INT_MAX, true, countFrom, &retPos, &retLines, &retLineStart, &retLineEnd);

	*linesDeleted = retLines;
	suppressResync_ = false;
}

/*
** Same as BufEndOfLine, but takes in to account line breaks when wrapping
** is turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as true to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
**
** Note that the definition of the end of a line is less clear when continuous
** wrap is on.  With continuous wrap off, it's just a pointer to the newline
** that ends the line.  When it's on, it's the character beyond the last
** DISPLAYABLE character on the line, where a whitespace character which has
** been "converted" to a newline for wrapping is not considered displayable.
** Also note that, a line can be wrapped at a non-whitespace character if the
** line had no whitespace.  In this case, this routine returns a pointer to
** the start of the next line.  This is also consistent with the model used by
** visLineLength.
*/
int64_t TextArea::TextDEndOfLine(int64_t pos, bool startPosIsLineStart) const {
    int64_t retLines;
    int64_t retPos;
    int64_t retLineStart;
    int64_t retLineEnd;

	// If we're not wrapping use more efficient BufEndOfLine
	if (!P_continuousWrap) {
		return buffer_->BufEndOfLine(pos);
	}

	if (pos == buffer_->BufGetLength()) {
		return pos;
	}

	wrappedLineCounter(buffer_, pos, buffer_->BufGetLength(), 1, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLineEnd;
}

/*
** Update the line starts array, topLineNum, firstChar and lastChar after a
** modification to the text buffer, given by the position where the change
** began "pos", and the nmubers of characters and lines inserted and deleted.
*/
void TextArea::updateLineStarts(int64_t pos, int64_t charsInserted, int64_t charsDeleted, int64_t linesInserted, int64_t linesDeleted, int *scrolled) {

    int lineOfPos;
    int lineOfEnd;
    int nVisLines = nVisibleLines_;    
    int64_t charDelta = charsInserted - charsDeleted;
    int64_t lineDelta = linesInserted - linesDeleted;

	/* If all of the changes were before the displayed text, the display
	   doesn't change, just update the top line num and offset the line
	   start entries and first and last characters */
	if (pos + charsDeleted < firstChar_) {
		topLineNum_ += lineDelta;
        for (int i = 0; i < nVisLines && lineStarts_[i] != -1; i++) {
            lineStarts_[i] += charDelta;
        }

		firstChar_ += charDelta;
		lastChar_ += charDelta;
		*scrolled = false;
		return;
	}

	/* The change began before the beginning of the displayed text, but
	   part or all of the displayed text was deleted */
	if (pos < firstChar_) {
		// If some text remains in the window, anchor on that
        if (posToVisibleLineNum(pos + charsDeleted, &lineOfEnd) && ++lineOfEnd < nVisLines && lineStarts_[lineOfEnd] != -1) {
            topLineNum_ = std::max<int64_t>(1, topLineNum_ + lineDelta);
            firstChar_ = TextDCountBackwardNLines(lineStarts_[lineOfEnd] + charDelta, lineOfEnd);
			// Otherwise anchor on original line number and recount everything
		} else {
			if (topLineNum_ > nBufferLines_ + lineDelta) {
				topLineNum_ = 1;
				firstChar_ = 0;
			} else
				firstChar_ = TextDCountForwardNLines(0, topLineNum_ - 1, true);
		}
		calcLineStarts(0, nVisLines - 1);

		// calculate lastChar by finding the end of the last displayed line
		calcLastChar();
		*scrolled = true;
		return;
	}

	/* If the change was in the middle of the displayed text (it usually is),
	   salvage as much of the line starts array as possible by moving and
	   offsetting the entries after the changed area, and re-counting the
	   added lines or the lines beyond the salvaged part of the line starts
	   array */
	if (pos <= lastChar_) {
		// find line on which the change began
		posToVisibleLineNum(pos, &lineOfPos);
		// salvage line starts after the changed area
		if (lineDelta == 0) {
            for (int i = lineOfPos + 1; i < nVisLines && lineStarts_[i] != -1; i++)
                lineStarts_[i] += charDelta;
		} else if (lineDelta > 0) {
            for (int64_t i = nVisLines - 1; i >= lineOfPos + lineDelta + 1; i--)
                lineStarts_[i] = lineStarts_[i - lineDelta] + (lineStarts_[i - lineDelta] == -1 ? 0 : charDelta);
		} else /* (lineDelta < 0) */ {
            for (int i = std::max(0, lineOfPos + 1); i < nVisLines + lineDelta; i++)
                lineStarts_[i] = lineStarts_[i - lineDelta] + (lineStarts_[i - lineDelta] == -1 ? 0 : charDelta);
		}

		// fill in the missing line starts
		if (linesInserted >= 0)
			calcLineStarts(lineOfPos + 1, lineOfPos + linesInserted);
		if (lineDelta < 0)
			calcLineStarts(nVisLines + lineDelta, nVisLines);

		// calculate lastChar by finding the end of the last displayed line
		calcLastChar();
		*scrolled = false;
		return;
	}

	/* Change was past the end of the displayed text, but displayable by virtue
	   of being an insert at the end of the buffer into visible blank lines */
	if (emptyLinesVisible()) {
		posToVisibleLineNum(pos, &lineOfPos);
		calcLineStarts(lineOfPos, lineOfPos + linesInserted);
		calcLastChar();

		*scrolled = false;
		return;
	}

	// Change was beyond the end of the buffer and not visible, do nothing
	*scrolled = false;
}

/*
** Scan through the text in the buffer and recalculate the line
** starts array values beginning at index "startLine" and continuing through
** (including) "endLine".  It assumes that the line starts entry preceding
** "startLine" (or firstChar if startLine is 0) is good, and re-counts
** newlines to fill in the requested entries.  Out of range values for
** "startLine" and "endLine" are acceptable.
*/
void TextArea::calcLineStarts(int startLine, int endLine) {

    int64_t bufLen = buffer_->BufGetLength();    
    int64_t lineEnd;
    int64_t nextLineStart;
	int nVis = nVisibleLines_;

	// Clean up (possibly) messy input parameters
    if (nVis == 0) {
		return;
    }

    endLine   = qBound(0, endLine, nVis - 1);
    startLine = qBound(0, startLine, nVis - 1);

    if (startLine > endLine) {
		return;
    }

	// Find the last known good line number -> position mapping
	if (startLine == 0) {
        lineStarts_[0] = firstChar_;
		startLine = 1;
	}

    int64_t startPos = lineStarts_[startLine - 1];
    int line;

	/* If the starting position is already past the end of the text,
	   fill in -1's (means no text on line) and return */
	if (startPos == -1) {
        for (line = startLine; line <= endLine; line++) {
            lineStarts_[line] = -1;
        }

		return;
	}

	/* Loop searching for ends of lines and storing the positions of the
	   start of the next line in lineStarts */
	for (line = startLine; line <= endLine; line++) {
        findLineEnd(startPos, true, &lineEnd, &nextLineStart);
		startPos = nextLineStart;
		if (startPos >= bufLen) {
			/* If the buffer ends with a newline or line break, put
			   buf->BufGetLength() in the next line start position (instead of
			   a -1 which is the normal marker for an empty line) to
			   indicate that the cursor may safely be displayed there */
            if (line == 0 || (lineStarts_[line - 1] != bufLen && lineEnd != nextLineStart)) {
                lineStarts_[line] = bufLen;
				line++;
			}
			break;
		}
        lineStarts_[line] = startPos;
	}

	// Set any entries beyond the end of the text to -1
    for (; line <= endLine; line++) {
        lineStarts_[line] = -1;
    }
}

/*
** Given a TextDisplay with a complete, up-to-date lineStarts array, update
** the lastChar entry to point to the last buffer position displayed.
*/
void TextArea::calcLastChar() {
	int i;

	for (i = nVisibleLines_ - 1; i > 0 && lineStarts_[i] == -1; i--) {
		;
	}

    lastChar_ = (i < 0) ? 0 : TextDEndOfLine(lineStarts_[i], true);
}

/*
** Same as BufCountBackwardNLines, but takes in to account line breaks when
** wrapping is turned on.
*/
int64_t TextArea::TextDCountBackwardNLines(int64_t startPos, int64_t nLines) const {

    int64_t retLines;
    int64_t retPos;
    int64_t retLineStart;
    int64_t retLineEnd;

	// If we're not wrapping, use the more efficient BufCountBackwardNLines
	if (!P_continuousWrap) {
		return buffer_->BufCountBackwardNLines(startPos, nLines);
	}

    int64_t pos = startPos;
	while (true) {
        int64_t lineStart = buffer_->BufStartOfLine(pos);
        wrappedLineCounter(buffer_, lineStart, pos, INT_MAX, true, 0, &retPos, &retLines, &retLineStart, &retLineEnd);

        if (retLines > nLines) {
			return TextDCountForwardNLines(lineStart, retLines - nLines, true);
        }

		nLines -= retLines;
        pos = lineStart - 1;

        if (pos < 0) {
			return 0;
        }

		nLines -= 1;
	}
}

/*
** Return true if a separate absolute top line number is being maintained
** (for displaying line numbers or showing in the statistics line).
*/
bool TextArea::maintainingAbsTopLineNum() const {
	return P_continuousWrap && (lineNumWidth_ != 0 || needAbsTopLineNum_);
}

/*
** Count lines from the beginning of the buffer to reestablish the
** absolute (non-wrapped) top line number.  If mode is not continuous wrap,
** or the number is not being maintained, does nothing.
*/
void TextArea::resetAbsLineNum() {
	absTopLineNum_ = 1;
	offsetAbsLineNum(0);
}

/*
** Refresh a rectangle of the text display.  left and top are in coordinates of
** the text drawing window
*/
void TextArea::TextDRedisplayRect(const QRect &rect) {
    TextDRedisplayRect(rect.left(), rect.top(), rect.width(), rect.height());
}

/**
 * @brief TextArea::TextDRedisplayRect
 * @param left
 * @param top
 * @param width
 * @param height
 */
void TextArea::TextDRedisplayRect(int left, int top, int width, int height) {
	viewport()->update(QRect(left, top, width, height));
}

/**
 * Update the minimum, maximum, slider size, page increment, and value for
 * vertical scroll bar.
 *
 * @brief TextArea::updateVScrollBarRange
 */
void TextArea::updateVScrollBarRange() {

	// NOTE(eteran): so Qt's scrollbars are a little more straightforward than
	// Motif's so we've had to make some minor changes here to avoid having to
	// special case everywhere else.
	// Additionally, as an optimization, if we aren't in continuous wrap mode,
	// there is no need to use the approximation techniques
    int64_t sliderValue = topLineNum_;

	/* The Vert. scroll bar value and slider size directly represent the top
	   line number, and the number of visible lines respectively.  The scroll
	   bar maximum value is chosen to generally represent the size of the whole
	   buffer, with minor adjustments to keep the scroll bar widget happy */
	if(P_continuousWrap) {
        int sliderSize = std::max(nVisibleLines_, 1);
        auto sliderMax = std::max<int64_t>(nBufferLines_ + 2 + P_cursorVPadding, sliderSize + sliderValue);

		verticalScrollBar()->setMinimum(0);
        verticalScrollBar()->setMaximum(gsl::narrow<int>(sliderMax));
        verticalScrollBar()->setPageStep(std::max(1, nVisibleLines_ - 1));
        verticalScrollBar()->setValue(gsl::narrow<int>(sliderValue));
	} else {
		verticalScrollBar()->setMinimum(0);
        verticalScrollBar()->setMaximum(gsl::narrow<int>(std::max<int64_t>(0, nBufferLines_ - nVisibleLines_ + 2)));
        verticalScrollBar()->setPageStep(std::max(1, nVisibleLines_ - 1));
        verticalScrollBar()->setValue(gsl::narrow<int>(sliderValue));
	}
}

/**
 * Same as BufCountForwardNLines, but takes in to account line breaks when
 * wrapping is turned on. If the caller knows that startPos is at a line start,
 * it can pass "startPosIsLineStart" as true to make the call more efficient by
 * avoiding the additional step of scanning back to the last newline.
 *
 * @brief TextArea::TextDCountForwardNLines
 * @param startPos
 * @param nLines
 * @param startPosIsLineStart
 * @return
 */
int64_t TextArea::TextDCountForwardNLines(int64_t startPos, int64_t nLines, bool startPosIsLineStart) const {

    int64_t retLines;
    int64_t retPos;
    int64_t retLineStart;
    int64_t retLineEnd;

	// if we're not wrapping use more efficient BufCountForwardNLines
	if (!P_continuousWrap) {
		return buffer_->BufCountForwardNLines(startPos, nLines);
	}

	// wrappedLineCounter can't handle the 0 lines case
	if (nLines == 0) {
		return startPos;
	}

	// use the common line counting routine to count forward
	wrappedLineCounter(buffer_, startPos, buffer_->BufGetLength(), nLines, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retPos;
}

/*
** Re-calculate absolute top line number for a change in scroll position.
*/
void TextArea::offsetAbsLineNum(int64_t oldFirstChar) {
	if (maintainingAbsTopLineNum()) {
		if (firstChar_ < oldFirstChar) {
			absTopLineNum_ -= buffer_->BufCountLines(firstChar_, oldFirstChar);
		} else {
			absTopLineNum_ += buffer_->BufCountLines(oldFirstChar, firstChar_);
		}
	}
}

/*
** Finds both the end of the current line and the start of the next line.  Why?
** In continuous wrap mode, if you need to know both, figuring out one from the
** other can be expensive or error prone.  The problem comes when there's a
** trailing space or tab just before the end of the buffer.  To translate an
** end of line value to or from the next lines start value, you need to know
** whether the trailing space or tab is being used as a line break or just a
** normal character, and to find that out would otherwise require counting all
** the way back to the beginning of the line.
*/
void TextArea::findLineEnd(int64_t startPos, int64_t startPosIsLineStart, int64_t *lineEnd, int64_t *nextLineStart) {
    int64_t retLines;
    int64_t retLineStart;

	// if we're not wrapping use more efficient BufEndOfLine
	if (!P_continuousWrap) {
		*lineEnd = buffer_->BufEndOfLine(startPos);
        *nextLineStart = std::min(buffer_->BufGetLength(), *lineEnd + 1);
		return;
	}

	// use the wrapped line counter routine to count forward one line
	wrappedLineCounter(buffer_, startPos, buffer_->BufGetLength(), 1, startPosIsLineStart, 0, nextLineStart, &retLines, &retLineStart, lineEnd);
}

/**
 * Update the minimum, maximum, slider size, page increment, and value for the
 * horizontal scroll bar.  If scroll position is such that there is blank space
 * to the right of all lines of text, scroll back (adjust horizOffset but don't
 * redraw) to take up the slack and position the right edge of the text at the
 * right edge of the display.
 *
 * Note, there is some cost to this routine, since it scans the whole range of
 * displayed text, particularly since it's usually called for each typed
 * character!
 *
 * @brief TextArea::updateHScrollBarRange
 * @return
 */
bool TextArea::updateHScrollBarRange() {
    int maxWidth = 0;
    int64_t sliderMax;
	int sliderWidth;
    int64_t origHOffset = horizOffset_;

	if(!horizontalScrollBar()->isVisible()) {
        return false;
	}

	// Scan all the displayed lines to find the width of the longest line
	for (int i = 0; i < nVisibleLines_ && lineStarts_[i] != -1; i++) {
        maxWidth = std::max<int64_t>(measureVisLine(i), maxWidth);
	}

	/* If the scroll position is beyond what's necessary to keep all lines
	   in view, scroll to the left to bring the end of the longest line to
	   the right margin */
    if (maxWidth < rect_.width() + horizOffset_ && horizOffset_ > 0)
        horizOffset_ = std::max(0, maxWidth - rect_.width());

	// Readjust the scroll bar
    sliderWidth = rect_.width();
    sliderMax   = std::max(maxWidth, sliderWidth + horizOffset_);

	horizontalScrollBar()->setMinimum(0);
    horizontalScrollBar()->setMaximum(static_cast<int>(std::max<int64_t>(sliderMax - rect_.width(), 0)));
    horizontalScrollBar()->setPageStep(std::max(rect_.width() - 100, 10));
    horizontalScrollBar()->setValue(horizOffset_);

    // Return true if scroll position was changed
	return origHOffset != horizOffset_;
}

/**
 * Return true if there are lines visible with no corresponding buffer text
 * @brief TextArea::emptyLinesVisible
 * @return
 */
bool TextArea::emptyLinesVisible() const {
	return nVisibleLines_ > 0 && lineStarts_[nVisibleLines_ - 1] == -1;
}

/**
 * Find the line number of position "pos" relative to the first line of
 * displayed text. Returns false if the line is not displayed.
 *
 * @brief TextArea::posToVisibleLineNum
 * @param pos
 * @param lineNum
 * @return
 */
bool TextArea::posToVisibleLineNum(int64_t pos, int *lineNum) const {

	if (pos < firstChar_) {
        return false;
	}

	if (pos > lastChar_) {
		if (emptyLinesVisible()) {
			if (lastChar_ < buffer_->BufGetLength()) {
				if (!posToVisibleLineNum(lastChar_, lineNum)) {
                    qCritical("NEdit: Consistency check ptvl failed");
                    return false;
				}
				return ++(*lineNum) <= nVisibleLines_ - 1;
			} else {
                posToVisibleLineNum(std::max<int64_t>(lastChar_ - 1, 0), lineNum);
				return true;
			}
		}
        return false;
	}

	for (int i = nVisibleLines_ - 1; i >= 0; i--) {
		if (lineStarts_[i] != -1 && pos >= lineStarts_[i]) {
			*lineNum = i;
			return true;
		}
	}

    return false;
}

/**
 * When the cursor is at the left or right edge of the text, part of it sticks
 * off into the clipped region beyond the text.  Normal redrawing can not
 * overwrite this protruding part of the cursor, so it must be erased
 * independently by calling this routine.
 *
 * @brief TextArea::blankCursorProtrusions
 */
void TextArea::blankCursorProtrusions() {

    int  x;
    int width;
    QFontMetrics fm(font_);
    int fontWidth         = fm.maxWidth();
    int fontHeight        = ascent_ + descent_;
    const int left        = rect_.left();
    const int right       = rect_.right();
    const int cursorX     = cursor_.x();
    const int cursorY     = cursor_.y();
    const int cursorWidth = (fontWidth / 3) * 2;

	if (cursorX >= left - 1 && cursorX <= left + cursorWidth / 2 - 1) {
		x = cursorX - cursorWidth / 2;
		width = left - x;
	} else if (cursorX >= right - cursorWidth / 2 && cursorX <= right) {
		x = right;
		width = cursorX + cursorWidth / 2 + 2 - right;
	} else {
		return;
	}

    viewport()->update(QRect(x, cursorY, width, fontHeight));
}

/**
 * Return the width in pixels of the displayed line pointed to by "visLineNum"
 * @brief TextArea::measureVisLine
 * @param visLineNum
 * @return
 */
int64_t TextArea::measureVisLine(int visLineNum) const {

    int64_t width              = 0;
    int charCount              = 0;
    int64_t lineLen            = visLineLength(visLineNum);
    const int64_t lineStartPos = lineStarts_[visLineNum];
    char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];

    if (!styleBuffer_) {

        QFontMetrics fm(font_);

        for (int i = 0; i < lineLen; i++) {
            int64_t len = buffer_->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
            width += fm.width(asciiToUnicode(expandedChar, len));
			charCount += len;
		}
	} else {
        for (int i = 0; i < lineLen; i++) {
            int64_t len = buffer_->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
            auto styleChar = styleBuffer_->BufGetCharacter(lineStartPos + i);
            uint8_t style = static_cast<uint8_t>(styleChar) - ASCII_A;

            QFontMetrics styleFm(styleTable_[style].font);
            width += styleFm.width(asciiToUnicode(expandedChar, len));

			charCount += len;
		}
	}
	return width;
}

/**
 * Return the length of a line (number of displayable characters) by examining
 * entries in the line starts array rather than by scanning for newlines
 *
 * @brief TextArea::visLineLength
 * @param visLineNum
 * @return
 */
int64_t TextArea::visLineLength(int visLineNum) const {

    int64_t lineStartPos = lineStarts_[visLineNum];

    if (lineStartPos == -1) {
		return 0;
    }

    if (visLineNum + 1 >= nVisibleLines_) {
		return lastChar_ - lineStartPos;
    }

    int64_t nextLineStart = lineStarts_[visLineNum + 1];

    if (nextLineStart == -1) {
		return lastChar_ - lineStartPos;
    }

    if (wrapUsesCharacter(nextLineStart - 1)) {
		return nextLineStart - 1 - lineStartPos;
    }

	return nextLineStart - lineStartPos;
}

/*
** Line breaks in continuous wrap mode usually happen at newlines or
** whitespace.  This line-terminating character is not included in line
** width measurements and has a special status as a non-visible character.
** However, lines with no whitespace are wrapped without the benefit of a
** line terminating character, and this distinction causes endless trouble
** with all of the text display code which was originally written without
** continuous wrap mode and always expects to wrap at a newline character.
**
** Given the position of the end of the line, as returned by TextDEndOfLine
** or BufEndOfLine, this returns true if there is a line terminating
** character, and false if there's not.  On the last character in the
** buffer, this function can't tell for certain whether a trailing space was
** used as a wrap point, and just guesses that it wasn't.  So if an exact
** accounting is necessary, don't use this function.
*/
bool TextArea::wrapUsesCharacter(int64_t lineEndPos) const {

	if (!P_continuousWrap || lineEndPos == buffer_->BufGetLength()) {
		return true;
	}

    const char c = buffer_->BufGetCharacter(lineEndPos);
	return (c == '\n') || ((c == '\t' || c == ' ') && lineEndPos + 1 != buffer_->BufGetLength());
}

/*
** Extend the range of a redraw request (from *start to *end) with additional
** redraw requests resulting from changes to the attached style buffer (which
** contains auxiliary information for coloring or styling text).
*/
void TextArea::extendRangeForStyleMods(int64_t *start, int64_t *end) {
    const TextSelection *sel = &styleBuffer_->BufGetPrimary();
	int extended = false;

	/* The peculiar protocol used here is that modifications to the style
	   buffer are marked by selecting them with the buffer's primary selection.
	   The style buffer is usually modified in response to a modify callback on
	   the text buffer BEFORE TextDisplay.c's modify callback, so that it can keep
	   the style buffer in step with the text buffer.  The style-update
	   callback can't just call for a redraw, because TextDisplay hasn't processed
	   the original text changes yet.  Anyhow, to minimize redrawing and to
	   avoid the complexity of scheduling redraws later, this simple protocol
	   tells the text display's buffer modify callback to extend it's redraw
	   range to show the text color/and font changes as well. */
	if (sel->selected) {
		if (sel->start < *start) {
			*start = sel->start;
			extended = true;
		}
		if (sel->end > *end) {
			*end = sel->end;
			extended = true;
		}
	}

	/* If the selection was extended due to a style change, and some of the
	   fonts don't match in spacing, extend redraw area to end of line to
	   redraw characters exposed by possible font size changes */
    if (fixedFontWidth_ == -1 && extended) {
		*end = buffer_->BufEndOfLine(*end) + 1;
    }
}

/*
** Refresh all of the text between buffer positions "start" and "end"
** not including the character at the position "end".
** If end points beyond the end of the buffer, refresh the whole display
** after pos, including blank lines which are not technically part of
** any range of characters.
*/
void TextArea::textDRedisplayRange(int64_t start, int64_t end) {

    int startLine;
    int lastLine;    

	// If the range is outside of the displayed text, just return
	if (end < firstChar_ || (start > lastChar_ && !emptyLinesVisible()))
		return;

	// Clean up the starting and ending values
    start = qBound<int64_t>(0, start, buffer_->BufGetLength());
    end   = qBound<int64_t>(0, end,   buffer_->BufGetLength());

	// Get the starting and ending lines
	if (start < firstChar_) {
		start = firstChar_;
	}

	if (!posToVisibleLineNum(start, &startLine)) {
		startLine = nVisibleLines_ - 1;
	}

	if (end >= lastChar_) {
		lastLine = nVisibleLines_ - 1;
	} else {
		if (!posToVisibleLineNum(end, &lastLine)) {
			// shouldn't happen
			lastLine = nVisibleLines_ - 1;
		}
	}

	// Get the starting and ending positions within the lines
    int64_t startIndex = (lineStarts_[startLine] == -1) ? 0 : start - lineStarts_[startLine];
    int64_t endIndex;
	if (end >= lastChar_) {
        /* Request to redisplay beyond lastChar_, so tell redisplayLine() to
         * display everything to infy.  */
		endIndex = INT_MAX;
	} else if (lineStarts_[lastLine] == -1) {
        /*  Here, lastLine is determined by posToVisibleLineNum()
         * (see if/else above) but deemed to be out of display according to
         * lineStarts_. */
		endIndex = 0;
	} else {
		endIndex = end - lineStarts_[lastLine];
	}

    const int startLine32 = gsl::narrow<int>(startLine);
    const int lastLine32  = gsl::narrow<int>(lastLine);

	/* If the starting and ending lines are the same, redisplay the single
	   line between "start" and "end" */
	if (startLine == lastLine) {
        redisplayLineEx(startLine32, 0, INT_MAX, startIndex, endIndex);
		return;
	}

	// Redisplay the first line from "start"
    redisplayLineEx(startLine32, 0, INT_MAX, startIndex, INT_MAX);

	// Redisplay the lines in between at their full width
    for (int i = startLine32 + 1; i < lastLine32; i++) {
        redisplayLineEx(i, 0, INT_MAX, 0, INT_MAX);
    }

	// Redisplay the last line to "end"
    redisplayLineEx(lastLine32, 0, INT_MAX, 0, endIndex);
}

/**
 * @brief TextArea::redrawLineNumbersEx
 */
void TextArea::redrawLineNumbersEx() {
    viewport()->repaint(QRect(lineNumLeft_, rect_.top(), lineNumWidth_, rect_.height()));
}


/**
 * Refresh the line number area.  If clearAll is false, writes only over
 * the character cell areas.  Setting clearAll to true will clear out any
 * stray marks outside of the character cell area, which might have been
 * left from before a resize or font change.
 *
 * @brief TextArea::redrawLineNumbers
 * @param painter
 */
void TextArea::redrawLineNumbers(QPainter *painter) {

    const int lineHeight = ascent_ + descent_;

    // Don't draw if lineNumWidth == 0 (line numbers are hidden)
    if (lineNumWidth_ == 0) {
		return;
    }

    painter->setPen(lineNumFGPixel_);
    painter->setFont(font_);

	// Draw the line numbers, aligned to the text
    int y        = rect_.top();
    int64_t line = getAbsTopLineNum();

    for (int visLine = 0; visLine < nVisibleLines_; visLine++) {

        int64_t lineStart = lineStarts_[visLine];
		if (lineStart != -1 && (lineStart == 0 || buffer_->BufGetCharacter(lineStart - 1) == '\n')) {
            auto s = QString::number(line);
            QRect rect(lineNumLeft_, y, lineNumWidth_, ascent_ + descent_);
            painter->drawText(rect, Qt::TextSingleLine | Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignRight, s);
			line++;
		} else {
            if (visLine == 0) {
				line++;
            }
		}
		y += lineHeight;
	}
}

/*
 * A replacement for redisplayLine. Instead of directly painting, it will
 * calculate the rect that would be repainted and trigger an update of that
 * region. We will then repaint that region during the paint event
 */
void TextArea::redisplayLineEx(int visLineNum, int leftClip, int rightClip, int64_t leftCharIndex, int64_t rightCharIndex) {

	Q_UNUSED(leftCharIndex);
	Q_UNUSED(rightCharIndex);

	// If line is not displayed, skip it
	if (visLineNum < 0 || visLineNum >= nVisibleLines_) {
		return;
	}

	// Calculate y coordinate of the string to draw
    const int fontHeight = ascent_ + descent_;
    const int y = rect_.top() + visLineNum * fontHeight;

	viewport()->update(QRect(leftClip, y, rightClip - leftClip, fontHeight));
}

/*
** Redisplay the text on a single line represented by "visLineNum" (the
** number of lines down from the top of the display), limited by
** "leftClip" and "rightClip" window coordinates and "leftCharIndex" and
** "rightCharIndex" character positions (not including the character at
** position "rightCharIndex").
**
** The cursor is also drawn if it appears on the line.
*/
void TextArea::redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex) {

	int i;
    int startX;
    int64_t charWidth;
    int startIndex;
	int style;
    int64_t charLen;
	int outStartIndex;
    int cursorX = 0;
	bool hasCursor = false;
    int64_t dispIndexOffset;
    int64_t cursorPos = cursorPos_;
    char outStr[MAX_DISP_LINE_LEN];
	char baseChar;

    QFontMetrics fm(font_);

    // If line is not displayed, skip it
	if (visLineNum < 0 || visLineNum >= nVisibleLines_) {
		return;
	}

	// Shrink the clipping range to the active display area
    leftClip  = std::max(rect_.left(), leftClip);
    rightClip = std::min(rightClip, rect_.left() + rect_.width());

	if (leftClip > rightClip) {
		return;
	}

	// Calculate y coordinate of the string to draw
	const int fontHeight = ascent_ + descent_;
    const int y = rect_.top() + visLineNum * fontHeight;

	// Get the text, length, and  buffer position of the line to display
    const int64_t lineStartPos = lineStarts_[visLineNum];
	std::string lineStr;
    int64_t lineLen;
	if (lineStartPos == -1) {
		lineLen = 0;
	} else {
		lineLen = visLineLength(visLineNum);
		lineStr = buffer_->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);
	}

	/* Space beyond the end of the line is still counted in units of characters
	   of a standardized character width (this is done mostly because style
	   changes based on character position can still occur in this region due
	   to rectangular selections).  stdCharWidth must be non-zero to prevent a
	   potential infinite loop if x does not advance */
	const int stdCharWidth = fm.maxWidth();
	if (stdCharWidth <= 0) {
        qWarning("NEdit: Internal Error, bad font measurement");
		return;
	}

	/* Rectangular selections are based on "real" line starts (after a newline
	   or start of buffer).  Calculate the difference between the last newline
	   position and the line start we're using.  Since scanning back to find a
	   newline is expensive, only do so if there's actually a rectangular
	   selection which needs it */
    if (P_continuousWrap && (buffer_->BufGetPrimary().rangeTouchesRectSel(lineStartPos, lineStartPos + lineLen) || buffer_->BufGetSecondary().rangeTouchesRectSel(lineStartPos, lineStartPos + lineLen) || buffer_->BufGetHighlight().rangeTouchesRectSel(lineStartPos, lineStartPos + lineLen))) {
		dispIndexOffset = buffer_->BufCountDispChars(buffer_->BufStartOfLine(lineStartPos), lineStartPos);
	} else {
		dispIndexOffset = 0;
	}

	/* Step through character positions from the beginning of the line (even if
	   that's off the left edge of the displayed area) to find the first
	   character position that's not clipped, and the x coordinate for drawing
	   that character */
    int x = rect_.left() - horizOffset_;
	int outIndex = 0;

    int charIndex;
	for (charIndex = 0;; charIndex++) {

        char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];
        baseChar = '\0';
        charLen = (charIndex >= lineLen) ?
                    1 :
                    TextBuffer::BufExpandCharacter(baseChar = lineStr[charIndex], outIndex, expandedChar, buffer_->BufGetTabDist());

		style = styleOfPos(lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, baseChar);
		charWidth = charIndex >= lineLen ? stdCharWidth : stringWidth(expandedChar, charLen, style);

		if (x + charWidth >= leftClip && charIndex >= leftCharIndex) {
			startIndex = charIndex;
			outStartIndex = outIndex;
			startX = x;
			break;
		}
		x += charWidth;
		outIndex += charLen;
	}

	/* Scan character positions from the beginning of the clipping range, and
	   draw parts whenever the style changes (also note if the cursor is on
	   this line, and where it should be drawn to take advantage of the x
	   position which we've gone to so much trouble to calculate) */
    char *outPtr = outStr;

	outIndex = outStartIndex;
	x = startX;
	for (charIndex = startIndex; charIndex < rightCharIndex; charIndex++) {

		if (lineStartPos + charIndex == cursorPos) {
			if (charIndex < lineLen || (charIndex == lineLen && cursorPos >= buffer_->BufGetLength())) {
				hasCursor = true;
				cursorX = x - 1;
			} else if (charIndex == lineLen) {
				if (wrapUsesCharacter(cursorPos)) {
					hasCursor = true;
					cursorX = x - 1;
				}
			}
		}

        char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];
		baseChar = '\0';
        charLen = (charIndex >= lineLen) ?
                    1 :
                    TextBuffer::BufExpandCharacter(baseChar = lineStr[charIndex], outIndex, expandedChar, buffer_->BufGetTabDist());

		int charStyle = styleOfPos(lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, baseChar);

		for (i = 0; i < charLen; i++) {
			if (i != 0 && charIndex < lineLen && lineStr[charIndex] == '\t') {
				charStyle = styleOfPos(lineStartPos, lineLen, charIndex, outIndex + dispIndexOffset, '\t');
			}

			if (charStyle != style) {
				drawString(painter, style, startX, y, x, outStr, outPtr - outStr);
				outPtr = outStr;
				startX = x;
				style = charStyle;
			}

			if (charIndex < lineLen) {
				*outPtr = expandedChar[i];
				charWidth = stringWidth(&expandedChar[i], 1, charStyle);
			} else {
				charWidth = stdCharWidth;
			}

			outPtr++;
			x += charWidth;
			outIndex++;
		}

        if (outPtr - outStr + TextBuffer::MAX_EXP_CHAR_LEN >= MAX_DISP_LINE_LEN || x >= rightClip) {
			break;
		}

	}

	// Draw the remaining style segment
	drawString(painter, style, startX, y, x, outStr, outPtr - outStr);

	/* Draw the cursor if part of it appeared on the redisplayed part of
	   this line.  Also check for the cases which are not caught as the
	   line is scanned above: when the cursor appears at the very end
	   of the redisplayed section. */
    const int64_t y_orig = cursor_.y();
	if (cursorOn_) {
		if (hasCursor) {
			drawCursor(painter, cursorX, y);
		} else if (charIndex < lineLen && (lineStartPos + charIndex + 1 == cursorPos) && x == rightClip) {
			if (cursorPos >= buffer_->BufGetLength()) {
				drawCursor(painter, x - 1, y);
			} else {
				if (wrapUsesCharacter(cursorPos)) {
					drawCursor(painter, x - 1, y);
				}
			}
        } else if(rightCharIndex != INT_MAX) {
            if ((lineStartPos + rightCharIndex) == cursorPos) {
                drawCursor(painter, x - 1, y);
            }
        }
	}

	// If the y position of the cursor has changed, redraw the calltip
    if (hasCursor && (y_orig != cursor_.y() || y_orig != y)) {
		TextDRedrawCalltip(0);
	}
}

/*
** Determine the drawing method to use to draw a specific character from "buf".
** "lineStartPos" gives the character index where the line begins, "lineIndex",
** the number of characters past the beginning of the line, and "dispIndex",
** the number of displayed characters past the beginning of the line.  Passing
** lineStartPos of -1 returns the drawing style for "no text".
**
** Why not just: styleOfPos(pos)?  Because style applies to blank areas
** of the window beyond the text boundaries, and because this routine must also
** decide whether a position is inside of a rectangular selection, and do so
** efficiently, without re-counting character positions from the start of the
** line.
**
** Note that style is a somewhat incorrect name, drawing method would
** be more appropriate.
*/
int TextArea::styleOfPos(int64_t lineStartPos, int64_t lineLen, int64_t lineIndex, int64_t dispIndex, int thisChar) const {

    int style = 0;

    if (lineStartPos == -1 || !buffer_) {
        return FILL_MASK;
    }

    int64_t pos = lineStartPos + std::min(lineIndex, lineLen);

    if (lineIndex >= lineLen) {
        style = FILL_MASK;
    } else if (styleBuffer_) {
        style = static_cast<uint8_t>(styleBuffer_->BufGetCharacter(pos));
        if (style == unfinishedStyle_) {
            /* encountered "unfinished" style, trigger parsing */
            (unfinishedHighlightCB_)(this, pos, highlightCBArg_);
            style = static_cast<uint8_t>(styleBuffer_->BufGetCharacter(pos));
        }
    }

    if (buffer_->BufGetPrimary().inSelection(pos, lineStartPos, dispIndex)) {
        style |= PRIMARY_MASK;
    }

    if (buffer_->BufGetHighlight().inSelection(pos, lineStartPos, dispIndex)) {
        style |= HIGHLIGHT_MASK;
    }

    if (buffer_->BufGetSecondary().inSelection(pos, lineStartPos, dispIndex)) {
        style |= SECONDARY_MASK;
    }

    /* store in the RANGESET_MASK portion of style the rangeset index for pos */
    if (document_->rangesetTable_) {
        int rangesetIndex = document_->rangesetTable_->RangesetIndex1ofPos(pos, true);
        style |= ((rangesetIndex << RANGESET_SHIFT) & RANGESET_MASK);
    }

    /* store in the BACKLIGHT_MASK portion of style the background color class
       of the character thisChar */
    if (!bgClass_.empty()) {
        style |= (bgClass_[static_cast<uint8_t>(thisChar)] << BACKLIGHT_SHIFT);
    }
    return style;
}

/*
** Draw a string or blank area according to parameter "style", using the
** appropriate colors and drawing method for that style, with top left
** corner at x, y.  If style says to draw text, use "string" as source of
** characters, and draw "nChars", if style is FILL, erase
** rectangle where text would have drawn from x to toX and from y to
** the maximum y extent of the current font(s).
*/
void TextArea::drawString(QPainter *painter, int style, int64_t x, int y, int64_t toX, char *string, long nChars) {

    QColor bground       = palette().color(QPalette::Base);
    QColor fground       = palette().color(QPalette::Text);
    bool underlineStyle  = false;
    QFont renderFont     = font_;

	enum DrawType {
        DrawStyle,
        DrawHighlight,
        DrawSelect,
        DrawPlain
	};

    const DrawType drawType = [](int style) {
        // select a GC
        if (style & (STYLE_LOOKUP_MASK | BACKLIGHT_MASK | RANGESET_MASK)) {
            return DrawStyle;
        } else if (style & HIGHLIGHT_MASK) {
            return DrawHighlight;
        } else if (style & PRIMARY_MASK) {
            return DrawSelect;
        } else {
            return DrawPlain;
        }
    }(style);

    switch(drawType) {
    case DrawHighlight:
        fground = highlightFGPixel_;
        bground = highlightBGPixel_;
        break;
    case DrawSelect:
        fground = palette().color(QPalette::HighlightedText);
        bground = palette().color(QPalette::Highlight);
        break;
    case DrawPlain:
        break;
    case DrawStyle:
        {
            // we have work to do
            StyleTableEntry *styleRec;
            /* Set font, color, and gc depending on style.  For normal text, GCs
               for normal drawing, or drawing within a selection or highlight are
               pre-allocated and pre-configured.  For syntax highlighting, GCs are
               configured here, on the fly. */
            if (style & STYLE_LOOKUP_MASK) {
                styleRec = &styleTable_[(style & STYLE_LOOKUP_MASK) - ASCII_A];
                underlineStyle = styleRec->underline;

                renderFont  = styleRec->font;
                fground = styleRec->color;
                // here you could pick up specific select and highlight fground
            } else {
                styleRec = nullptr;
                renderFont = font_;
                fground = palette().color(QPalette::Text);
            }

            /* Background color priority order is:
            ** 1 Primary(Selection),
            ** 2 Highlight(Parens),
            ** 3 Rangeset
            ** 4 SyntaxHighlightStyle,
            ** 5 Backlight (if NOT fill)
            ** 6 DefaultBackground
            */
            if(style & PRIMARY_MASK) {
                bground = palette().color(QPalette::Highlight);

                // NOTE(eteran): enabling this makes working with darker themes a lot nicer
                // basically what it does is make it so highlights are disabled inside of
                // selections. Giving the user control over the foreground color inside
                // a highlight selection.
                if(!P_colorizeHighlightedText) {
                    fground = palette().color(QPalette::HighlightedText);
                }

            } else if(style & HIGHLIGHT_MASK) {
                bground = highlightBGPixel_;
                if(!P_colorizeHighlightedText) {
                    fground = highlightFGPixel_;
                }
            } else if(style & RANGESET_MASK) {
                bground = getRangesetColor((style & RANGESET_MASK) >> RANGESET_SHIFT, bground);
            } else if(styleRec && !styleRec->bgColorName.isNull()) {
                bground = styleRec->bgColor;
            } else if((style & BACKLIGHT_MASK) && !(style & FILL_MASK)) {
                bground = bgClassPixel_[(style >> BACKLIGHT_SHIFT) & 0xff];
            } else {
                bground = palette().color(QPalette::Base);
            }

            if (fground == bground) { // B&W kludge
                fground = palette().color(QPalette::Base);
            }
        }
        break;
    }

	// Draw blank area rather than text, if that was the request
	if (style & FILL_MASK) {

		// wipes out to right hand edge of widget
        if (toX >= rect_.left()) {
            painter->fillRect(
                        QRect(
                            static_cast<int>(std::max<int64_t>(x, rect_.left())),
                            y,
                            static_cast<int>(toX - std::max<int64_t>(x, rect_.left())),
                            ascent_ + descent_
                            ),
                        bground);
		}

		return;
	}

#if 0
    QFontMetrics fm(renderFont);

	/* If any space around the character remains unfilled (due to use of
	   different sized fonts for highlighting), fill in above or below
	   to erase previously drawn characters */
	if (fm.ascent() < ascent_) {
		painter->fillRect(QRect(x, y, toX - x, ascent_ - fm.ascent()), bground);
	}

	if (fm.descent() < descent_) {
		painter->fillRect(QRect(x, y + ascent_ + fm.descent(), toX - x, descent_ - fm.descent()), bground);
	}
#endif

	// Underline if style is secondary selection
	if (style & SECONDARY_MASK || underlineStyle) {
        renderFont.setUnderline(true);
	}

    auto s = asciiToUnicode(string, nChars);

    QRect rect(
                static_cast<int>(x),
                y,
                static_cast<int>(toX - x),
                ascent_ + descent_);

    // TODO(eteran): 2.0, OPTIMIZATION? since Qt will auto-fill the BG with the
	//               default base color we only need to play with the
	//               background mode if drawing a non-base color.
	//               Probably same with font
    painter->save();
    painter->setFont(renderFont);
    painter->fillRect(rect, bground);
    painter->setPen(fground);
    painter->drawText(rect, Qt::TextSingleLine | Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignLeft, s);
    painter->restore();
}

/**
 * Draw a cursor with top center at x, y.
 *
 * @brief TextArea::drawCursor
 * @param painter
 * @param x
 * @param y
 */
void TextArea::drawCursor(QPainter *painter, int x, int y) {

	QPainterPath path;
    QFontMetrics fm(font_);

    // NOTE(eteran): the original code used fontStruct_->min_bounds.width
    // this doesn't matter for fixed sized fonts, but for variable sized ones
    // we aren't quite right. I've approximated this with the width of 'i', but
    // in some fonts, maybe that's not right?
    int fontWidth  = fm.width(QLatin1Char('i'));
    int fontHeight = ascent_ + descent_;

    // NOTE(eteran): some minor adjustments to get things to align "just right"
    // This wasn't needed when using bitmapped X11 fonts, so I assume there is
    // a slight discrepancy between that and Qt's high quality rendering.
    x += 1;
    y += 1;
    fontHeight -= 1;

    int64_t bot = y + fontHeight - 1;

    if (x < rect_.left() - 1 || x > rect_.left() + rect_.width()) {
		return;
	}

	/* For cursors other than the block, make them around 2/3 of a character
	   width, rounded to an even number of pixels so that X will draw an
	   odd number centered on the stem at x. */
	int cursorWidth = (fontWidth / 3) * 2;
    int64_t left  = x - cursorWidth / 2;
    int64_t right = left + cursorWidth;

	// Create segments and draw cursor
	switch(cursorStyle_) {
    case CursorStyles::Caret: {
        const int64_t midY = bot - fontHeight / 5;

		path.moveTo(left, bot);
		path.lineTo(x, midY);
		path.moveTo(x, midY);
		path.lineTo(right, bot);
		path.moveTo(left, bot);
		path.lineTo(x, midY - 1);
		path.moveTo(x, midY - 1);
		path.lineTo(right, bot);
		break;
    }
    case CursorStyles::Normal: {
        path.moveTo(left, y);
        path.lineTo(right, y);
        path.moveTo(x, y);
        path.lineTo(x, bot);
        path.moveTo(left, bot);
        path.lineTo(right, bot);
        break;
    }
    case CursorStyles::Heavy: {
        path.moveTo(x - 1, y);
        path.lineTo(x - 1, bot);
        path.moveTo(x, y);
        path.lineTo(x, bot);
        path.moveTo(x + 1, y);
        path.lineTo(x + 1, bot);
        path.moveTo(left, y);
        path.lineTo(right, y);
        path.moveTo(left, bot);
        path.lineTo(right, bot);
        break;
    }
    case CursorStyles::Dim: {
        const int64_t midY = y + fontHeight / 2;

        path.moveTo(x, y);
        path.lineTo(x, y);
        path.moveTo(x, midY);
        path.lineTo(x, midY);
        path.moveTo(x, bot);
        path.lineTo(x, bot);
        break;
    }
    case CursorStyles::Block: {
        right = x + fontWidth;

        path.moveTo(x, y);
        path.lineTo(right, y);
        path.moveTo(right, y);
        path.lineTo(right, bot);
        path.moveTo(right, bot);
        path.lineTo(x, bot);
        path.moveTo(x, bot);
        path.lineTo(x, y);
        break;
    }
    }

    painter->save();
    painter->setClipping(false);
    painter->setPen(cursorFGPixel_);
    painter->drawPath(path);
    painter->setClipping(true);
    painter->restore();

	// Save the last position drawn
    cursor_ = QPoint(x, y);
}

/**
 * @brief TextArea::getRangesetColor
 * @param ind
 * @param bground
 * @return
 */
QColor TextArea::getRangesetColor(int ind, QColor bground) const {

	if (ind > 0) {
		ind--;
        const std::shared_ptr<RangesetTable> &tab = document_->rangesetTable_;

        QColor color;
        bool valid = tab->RangesetTableGetColorValid(ind, &color);
        if (!valid) {
            const QString color_name = tab->RangesetTableGetColorName(ind);
            if (!color_name.isNull()) {
                color = X11Colors::fromString(color_name);
			}
            tab->RangesetTableAssignColorPixel(ind, color);
		}

		if (color.isValid()) {
            return color;
		}
	}

	return bground;
}

/**
 * Change the size of the displayed text area
 *
 * @brief TextArea::TextDResize
 * @param width
 * @param height
 */
void TextArea::TextDResize(int width, int height) {

	int oldVisibleLines = nVisibleLines_;
    bool canRedraw      = true;
    int newVisibleLines = height / (ascent_ + descent_);
    int redrawAll       = false;
    int oldWidth        = rect_.width();
    int exactHeight     = height - height % (ascent_ + descent_);

    rect_.setSize({width, height});

	/* In continuous wrap mode, a change in width affects the total number of
	   lines in the buffer, and can leave the top line number incorrect, and
	   the top character no longer pointing at a valid line start */
	if (P_continuousWrap && P_wrapMargin == 0 && width != oldWidth) {
        int64_t oldFirstChar = firstChar_;
		nBufferLines_ = TextDCountLines(0, buffer_->BufGetLength(), true);
		firstChar_ = TextDStartOfLine(firstChar_);
        topLineNum_ = TextDCountLines(0, firstChar_, true) + 1;
		redrawAll = true;
		offsetAbsLineNum(oldFirstChar);
	}

	/* reallocate and update the line starts array, which may have changed
	   size and/or contents. (contents can change in continuous wrap mode
	   when the width changes, even without a change in height) */
	if (oldVisibleLines < newVisibleLines) {
        lineStarts_.resize(newVisibleLines);
	}

	nVisibleLines_ = newVisibleLines;
	calcLineStarts(0, newVisibleLines);
	calcLastChar();

	/* if the window became shorter, there may be partially drawn
	   text left at the bottom edge, which must be cleaned up */
	if (canRedraw && oldVisibleLines > newVisibleLines && exactHeight != height) {
        viewport()->update(QRect(rect_.left(), rect_.top() + exactHeight, rect_.width(), height - exactHeight));
	}

	/* if the window became taller, there may be an opportunity to display
	   more text by scrolling down */
	if (canRedraw && oldVisibleLines < newVisibleLines && topLineNum_ + nVisibleLines_ > nBufferLines_) {
        setScroll(std::max<int64_t>(1, nBufferLines_ - nVisibleLines_ + 2 + P_cursorVPadding), horizOffset_, false, false);
	}

	/* Update the scroll bar page increment size (as well as other scroll
	   bar parameters.  If updating the horizontal range caused scrolling,
	   redraw */
	updateVScrollBarRange();
    if (updateHScrollBarRange()) {
		redrawAll = true;
    }

	// If a full redraw is needed
    if (redrawAll && canRedraw) {
		TextDRedisplayRect(rect_);
    }

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();

	/* Refresh the line number display to draw more line numbers, or
	   erase extras */
    redrawLineNumbersEx();

	// Redraw the calltip
	TextDRedrawCalltip(0);
}

/*
** Same as BufCountLines, but takes in to account wrapping if wrapping is
** turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as true to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
*/
int64_t TextArea::TextDCountLines(int64_t startPos, int64_t endPos, bool startPosIsLineStart) {
    int64_t retLines;
    int64_t retPos;
    int64_t retLineStart;
    int64_t retLineEnd;

	// If we're not wrapping use simple (and more efficient) BufCountLines
	if (!P_continuousWrap) {
		return buffer_->BufCountLines(startPos, endPos);
	}

	wrappedLineCounter(buffer_, startPos, endPos, INT_MAX, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLines;
}

/**
 * @brief TextArea::setScroll
 * @param topLineNum
 * @param horizOffset
 * @param updateVScrollBar
 * @param updateHScrollBar
 */
void TextArea::setScroll(int64_t topLineNum, int horizOffset, bool updateVScrollBar, bool updateHScrollBar) {

	/* Do nothing if scroll position hasn't actually changed or there's no
	   window to draw in yet */
	if ((horizOffset_ == horizOffset && topLineNum_ == topLineNum)) {
		return;
	}

	/* If part of the cursor is protruding beyond the text clipping region,
	   clear it off */
	blankCursorProtrusions();

	/* If the vertical scroll position has changed, update the line
	   starts array and related counters in the text display */
	offsetLineStarts(topLineNum);

	// Just setting horizOffset_ is enough information for redisplay
	horizOffset_ = horizOffset;

	/* Update the scroll bar positions if requested, note: updating the
	   horizontal scroll bars can have the further side-effect of changing
	   the horizontal scroll position, horizOffset_ */
	if (updateVScrollBar) {
		updateVScrollBarRange();
	}

	if (updateHScrollBar) {
		updateHScrollBarRange();
	}

	// NOTE(eteran): the original code seemed to do some cleverness
	//               involving copying the parts that were "moved"
	//               to avoid doing some work. For now, we'll just repaint
	//               the whole thing. It's not as fast/clever, but it'll work
	viewport()->update();

    const int64_t lineDelta = topLineNum_ - topLineNum;

	// Refresh line number/calltip display if its up and we've scrolled vertically
	if (lineDelta != 0) {
        redrawLineNumbersEx();
		TextDRedrawCalltip(0);
	}
}

/**
 * @brief TextArea::TextDSetCursorStyle
 * @param style
 */
void TextArea::TextDSetCursorStyle(CursorStyles style) {
	cursorStyle_ = style;
	blankCursorProtrusions();
	if (cursorOn_) {
		textDRedisplayRange(cursorPos_ - 1, cursorPos_ + 1);
	}
}

/**
 * @brief TextArea::TextDBlankCursor
 */
void TextArea::TextDBlankCursor() {
    if (!cursorOn_) {
		return;
    }

	blankCursorProtrusions();
    cursorOn_ = false;
	textDRedisplayRange(cursorPos_ - 1, cursorPos_ + 1);
}

/**
 * @brief TextArea::TextDUnblankCursor
 */
void TextArea::TextDUnblankCursor() {
	if (!cursorOn_) {
		cursorOn_ = true;
		textDRedisplayRange(cursorPos_ - 1, cursorPos_ + 1);
	}
}

/*
** Offset the line starts array, topLineNum, firstChar and lastChar, for a new
** vertical scroll position given by newTopLineNum.  If any currently displayed
** lines will still be visible, salvage the line starts values, otherwise,
** count lines from the nearest known line start (start or end of buffer, or
** the closest value in the lineStarts array)
*/
void TextArea::offsetLineStarts(int64_t newTopLineNum) {
    int64_t oldTopLineNum = topLineNum_;
    int64_t oldFirstChar  = firstChar_;
    int64_t lineDelta     = newTopLineNum - oldTopLineNum;
    int nVisLines         = nVisibleLines_;
    int64_t lastLineNum;

	// If there was no offset, nothing needs to be changed
	if (lineDelta == 0)
		return;

	/* Find the new value for firstChar by counting lines from the nearest
	   known line start (start or end of buffer, or the closest value in the
	   lineStarts array) */
	lastLineNum = oldTopLineNum + nVisLines - 1;
	if (newTopLineNum < oldTopLineNum && newTopLineNum < -lineDelta) {
		firstChar_ = TextDCountForwardNLines(0, newTopLineNum - 1, true);
	} else if (newTopLineNum < oldTopLineNum) {
		firstChar_ = TextDCountBackwardNLines(firstChar_, -lineDelta);
	} else if (newTopLineNum < lastLineNum) {
        firstChar_ = lineStarts_[newTopLineNum - oldTopLineNum];
	} else if (newTopLineNum - lastLineNum < nBufferLines_ - newTopLineNum) {
        firstChar_ = TextDCountForwardNLines(lineStarts_[nVisLines - 1], newTopLineNum - lastLineNum, true);
	} else {
		firstChar_ = TextDCountBackwardNLines(buffer_->BufGetLength(), nBufferLines_ - newTopLineNum + 1);
	}

	// Fill in the line starts array
	if (lineDelta < 0 && -lineDelta < nVisLines) {
        for (int64_t i = nVisLines - 1; i >= -lineDelta; i--) {
            lineStarts_[i] = lineStarts_[i + lineDelta];
        }

		calcLineStarts(0, -lineDelta);
	} else if (lineDelta > 0 && lineDelta < nVisLines) {
        for (int64_t i = 0; i < nVisLines - lineDelta; i++) {
            lineStarts_[i] = lineStarts_[i + lineDelta];
        }

		calcLineStarts(nVisLines - lineDelta, nVisLines - 1);
    } else {
		calcLineStarts(0, nVisLines);
    }

	// Set lastChar and topLineNum
	calcLastChar();
	topLineNum_ = newTopLineNum;

	/* If we're numbering lines or being asked to maintain an absolute line
	   number, re-calculate the absolute line number */
	offsetAbsLineNum(oldFirstChar);
}

/*
** Set the scroll position of the text display vertically by line number and
** horizontally by pixel offset from the left margin
*/
void TextArea::TextDSetScroll(int64_t topLineNum, int horizOffset) {

	int vPadding = P_cursorVPadding;

	// Limit the requested scroll position to allowable values
	if (topLineNum < 1) {
		topLineNum = 1;
	} else if ((topLineNum > topLineNum_) && (topLineNum > (nBufferLines_ + 2 - nVisibleLines_ + vPadding))) {
		topLineNum = std::max(topLineNum_, nBufferLines_ + 2 - nVisibleLines_ + vPadding);
	}

	int sliderMax = horizontalScrollBar()->maximum();

    horizOffset = qBound(0, horizOffset, sliderMax);

	setScroll(topLineNum, horizOffset, true, true);
}

/*
** Update the position of the current calltip if one exists, else do nothing
*/
void TextArea::TextDRedrawCalltip(int calltipID) {

	if (calltip_.ID == 0) {
		return;
	}

	if (calltipID != 0 && calltipID != calltip_.ID) {
		return;
	}

    if(!calltipWidget_) {
        return;
    }

    int rel_x;
    int rel_y;

	if (calltip_.anchored) {
		// Put it at the anchor position
		if (!TextDPositionToXY(calltip_.pos, &rel_x, &rel_y)) {
            if (calltip_.alignMode == TipAlignStrict::Strict) {
				TextDKillCalltip(calltip_.ID);
			}
			return;
		}
	} else {
		if (calltip_.pos < 0) {
			// First display of tip with cursor offscreen (detected in ShowCalltip)
            calltip_.pos = rect_.width() / 2;
            calltip_.hAlign = TipHAlignMode::Center;
            rel_y = rect_.height() / 3;
		} else if (!TextDPositionToXY(cursorPos_, &rel_x, &rel_y)) {
			// Window has scrolled and tip is now offscreen
            if (calltip_.alignMode == TipAlignStrict::Strict) {
				TextDKillCalltip(calltip_.ID);
			}
			return;
		}
		rel_x = calltip_.pos;
    }

    int lineHeight  = ascent_ + descent_;
    int tipWidth    = calltipWidget_->width();
    int tipHeight   = calltipWidget_->height();    
	int flip_delta;

    constexpr int borderWidth = 1;
	rel_x += borderWidth;
	rel_y += lineHeight / 2 + borderWidth;

	// Adjust rel_x for horizontal alignment modes
    switch(calltip_.hAlign) {
    case TipHAlignMode::Center:
        rel_x -= tipWidth / 2;
        break;
    case TipHAlignMode::Right:
        rel_x -= tipWidth;
        break;
    default:
        break;
    }

    // Adjust rel_y for vertical alignment modes
    switch(calltip_.vAlign) {
    case TipVAlignMode::Above:
        flip_delta = tipHeight + lineHeight + (2 * borderWidth);
        rel_y -= flip_delta;
        break;
    default:
        flip_delta = -(tipHeight + lineHeight + (2 * borderWidth));
        break;
    }

    QPoint abs = mapToGlobal(QPoint(rel_x, rel_y));

	// If we're not in strict mode try to keep the tip on-screen
    if (calltip_.alignMode == TipAlignStrict::Sloppy) {

        QDesktopWidget *desktop = QApplication::desktop();

		// make sure tip doesn't run off right or left side of screen
        if (abs.x() + tipWidth >= desktop->width() - CALLTIP_EDGE_GUARD) {
            abs.setX(desktop->width() - tipWidth - CALLTIP_EDGE_GUARD);
        }

        if (abs.x() < CALLTIP_EDGE_GUARD) {
            abs.setX(CALLTIP_EDGE_GUARD);
        }

		// Try to keep the tip onscreen vertically if possible
        if (desktop->height() > tipHeight && offscreenV(desktop, abs.y(), tipHeight)) {
			// Maybe flipping from below to above (or vice-versa) will help
            if (!offscreenV(desktop, abs.y() + flip_delta, tipHeight)) {
                abs.setY(abs.y() + flip_delta);
            }

			// Make sure the tip doesn't end up *totally* offscreen
            else if (abs.y() + tipHeight < 0) {
                abs.setY(CALLTIP_EDGE_GUARD);
            } else if (abs.y() >= desktop->height()) {
                abs.setY(desktop->height() - tipHeight - CALLTIP_EDGE_GUARD);
            }
			// If no case applied, just go with the default placement.
		}
	}

    calltipWidget_->move(abs);
    calltipWidget_->show();
}

/*
** Read the background color class specification string in str, allocating the
** necessary colors, and allocating and setting up the character->class_no and
** class_no->pixel map arrays, returned via *pp_bgClass and *pp_bgClassPixel
** respectively.
** Note: the allocation of class numbers could be more intelligent: there can
** never be more than 256 of these (one per character); but I don't think
** there'll be a pressing need. I suppose the scanning of the specification
** could be better too, but then, who cares!
*/
void TextArea::TextDSetupBGClassesEx(const QString &str) {
    TextDSetupBGClasses(str, &bgClassPixel_, &bgClass_, palette().color(QPalette::Base));
}

void TextArea::TextDSetupBGClasses(const QString &s, std::vector<QColor> *pp_bgClassPixel, std::vector<uint8_t> *pp_bgClass, const QColor &bgPixelDefault) {

    pp_bgClassPixel->clear();
    pp_bgClass->clear();

	if (s.isEmpty()) {
		return;
	}

    uint8_t bgClass[256]     = {};
	QColor bgClassPixel[256] = {};


	// default for all chars is class number zero, for standard background
	bgClassPixel[0] = bgPixelDefault;

	/* since class no == 0 in a "style" has no set bits in BACKLIGHT_MASK
	   (see styleOfPos()), when drawString() is called for text with a
	   backlight class no of zero, bgClassPixel[0] is never consulted, and
	   the default background color is chosen. */

	/* The format of the class string s is:
			  low[-high]{,low[-high]}:color{;low-high{,low[-high]}:color}
		  eg
			  32-255:#f0f0f0;1-31,127:red;128-159:orange;9-13:#e5e5e5
	   where low and high represent a character range between ordinal
	   ASCII values. Using strtol() allows automatic octal, dec and hex
	   reading of low and high. The example format sets backgrounds as follows:
			  char   1 - 8    colored red     (control characters)
			  char   9 - 13   colored #e5e5e5 (isspace() control characters)
			  char  14 - 31   colored red     (control characters)
			  char  32 - 126  colored #f0f0f0
			  char 127        colored red     (delete character)
			  char 128 - 159  colored orange  ("shifted" control characters)
			  char 160 - 255  colored #f0f0f0
	   Notice that some of the later ranges overwrite the class values defined
	   for earlier ones (eg the first clause, 32-255:#f0f0f0 sets the DEL
	   character background color to #f0f0f0; it is then set to red by the
	   clause 1-31,127:red). */

    size_t class_no = 1;
	QStringList formats = s.split(QLatin1Char(';'), QString::SkipEmptyParts);
	for(const QString &format : formats) {

		QStringList s1 = format.split(QLatin1Char(':'), QString::SkipEmptyParts);
		if(s1.size() == 2) {
			QString ranges = s1[0];
			QString color  = s1[1];

			if(class_no > UINT8_MAX) {
				break;
			}

			// NOTE(eteran): the original code started at index 1
            // by incrementing before using it. This code post increments
			// starting the classes at 0, which allows NUL characters
			// to be styled correctly. I am not aware of any negative
			// side effects of this.
            const auto nextClass = static_cast<uint8_t>(class_no++);

            QColor pix = X11Colors::fromString(color);
            bgClassPixel[nextClass] = pix;

			QStringList rangeList = ranges.split(QLatin1Char(','), QString::SkipEmptyParts);
			for(const QString &range : rangeList) {
				QRegExp regex(QLatin1String("([0-9]+)(?:-([0-9]+))?"));
				if(regex.exactMatch(range)) {

					const QString lo = regex.cap(1);
					const QString hi = regex.cap(2);

					bool loOK;
					bool hiOK;
					int lowerBound = lo.toInt(&loOK);
					int upperBound = hi.toInt(&hiOK);

					if(loOK) {
						if(!hiOK) {
							upperBound = lowerBound;
						}

						for(int i = lowerBound; i <= upperBound; ++i) {
							bgClass[i] = nextClass;
						}
					}
				}
			}
		}
	}

    std::vector<uint8_t> backgroundClass;
    backgroundClass.reserve(256);
    std::copy_n(bgClass, 256, std::back_inserter(backgroundClass));

    std::vector<QColor> backgroundPixel;
    backgroundPixel.reserve(class_no);
    std::copy_n(bgClassPixel, class_no, std::back_inserter(backgroundPixel));

    *pp_bgClass      = backgroundClass;
    *pp_bgClassPixel = backgroundPixel;
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** false if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
bool TextArea::TextDPositionToXY(int64_t pos, QPoint *coord) const {
    int x;
    int y;
    bool r = TextDPositionToXY(pos, &x, &y);
    coord->setX(x);
    coord->setY(y);
    return r;
}

bool TextArea::TextDPositionToXY(int64_t pos, int *x, int *y) const {

    char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];
    int64_t lineLen;
    int64_t lineStartPos;
    int charIndex;
    int fontHeight;
    int outIndex;
    int visLineNum;
    int xStep;

	// If position is not displayed, return false
    if (pos < firstChar_ || (pos > lastChar_ && !emptyLinesVisible())) {
        return false;
    }

	// Calculate y coordinate
    if (!posToVisibleLineNum(pos, &visLineNum)) {
        return false;
    }

    fontHeight = ascent_ + descent_;
    *y = gsl::narrow<int>(rect_.top() + visLineNum * fontHeight + fontHeight / 2);

	/* Get the text, length, and  buffer position of the line. If the position
	   is beyond the end of the buffer and should be at the first position on
	   the first empty line, don't try to get or scan the text  */
	lineStartPos = lineStarts_[visLineNum];
	if (lineStartPos == -1) {
        *x = rect_.left() - horizOffset_;
		return true;
	}
	lineLen = visLineLength(visLineNum);
	std::string lineStr = buffer_->BufGetRangeEx(lineStartPos, lineStartPos + lineLen);

	/* Step through character positions from the beginning of the line
	   to "pos" to calculate the x coordinate */
    xStep = rect_.left() - horizOffset_;
	outIndex = 0;
    for (charIndex = 0; charIndex < pos - lineStartPos; charIndex++) {
        const int64_t charLen = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, buffer_->BufGetTabDist());
        const int charStyle   = styleOfPos(lineStartPos, lineLen, charIndex, outIndex, lineStr[charIndex]);

		xStep += stringWidth(expandedChar, charLen, charStyle);
		outIndex += charLen;
	}
	*x = xStep;
	return true;
}

// Change the (non syntax-highlit) colors
void TextArea::TextDSetColors(const QColor &textFgP, const QColor &textBgP, const QColor &selectFgP, const QColor &selectBgP, const QColor &hiliteFgP, const QColor &hiliteBgP, const QColor &lineNoFgP, const QColor &cursorFgP) {

	// Update the stored pixels
    QPalette pal = palette();
	pal.setColor(QPalette::Text, textFgP);              // foreground color
	pal.setColor(QPalette::Base, textBgP);              // background
	pal.setColor(QPalette::Highlight, selectBgP);       // highlight background
	pal.setColor(QPalette::HighlightedText, selectFgP); // highlight foreground
    setPalette(pal);

	highlightFGPixel_ = hiliteFgP;
	highlightBGPixel_ = hiliteBgP;
	lineNumFGPixel_   = lineNoFgP;
	cursorFGPixel_    = cursorFgP;

	// Redisplay
	TextDRedisplayRect(rect_);
    redrawLineNumbersEx();
}

/*
** Cancel any drag operation that might be in progress.  Should be included
** in nearly every key event to cleanly end any dragging before edits are made
** which might change the insert position or the content of the buffer during
** a drag operation)
*/
void TextArea::cancelDrag() {
	DragStates dragState = dragState_;

	autoScrollTimer_->stop();

	switch(dragState) {
	case SECONDARY_DRAG:
	case SECONDARY_RECT_DRAG:
		buffer_->BufSecondaryUnselect();
		break;
	case PRIMARY_BLOCK_DRAG:
		CancelBlockDrag();
		break;
	case MOUSE_PAN:
		viewport()->setCursor(Qt::ArrowCursor);
		break;
	case NOT_CLICKED:
		dragState_ = DRAG_CANCELED;
		break;
	default:
		break;
	}
}

/*
** Cursor movement functions
*/
bool TextArea::TextDMoveRight() {
	if (cursorPos_ >= buffer_->BufGetLength()) {
        return false;
	}

	TextDSetInsertPosition(cursorPos_ + 1);
	return true;
}

/*
** Set the position of the text insertion cursor
*/
void TextArea::TextDSetInsertPosition(int64_t newPos) {
	// make sure new position is ok, do nothing if it hasn't changed
	if (newPos == cursorPos_) {
		return;
	}

    newPos = qBound<int64_t>(0, newPos, buffer_->BufGetLength());

	// cursor movement cancels vertical cursor motion column
	cursorPreferredCol_ = -1;

	// erase the cursor at it's previous position
	TextDBlankCursor();

	// draw it at its new position
	cursorPos_ = newPos;
	cursorOn_ = true;
	textDRedisplayRange(cursorPos_ - 1, cursorPos_ + 1);
}

void TextArea::checkAutoShowInsertPos() {

	if (P_autoShowInsertPos) {
		TextDMakeInsertPosVisible();
	}
}

/*
** Scroll the display to bring insertion cursor into view.
**
** Note: it would be nice to be able to do this without counting lines twice
** (setScroll counts them too) and/or to count from the most efficient
** starting point, but the efficiency of this routine is not as important to
** the overall performance of the text display.
*/
void TextArea::TextDMakeInsertPosVisible() {
    int hOffset;
    int64_t topLine;
    int x;
    int y;
    int64_t cursorPos    = cursorPos_;
    int64_t linesFromTop = 0;
    int cursorVPadding   = P_cursorVPadding;

	hOffset = horizOffset_;
	topLine = topLineNum_;

	// Don't do padding if this is a mouse operation
	bool do_padding = ((dragState_ == NOT_CLICKED) && (cursorVPadding > 0));

	// Find the new top line number
	if (cursorPos < firstChar_) {
		topLine -= TextDCountLines(cursorPos, firstChar_, false);
		// linesFromTop = 0;
	} else if (cursorPos > lastChar_ && !emptyLinesVisible()) {
		topLine += TextDCountLines(lastChar_ - (wrapUsesCharacter(lastChar_) ? 0 : 1), cursorPos, false);
		linesFromTop = nVisibleLines_ - 1;
	} else if (cursorPos == lastChar_ && !emptyLinesVisible() && !wrapUsesCharacter(lastChar_)) {
		topLine++;
		linesFromTop = nVisibleLines_ - 1;
	} else {
		// Avoid extra counting if cursorVPadding is disabled
		if (do_padding)
			linesFromTop = TextDCountLines(firstChar_, cursorPos, true);
	}
	if (topLine < 1) {
        qCritical("NEdit: internal consistency check tl1 failed");
		topLine = 1;
	}

	if (do_padding) {
		// Keep the cursor away from the top or bottom of screen.
        if (nVisibleLines_ <= 2 * cursorVPadding) {
			topLine += (linesFromTop - nVisibleLines_ / 2);
            topLine = std::max<int64_t>(topLine, 1);
        } else if (linesFromTop < cursorVPadding) {
			topLine -= (cursorVPadding - linesFromTop);
            topLine = std::max<int64_t>(topLine, 1);
        } else if (linesFromTop > nVisibleLines_ - cursorVPadding - 1) {
			topLine += (linesFromTop - (nVisibleLines_ - cursorVPadding - 1));
		}
	}

	/* Find the new setting for horizontal offset (this is a bit ungraceful).
	   If the line is visible, just use TextDPositionToXY to get the position
	   to scroll to, otherwise, do the vertical scrolling first, then the
	   horizontal */
	if (!TextDPositionToXY(cursorPos, &x, &y)) {
        setScroll(topLine, hOffset, true, true);
		if (!TextDPositionToXY(cursorPos, &x, &y)) {
			return; // Give up, it's not worth it (but why does it fail?)
		}
	}

    if (x > rect_.left() + rect_.width()) {
        hOffset += x - (rect_.left() + rect_.width());
    } else if (x < rect_.left()) {
        hOffset += x - rect_.left();
	}

	// Do the scroll
	setScroll(topLine, hOffset, true, true);
}

/*
** Cancel a block drag operation
*/
void TextArea::CancelBlockDrag() {

    auto origBuf = std::move(dragOrigBuf_);
    const TextSelection *origSel = &origBuf->BufGetPrimary();
    int64_t modRangeStart = -1;
    int64_t origModRangeEnd;
    int64_t bufModRangeEnd;

    /* If the operation was a move, make the modify range reflect the
       removal of the text from the starting position */
    if (dragSourceDeleted_ != 0) {
        trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragSourceDeletePos_, dragSourceInserted_, dragSourceDeleted_);
    }

    /* Include the insert being undone from the last step in the modified
       range. */
    trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragInsertPos_, dragInserted_, dragDeleted_);

    // Make the changes in the buffer
    std::string repText = origBuf->BufGetRangeEx(modRangeStart, origModRangeEnd);
    buffer_->BufReplaceEx(modRangeStart, bufModRangeEnd, repText);

    // Reset the selection and cursor position
    if (origSel->rectangular) {
        buffer_->BufRectSelect(origSel->start, origSel->end, origSel->rectStart, origSel->rectEnd);
    } else {
        buffer_->BufSelect(origSel->start, origSel->end);
    }

    TextDSetInsertPosition(buffer_->BufCursorPosHint());

    callMovedCBs();

    emTabsBeforeCursor_ = 0;

    // Free the backup buffer
    origBuf.reset();

    // Indicate end of drag
    dragState_ = DRAG_CANCELED;

    // Call finish-drag calback
    DragEndEvent endStruct;
    endStruct.startPos       = 0;
    endStruct.nCharsDeleted  = 0;
    endStruct.nCharsInserted = 0;

    for(auto &c : dragEndCallbacks_) {
        c.first(this, &endStruct, c.second);
    }
}

/*
** Do operations triggered by cursor movement: Call cursor movement callback
** procedure(s), and cancel marker indicating that the cursor is after one or
** more just-entered emulated tabs (spaces to be deleted as a unit).
*/
void TextArea::callCursorMovementCBs() {
	emTabsBeforeCursor_ = 0;
    callMovedCBs();
}

/*
** For actions involving cursor movement, "extend" keyword means incorporate
** the new cursor position in the selection, and lack of an "extend" keyword
** means cancel the existing selection
*/
void TextArea::checkMoveSelectionChange(EventFlags flags, int64_t startPos) {

	bool extend = flags & ExtendFlag;
	if (extend) {
		bool rect = flags & RectFlag;
		keyMoveExtendSelection(startPos, rect);
	} else {
        buffer_->BufUnselect();
	}
}

/*
** If a selection change was requested via a keyboard command for moving
** the insertion cursor (usually with the "extend" keyword), adjust the
** selection to include the new cursor position, or begin a new selection
** between startPos and the new cursor position with anchor at startPos.
*/
void TextArea::keyMoveExtendSelection(int64_t origPos, bool rectangular) {

    const TextSelection *sel = &buffer_->BufGetPrimary();
    int64_t newPos         = cursorPos_;
    int64_t startPos;
    int64_t endPos;
    int64_t startCol;
    int64_t endCol;
    int64_t newCol;
    int64_t origCol;
    int64_t anchor;
    int64_t rectAnchor;
    int64_t anchorLineStart;

	if ((sel->selected || sel->zeroWidth) && sel->rectangular && rectangular) {

		// rect -> rect
		newCol   = buffer_->BufCountDispChars(buffer_->BufStartOfLine(newPos), newPos);
		startCol = std::min(rectAnchor_, newCol);
		endCol   = std::max(rectAnchor_, newCol);
		startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));

        buffer_->BufRectSelect(startPos, endPos, startCol, endCol);

	} else if (sel->selected && rectangular) { // plain -> rect

		newCol = buffer_->BufCountDispChars(buffer_->BufStartOfLine(newPos), newPos);

		if (abs(newPos - sel->start) < abs(newPos - sel->end)) {
			anchor = sel->end;
		} else {
			anchor = sel->start;
		}

		anchorLineStart = buffer_->BufStartOfLine(anchor);
		rectAnchor      = buffer_->BufCountDispChars(anchorLineStart, anchor);
		anchor_         = anchor;
		rectAnchor_     = rectAnchor;

        buffer_->BufRectSelect(buffer_->BufStartOfLine(std::min(anchor, newPos)), buffer_->BufEndOfLine(std::max(anchor, newPos)), std::min(rectAnchor, newCol), std::max(rectAnchor, newCol));

	} else if (sel->selected && sel->rectangular) { // rect -> plain

		startPos = buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(sel->start), sel->rectStart);
		endPos = buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(sel->end), sel->rectEnd);

		if (abs(origPos - startPos) < abs(origPos - endPos)) {
			anchor = endPos;
		} else {
			anchor = startPos;
		}

		buffer_->BufSelect(anchor, newPos);

	} else if (sel->selected) { // plain -> plain

		if (abs(origPos - sel->start) < abs(origPos - sel->end)) {
			anchor = sel->end;
		} else {
			anchor = sel->start;
		}

		buffer_->BufSelect(anchor, newPos);

	} else if (rectangular) { // no sel -> rect

        origCol     = buffer_->BufCountDispChars(buffer_->BufStartOfLine(origPos), origPos);
        newCol      = buffer_->BufCountDispChars(buffer_->BufStartOfLine(newPos), newPos);
        startCol    = std::min(newCol, origCol);
        endCol      = std::max(newCol, origCol);
        startPos    = buffer_->BufStartOfLine(std::min(origPos, newPos));
        endPos      = buffer_->BufEndOfLine(std::max(origPos, newPos));
        anchor_     = origPos;
		rectAnchor_ = origCol;
        buffer_->BufRectSelect(startPos, endPos, startCol, endCol);

	} else { // no sel -> plain

		anchor_ = origPos;
		rectAnchor_ = buffer_->BufCountDispChars(buffer_->BufStartOfLine(origPos), origPos);
		buffer_->BufSelect(anchor_, newPos);
	}
}


bool TextArea::TextDMoveLeft() {
	if (cursorPos_ <= 0) {
        return false;
	}

	TextDSetInsertPosition(cursorPos_ - 1);
	return true;
}

bool TextArea::TextDMoveUp(bool absolute) {
    int64_t lineStartPos;
    int64_t prevLineStartPos;
    int visLineNum;

	/* Find the position of the start of the line.  Use the line starts array
	   if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (absolute) {
		lineStartPos = buffer_->BufStartOfLine(cursorPos_);
		visLineNum = -1;
    } else if (posToVisibleLineNum(cursorPos_, &visLineNum)) {
        Q_ASSERT(visLineNum >= 0);
        lineStartPos = lineStarts_[visLineNum];
    } else {
        lineStartPos = TextDStartOfLine(cursorPos_);
        visLineNum = -1;
	}

    if (lineStartPos == 0) {
        return false;
    }

	// Decide what column to move to, if there's a preferred column use that
    int64_t column = cursorPreferredCol_ >= 0 ? cursorPreferredCol_ : buffer_->BufCountDispChars(lineStartPos, cursorPos_);

	// count forward from the start of the previous line to reach the column
	if (absolute) {
		prevLineStartPos = buffer_->BufCountBackwardNLines(lineStartPos, 1);
	} else if (visLineNum != -1 && visLineNum != 0) {        
		prevLineStartPos = lineStarts_[visLineNum - 1];
	} else {
		prevLineStartPos = TextDCountBackwardNLines(lineStartPos, 1);
	}

    int64_t newPos = buffer_->BufCountForwardDispChars(prevLineStartPos, column);
    if (P_continuousWrap && !absolute) {
        newPos = std::min(newPos, TextDEndOfLine(prevLineStartPos, true));
    }

	// move the cursor
	TextDSetInsertPosition(newPos);

	// if a preferred column wasn't aleady established, establish it
	cursorPreferredCol_ = column;

	return true;
}

bool TextArea::TextDMoveDown(bool absolute) {
    int64_t lineStartPos;
    int64_t nextLineStartPos;
    int64_t newPos;
    int visLineNum;

	if (cursorPos_ == buffer_->BufGetLength()) {
        return false;
	}

	if (absolute) {
		lineStartPos = buffer_->BufStartOfLine(cursorPos_);
		visLineNum = -1;
	} else if (posToVisibleLineNum(cursorPos_, &visLineNum)) {
		lineStartPos = lineStarts_[visLineNum];
	} else {
        lineStartPos = TextDStartOfLine(cursorPos_);
		visLineNum = -1;
	}

    int64_t column = cursorPreferredCol_ >= 0 ? cursorPreferredCol_ : buffer_->BufCountDispChars(lineStartPos, cursorPos_);

	if (absolute)
		nextLineStartPos = buffer_->BufCountForwardNLines(lineStartPos, 1);
	else
		nextLineStartPos = TextDCountForwardNLines(lineStartPos, 1, true);

	newPos = buffer_->BufCountForwardDispChars(nextLineStartPos, column);

	if (P_continuousWrap && !absolute) {
        newPos = std::min(newPos, TextDEndOfLine(nextLineStartPos, true));
	}

	TextDSetInsertPosition(newPos);
	cursorPreferredCol_ = column;

	return true;
}

bool TextArea::checkReadOnly() const {
	if (P_readOnly) {
		QApplication::beep();
		return true;
	}
    return false;
}

/*
** Insert text "chars" at the cursor position, respecting pending delete
** selections, overstrike, and handling cursor repositioning as if the text
** had been typed.  If autoWrap is on wraps the text to fit within the wrap
** margin, auto-indenting where the line was wrapped (but nowhere else).
** "allowPendingDelete" controls whether primary selections in the widget are
** treated as pending delete selections (true), or ignored (false). "event"
** is optional and is just passed on to the cursor movement callbacks.
*/
void TextArea::TextInsertAtCursorEx(view::string_view chars, bool allowPendingDelete, bool allowWrap) {

    QFontMetrics fm(font_);
	int fontWidth   = fm.maxWidth();

	// Don't wrap if auto-wrap is off or suppressed, or it's just a newline
    if (!allowWrap || !P_autoWrap || chars.compare("\n") == 0) {
		simpleInsertAtCursorEx(chars, allowPendingDelete);
		return;
	}

	/* If this is going to be a pending delete operation, the real insert
	   position is the start of the selection.  This will make rectangular
	   selections wrap strangely, but this routine should rarely be used for
	   them, and even more rarely when they need to be wrapped. */
    bool replaceSel = allowPendingDelete && pendingSelection();
    int64_t cursorPos = replaceSel ? buffer_->BufGetPrimary().start : cursorPos_;

	/* If the text is only one line and doesn't need to be wrapped, just insert
	   it and be done (for efficiency only, this routine is called for each
	   character typed). (Of course, it may not be significantly more efficient
	   than the more general code below it, so it may be a waste of time!) */
    int wrapMargin   = P_wrapMargin != 0 ? P_wrapMargin : rect_.width() / fontWidth;
    int64_t lineStartPos = buffer_->BufStartOfLine(cursorPos);
    int64_t colNum       = buffer_->BufCountDispChars(lineStartPos, cursorPos);

	auto it = chars.begin();
	for (; it != chars.end() && *it != '\n'; it++) {
        colNum += TextBuffer::BufCharWidth(*it, colNum, buffer_->BufGetTabDist());
	}

    const bool singleLine = (it == chars.end());
	if (colNum < wrapMargin && singleLine) {
		simpleInsertAtCursorEx(chars, true);
		return;
	}

	// Wrap the text
    int64_t breakAt = 0;
    std::string lineStartText = buffer_->BufGetRangeEx(lineStartPos, cursorPos);
    std::string wrappedText = wrapTextEx(lineStartText, chars, lineStartPos, wrapMargin, replaceSel ? nullptr : &breakAt);

	/* Insert the text.  Where possible, use TextDInsert which is optimized
	   for less redraw. */
	if (replaceSel) {
		buffer_->BufReplaceSelectedEx(wrappedText);
        TextDSetInsertPosition(buffer_->BufCursorPosHint());
	} else if (P_overstrike) {
		if (breakAt == 0 && singleLine)
			TextDOverstrikeEx(wrappedText);
		else {
			buffer_->BufReplaceEx(cursorPos - breakAt, cursorPos, wrappedText);
            TextDSetInsertPosition(buffer_->BufCursorPosHint());
		}
	} else {
		if (breakAt == 0) {
			TextDInsertEx(wrappedText);
		} else {
			buffer_->BufReplaceEx(cursorPos - breakAt, cursorPos, wrappedText);
            TextDSetInsertPosition(buffer_->BufCursorPosHint());
		}
	}
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/*
** Return true if pending delete is on and there's a selection contiguous
** with the cursor ready to be deleted.  These criteria are used to decide
** if typing a character or inserting something should delete the selection
** first.
*/
bool TextArea::pendingSelection() const {
    const TextSelection *sel = &buffer_->BufGetPrimary();
    int64_t pos = cursorPos_;

	return P_pendingDelete && sel->selected && pos >= sel->start && pos <= sel->end;
}

/*
** Wrap multi-line text in argument "text" to be inserted at the end of the
** text on line "startLine" and return the result.  If "breakBefore" is
** non-nullptr, allow wrapping to extend back into "startLine", in which case
** the returned text will include the wrapped part of "startLine", and
** "breakBefore" will return the number of characters at the end of
** "startLine" that were absorbed into the returned string.  "breakBefore"
** will return zero if no characters were absorbed into the returned string.
** The buffer offset of text in the widget's text buffer is needed so that
** smart indent (which can be triggered by wrapping) can search back farther
** in the buffer than just the text in startLine.
*/
std::string TextArea::wrapTextEx(view::string_view startLine, view::string_view text, int64_t bufOffset, int wrapMargin, int64_t *breakBefore) {

    int64_t startLineLen = startLine.size();
    int64_t breakAt;
    int64_t charsAdded;
    int64_t firstBreak = -1;
    int tabDist = buffer_->BufGetTabDist();
	std::string wrappedText;

	// Create a temporary text buffer and load it with the strings
    TextBuffer wrapBuf;
    wrapBuf.BufInsertEx(0, startLine);
    wrapBuf.BufAppendEx(text);

	/* Scan the buffer for long lines and apply wrapLine when wrapMargin is
	   exceeded.  limitPos enforces no breaks in the "startLine" part of the
	   string (if requested), and prevents re-scanning of long unbreakable
	   lines for each character beyond the margin */
    int64_t colNum       = 0;
    int64_t pos          = 0;
    int64_t lineStartPos = 0;
    int64_t limitPos     = (breakBefore == nullptr) ? startLineLen : 0;

    while (pos < wrapBuf.BufGetLength()) {
        char c = wrapBuf.BufGetCharacter(pos);
		if (c == '\n') {
			lineStartPos = limitPos = pos + 1;
			colNum = 0;
		} else {
            colNum += TextBuffer::BufCharWidth(c, colNum, tabDist);
			if (colNum > wrapMargin) {
                if (!wrapLine(&wrapBuf, bufOffset, lineStartPos, pos, limitPos, &breakAt, &charsAdded)) {
					limitPos = std::max(pos, limitPos);
				} else {
					lineStartPos = limitPos = breakAt + 1;
					pos += charsAdded;
                    colNum = wrapBuf.BufCountDispChars(lineStartPos, pos + 1);
					if (firstBreak == -1)
						firstBreak = breakAt;
				}
			}
		}
		pos++;
	}

	// Return the wrapped text, possibly including part of startLine
	if(!breakBefore) {
        wrappedText = wrapBuf.BufGetRangeEx(startLineLen, wrapBuf.BufGetLength());
	} else {
		*breakBefore = firstBreak != -1 && firstBreak < startLineLen ? startLineLen - firstBreak : 0;
        wrappedText = wrapBuf.BufGetRangeEx(startLineLen - *breakBefore, wrapBuf.BufGetLength());
	}
	return wrappedText;
}

/*
** Insert "text" (which must not contain newlines), overstriking the current
** cursor location.
*/
void TextArea::TextDOverstrikeEx(view::string_view text) {

    int64_t startPos    = cursorPos_;
    int64_t lineStart   = buffer_->BufStartOfLine(startPos);
    int textLen         = gsl::narrow<int>(text.size());
    int64_t p;
    int64_t endPos;

	std::string paddedText;
	bool paddedTextSet = false;

	// determine how many displayed character positions are covered
    int64_t startIndent = buffer_->BufCountDispChars(lineStart, startPos);
    int64_t indent = startIndent;
	for (char ch : text) {
        indent += TextBuffer::BufCharWidth(ch, indent, buffer_->BufGetTabDist());
	}
    int64_t endIndent = indent;

	/* find which characters to remove, and if necessary generate additional
	   padding to make up for removed control characters at the end */
	indent = startIndent;
	for (p = startPos;; p++) {
		if (p == buffer_->BufGetLength())
			break;
		char ch = buffer_->BufGetCharacter(p);
		if (ch == '\n')
			break;
        indent += TextBuffer::BufCharWidth(ch, indent, buffer_->BufGetTabDist());
		if (indent == endIndent) {
			p++;
			break;
		} else if (indent > endIndent) {
			if (ch != '\t') {
				p++;

				std::string padded;
				padded.reserve(text.size() + (indent - endIndent));

				padded.append(text.begin(), text.end());
				padded.append(indent - endIndent, ' ');
				paddedText = std::move(padded);
				paddedTextSet = true;
			}
			break;
		}
	}
	endPos = p;

	cursorToHint_ = startPos + textLen;
	buffer_->BufReplaceEx(startPos, endPos, !paddedTextSet ? text : paddedText);
	cursorToHint_ = NO_HINT;
}

/*
** Insert "text" at the current cursor location.  This has the same
** effect as inserting the text into the buffer using BufInsertEx and
** then moving the insert position after the newly inserted text, except
** that it's optimized to do less redrawing.
*/
void TextArea::TextDInsertEx(view::string_view text) {
    int64_t pos = cursorPos_;

    cursorToHint_ = pos + static_cast<int64_t>(text.size());
	buffer_->BufInsertEx(pos, text);
	cursorToHint_ = NO_HINT;
}

/*
** Wraps the end of a line beginning at lineStartPos and ending at lineEndPos
** in "buf", at the last white-space on the line >= limitPos.  (The implicit
** assumption is that just the last character of the line exceeds the wrap
** margin, and anywhere on the line we can wrap is correct).  Returns false if
** unable to wrap the line.  "breakAt", returns the character position at
** which the line was broken,
**
** Auto-wrapping can also trigger auto-indent.  The additional parameter
** bufOffset is needed when auto-indent is set to smart indent and the smart
** indent routines need to scan far back in the buffer.  "charsAdded" returns
** the number of characters added to acheive the auto-indent.  wrapMargin is
** used to decide whether auto-indent should be skipped because the indent
** string itself would exceed the wrap margin.
*/
int TextArea::wrapLine(TextBuffer *buf, int64_t bufOffset, int64_t lineStartPos, int64_t lineEndPos, int64_t limitPos, int64_t *breakAt, int64_t *charsAdded) {

    int64_t p;
	int column;

	/* Scan backward for whitespace or BOL.  If BOL, return false, no
	   whitespace in line at which to wrap */
    for (p = lineEndPos;; p--) {
		if (p < lineStartPos || p < limitPos) {
            return false;
		}

		char c = buf->BufGetCharacter(p);
		if (c == '\t' || c == ' ') {
			break;
		}
	}

	/* Create an auto-indent string to insert to do wrap.  If the auto
	   indent string reaches the wrap position, slice the auto-indent
	   back off and return to the left margin */
	std::string indentStr;
	if (P_autoIndent || P_smartIndent) {
		indentStr = createIndentStringEx(buf, bufOffset, lineStartPos, lineEndPos, &column);
		if (column >= p - lineStartPos) {
			indentStr.resize(1);
		}
	} else {
		indentStr = "\n";
	}

	/* Replace the whitespace character with the auto-indent string
	   and return the stats */
	buf->BufReplaceEx(p, p + 1, indentStr);

	*breakAt = p;
    *charsAdded = indentStr.size() - 1;
	return true;
}

/*
** Create and return an auto-indent string to add a newline at lineEndPos to a
** line starting at lineStartPos in buf.  "buf" may or may not be the real
** text buffer for the widget.  If it is not the widget's text buffer it's
** offset position from the real buffer must be specified in "bufOffset" to
** allow the smart-indent routines to scan back as far as necessary. The
** string length is returned in "length" (or "length" can be passed as nullptr,
** and the indent column is returned in "column" (if non nullptr).
*/
std::string TextArea::createIndentStringEx(TextBuffer *buf, int64_t bufOffset, int64_t lineStartPos, int64_t lineEndPos, int *column) {

	int indent = -1;
    int tabDist = buffer_->BufGetTabDist();
	int i;
    int useTabs = buffer_->BufGetUseTabs();
	
	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (P_smartIndent && (lineStartPos == 0 || buf == buffer_)) {
		SmartIndentEvent smartIndent;
		smartIndent.reason        = NEWLINE_INDENT_NEEDED;
		smartIndent.pos           = lineEndPos + bufOffset;
		smartIndent.indentRequest = 0;
        smartIndent.charsTyped    = view::string_view();

		for(auto &c : smartIndentCallbacks_) {
			c.first(this, &smartIndent, c.second);
		}

		indent = smartIndent.indentRequest;
	}

	// If smart indent wasn't used, measure the indent distance of the line
	if (indent == -1) {
		indent = 0;
        for (int64_t pos = lineStartPos; pos < lineEndPos; pos++) {
			char c = buf->BufGetCharacter(pos);
			if (c != ' ' && c != '\t') {
				break;
			}

			if (c == '\t') {
				indent += tabDist - (indent % tabDist);
			} else {
				indent++;
			}
		}
	}

	// Allocate and create a string of tabs and spaces to achieve the indent
	std::string indentStr;
    indentStr.reserve(static_cast<size_t>(indent + 2));

	auto indentPtr = std::back_inserter(indentStr);

	*indentPtr++ = '\n';
	if (useTabs) {
		for (i = 0; i < indent / tabDist; i++)
			*indentPtr++ = '\t';
		for (i = 0; i < indent % tabDist; i++)
			*indentPtr++ = ' ';
	} else {
		for (i = 0; i < indent; i++)
			*indentPtr++ = ' ';
	}

	// Return any requested stats
	if(column) {
		*column = indent;
	}

	return indentStr;
}

void TextArea::newlineNoIndentAP(EventFlags flags) {

    EMIT_EVENT_0("newline_no_indent");

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	simpleInsertAtCursorEx("\n", true);
    buffer_->BufUnselect();
}

void TextArea::newlineAndIndentAP(EventFlags flags) {

    EMIT_EVENT_0("newline_and_indent");

	int column;

	if (checkReadOnly()) {
		return;
	}

	cancelDrag();

	/* Create a string containing a newline followed by auto or smart
	   indent string */
    int64_t cursorPos = cursorPos_;
    int64_t lineStartPos = buffer_->BufStartOfLine(cursorPos);
    std::string indentStr = createIndentStringEx(buffer_, 0, lineStartPos, cursorPos, &column);

	// Insert it at the cursor
	simpleInsertAtCursorEx(indentStr, true);

	if (P_emulateTabs > 0) {
		/*  If emulated tabs are on, make the inserted indent deletable by
			tab. Round this up by faking the column a bit to the right to
			let the user delete half-tabs with one keypress.  */
		column += P_emulateTabs - 1;
		emTabsBeforeCursor_ = column / P_emulateTabs;
	}

    buffer_->BufUnselect();
}

/*
** Check if tab emulation is on and if there are emulated tabs before the
** cursor, and if so, delete an emulated tab as a unit.  Also finishes up
** by calling checkAutoShowInsertPos and callCursorMovementCBs, so the
** calling action proc can just return (this is necessary to preserve
** emTabsBeforeCursor which is otherwise cleared by callCursorMovementCBs).
*/
int TextArea::deleteEmulatedTab() {

	int emTabDist          = P_emulateTabs;
	int emTabsBeforeCursor = emTabsBeforeCursor_;
    int64_t startIndent;
    int64_t toIndent;
    int64_t insertPos;
    int64_t startPos;
    int64_t lineStart;
    int64_t pos;
	int indent;
	int startPosIndent;
	char c;

    if (emTabDist <= 0 || emTabsBeforeCursor <= 0) {
        return false;
    }

	// Find the position of the previous tab stop
	insertPos = cursorPos_;
	lineStart = buffer_->BufStartOfLine(insertPos);
	startIndent = buffer_->BufCountDispChars(lineStart, insertPos);
	toIndent = (startIndent - 1) - ((startIndent - 1) % emTabDist);

	/* Find the position at which to begin deleting (stop at non-whitespace
	   characters) */
	startPosIndent = indent = 0;
	startPos = lineStart;
	for (pos = lineStart; pos < insertPos; pos++) {
		c = buffer_->BufGetCharacter(pos);
        indent += TextBuffer::BufCharWidth(c, indent, buffer_->BufGetTabDist());
		if (indent > toIndent)
			break;
		startPosIndent = indent;
		startPos = pos + 1;
	}

	// Just to make sure, check that we're not deleting any non-white chars
	for (pos = insertPos - 1; pos >= startPos; pos--) {
		c = buffer_->BufGetCharacter(pos);
		if (c != ' ' && c != '\t') {
			startPos = pos + 1;
			break;
		}
	}

	/* Do the text replacement and reposition the cursor.  If any spaces need
	   to be inserted to make up for a deleted tab, do a BufReplaceEx, otherwise,
	   do a BufRemove. */
	if (startPosIndent < toIndent) {

        std::string spaceString(static_cast<size_t>(toIndent - startPosIndent), ' ');

		buffer_->BufReplaceEx(startPos, insertPos, spaceString);
		TextDSetInsertPosition(startPos + toIndent - startPosIndent);
	} else {
		buffer_->BufRemove(startPos, insertPos);
		TextDSetInsertPosition(startPos);
	}

	/* The normal cursor movement stuff would usually be called by the action
	   routine, but this wraps around it to restore emTabsBeforeCursor */
	checkAutoShowInsertPos();
	callCursorMovementCBs();

	/* Decrement and restore the marker for consecutive emulated tabs, which
	   would otherwise have been zeroed by callCursorMovementCBs */
	emTabsBeforeCursor_ = emTabsBeforeCursor - 1;
    return true;
}

/*
** If there's a selection, delete it and position the cursor where the
** selection was deleted.  (Called by routines which do deletion to check
** first for and do possible selection delete)
*/
bool TextArea::deletePendingSelection() {

    if (buffer_->BufGetPrimary().selected) {
		buffer_->BufRemoveSelected();
        TextDSetInsertPosition(buffer_->BufCursorPosHint());
		checkAutoShowInsertPos();
		callCursorMovementCBs();
		return true;
    } else {
        return false;
    }
}

int64_t TextArea::startOfWord(int64_t pos) const {

    if(buffer_->BufIsEmpty()) {
        return 0;
    }

    const char ch = buffer_->BufGetCharacter(pos);

    boost::optional<int64_t> startPos;

    if (ch == ' ' || ch == '\t') {
        startPos = spanBackward(buffer_, pos, view::string_view(" \t", 2), false);
    } else if (P_delimiters.find(ch) != std::string::npos) {
        startPos = spanBackward(buffer_, pos, P_delimiters, true);
	} else {
        startPos = buffer_->BufSearchBackwardEx(pos, P_delimiters);
	}

    if(!startPos) {
        return 0;
    }

    return std::min(pos, *startPos + 1);
}

int64_t TextArea::endOfWord(int64_t pos) const {

    if(buffer_->BufIsEmpty()) {
        return 0;
    }

    const char ch = buffer_->BufGetCharacter(pos);

    boost::optional<int64_t> endPos;

    if (ch == ' ' || ch == '\t') {
        endPos = spanForward(buffer_, pos, view::string_view(" \t", 2), false);
    } else if (P_delimiters.find(ch) != std::string::npos) {
        endPos = spanForward(buffer_, pos, P_delimiters, true);
	} else {
        endPos = buffer_->BufSearchForwardEx(pos, P_delimiters);
	}

    if (!endPos) {
        return buffer_->BufGetLength();
    }

    return *endPos;
}

/*
** Search backwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character BEFORE "startPos". If
** ignoreSpace is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
boost::optional<int64_t> TextArea::spanBackward(TextBuffer *buf, int64_t startPos, view::string_view searchChars, bool ignoreSpace) const {

	if (startPos == 0) {
        return boost::none;
	}

    int64_t pos = startPos - 1;
	while (pos >= 0) {

        auto it = std::find_if(searchChars.begin(), searchChars.end(), [ignoreSpace, buf, pos](char ch) {
            if (!(ignoreSpace && (ch == ' ' || ch == '\t' || ch == '\n'))) {
                if (buf->BufGetCharacter(pos) == ch) {
                    return true;
                }
            }

            return false;
        });

        if(it == searchChars.end()) {
            return pos;
		}

        pos--;
	}

    return boost::none;
}

/*
** Search forwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character "startPos". If ignoreSpace
** is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
boost::optional<int64_t> TextArea::spanForward(TextBuffer *buf, int64_t startPos, view::string_view searchChars, bool ignoreSpace) const {

    int64_t pos = startPos;
	while (pos < buf->BufGetLength()) {

        auto it = std::find_if(searchChars.begin(), searchChars.end(), [ignoreSpace, buf, pos](char ch) {
            if (!(ignoreSpace && (ch == ' ' || ch == '\t' || ch == '\n'))) {
                if (buf->BufGetCharacter(pos) == ch) {
                    return true;
                }
            }
            return false;
        });

        if (it == searchChars.end()) {
            return pos;
		}
		pos++;
	}

    return boost::none;
}

void TextArea::processShiftUpAP(EventFlags flags) {

    EMIT_EVENT_0("process_shift_up");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;
	bool abs    = flags & AbsoluteFlag;

	cancelDrag();

	if (!TextDMoveUp(abs)) {
		ringIfNecessary(silent);
	}

	keyMoveExtendSelection(insertPos, flags & RectFlag);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::processShiftDownAP(EventFlags flags) {

    EMIT_EVENT_0("process_shift_down");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;
	bool abs    = flags & AbsoluteFlag;

	cancelDrag();
	if (!TextDMoveDown(abs))
		ringIfNecessary(silent);
	keyMoveExtendSelection(insertPos, flags & RectFlag);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::keySelectAP(EventFlags flags) {

    EMIT_EVENT_0("key_select");

	int stat;
    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();

	if (flags & LeftFlag){
		stat = TextDMoveLeft();
	} else if (flags & RightFlag) {
		stat = TextDMoveRight();
	} else if (flags & UpFlag) {
		stat = TextDMoveUp(false);
	} else if (flags & DownFlag) {
		stat = TextDMoveDown(false);
	} else {
		keyMoveExtendSelection(insertPos, flags & RectFlag);
		return;
	}

	if (!stat) {
		ringIfNecessary(silent);
	} else {
		keyMoveExtendSelection(insertPos, flags & RectFlag);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::TextCutClipboard() {

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

    if (!buffer_->BufGetPrimary().selected) {
		QApplication::beep();
		return;
	}

	CopyToClipboard();
	buffer_->BufRemoveSelected();
    TextDSetInsertPosition(buffer_->BufCursorPosHint());
	checkAutoShowInsertPos();
}

/*
** Copy the primary selection to the clipboard
*/
void TextArea::CopyToClipboard() {

	// Get the selected text, if there's no selection, do nothing
	std::string text = buffer_->BufGetSelectionTextEx();
	if (text.empty()) {
		return;
	}

	QApplication::clipboard()->setText(QString::fromStdString(text));
}

void TextArea::TextPasteClipboard() {
	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	InsertClipboard(false);
	callCursorMovementCBs();
}

void TextArea::TextColPasteClipboard() {
	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	InsertClipboard(true);
	callCursorMovementCBs();
}

/*
** Insert the X CLIPBOARD selection at the cursor position.  If isColumnar,
** do an BufInsertColEx for a columnar paste instead of BufInsertEx.
*/
void TextArea::InsertClipboard(bool isColumnar) {

	auto buf = buffer_;

	const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Clipboard);
	if(!mimeData->hasText()) {
		return;
	}

	std::string contents = mimeData->text().toStdString();

	// Insert it in the text widget
    if (isColumnar && !buf->BufGetPrimary().selected) {
        int64_t cursorPos = cursorPos_;
        int64_t cursorLineStart = buf->BufStartOfLine(cursorPos);
        int64_t column = buf->BufCountDispChars(cursorLineStart, cursorPos);

		if (P_overstrike) {
			buf->BufOverlayRectEx(cursorLineStart, column, -1, contents, nullptr, nullptr);
		} else {
			buf->BufInsertColEx(column, cursorLineStart, contents, nullptr, nullptr);
		}
		TextDSetInsertPosition(buf->BufCountForwardDispChars(cursorLineStart, column));
		if (P_autoShowInsertPos)
			TextDMakeInsertPosVisible();
	} else {
		TextInsertAtCursorEx(contents, true, P_autoWrapPastedText);
	}
}

/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursorEx, but without the complicated auto-wrap
** scanning and re-formatting.
*/
void TextArea::simpleInsertAtCursorEx(view::string_view chars, bool allowPendingDelete) {

	if (allowPendingDelete && pendingSelection()) {
		buffer_->BufReplaceSelectedEx(chars);
        TextDSetInsertPosition(buffer_->BufCursorPosHint());
	} else if (P_overstrike) {

		size_t index = chars.find('\n');
		if(index != view::string_view::npos) {
			TextDInsertEx(chars);
		} else {
			TextDOverstrikeEx(chars);
		}
	} else {
		TextDInsertEx(chars);
	}

	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::copyPrimaryAP(EventFlags flags) {

    EMIT_EVENT_0("copy_primary");

    const TextSelection *primary = &buffer_->BufGetPrimary();
	bool rectangular = flags & RectFlag;
    int64_t insertPos;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (primary->selected && rectangular) {
		std::string textToCopy = buffer_->BufGetSelectionTextEx();
		insertPos = cursorPos_;
        int64_t col = buffer_->BufCountDispChars(buffer_->BufStartOfLine(insertPos), insertPos);
		buffer_->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
        TextDSetInsertPosition(buffer_->BufCursorPosHint());

		checkAutoShowInsertPos();
	} else if (primary->selected) {
		std::string textToCopy = buffer_->BufGetSelectionTextEx();
		insertPos = cursorPos_;
		buffer_->BufInsertEx(insertPos, textToCopy);
        TextDSetInsertPosition(insertPos + textToCopy.size());

		checkAutoShowInsertPos();
	} else if (rectangular) {
        if (!TextDPositionToXY(cursorPos_, &btnDownCoord_)) {
			return; // shouldn't happen
		}

		InsertPrimarySelection(true);
	} else {
		InsertPrimarySelection(false);
	}
}

/*
** Insert the X PRIMARY selection (from whatever window currently owns it)
** at the cursor position.
*/
void TextArea::InsertPrimarySelection(bool isColumnar) {

    if(!QApplication::clipboard()->supportsSelection()) {
        QApplication::beep();
        return;
    }

	const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
	if(!mimeData->hasText()) {
		return;
	}

	std::string string = mimeData->text().toStdString();

	// Insert it in the text widget
	if (isColumnar) {
        int64_t column;
        int64_t row;
        int64_t cursorPos       = cursorPos_;
        int64_t cursorLineStart = buffer_->BufStartOfLine(cursorPos);

		TextDXYToUnconstrainedPosition(
			btnDownCoord_,
			&row,
			&column);

		buffer_->BufInsertColEx(column, cursorLineStart, string, nullptr, nullptr);
        TextDSetInsertPosition(buffer_->BufCursorPosHint());
	} else {
		TextInsertAtCursorEx(string, false, P_autoWrapPastedText);
	}
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
void TextArea::TextDXYToUnconstrainedPosition(const QPoint &coord, int64_t *row, int64_t *column) const {
    xyToUnconstrainedPos(coord.x(), coord.y(), row, column, PositionTypes::CURSOR_POS);
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font is
** proportional, since there are no absolute columns.  The parameter posType
** specifies how to interpret the position: CURSOR_POS means translate the
** coordinates to the nearest position between characters, and CHARACTER_POS
** means translate the position to the nearest character cell.
*/
void TextArea::xyToUnconstrainedPos(const QPoint &pos, int64_t *row, int64_t *column, PositionTypes posType) const {
    xyToUnconstrainedPos(pos.x(), pos.y(), row, column, posType);
}

void TextArea::xyToUnconstrainedPos(int x, int y, int64_t *row, int64_t *column, PositionTypes posType) const {

    QFontMetrics fm(font_);
    int fontHeight = ascent_ + descent_;
	int fontWidth = fm.maxWidth();

	// Find the visible line number corresponding to the y coordinate
    *row = (y - rect_.top()) / fontHeight;

    if (*row < 0) {
		*row = 0;
    }

    if (*row >= nVisibleLines_) {
		*row = nVisibleLines_ - 1;
    }

    *column = ((x - rect_.left()) + horizOffset_ + (posType == PositionTypes::CURSOR_POS ? fontWidth / 2 : 0)) / fontWidth;

    if (*column < 0) {
		*column = 0;
    }
}

void TextArea::beginningOfFileAP(EventFlags flags) {

    EMIT_EVENT_0("beginning_of_file");

    int64_t insertPos = cursorPos_;

	cancelDrag();
	if (flags & ScrollbarFlag) {
		if (topLineNum_ != 1) {
			TextDSetScroll(1, horizOffset_);
		}
	} else {
		TextDSetInsertPosition(0);
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::endOfFileAP(EventFlags flags) {

    EMIT_EVENT_0("end_of_file");

    int64_t insertPos = cursorPos_;

	cancelDrag();
	if (flags & ScrollbarFlag) {
        int64_t lastTopLine = std::max<int64_t>(1, nBufferLines_ - (nVisibleLines_ - 2) + P_cursorVPadding);
		if (lastTopLine != topLineNum_) {
			TextDSetScroll(lastTopLine, horizOffset_);
		}
	} else {
		TextDSetInsertPosition(buffer_->BufGetLength());
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::backwardWordAP(EventFlags flags) {

    EMIT_EVENT_0("backward_word");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}

    int64_t pos = std::max<int64_t>(insertPos - 1, 0);
    while (P_delimiters.find(buffer_->BufGetCharacter(pos)) != std::string::npos && pos > 0) {
		pos--;
	}

	pos = startOfWord(pos);

	TextDSetInsertPosition(pos);
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::forwardWordAP(EventFlags flags) {

    EMIT_EVENT_0("forward_word");

    int64_t insertPos = cursorPos_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == buffer_->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}

    int64_t pos = insertPos;

	if (flags & TailFlag) {
		for (; pos < buffer_->BufGetLength(); pos++) {
            if (P_delimiters.find(buffer_->BufGetCharacter(pos)) == std::string::npos) {
				break;
			}
		}
        if (P_delimiters.find(buffer_->BufGetCharacter(pos)) == std::string::npos) {
			pos = endOfWord(pos);
		}
	} else {
        if (P_delimiters.find(buffer_->BufGetCharacter(pos)) == std::string::npos) {
			pos = endOfWord(pos);
		}
		for (; pos < buffer_->BufGetLength(); pos++) {
            if (P_delimiters.find(buffer_->BufGetCharacter(pos)) == std::string::npos) {
				break;
			}
		}
	}

	TextDSetInsertPosition(pos);
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::forwardParagraphAP(EventFlags flags) {

    EMIT_EVENT_0("forward_paragraph");

    int64_t pos;
    int64_t insertPos = cursorPos_;
	static const char whiteChars[] = " \t";
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == buffer_->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}

    pos = std::min(buffer_->BufEndOfLine(insertPos) + 1, buffer_->BufGetLength());
	while (pos < buffer_->BufGetLength()) {
		char c = buffer_->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos++;
		else
            pos = std::min(buffer_->BufEndOfLine(pos) + 1, buffer_->BufGetLength());
	}
	TextDSetInsertPosition(std::min(pos + 1, buffer_->BufGetLength()));
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::backwardParagraphAP(EventFlags flags) {

    EMIT_EVENT_0("backward_paragraph");

    int64_t parStart;
    int64_t pos;
    int64_t insertPos = cursorPos_;
	static const char whiteChars[] = " \t";
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}
    parStart = buffer_->BufStartOfLine(std::max<int64_t>(insertPos - 1, 0));
    pos = std::max<int64_t>(parStart - 2, 0);
	while (pos > 0) {
		char c = buffer_->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos--;
		else {
			parStart = buffer_->BufStartOfLine(pos);
            pos = std::max<int64_t>(parStart - 2, 0);
		}
	}
	TextDSetInsertPosition(parStart);
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::processTabAP(EventFlags flags) {

    EMIT_EVENT_0("process_tab");

    const TextSelection *sel = &buffer_->BufGetPrimary();
	int emTabDist          = P_emulateTabs;
	int emTabsBeforeCursor = emTabsBeforeCursor_;
    int64_t indent;

	if (checkReadOnly()) {
		return;
	}

	cancelDrag();

	// If emulated tabs are off, just insert a tab
	if (emTabDist <= 0) {
		TextInsertAtCursorEx("\t", true, true);
		return;
	}

	/* Find the starting and ending indentation.  If the tab is to
	   replace an existing selection, use the start of the selection
	   instead of the cursor position as the indent.  When replacing
	   rectangular selections, tabs are automatically recalculated as
	   if the inserted text began at the start of the line */
    int64_t insertPos = pendingSelection() ? sel->start : cursorPos_;
    int64_t lineStart = buffer_->BufStartOfLine(insertPos);

	if (pendingSelection() && sel->rectangular) {
		insertPos = buffer_->BufCountForwardDispChars(lineStart, sel->rectStart);
	}

    int64_t startIndent = buffer_->BufCountDispChars(lineStart, insertPos);
    int64_t toIndent = startIndent + emTabDist - (startIndent % emTabDist);
	if (pendingSelection() && sel->rectangular) {
		toIndent -= startIndent;
		startIndent = 0;
	}

	// Allocate a buffer assuming all the inserted characters will be spaces
	std::string outStr;
    outStr.reserve(static_cast<size_t>(toIndent - startIndent));

	// Add spaces and tabs to outStr until it reaches toIndent
	auto outPtr = std::back_inserter(outStr);
	indent = startIndent;
	while (indent < toIndent) {
        int64_t tabWidth = TextBuffer::BufCharWidth('\t', indent, buffer_->BufGetTabDist());
        if (buffer_->BufGetUseTabs() && tabWidth > 1 && indent + tabWidth <= toIndent) {
			*outPtr++ = '\t';
			indent += tabWidth;
		} else {
			*outPtr++ = ' ';
			indent++;
		}
	}

	// Insert the emulated tab
	TextInsertAtCursorEx(outStr, true, true);

	// Restore and ++ emTabsBeforeCursor cleared by TextInsertAtCursorEx
	emTabsBeforeCursor_ = emTabsBeforeCursor + 1;

    buffer_->BufUnselect();
}

/*
** Select the word or whitespace adjacent to the cursor, and move the cursor
** to its end.  pointerX is used as a tie-breaker, when the cursor is at the
** boundary between a word and some white-space.  If the cursor is on the
** left, the word or space on the left is used.  If it's on the right, that
** is used instead.
*/
void TextArea::selectWord(int pointerX) {

    int x;
    int y;
    int64_t insertPos = cursorPos_;

    if(TextDPositionToXY(insertPos, &x, &y)) {
        if (pointerX < x && insertPos > 0 && buffer_->BufGetCharacter(insertPos - 1) != '\n') {
            --insertPos;
        }

        buffer_->BufSelect(startOfWord(insertPos), endOfWord(insertPos));
    }
}

/*
** Select the line containing the cursor, including the terminating newline,
** and move the cursor to its end.
*/
void TextArea::selectLine() {

    int64_t insertPos = cursorPos_;

    int64_t endPos = buffer_->BufEndOfLine(insertPos);
    int64_t startPos = buffer_->BufStartOfLine(insertPos);

    buffer_->BufSelect(startPos, std::min(endPos + 1, buffer_->BufGetLength()));
	TextDSetInsertPosition(endPos);
}

void TextArea::moveDestinationAP(QMouseEvent *event) {

    // Move the cursor
    TextDSetInsertPosition(TextDXYToPosition(event->pos()));
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/*
** Translate window coordinates to the nearest text cursor position.
*/
int64_t TextArea::TextDXYToPosition(const QPoint &coord) const {
    return xyToPos(coord, PositionTypes::CURSOR_POS);
}

/*
** Translate window coordinates to the nearest (insert cursor or character
** cell) text position.  The parameter posType specifies how to interpret the
** position: CURSOR_POS means translate the coordinates to the nearest cursor
** position, and CHARACTER_POS means return the position of the character
** closest to (x, y).
*/
int64_t TextArea::xyToPos(const QPoint &pos, PositionTypes posType) const {
    return xyToPos(pos.x(), pos.y(), posType);
}

int64_t TextArea::xyToPos(int x, int y, PositionTypes posType) const {

	// Find the visible line number corresponding to the y coordinate
    int fontHeight = ascent_ + descent_;
    int visLineNum = (y - rect_.top()) / fontHeight;

    if (visLineNum < 0) {
		return firstChar_;
    }

    if (visLineNum >= nVisibleLines_) {
		visLineNum = nVisibleLines_ - 1;
    }

	// Find the position at the start of the line
    int64_t lineStart = lineStarts_[visLineNum];

	// If the line start was empty, return the last position in the buffer
    if (lineStart == -1) {
		return buffer_->BufGetLength();
    }

	// Get the line text and its length
    int64_t lineLen = visLineLength(visLineNum);
	std::string lineStr = buffer_->BufGetRangeEx(lineStart, lineStart + lineLen);

	/* Step through character positions from the beginning of the line
	   to find the character position corresponding to the x coordinate */
    int64_t xStep = rect_.left() - horizOffset_;
    int outIndex = 0;
    for (int charIndex = 0; charIndex < lineLen; charIndex++) {

        char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];

        const int64_t charLen   = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, buffer_->BufGetTabDist());
        const int charStyle     = styleOfPos(lineStart, lineLen, charIndex, outIndex, lineStr[charIndex]);
        const int64_t charWidth = stringWidth(expandedChar, charLen, charStyle);

        if (x < xStep + (posType == PositionTypes::CURSOR_POS ? charWidth / 2 : charWidth)) {
			return lineStart + charIndex;
		}
		xStep += charWidth;
		outIndex += charLen;
	}

	/* If the x position was beyond the end of the line, return the position
	   of the newline at the end of the line */
	return lineStart + lineLen;
}

/*
** Correct a column number based on an unconstrained position (as returned by
** TextDXYToUnconstrainedPosition) to be relative to the last actual newline
** in the buffer before the row and column position given, rather than the
** last line start created by line wrapping.  This is an adapter
** for rectangular selections and code written before continuous wrap mode,
** which thinks that the unconstrained column is the number of characters
** from the last newline.  Obviously this is time consuming, because it
** invloves character re-counting.
*/
int64_t TextArea::TextDOffsetWrappedColumn(int64_t row, int64_t column) const {

	if (!P_continuousWrap || row < 0 || row > nVisibleLines_) {
		return column;
	}

    const int64_t dispLineStart = lineStarts_[static_cast<int>(row)];
	if (dispLineStart == -1) {
		return column;
	}

    const int64_t lineStart = buffer_->BufStartOfLine(dispLineStart);
	return column + buffer_->BufCountDispChars(lineStart, dispLineStart);
}

/*
** Reset drag state and cancel the auto-scroll timer
*/
void TextArea::endDrag() {

	autoScrollTimer_->stop();

	if (dragState_ == MOUSE_PAN) {
		viewport()->setCursor(Qt::ArrowCursor);
	}

	dragState_ = NOT_CLICKED;
}

bool TextArea::clickTracker(QMouseEvent *event, bool inDoubleClickHandler) {
    // track mouse click count
    clickTimer_->start(QApplication::doubleClickInterval());

    if (clickCount_ < 4 && clickPos_ == event->pos()) {
        clickCount_++;
    } else {
        clickCount_ = 0;
    }

    clickPos_ = event->pos();

    switch (clickCount_) {
    case 1:
        return true;
    case 2:
        if (inDoubleClickHandler) {
            return true;
        } else {
            mouseDoubleClickEvent(event);
            return false;
        }
    case 3:
        mouseTripleClickEvent(event);
        return false;
    case 4:
        mouseQuadrupleClickEvent(event);
        return false;
    }

    return true;
}

/**
 * @brief TextArea::clickTimeout
 */
void TextArea::clickTimeout() {
    clickCount_ = 0;
}

/**
 * @brief TextArea::selectAllAP
 * @param flags
 */
void TextArea::selectAllAP(EventFlags flags) {

    EMIT_EVENT_0("select_all");

	cancelDrag();
	buffer_->BufSelect(0, buffer_->BufGetLength());
}

/**
 * @brief TextArea::deselectAllAP
 * @param flags
 */
void TextArea::deselectAllAP(EventFlags flags) {

    EMIT_EVENT_0("deselect_all");

	cancelDrag();
    buffer_->BufUnselect();
}

/**
 * @brief TextArea::extendStartAP
 * @param event
 * @param flags
 */
void TextArea::extendStartAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("extend_start");

    const TextSelection *sel = &buffer_->BufGetPrimary();
    int64_t anchor;
    int64_t rectAnchor;
    int64_t anchorLineStart;
    int64_t row;
    int64_t column;

	// Find the new anchor point for the rest of this drag operation
    int64_t newPos = TextDXYToPosition(event->pos());
    TextDXYToUnconstrainedPosition(event->pos(), &row, &column);
	column = TextDOffsetWrappedColumn(row, column);
	if (sel->selected) {
		if (sel->rectangular) {

			rectAnchor = column < (sel->rectEnd + sel->rectStart) / 2 ? sel->rectEnd : sel->rectStart;
			anchorLineStart = buffer_->BufStartOfLine(newPos < (sel->end + sel->start) / 2 ? sel->end : sel->start);
			anchor          = buffer_->BufCountForwardDispChars(anchorLineStart, rectAnchor);

		} else {
			if (abs(newPos - sel->start) < abs(newPos - sel->end)) {
				anchor = sel->end;
			} else {
				anchor = sel->start;
			}

			anchorLineStart = buffer_->BufStartOfLine(anchor);
			rectAnchor     = buffer_->BufCountDispChars(anchorLineStart, anchor);
		}
	} else {
		anchor = cursorPos_;
		anchorLineStart = buffer_->BufStartOfLine(anchor);
		rectAnchor      = buffer_->BufCountDispChars(anchorLineStart, anchor);
	}
	anchor_ = anchor;
	rectAnchor_ = rectAnchor;

	// Make the new selection
	if (flags & RectFlag) {
        buffer_->BufRectSelect(buffer_->BufStartOfLine(std::min(anchor, newPos)), buffer_->BufEndOfLine(std::max(anchor, newPos)), std::min(rectAnchor, column), std::max(rectAnchor, column));
	} else {
		buffer_->BufSelect(std::min(anchor, newPos), std::max(anchor, newPos));
	}

	/* Never mind the motion threshold, go right to dragging since
	   extend-start is unambiguously the start of a selection */
	dragState_ = PRIMARY_DRAG;

	// Don't do by-word or by-line adjustment, just by character
	clickCount_ = 0;

	// Move the cursor
	TextDSetInsertPosition(newPos);
	callCursorMovementCBs();
}

/**
 * @brief TextArea::extendAdjustAP
 * @param event
 * @param flags
 */
void TextArea::extendAdjustAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("extend_adjust");

	DragStates dragState = dragState_;
	const bool rectDrag = flags & RectFlag;

	// Make sure the proper initialization was done on mouse down
	if (dragState != PRIMARY_DRAG && dragState != PRIMARY_CLICKED && dragState != PRIMARY_RECT_DRAG) {
		return;
	}

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (dragState_ == PRIMARY_CLICKED) {

        const QPoint point = event->pos() - btnDownCoord_;
        if(point.manhattanLength() > QApplication::startDragDistance()) {
			dragState_ = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
		} else {
			return;
		}
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	dragState_ = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
    checkAutoScroll(event->pos());

	// Adjust the selection and move the cursor
    adjustSelection(event->pos());
}

/*
** Adjust the selection as the mouse is dragged to position: (x, y).
*/
void TextArea::adjustSelection(const QPoint &coord) {

    int64_t row;
    int64_t col;
    int64_t startPos;
    int64_t endPos;
    int64_t newPos = TextDXYToPosition(coord);

	// Adjust the selection
	if (dragState_ == PRIMARY_RECT_DRAG) {

		TextDXYToUnconstrainedPosition(coord, &row, &col);
		col          = TextDOffsetWrappedColumn(row, col);
        int64_t startCol = std::min(rectAnchor_, col);
        int64_t endCol   = std::max(rectAnchor_, col);
		startPos     = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		endPos       = buffer_->BufEndOfLine(std::max(anchor_, newPos));
        buffer_->BufRectSelect(startPos, endPos, startCol, endCol);

	} else if (clickCount_ == 1) { //multiClickState_ == ONE_CLICK) {
		startPos = startOfWord(std::min(anchor_, newPos));
		endPos   = endOfWord(std::max(anchor_, newPos));

        buffer_->BufSelect(startPos, endPos);

		newPos   = newPos < anchor_ ? startPos : endPos;

	} else if (clickCount_ == 2) { //multiClickState_ == TWO_CLICKS) {
		startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));

        buffer_->BufSelect(startPos, std::min(endPos + 1, buffer_->BufGetLength()));

		newPos   = newPos < anchor_ ? startPos : endPos;
	} else {
		buffer_->BufSelect(anchor_, newPos);
	}

	// Move the cursor
	TextDSetInsertPosition(newPos);
	callCursorMovementCBs();
}

/*
** Given a new mouse pointer location, pass the position on to the
** autoscroll timer routine, and make sure the timer is on when it's
** needed and off when it's not.
*/
void TextArea::checkAutoScroll(const QPoint &coord) {

	// Is the pointer in or out of the window?
    const bool inWindow = viewport()->rect().contains(coord);

	// If it's in the window, cancel the timer procedure
	if (inWindow) {
		autoScrollTimer_->stop();
		return;
	}

	// If the timer is not already started, start it
	autoScrollTimer_->start(0);

	// Pass on the newest mouse location to the autoscroll routine
	mouseCoord_ = coord;
}

void TextArea::deleteToStartOfLineAP(EventFlags flags) {

    EMIT_EVENT_0("delete_to_start_of_line");

    int64_t insertPos = cursorPos_;
    int64_t startOfLine;

	bool silent = (flags & NoBellFlag);

	if (flags & WrapFlag) {
		startOfLine = TextDStartOfLine(insertPos);
	} else {
		startOfLine = buffer_->BufStartOfLine(insertPos);
	}

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (deletePendingSelection()) {
		return;
	}

	if (insertPos == startOfLine) {
		ringIfNecessary(silent);
		return;
	}
	buffer_->BufRemove(startOfLine, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::mousePanAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("mouse_pan");

    int lineHeight = ascent_ + descent_;
    int64_t topLineNum;
    int horizOffset;

    switch(dragState_) {
    case MOUSE_PAN:
        TextDSetScroll((btnDownCoord_.y() - event->y() + lineHeight / 2) / lineHeight, btnDownCoord_.x() - event->x());
        break;
    case NOT_CLICKED:
        TextDGetScroll(&topLineNum, &horizOffset);
        btnDownCoord_.setX(event->x() + horizOffset);
        btnDownCoord_.setY(event->y() + gsl::narrow<int>(topLineNum) * lineHeight);
        dragState_ = MOUSE_PAN;

        viewport()->setCursor(Qt::SizeAllCursor);
        break;
    default:
        cancelDrag();
        break;
    }
}

/*
** Get the current scroll position for the text display, in terms of line
** number of the top line and horizontal pixel offset from the left margin
*/
void TextArea::TextDGetScroll(int64_t *topLineNum, int *horizOffset) {
	*topLineNum = topLineNum_;
	*horizOffset = horizOffset_;
}

void TextArea::copyToOrEndDragAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("copy_to_end_drag");

    DragStates dragState = dragState_;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		copyToAP(event, flags | SupressRecording);
		return;
	}

	FinishBlockDrag();
}

void TextArea::copyToAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("copy_to");

    DragStates dragState     = dragState_;
    const TextSelection *secondary = &buffer_->BufGetSecondary();
    const TextSelection *primary   = &buffer_->BufGetPrimary();
    bool rectangular         = secondary->rectangular;

	endDrag();
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED)) {
		return;
	}

    if (checkReadOnly()) {
        buffer_->BufSecondaryUnselect();
        return;
    }

	if (secondary->selected) {

        TextDBlankCursor();
        std::string textToCopy = buffer_->BufGetSecSelectTextEx();
        if (primary->selected && rectangular) {
            int64_t insertPos = cursorPos_;
            Q_UNUSED(insertPos);
            buffer_->BufReplaceSelectedEx(textToCopy);
            TextDSetInsertPosition(buffer_->BufCursorPosHint());
        } else if (rectangular) {
            int64_t insertPos = cursorPos_;
            int64_t lineStart = buffer_->BufStartOfLine(insertPos);
            int64_t column = buffer_->BufCountDispChars(lineStart, insertPos);
            buffer_->BufInsertColEx(column, lineStart, textToCopy, nullptr, nullptr);
            TextDSetInsertPosition(buffer_->BufCursorPosHint());
        } else {
            TextInsertAtCursorEx(textToCopy, true, P_autoWrapPastedText);
        }

        buffer_->BufSecondaryUnselect();
        TextDUnblankCursor();

	} else if (primary->selected) {
		std::string textToCopy = buffer_->BufGetSelectionTextEx();
        TextDSetInsertPosition(TextDXYToPosition(event->pos()));
		TextInsertAtCursorEx(textToCopy, false, P_autoWrapPastedText);
	} else {
        TextDSetInsertPosition(TextDXYToPosition(event->pos()));
		InsertPrimarySelection(false);
	}
}

/*
** Complete a block text drag operation
*/
void TextArea::FinishBlockDrag() {

    int64_t modRangeStart = -1;
    int64_t origModRangeEnd;
    int64_t bufModRangeEnd;

	/* Find the changed region of the buffer, covering both the deletion
	   of the selected text at the drag start position, and insertion at
	   the drag destination */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragSourceDeletePos_, dragSourceInserted_, dragSourceDeleted_);
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragInsertPos_, dragInserted_, dragDeleted_);

	// Get the original (pre-modified) range of text from saved backup buffer
	std::string deletedText = dragOrigBuf_->BufGetRangeEx(modRangeStart, origModRangeEnd);

	// Free the backup buffer
    dragOrigBuf_.reset();

	// Return to normal drag state
	dragState_ = NOT_CLICKED;

	// Call finish-drag calback
	DragEndEvent endStruct;
	endStruct.startPos       = modRangeStart;
	endStruct.nCharsDeleted  = origModRangeEnd - modRangeStart;
	endStruct.nCharsInserted = bufModRangeEnd  - modRangeStart;
	endStruct.deletedText    = deletedText;

	for(auto &c : dragEndCallbacks_) {
		c.first(this, &endStruct, c.second);
	}
}

/**
 * @brief TextArea::secondaryOrDragStartAP
 * @param event
 * @param flags
 */
void TextArea::secondaryOrDragStartAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("secondary_or_drag_start");

	/* If the click was outside of the primary selection, this is not
	   a drag, start a secondary selection */
    if (!buffer_->BufGetPrimary().selected || !TextDInSelection(event->pos())) {
        secondaryStartAP(event, flags | SupressRecording);
		return;
	}

	if (checkReadOnly()) {
		return;
	}

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   drag, and where to drag to */
    btnDownCoord_ = event->pos();
	dragState_    = CLICKED_IN_SELECTION;
}

/*
** Return true if position (x, y) is inside of the primary selection
*/
bool TextArea::TextDInSelection(const QPoint &p) const {

    int64_t pos = xyToPos(p, PositionTypes::CHARACTER_POS);

    int64_t row;
    int64_t column;
    xyToUnconstrainedPos(p, &row, &column, PositionTypes::CHARACTER_POS);

    if (buffer_->BufGetPrimary().rangeTouchesRectSel(firstChar_, lastChar_)) {
		column = TextDOffsetWrappedColumn(row, column);
    }

    return buffer_->BufGetPrimary().inSelection(pos, buffer_->BufStartOfLine(pos), column);
}

/**
 * @brief TextArea::secondaryStartAP
 * @param event
 * @param flags
 */
void TextArea::secondaryStartAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("secondary_start");

    const TextSelection *sel = &buffer_->BufGetSecondary();
    int64_t anchor;
    int64_t row;
    int64_t column;

	// Find the new anchor point and make the new selection
    int64_t pos = TextDXYToPosition(event->pos());
	if (sel->selected) {
		if (abs(pos - sel->start) < abs(pos - sel->end)) {
			anchor = sel->end;
		} else {
			anchor = sel->start;
		}
		buffer_->BufSecondarySelect(anchor, pos);
	} else {
		anchor = pos;
		Q_UNUSED(anchor);
	}

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   selection, (and where the selection began) */
    btnDownCoord_ = event->pos();
	anchor_       = pos;

    TextDXYToUnconstrainedPosition(event->pos(), &row, &column);

	column = TextDOffsetWrappedColumn(row, column);
	rectAnchor_ = column;
	dragState_ = SECONDARY_CLICKED;
}

/**
 * @brief TextArea::secondaryOrDragAdjustAP
 * @param event
 * @param flags
 */
void TextArea::secondaryOrDragAdjustAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("secondary_or_drag_adjust");

	DragStates dragState = dragState_;

	/* Only dragging of blocks of text is handled in this action proc.
	   Otherwise, defer to secondaryAdjust to handle the rest */
	if (dragState != CLICKED_IN_SELECTION && dragState != PRIMARY_BLOCK_DRAG) {
        secondaryAdjustAP(event, flags | SupressRecording);
		return;
	}

	/* Decide whether the mouse has moved far enough from the
	   initial mouse down to be considered a drag */
	if (dragState_ == CLICKED_IN_SELECTION) {
        const QPoint point = event->pos() - btnDownCoord_;
        if(point.manhattanLength() > QApplication::startDragDistance()) {
			BeginBlockDrag();
		} else {
			return;
		}
	}

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll({event->x(), event->y()});

	// Adjust the selection
	BlockDragSelection(
        event->pos(),
		(flags & OverlayFlag) ?
			((flags & CopyFlag) ? DRAG_OVERLAY_COPY : DRAG_OVERLAY_MOVE) :
			((flags & CopyFlag) ? DRAG_COPY         : DRAG_MOVE));
}

void TextArea::secondaryAdjustAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("secondary_adjust");

	DragStates dragState = dragState_;
	bool rectDrag = flags & RectFlag;

	// Make sure the proper initialization was done on mouse down
	if (dragState != SECONDARY_DRAG && dragState != SECONDARY_RECT_DRAG && dragState != SECONDARY_CLICKED)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (dragState_ == SECONDARY_CLICKED) {
        const QPoint point = event->pos() - btnDownCoord_;
        if(point.manhattanLength() > QApplication::startDragDistance()) {
			dragState_ = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;
		} else {
			return;
		}
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	dragState_ = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll({event->x(), event->y()});

	// Adjust the selection
	adjustSecondarySelection({event->x(), event->y()});
}

/*
** Start the process of dragging the current primary-selected text across
** the window (move by dragging, as opposed to dragging to create the
** selection)
*/
void TextArea::BeginBlockDrag() {

    QFontMetrics fm(font_);
    int fontHeight     = ascent_ + descent_;
	int fontWidth      = fm.maxWidth();
    const TextSelection *sel = &buffer_->BufGetPrimary();
    int64_t nLines;
    int64_t mousePos;
    int x;
    int y;

	/* Save a copy of the whole text buffer as a backup, and for
	   deriving changes */
    dragOrigBuf_ = std::make_unique<TextBuffer>();
    dragOrigBuf_->BufSetSyncXSelection(false);
    dragOrigBuf_->BufSetTabDistance(buffer_->BufGetTabDist());
    dragOrigBuf_->BufSetUseTabs(buffer_->BufGetUseTabs());

    dragOrigBuf_->BufSetAllEx(buffer_->BufGetAllEx());

	if (sel->rectangular) {
        dragOrigBuf_->BufRectSelect(sel->start, sel->end, sel->rectStart, sel->rectEnd);
	} else {
		dragOrigBuf_->BufSelect(sel->start, sel->end);
	}

	/* Record the mouse pointer offsets from the top left corner of the
	   selection (the position where text will actually be inserted In dragging
	   non-rectangular selections)  */
	if (sel->rectangular) {
        dragXOffset_ = btnDownCoord_.x() + horizOffset_ - rect_.left() - sel->rectStart * fontWidth;
	} else {
		if (!TextDPositionToXY(sel->start, &x, &y)) {
            x = buffer_->BufCountDispChars(TextDStartOfLine(sel->start), sel->start) * fontWidth + rect_.left() - horizOffset_;
		}
        dragXOffset_ = btnDownCoord_.x() - x;
	}

	mousePos = TextDXYToPosition(btnDownCoord_);
	nLines = buffer_->BufCountLines(sel->start, mousePos);

    dragYOffset_ = nLines * fontHeight + (((btnDownCoord_.y() - P_marginHeight) % fontHeight) - fontHeight / 2);
	dragNLines_  = buffer_->BufCountLines(sel->start, sel->end);

	/* Record the current drag insert position and the information for
	   undoing the fictional insert of the selection in its new position */
	dragInsertPos_ = sel->start;
	dragInserted_ = sel->end - sel->start;
	if (sel->rectangular) {
        TextBuffer testBuf;
        testBuf.BufSetSyncXSelection(false);

		std::string testText = buffer_->BufGetRangeEx(sel->start, sel->end);
        testBuf.BufSetTabDistance(buffer_->BufGetTabDist());
        testBuf.BufSetUseTabs(buffer_->BufGetUseTabs());
        testBuf.BufSetAllEx(testText);

        testBuf.BufRemoveRect(0, sel->end - sel->start, sel->rectStart, sel->rectEnd);
        dragDeleted_ = testBuf.BufGetLength();
		dragRectStart_ = sel->rectStart;
	} else {
		dragDeleted_ = 0;
		dragRectStart_ = 0;
	}

	dragType_            = DRAG_MOVE;
	dragSourceDeletePos_ = sel->start;
	dragSourceInserted_  = dragDeleted_;
	dragSourceDeleted_   = dragInserted_;

	/* For non-rectangular selections, fill in the rectangular information in
	   the selection for overlay mode drags which are done rectangularly */
	if (!sel->rectangular) {
        int64_t lineStart = buffer_->BufStartOfLine(sel->start);
		if (dragNLines_ == 0) {
            dragOrigBuf_->BufGetPrimary().rectStart = buffer_->BufCountDispChars(lineStart, sel->start);
            dragOrigBuf_->BufGetPrimary().rectEnd   = buffer_->BufCountDispChars(lineStart, sel->end);
		} else {
            int64_t lineEnd = buffer_->BufGetCharacter(sel->end - 1) == '\n' ? sel->end - 1 : sel->end;
            findTextMargins(buffer_, lineStart, lineEnd, &dragOrigBuf_->BufGetPrimary().rectStart, &dragOrigBuf_->BufGetPrimary().rectEnd);
		}
	}

	// Set the drag state to announce an ongoing block-drag
	dragState_ = PRIMARY_BLOCK_DRAG;

	// Call the callback announcing the start of a block drag
	for(auto &c : dragStartCallbacks_) {
		c.first(this, c.second);
	}
}

/*
** Reposition the primary-selected text that is being dragged as a block
** for a new mouse position of (x, y)
*/
void TextArea::BlockDragSelection(const QPoint &pos, BlockDragTypes dragType) {

    QFontMetrics fm(font_);
    int fontHeight               = ascent_ + descent_;
    int fontWidth                = fm.maxWidth();
    auto &origBuf                = dragOrigBuf_;
    int dragXOffset              = dragXOffset_;
    const TextSelection *origSel = &origBuf->BufGetPrimary();
    bool rectangular             = origSel->rectangular;
    BlockDragTypes oldDragType   = dragType_;
    int64_t nLines               = dragNLines_;
    int64_t insLineNum;
    int64_t insLineStart;
    int64_t insRectStart;
    int64_t insStart;
    int64_t modRangeStart   = -1;
    int64_t tempModRangeEnd = -1;
    int64_t bufModRangeEnd  = -1;
    int64_t referenceLine;
    int64_t referencePos;
    int64_t tempStart;
    int64_t tempEnd;
    int64_t insertInserted;
    int64_t insertDeleted;
    int64_t origSelLineEnd;
    int64_t sourceInserted;
    int64_t sourceDeleted;
    int64_t sourceDeletePos;

	if (dragState_ != PRIMARY_BLOCK_DRAG) {
		return;
	}

	/* The operation of block dragging is simple in theory, but not so simple
	   in practice.  There is a backup buffer (dragOrigBuf_) which
	   holds a copy of the buffer as it existed before the drag.  When the
	   user drags the mouse to a new location, this routine is called, and
	   a temporary buffer is created and loaded with the local part of the
	   buffer (from the backup) which might be changed by the drag.  The
	   changes are all made to this temporary buffer, and the parts of this
	   buffer which then differ from the real (displayed) buffer are used to
	   replace those parts, thus one replace operation serves as both undo
	   and modify.  This double-buffering of the operation prevents excessive
	   redrawing (though there is still plenty of needless redrawing due to
	   re-selection and rectangular operations).

	   The hard part is keeping track of the changes such that a single replace
	   operation will do everyting.  This is done using a routine called
	   trackModifyRange which tracks expanding ranges of changes in the two
	   buffers in modRangeStart, tempModRangeEnd, and bufModRangeEnd. */

	/* Create a temporary buffer for accumulating changes which will
	   eventually be replaced in the real buffer.  Load the buffer with the
	   range of characters which might be modified in this drag step
	   (this could be tighter, but hopefully it's not too slow) */
    TextBuffer tempBuf;
    tempBuf.BufSetSyncXSelection(false);
    tempBuf.BufSetTabDist(buffer_->BufGetTabDist());
    tempBuf.BufSetUseTabs(buffer_->BufGetUseTabs());

    tempStart = std::min<int64_t>({dragInsertPos_, origSel->start, buffer_->BufCountBackwardNLines(firstChar_, nLines + 2)});
    tempEnd   = buffer_->BufCountForwardNLines(std::max<int64_t>({dragInsertPos_, origSel->start, lastChar_}), nLines + 2) + origSel->end - origSel->start;

	const std::string text = origBuf->BufGetRangeEx(tempStart, tempEnd);
    tempBuf.BufSetAllEx(text);

	// If the drag type is USE_LAST, use the last dragType applied
	if (dragType == USE_LAST) {
		dragType = dragType_;
	}

	bool overlay = (dragType == DRAG_OVERLAY_MOVE) || (dragType == DRAG_OVERLAY_COPY);

	/* Overlay mode uses rectangular selections whether or not the original
	   was rectangular.  To use a plain selection as if it were rectangular,
	   the start and end positions need to be moved to the line boundaries
	   and trailing newlines must be excluded */
    int64_t origSelLineStart = origBuf->BufStartOfLine(origSel->start);

	if (!rectangular && origBuf->BufGetCharacter(origSel->end - 1) == '\n') {
		origSelLineEnd = origSel->end - 1;
	} else {
		origSelLineEnd = origBuf->BufEndOfLine(origSel->end);
	}

	if (!rectangular && overlay && nLines != 0) {
		dragXOffset -= fontWidth * (origSel->rectStart - (origSel->start - origSelLineStart));
	}

	/* If the drag operation is of a different type than the last one, and the
	   operation is a move, expand the modified-range to include undoing the
	   text-removal at the site from which the text was dragged. */
	if (dragType != oldDragType && dragSourceDeleted_ != 0) {
		trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, dragSourceDeletePos_, dragSourceInserted_, dragSourceDeleted_);
	}

	/* Do, or re-do the original text removal at the site where a move began.
	   If this part has not changed from the last call, do it silently to
	   bring the temporary buffer in sync with the real (displayed)
	   buffer.  If it's being re-done, track the changes to complete the
	   redo operation begun above */
	if (dragType == DRAG_MOVE || dragType == DRAG_OVERLAY_MOVE) {
		if (rectangular || overlay) {
            int64_t prevLen = tempBuf.BufGetLength();
            int64_t origSelLen = origSelLineEnd - origSelLineStart;

			if (overlay) {
                tempBuf.BufClearRect(origSelLineStart - tempStart, origSelLineEnd - tempStart, origSel->rectStart, origSel->rectEnd);
			} else {
                tempBuf.BufRemoveRect(origSelLineStart - tempStart, origSelLineEnd - tempStart, origSel->rectStart, origSel->rectEnd);
			}

			sourceDeletePos = origSelLineStart;
            sourceInserted = origSelLen - prevLen + tempBuf.BufGetLength();
			sourceDeleted = origSelLen;
		} else {
            tempBuf.BufRemove(origSel->start - tempStart, origSel->end - tempStart);
			sourceDeletePos = origSel->start;
			sourceInserted  = 0;
			sourceDeleted   = origSel->end - origSel->start;
		}

		if (dragType != oldDragType) {
			trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, sourceDeletePos, sourceInserted, sourceDeleted);
		}

	} else {
		sourceDeletePos = 0;
		sourceInserted  = 0;
		sourceDeleted   = 0;
	}

	/* Expand the modified-range to include undoing the insert from the last
	   call. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, dragInsertPos_, dragInserted_, dragDeleted_);

	/* Find the line number and column of the insert position.  Note that in
	   continuous wrap mode, these must be calculated as if the text were
	   not wrapped */
    int64_t row;
    int64_t column;
	TextDXYToUnconstrainedPosition(
        QPoint(
            std::max(0, pos.x() - dragXOffset),
            std::max(0, pos.y() - (dragYOffset_ % fontHeight))
        ),
		&row, &column);


	column = TextDOffsetWrappedColumn(row, column);
	row    = TextDOffsetWrappedRow(row);
	insLineNum = row + topLineNum_ - dragYOffset_ / fontHeight;

	/* find a common point of reference between the two buffers, from which
	   the insert position line number can be translated to a position */
	if (firstChar_ > modRangeStart) {
		referenceLine = topLineNum_ - buffer_->BufCountLines(modRangeStart, firstChar_);
		referencePos = modRangeStart;
	} else {
		referencePos = firstChar_;
		referenceLine = topLineNum_;
	}

	/* find the position associated with the start of the new line in the
	   temporary buffer */
    insLineStart = findRelativeLineStart(&tempBuf, referencePos - tempStart, referenceLine, insLineNum) + tempStart;
    if (insLineStart - tempStart == tempBuf.BufGetLength()) {
        insLineStart = tempBuf.BufStartOfLine(insLineStart - tempStart) + tempStart;
    }

	// Find the actual insert position
	if (rectangular || overlay) {
		insStart = insLineStart;
		insRectStart = column;
	} else { // note, this will fail with proportional fonts
        insStart = tempBuf.BufCountForwardDispChars(insLineStart - tempStart, column) + tempStart;
		insRectStart = 0;
	}

	/* If the position is the same as last time, don't bother drawing (it
	   would be nice if this decision could be made earlier) */
	if (insStart == dragInsertPos_ && insRectStart == dragRectStart_ && dragType == oldDragType) {
		return;
	}

	// Do the insert in the temporary buffer
	if (rectangular || overlay) {

		std::string insText = origBuf->BufGetTextInRectEx(origSelLineStart, origSelLineEnd, origSel->rectStart, origSel->rectEnd);
		if (overlay) {
            tempBuf.BufOverlayRectEx(insStart - tempStart, insRectStart, insRectStart + origSel->rectEnd - origSel->rectStart, insText, &insertInserted, &insertDeleted);
		} else {
            tempBuf.BufInsertColEx(insRectStart, insStart - tempStart, insText, &insertInserted, &insertDeleted);
		}
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, insertInserted, insertDeleted);

	} else {
		std::string insText = origBuf->BufGetSelectionTextEx();
        tempBuf.BufInsertEx(insStart - tempStart, insText);
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, origSel->end - origSel->start, 0);
		insertInserted = origSel->end - origSel->start;
		insertDeleted = 0;
	}

	// Make the changes in the real buffer
    std::string repText = tempBuf.BufGetRangeEx(modRangeStart - tempStart, tempModRangeEnd - tempStart);

	TextDBlankCursor();
	buffer_->BufReplaceEx(modRangeStart, bufModRangeEnd, repText);

	// Store the necessary information for undoing this step
	dragInsertPos_       = insStart;
	dragRectStart_       = insRectStart;
	dragInserted_        = insertInserted;
	dragDeleted_         = insertDeleted;
	dragSourceDeletePos_ = sourceDeletePos;
	dragSourceInserted_  = sourceInserted;
	dragSourceDeleted_   = sourceDeleted;
	dragType_            = dragType;


    // NOTE(eteran): as an optimization, if we are moving a block
    // but not combining it with the new location's content, then the selected
    // text is functionally the same, so we don't need to constantly syncronize
    // the X selection.
    bool prev = true;
    if(dragType == DRAG_OVERLAY_MOVE) {
        prev = buffer_->BufSetSyncXSelection(false);
    }

    // Reset the selection and cursor position
	if (rectangular || overlay) {
        int64_t insRectEnd = insRectStart + origSel->rectEnd - origSel->rectStart;
        buffer_->BufRectSelect(insStart, insStart + insertInserted, insRectStart, insRectEnd);
		TextDSetInsertPosition(buffer_->BufCountForwardDispChars(buffer_->BufCountForwardNLines(insStart, dragNLines_), insRectEnd));
	} else {
		buffer_->BufSelect(insStart, insStart + origSel->end - origSel->start);
		TextDSetInsertPosition(insStart + origSel->end - origSel->start);
	}

    if(dragType == DRAG_OVERLAY_MOVE) {
        buffer_->BufSetSyncXSelection(prev);
    }

	TextDUnblankCursor();
    callMovedCBs();
	emTabsBeforeCursor_ = 0;
}

void TextArea::callMovedCBs() {
    for(auto &c : movedCallbacks_) {
        c.first(this, c.second);
    }
}

void TextArea::adjustSecondarySelection(const QPoint &coord) {


    int64_t newPos = TextDXYToPosition(coord);

	if (dragState_ == SECONDARY_RECT_DRAG) {

        int64_t row;
        int64_t col;
		TextDXYToUnconstrainedPosition(coord, &row, &col);
		col          = TextDOffsetWrappedColumn(row, col);
        int64_t startCol = std::min(rectAnchor_, col);
        int64_t endCol   = std::max(rectAnchor_, col);
        int64_t startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
        int64_t endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));
		buffer_->BufSecRectSelect(startPos, endPos, startCol, endCol);
	} else {
		buffer_->BufSecondarySelect(anchor_, newPos);
	}
}

/*
** Correct a row number from an unconstrained position (as returned by
** TextDXYToUnconstrainedPosition) to a straight number of newlines from the
** top line of the display.  Because rectangular selections are based on
** newlines, rather than display wrapping, and anywhere a rectangular selection
** needs a row, it needs it in terms of un-wrapped lines.
*/
int64_t TextArea::TextDOffsetWrappedRow(int row) const {
	if (!P_continuousWrap || row < 0 || row > nVisibleLines_) {
		return row;
	}

	return buffer_->BufCountLines(firstChar_, lineStarts_[row]);
}

void TextArea::setWordDelimiters(const std::string &delimiters) {

    // add mandatory delimiters blank, tab, and newline to the list
    P_delimiters = " \t\n";
    P_delimiters += delimiters;
}

void TextArea::setAutoShowInsertPos(bool value) {
    P_autoShowInsertPos = value;
}

void TextArea::setEmulateTabs(int value) {
	P_emulateTabs = value;
}

void TextArea::setWrapMargin(int value) {
	TextDSetWrapMode(P_continuousWrap, value);
}

void TextArea::setLineNumCols(int value) {

	P_lineNumCols = value;

    QFontMetrics fm(font_);

	int marginWidth = P_marginWidth;
	int charWidth   = fm.maxWidth();
	int lineNumCols = P_lineNumCols;

	if (lineNumCols == 0) {
		TextDSetLineNumberArea(0, 0, marginWidth);
		P_columns = (viewport()->width() - marginWidth * 2) / charWidth;
	} else {
		TextDSetLineNumberArea(marginWidth, charWidth * lineNumCols, 2 * marginWidth + charWidth * lineNumCols);
		P_columns = (viewport()->width() - marginWidth * 3 - charWidth * lineNumCols) / charWidth;
	}
}

/*
** Define area for drawing line numbers.  A width of 0 disables line
** number drawing.
*/
void TextArea::TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft) {
    int newWidth = rect_.width() + rect_.left() - textLeft;
	lineNumLeft_ = lineNumLeft;
	lineNumWidth_ = lineNumWidth;
    rect_.setLeft(textLeft);

	resetAbsLineNum();
    TextDResize(newWidth, rect_.height());
    TextDRedisplayRect(0, rect_.top(), INT_MAX, rect_.height());
}

void TextArea::TextDSetWrapMode(bool wrap, int wrapMargin) {

	P_continuousWrap = wrap;
	P_wrapMargin     = wrapMargin;

	// wrapping can change change the total number of lines, re-count
	nBufferLines_ = TextDCountLines(0, buffer_->BufGetLength(), true);

    /* changing wrap margins wrap or changing from wrapped mode to non-wrapped
     * can leave the character at the top no longer at a line start, and/or
     * change the line number */
	firstChar_  = TextDStartOfLine(firstChar_);
    topLineNum_ = TextDCountLines(0, firstChar_, true) + 1;
	resetAbsLineNum();

	// update the line starts array
	calcLineStarts(0, nVisibleLines_);
	calcLastChar();

    /* Update the scroll bar page increment size (as well as other scroll
     * bar parameters) */
	updateVScrollBarRange();
	updateHScrollBarRange();

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();

	// Do a full redraw
    TextDRedisplayRect(0, rect_.top(), rect_.width() + rect_.left(), rect_.height());
}


void TextArea::deleteToEndOfLineAP(EventFlags flags) {

    EMIT_EVENT_0("delete_to_end_of_line");

    int64_t insertPos = cursorPos_;
    int64_t endOfLine;

	bool silent = flags & NoBellFlag;
	if (flags & AbsoluteFlag) {
		endOfLine = buffer_->BufEndOfLine(insertPos);
	} else {
		endOfLine = TextDEndOfLine(insertPos, false);
	}

	cancelDrag();
	if (checkReadOnly())
		return;

	if (deletePendingSelection()) {
		return;
	}

	if (insertPos == endOfLine) {
		ringIfNecessary(silent);
		return;
	}
	buffer_->BufRemove(insertPos, endOfLine);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::cutPrimaryAP(EventFlags flags) {

    EMIT_EVENT_0("cut_primary");

    const TextSelection *primary = &buffer_->BufGetPrimary();

	bool rectangular = flags & RectFlag;
    int64_t insertPos;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (primary->selected && rectangular) {
		std::string textToCopy = buffer_->BufGetSelectionTextEx();
		insertPos = cursorPos_;
        int64_t col = buffer_->BufCountDispChars(buffer_->BufStartOfLine(insertPos), insertPos);
		buffer_->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
        TextDSetInsertPosition(buffer_->BufCursorPosHint());

		buffer_->BufRemoveSelected();
		checkAutoShowInsertPos();
	} else if (primary->selected) {
		std::string textToCopy = buffer_->BufGetSelectionTextEx();
		insertPos = cursorPos_;
		buffer_->BufInsertEx(insertPos, textToCopy);
        TextDSetInsertPosition(insertPos + textToCopy.size());

		buffer_->BufRemoveSelected();
		checkAutoShowInsertPos();
	} else if (rectangular) {
        if (!TextDPositionToXY(cursorPos_, &btnDownCoord_)) {
			return; // shouldn't happen
		}
		MovePrimarySelection(true);
	} else {
		MovePrimarySelection(false);
	}
}

/*
** Insert the contents of the PRIMARY selection at the cursor position in
** widget "w" and delete the contents of the selection in its current owner
** (if the selection owner supports DELETE targets).
*/
void TextArea::MovePrimarySelection(bool isColumnar) {
	Q_UNUSED(isColumnar);

    // TODO(eteran): implement MovePrimarySelection
    qWarning("MovePrimarySelection is not implemented");
}


void TextArea::moveToOrEndDragAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("move_to_or_end_drag");

	DragStates dragState = dragState_;

	if (dragState != PRIMARY_BLOCK_DRAG) {
        moveToAP(event, flags | SupressRecording);
		return;
	}

	FinishBlockDrag();
}

void TextArea::moveToAP(QMouseEvent *event, EventFlags flags) {

    EMIT_EVENT_0("move_to");

    DragStates dragState = dragState_;

    const TextSelection *secondary = &buffer_->BufGetSecondary();
    const TextSelection *primary   = &buffer_->BufGetPrimary();

	int rectangular = secondary->rectangular;

	endDrag();

	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED)) {
		return;
	}

	if (checkReadOnly()) {
		buffer_->BufSecondaryUnselect();
		return;
	}

	if (secondary->selected) {

        std::string textToCopy = buffer_->BufGetSecSelectTextEx();
        if (primary->selected && rectangular) {
            int64_t insertPos = cursorPos_;
            Q_UNUSED(insertPos);
            buffer_->BufReplaceSelectedEx(textToCopy);
            TextDSetInsertPosition(buffer_->BufCursorPosHint());
        } else if (rectangular) {
            int64_t insertPos = cursorPos_;
            int64_t lineStart = buffer_->BufStartOfLine(insertPos);
            int64_t column = buffer_->BufCountDispChars(lineStart, insertPos);
            buffer_->BufInsertColEx(column, lineStart, textToCopy, nullptr, nullptr);
            TextDSetInsertPosition(buffer_->BufCursorPosHint());
        } else {
            TextInsertAtCursorEx(textToCopy, true, P_autoWrapPastedText);
        }

        buffer_->BufRemoveSecSelect();
        buffer_->BufSecondaryUnselect();

	} else if (primary->selected) {
		std::string textToCopy = buffer_->BufGetRangeEx(primary->start, primary->end);
        TextDSetInsertPosition(TextDXYToPosition(event->pos()));
		TextInsertAtCursorEx(textToCopy, false, P_autoWrapPastedText);

		buffer_->BufRemoveSelected();
        buffer_->BufUnselect();
	} else {
        TextDSetInsertPosition(TextDXYToPosition(event->pos()));
		MovePrimarySelection(false);
	}
}

void TextArea::exchangeAP(QMouseEvent *event, EventFlags flags) {

    Q_UNUSED(event);
    EMIT_EVENT_0("exchange");

    const TextSelection *sec     = &buffer_->BufGetSecondary();
    const TextSelection *primary = &buffer_->BufGetPrimary();

	DragStates dragState = dragState_; // save before endDrag
	bool silent = flags & NoBellFlag;

	endDrag();
	if (checkReadOnly())
		return;

	/* If there's no secondary selection here, or the primary and secondary
	   selection overlap, just beep and return */
	if (!sec->selected || (primary->selected && ((primary->start <= sec->start && primary->end > sec->start) || (sec->start <= primary->start && sec->end > primary->start)))) {
		buffer_->BufSecondaryUnselect();
		ringIfNecessary(silent);
		/* If there's no secondary selection, but the primary selection is
		   being dragged, we must not forget to finish the dragging.
		   Otherwise, modifications aren't recorded. */
		if (dragState == PRIMARY_BLOCK_DRAG) {
			FinishBlockDrag();
		}
		return;
	}

	// if the primary selection is in another widget, use selection routines
	if (!primary->selected) {
		ExchangeSelections();
		return;
	}

	// Both primary and secondary are in this widget, do the exchange here
	std::string primaryText = buffer_->BufGetSelectionTextEx();
	std::string secText     = buffer_->BufGetSecSelectTextEx();

    const bool secWasRect = sec->rectangular;
	buffer_->BufReplaceSecSelectEx(primaryText);
    const int64_t newPrimaryStart = primary->start;
	buffer_->BufReplaceSelectedEx(secText);
    const int64_t newPrimaryEnd = newPrimaryStart + secText.size();

	buffer_->BufSecondaryUnselect();
	if (secWasRect) {
        TextDSetInsertPosition(buffer_->BufCursorPosHint());
	} else {
		buffer_->BufSelect(newPrimaryStart, newPrimaryEnd);
        TextDSetInsertPosition(newPrimaryEnd);
	}
	checkAutoShowInsertPos();
}

/*
** Exchange Primary and secondary selections (to be called by the widget
** with the secondary selection)
*/
void TextArea::ExchangeSelections() {

    if (!buffer_->BufGetSecondary().selected) {
		return;
    }

    // TODO(eteran): implement ExchangeSelections
    qWarning("ExchangeSelections not implemented");

	/* Initiate an long series of events:
	** 1) get the primary selection,
	** 2) replace the primary selection with this widget's secondary,
	** 3) replace this widget's secondary with the text returned from getting
	**    the primary selection.  This could be done with a much more efficient
	**    MULTIPLE request following ICCCM conventions, but the X toolkit
	**    MULTIPLE handling routines can't handle INSERT_SELECTION requests
	**    inside of MULTIPLE requests, because they don't allow access to the
	**    requested property atom in  inside of an XtConvertSelectionProc.
	**    It's simply not worth duplicating all of Xt's selection handling
	**    routines for a little performance, and this would make the code
    **    incompatible with Motif text widgets */
}

/*
** Returns the absolute (non-wrapped) line number of the first line displayed.
** Returns 0 if the absolute top line number is not being maintained.
*/
int64_t TextArea::getAbsTopLineNum() const {

	if (!P_continuousWrap) {
		return topLineNum_;
	}

	if (maintainingAbsTopLineNum()) {
		return absTopLineNum_;
	}

	return 0;
}

QColor TextArea::getForegroundPixel() const {
    QPalette pal = palette();
    return pal.color(QPalette::Text);
}

QColor TextArea::getBackgroundPixel() const {
    QPalette pal = palette();
    return pal.color(QPalette::Base);
}

void TextArea::setReadOnly(bool value) {
	P_readOnly = value;
}

void TextArea::setOverstrike(bool value) {
	P_overstrike = value;

	switch(getCursorStyle()) {
    case CursorStyles::Block:
        TextDSetCursorStyle(P_heavyCursor ? CursorStyles::Heavy : CursorStyles::Normal);
		break;
    case CursorStyles::Normal:
    case CursorStyles::Heavy:
        TextDSetCursorStyle(CursorStyles::Block);
        break;
	default:
		break;
	}
}

CursorStyles TextArea::getCursorStyle() const {
	return cursorStyle_;
}

void TextArea::setCursorVPadding(int value) {
	P_cursorVPadding = value;
}

QFont TextArea::getFont() const {
    return font_;
}

void TextArea::setFont(const QFont &font) {

    // did the font change?
    const bool reconfigure = (P_lineNumCols != 0);

    TextDSetFont(font);

	/* Setting the lineNumCols resource tells the text widget to hide or
	   show, or change the number of columns of the line number display,
	   which requires re-organizing the x coordinates of both the line
	   number display and the main text display */
	if(reconfigure) {
		setLineNumCols(getLineNumCols());
	}
}

QRect TextArea::getRect() const {
    return rect_;
}

int TextArea::getLineNumCols() const {
	return P_lineNumCols;
}

void TextArea::setAutoWrap(bool value) {
	P_autoWrap = value;
}

void TextArea::setContinuousWrap(bool value) {
	TextDSetWrapMode(value, P_wrapMargin);
}

void TextArea::setAutoIndent(bool value) {
	P_autoIndent = value;
}

void TextArea::setSmartIndent(bool value) {
	P_smartIndent = value;
}

void TextArea::pageLeftAP(EventFlags flags) {

    EMIT_EVENT_0("page_left");

    QFontMetrics fm(font_);
    int64_t insertPos = cursorPos_;
    int maxCharWidth  = fm.maxWidth();
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) {
		if (horizOffset_ == 0) {
			ringIfNecessary(silent);
			return;
		}
        int horizOffset = std::max(0, horizOffset_ - rect_.width());
		TextDSetScroll(topLineNum_, horizOffset);
	} else {
        int64_t lineStartPos = buffer_->BufStartOfLine(insertPos);
		if (insertPos == lineStartPos && horizOffset_ == 0) {
			ringIfNecessary(silent);
			return;
		}
        int64_t indent = buffer_->BufCountDispChars(lineStartPos, insertPos);
        int64_t pos = buffer_->BufCountForwardDispChars(lineStartPos, std::max<int64_t>(0, indent - rect_.width() / maxCharWidth));
		TextDSetInsertPosition(pos);
        TextDSetScroll(topLineNum_, std::max(0, horizOffset_ - rect_.width()));
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::pageRightAP(EventFlags flags) {

    EMIT_EVENT_0("page_right");

    QFontMetrics fm(font_);
    int64_t insertPos      = cursorPos_;
    int maxCharWidth       = fm.maxWidth();
    int64_t oldHorizOffset = horizOffset_;
	bool silent = flags & NoBellFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) {
        const int sliderMax    = horizontalScrollBar()->maximum();
        const int horizOffset = std::min(horizOffset_ + rect_.width(), sliderMax);

		if (horizOffset_ == horizOffset) {
			ringIfNecessary(silent);
			return;
		}

		TextDSetScroll(topLineNum_, horizOffset);
	} else {
        int64_t lineStartPos = buffer_->BufStartOfLine(insertPos);
        int64_t indent       = buffer_->BufCountDispChars(lineStartPos, insertPos);
        int64_t pos          = buffer_->BufCountForwardDispChars(lineStartPos, indent + rect_.width() / maxCharWidth);

		TextDSetInsertPosition(pos);
        TextDSetScroll(topLineNum_, horizOffset_ + rect_.width());

		if (horizOffset_ == oldHorizOffset && insertPos == pos) {
			ringIfNecessary(silent);
		}

		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::nextPageAP(EventFlags flags) {

    EMIT_EVENT_0("next_page");

    int64_t lastTopLine = std::max<int64_t>(1, nBufferLines_ - (nVisibleLines_ - 2) + P_cursorVPadding);
    int64_t insertPos = cursorPos_;
    int64_t column = 0;
    int visLineNum;
    int64_t lineStartPos;
    int64_t pos;
    int64_t targetLine;
	int pageForwardCount = std::max(1, nVisibleLines_ - 1);

	bool silent         = flags & NoBellFlag;
	bool maintainColumn = flags & ColumnFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) { // scrollbar only
		targetLine = std::min(topLineNum_ + pageForwardCount, lastTopLine);

		if (targetLine == topLineNum_) {
			ringIfNecessary(silent);
			return;
		}
		TextDSetScroll(targetLine, horizOffset_);
	} else if (flags & StutterFlag) { // Mac style
		// move to bottom line of visible area
		// if already there, page down maintaining preferrred column
        targetLine = std::max<int64_t>(std::min<int64_t>(nVisibleLines_ - 1, nBufferLines_), 0);
		column = TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == lineStarts_[targetLine]) {
			if (insertPos >= buffer_->BufGetLength() || topLineNum_ == lastTopLine) {
				ringIfNecessary(silent);
				return;
			}
			targetLine = std::min(topLineNum_ + pageForwardCount, lastTopLine);
			pos = TextDCountForwardNLines(insertPos, pageForwardCount, false);
			if (maintainColumn) {
				pos = TextDPosOfPreferredCol(column, pos);
			}
			TextDSetInsertPosition(pos);
			TextDSetScroll(targetLine, horizOffset_);
		} else {
			pos = lineStarts_[targetLine];
			while (targetLine > 0 && pos == -1) {
				--targetLine;
				pos = lineStarts_[targetLine];
			}
			if (lineStartPos == pos) {
				ringIfNecessary(silent);
				return;
			}
			if (maintainColumn) {
				pos = TextDPosOfPreferredCol(column, pos);
			}
			TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
		if (maintainColumn) {
			cursorPreferredCol_ = column;
		} else {
			cursorPreferredCol_ = -1;
		}
	} else { // "standard"
		if (insertPos >= buffer_->BufGetLength() && topLineNum_ == lastTopLine) {
			ringIfNecessary(silent);
			return;
		}
		if (maintainColumn) {
			column = TextDPreferredColumn(&visLineNum, &lineStartPos);
		}
		targetLine = topLineNum_ + nVisibleLines_ - 1;
		if (targetLine < 1)
			targetLine = 1;
		if (targetLine > lastTopLine)
			targetLine = lastTopLine;
		pos = TextDCountForwardNLines(insertPos, nVisibleLines_ - 1, false);
		if (maintainColumn) {
			pos = TextDPosOfPreferredCol(column, pos);
		}
		TextDSetInsertPosition(pos);
		TextDSetScroll(targetLine, horizOffset_);
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
		if (maintainColumn) {
			cursorPreferredCol_ = column;
		} else {
			cursorPreferredCol_ = -1;
		}
	}
}

void TextArea::previousPageAP(EventFlags flags) {

    EMIT_EVENT_0("previous_page");

    int64_t insertPos = cursorPos_;
    int visLineNum;
    int64_t lineStartPos;
	int pageBackwardCount = std::max(1, nVisibleLines_ - 1);

	bool silent         = flags & NoBellFlag;
	bool maintainColumn = flags & ColumnFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) { // scrollbar only
        int64_t targetLine = std::max<int64_t>(topLineNum_ - pageBackwardCount, 1);

		if (targetLine == topLineNum_) {
			ringIfNecessary(silent);
			return;
		}
		TextDSetScroll(targetLine, horizOffset_);
	} else if (flags & StutterFlag) { // Mac style
		// move to top line of visible area
		// if already there, page up maintaining preferrred column if required
        int64_t targetLine = 0;
        int64_t column = TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == lineStarts_[targetLine]) {
			if (topLineNum_ == 1 && (maintainColumn || column == 0)) {
				ringIfNecessary(silent);
				return;
			}
            targetLine = std::max<int64_t>(topLineNum_ - pageBackwardCount, 1);

            int64_t pos = TextDCountBackwardNLines(insertPos, pageBackwardCount);
			if (maintainColumn) {
				pos = TextDPosOfPreferredCol(column, pos);
			}

			TextDSetInsertPosition(pos);
			TextDSetScroll(targetLine, horizOffset_);
		} else {
            int64_t pos = lineStarts_[targetLine];
			if (maintainColumn) {
				pos = TextDPosOfPreferredCol(column, pos);
			}
			TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();

		if (maintainColumn) {
			cursorPreferredCol_ = column;
		} else {
			cursorPreferredCol_ = -1;
		}
	} else { // "standard"
		if (insertPos <= 0 && topLineNum_ == 1) {
			ringIfNecessary(silent);
			return;
		}

        int64_t column = 0;
        if (maintainColumn) {
			column = TextDPreferredColumn(&visLineNum, &lineStartPos);
		}

        int64_t targetLine = topLineNum_ - (nVisibleLines_ - 1);
        if (targetLine < 1) {
			targetLine = 1;
        }

        int64_t pos = TextDCountBackwardNLines(insertPos, nVisibleLines_ - 1);
		if (maintainColumn) {
			pos = TextDPosOfPreferredCol(column, pos);
		}

		TextDSetInsertPosition(pos);
		TextDSetScroll(targetLine, horizOffset_);
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();

		if (maintainColumn) {
			cursorPreferredCol_ = column;
		} else {
			cursorPreferredCol_ = -1;
		}
	}
}

/*
** Return the current preferred column along with the current
** visible line index (-1 if not visible) and the lineStartPos
** of the current insert position.
*/
int64_t TextArea::TextDPreferredColumn(int *visLineNum, int64_t *lineStartPos) {

	/* Find the position of the start of the line.  Use the line starts array
	if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (posToVisibleLineNum(cursorPos_, visLineNum)) {
		*lineStartPos = lineStarts_[*visLineNum];
	} else {
		*lineStartPos =TextDStartOfLine(cursorPos_);
		*visLineNum = -1;
	}

	// Decide what column to move to, if there's a preferred column use that
    int64_t column = (cursorPreferredCol_ >= 0) ? cursorPreferredCol_ : buffer_->BufCountDispChars(*lineStartPos, cursorPos_);
    return column;
}

/*
** Return the insert position of the requested column given
** the lineStartPos.
*/
int64_t TextArea::TextDPosOfPreferredCol(int64_t column, int64_t lineStartPos) {

    int64_t newPos = buffer_->BufCountForwardDispChars(lineStartPos, column);
	if (P_continuousWrap) {
        newPos = std::min(newPos, TextDEndOfLine(lineStartPos, true));
	}

	return newPos;
}

/*
** Return the cursor position
*/
int64_t TextArea::TextGetCursorPos() const {
	return TextDGetInsertPosition();
}

int64_t TextArea::TextDGetInsertPosition() const {
	return cursorPos_;
}

/*
** If the text widget is maintaining a line number count appropriate to "pos"
** return the line and column numbers of pos, otherwise return false.  If
** continuous wrap mode is on, returns the absolute line number (as opposed to
** the wrapped line number which is used for scrolling).  THIS ROUTINE ONLY
** WORKS FOR DISPLAYED LINES AND, IN CONTINUOUS WRAP MODE, ONLY WHEN THE
** ABSOLUTE LINE NUMBER IS BEING MAINTAINED.  Otherwise, it returns false.
*/
bool TextArea::TextDPosToLineAndCol(int64_t pos, int64_t *lineNum, int64_t *column) {

	/* In continuous wrap mode, the absolute (non-wrapped) line count is
	   maintained separately, as needed.  Only return it if we're actually
	   keeping track of it and pos is in the displayed text */
	if (P_continuousWrap) {
        if (!maintainingAbsTopLineNum() || pos < firstChar_ || pos > lastChar_) {
            return false;
        }

		*lineNum = absTopLineNum_ + buffer_->BufCountLines(firstChar_, pos);
        *column  = buffer_->BufCountDispChars(buffer_->BufStartOfLine(pos), pos);
		return true;
	}

    // Only return the data if pos is within the displayed text
    int visLineNum;
    if (!posToVisibleLineNum(pos, &visLineNum)) {
        return false;
    }

    *column = buffer_->BufCountDispChars(lineStarts_[visLineNum], pos);
    *lineNum = visLineNum + topLineNum_;
    return true;
}

void TextArea::addCursorMovementCallback(cursorMovedCBEx callback, void *arg) {
	movedCallbacks_.push_back(qMakePair(callback, arg));
}

void TextArea::addDragStartCallback(dragStartCBEx callback, void *arg) {
	dragStartCallbacks_.push_back(qMakePair(callback, arg));
}

void TextArea::addDragEndCallback(dragEndCBEx callback, void *arg) {
	dragEndCallbacks_.push_back(qMakePair(callback, arg));
}

void TextArea::addSmartIndentCallback(smartIndentCBEx callback, void *arg) {
	smartIndentCallbacks_.push_back(qMakePair(callback, arg));
}

bool TextArea::focusNextPrevChild(bool next) {

    // Prevent tab from changing focus
    Q_UNUSED(next);
    return false;
}


int TextArea::getEmulateTabs() const {
    return P_emulateTabs;
}

/*
** Set the cursor position
*/
void TextArea::TextSetCursorPos(int64_t pos) {
    TextDSetInsertPosition(pos);
    checkAutoShowInsertPos();
    callCursorMovementCBs();
}

void TextArea::setModifyingTabDist(int tabDist) {
    modifyingTabDist_ = tabDist;
}

int64_t TextArea::getBufferLinesCount() const {
    return nBufferLines_;
}

int TextArea::fontAscent() const {
    return ascent_;
}

int TextArea::fontDescent() const {
    return descent_;
}

/*
** Find the height currently being used to display text, which is
** a composite of all of the active highlighting fonts as determined by the
** text display component
*/
int TextArea::getFontHeight() const {
    return fontAscent() + fontDescent();
}

/*
** Attach (or remove) highlight information in text display and redisplay.
** Highlighting information consists of a style buffer which parallels the
** normal text buffer, but codes font and color information for the display;
** a style table which translates style buffer codes (indexed by buffer
** character - 65 (ASCII code for 'A')) into fonts and colors; and a callback
** mechanism for as-needed highlighting, triggered by a style buffer entry of
** "unfinishedStyle".  Style buffer can trigger additional redisplay during
** a normal buffer modification if the buffer contains a primary selection
** (see extendRangeForStyleMods for more information on this protocol).
**
** Style buffers, tables and their associated memory are managed by the caller.
*/
void TextArea::TextDAttachHighlightData(const std::shared_ptr<TextBuffer> &styleBuffer, const std::vector<StyleTableEntry> &styleTable, char unfinishedStyle, unfinishedStyleCBProcEx unfinishedHighlightCB, void *user) {
    styleBuffer_           = styleBuffer;
    styleTable_            = styleTable;
    unfinishedStyle_       = unfinishedStyle;
    unfinishedHighlightCB_ = unfinishedHighlightCB;
    highlightCBArg_        = user;

    /* Call TextDSetFont to combine font information from style table and
       primary font, adjust font-related parameters, and then redisplay */
    TextDSetFont(font_);
}

QTimer *TextArea::cursorBlinkTimer() const {
    return cursorBlinkTimer_;
}

int64_t TextArea::TextFirstVisiblePos() const {
    return firstChar_;
}

int64_t TextArea::TextLastVisiblePos() const {
    return lastChar_;
}

const std::shared_ptr<TextBuffer> &TextArea::getStyleBuffer() const {
    return styleBuffer_;
}

/**
 * @brief TextArea::updateFontHeightMetrics
 * @param font
 */
void TextArea::updateFontHeightMetrics(const QFont &font) {

    QFontMetrics fm(font);

    int maxAscent  = fm.ascent();
    int maxDescent = fm.descent();

    /* If there is a (syntax highlighting) style table in use, find the new
       maximum font height for this text display */
    for(const StyleTableEntry &style : styleTable_) {
        QFontMetrics styleFM(style.font);

        maxAscent  = std::max(maxAscent, styleFM.ascent());
        maxDescent = std::max(maxDescent, styleFM.descent());
    }

    ascent_  = maxAscent;
    descent_ = maxDescent;
}

void TextArea::updateFontWidthMetrics(const QFont &font) {
    QFontMetrics fm(font);
    QFontInfo    fi(font);

    // If all of the current fonts are fixed and match in width, compute
    int fontWidth = fm.maxWidth();
    if(!fi.fixedPitch()) {
        fontWidth = -1;
    } else {
        for(const StyleTableEntry &style : styleTable_) {
            QFontMetrics styleFM(style.font);
            QFontInfo    styleFI(style.font);

            if ((styleFM.maxWidth() != fontWidth || !styleFI.fixedPitch())) {
                fontWidth = -1;
            }
        }
    }
    fixedFontWidth_ = fontWidth;
}

/*
** Change the (non highlight) font
*/
void TextArea::TextDSetFont(const QFont &font) {

    // If font size changes, cursor will be redrawn in a new position
    blankCursorProtrusions();

    updateFontHeightMetrics(font);
    updateFontWidthMetrics(font);

    // Don't let the height dip below one line, or bad things can happen
    if (rect_.height() < ascent_ + descent_) {
        rect_.setHeight(ascent_ + descent_);
    }

    font_ = font;

    // Do a full resize to force recalculation of font related parameters
    const int width  = rect_.width();
    const int height = rect_.height();

    rect_.setSize({0, 0});

    TextDResize(width, height);

    // Redisplay
    TextDRedisplayRect(rect_);

    // Clean up line number area in case spacing has changed
    redrawLineNumbersEx();
}

int TextArea::getLineNumWidth() const {
    return lineNumWidth_;
}

int TextArea::getLineNumLeft() const {
    return lineNumLeft_;
}

int TextArea::getRows() const {
    return P_rows;
}

int TextArea::getMarginHeight() const {
    return P_marginHeight;
}

int TextArea::getMarginWidth() const {
    return P_marginWidth;
}

/*
** Fetch text from the widget's buffer, adding wrapping newlines to emulate
** effect acheived by wrapping in the text display in continuous wrap mode.
*/
std::string TextArea::TextGetWrappedEx(int64_t startPos, int64_t endPos) {

    if (!P_continuousWrap || startPos == endPos) {
        return buffer_->BufGetRangeEx(startPos, endPos);
    }

    /* Create a text buffer with a good estimate of the size that adding
       newlines will expand it to.  Since it's a text buffer, if we guess
       wrong, it will fail softly, and simply expand the size */
    TextBuffer outBuf((endPos - startPos) + (endPos - startPos) / 5);
    int outPos = 0;

    /* Go (displayed) line by line through the buffer, adding newlines where
       the text is wrapped at some character other than an existing newline */
    int64_t fromPos = startPos;
    int64_t toPos = TextDCountForwardNLines(startPos, 1, false);
    while (toPos < endPos) {
        outBuf.BufCopyFromBuf(buffer_, fromPos, toPos, outPos);
        outPos += toPos - fromPos;
        char c = outBuf.BufGetCharacter(outPos - 1);
        if (c == ' ' || c == '\t') {
            outBuf.BufReplaceEx(outPos - 1, outPos, "\n");
        }
        else if (c != '\n') {
            outBuf.BufInsertEx(outPos, '\n');
            outPos++;
        }
        fromPos = toPos;
        toPos = TextDCountForwardNLines(fromPos, 1, true);
    }
    outBuf.BufCopyFromBuf(buffer_, fromPos, endPos, outPos);

    // return the contents of the output buffer as a string
    return outBuf.BufGetAllEx();
}

void TextArea::setStyleBuffer(const std::shared_ptr<TextBuffer> &buffer) {
    styleBuffer_ = buffer;
}

int TextArea::getWrapMargin() const {
    return P_wrapMargin;
}

int TextArea::getColumns() const {
    return P_columns;
}

void TextArea::insertStringAP(const QString &string, EventFlags flags) {

    EMIT_EVENT_0("insert_string");

    cancelDrag();
    if (checkReadOnly()) {
        return;
    }

    std::string str = string.toStdString();

    if (P_smartIndent) {
		SmartIndentEvent smartIndent;
        smartIndent.reason        = CHAR_TYPED;
        smartIndent.pos           = cursorPos_;
        smartIndent.indentRequest = 0;
        smartIndent.charsTyped    = str;

        for(auto &c : smartIndentCallbacks_) {
            c.first(this, &smartIndent, c.second);
        }
    }

    TextInsertAtCursorEx(str, true, true);
    buffer_->BufUnselect();
}

/*
** Translate line and column to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
int64_t TextArea::TextDLineAndColToPos(int64_t lineNum, int64_t column) {

    int i;
    int64_t lineStart = 0;

    // Count lines    
    if (lineNum < 1) {
        lineNum = 1;
    }

    int64_t lineEnd = -1;
    for (i = 1; i <= lineNum && lineEnd < buffer_->BufGetLength(); i++) {
        lineStart = lineEnd + 1;
        lineEnd = buffer_->BufEndOfLine(lineStart);
    }

    // If line is beyond end of buffer, position at last character in buffer
    if (lineNum >= i) {
        return lineEnd;
    }

    // Start character index at zero
    int charIndex = 0;

    // Only have to count columns if column isn't zero (or negative)
    if (column > 0) {

        int64_t charLen = 0;

        // Count columns, expanding each character
        std::string lineStr = buffer_->BufGetRangeEx(lineStart, lineEnd);
        int outIndex = 0;
        for (int64_t i = lineStart; i < lineEnd; i++, charIndex++) {

            char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];
            charLen = TextBuffer::BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar, buffer_->BufGetTabDist());

            if (outIndex + charLen >= column) {
                break;
            }

            outIndex += charLen;
        }

        /* If the column is in the middle of an expanded character, put cursor
         * in front of character if in first half of character, and behind
         * character if in last half of character
         */
        if (column >= outIndex + (charLen / 2))
            charIndex++;

        // If we are beyond the end of the line, back up one space
        if ((i >= lineEnd) && (charIndex > 0))
            charIndex--;
    }

    // Position is the start of the line plus the index into line buffer
    return lineStart + charIndex;
}

int TextArea::TextDGetCalltipID(int id) const {

    if (id == 0) {
        return calltip_.ID;
    } else if (id == calltip_.ID) {
        return id;
    } else {
        return 0;
    }
}

void TextArea::TextDKillCalltip(int id) {
    if (calltip_.ID == 0) {
        return;
    }

    if (id == 0 || id == calltip_.ID) {
        if(calltipWidget_) {
            calltipWidget_->hide();
        }

        calltip_.ID = 0;
    }
}

int TextArea::TextDShowCalltip(const QString &text, bool anchored, int pos, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignStrict alignMode) {

    static int StaticCalltipID = 1;

    int rel_x;
    int rel_y;

    // Destroy any previous calltip
    TextDKillCalltip(0);

    if(!calltipWidget_) {
        calltipWidget_ = new CallTipWidget(this, Qt::Tool | Qt::FramelessWindowHint);
    }

    // Expand any tabs in the calltip and make it an XmString
    QString textCpy = expandAllTabsEx(text, buffer_->BufGetTabDistance());

    // Figure out where to put the tip
    if (anchored) {
        // Put it at the specified position
        // If position is not displayed, return 0
        if (pos < firstChar_ || pos > lastChar_) {
            QApplication::beep();
            return 0;
        }
        calltip_.pos = pos;
    } else {
        /* Put it next to the cursor, or in the center of the window if the
            cursor is offscreen and mode != strict */
        if (!TextDPositionToXY(TextGetCursorPos(), &rel_x, &rel_y)) {
            if (alignMode == TipAlignStrict::Strict) {
                QApplication::beep();
                return 0;
            }
            calltip_.pos = -1;
        } else
            // Store the x-offset for use when redrawing
            calltip_.pos = rel_x;
    }

    // Should really bounds-check these enumerations...
    calltip_.ID        = StaticCalltipID;
    calltip_.anchored  = anchored;
    calltip_.hAlign    = hAlign;
    calltip_.vAlign    = vAlign;
    calltip_.alignMode = alignMode;

    /* Increment the static calltip ID.  Macro variables can only be int,
        not unsigned, so have to work to keep it > 0 on overflow */
    if (++StaticCalltipID <= 0) {
        StaticCalltipID = 1;
    }

    calltipWidget_->setText(textCpy);
    TextDRedrawCalltip(0);

    return calltip_.ID;
}

/*
** In continuous wrap mode, internal line numbers are calculated after
** wrapping.  A separate non-wrapped line count is maintained when line
** numbering is turned on.  There is some performance cost to maintaining this
** line count, so normally absolute line numbers are not tracked if line
** numbering is off.  This routine allows callers to specify that they still
** want this line count maintained (for use via TextDPosToLineAndCol).
** More specifically, this allows the line number reported in the statistics
** line to be calibrated in absolute lines, rather than post-wrapped lines.
*/
void TextArea::TextDMaintainAbsLineNum(bool state) {
    needAbsTopLineNum_ = state;
    resetAbsLineNum();
}

TextBuffer *TextArea::TextGetBuffer() const {
    return buffer_;
}

int TextArea::TextDMinFontWidth(bool considerStyles) const {

    QFontMetrics fm(font_);

    int fontWidth = fm.maxWidth();
    if (considerStyles) {
        for(const StyleTableEntry &style : styleTable_) {
            QFontMetrics fm(style.font);

            // NOTE(eteran): this was min_bounds.width, we just assume that 'i' is the thinnest character
            fontWidth = std::min(fontWidth, fm.width(QLatin1Char('i')));
        }
    }

	return fontWidth;
}

int TextArea::TextDMaxFontWidth(bool considerStyles) const {

    QFontMetrics fm(font_);

    int fontWidth = fm.maxWidth();
    if (considerStyles) {
        for(const StyleTableEntry &style : styleTable_) {
            QFontMetrics fm(style.font);
            fontWidth = std::max(fontWidth, fm.maxWidth());
        }
    }

	return fontWidth;
}

int TextArea::TextNumVisibleLines() const {
    return nVisibleLines_;
}

int64_t TextArea::TextFirstVisibleLine() const {
    return topLineNum_;
}

int TextArea::TextVisibleWidth() const {
    return rect_.width();
}

void TextArea::beginningOfSelectionAP(EventFlags flags) {

    EMIT_EVENT_0("beginning_of_selection");

    int64_t start;
    int64_t end;
    bool isRect;
    int64_t rectStart;
    int64_t rectEnd;

    if (!buffer_->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd)) {
        return;
    }

    if (!isRect) {
        TextSetCursorPos(start);
    } else {
        TextSetCursorPos(buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(start), rectStart));
    }
}

void TextArea::deleteSelectionAP(EventFlags flags) {

    EMIT_EVENT_0("delete_selection");

    cancelDrag();
    if (checkReadOnly()) {
        return;
    }

    deletePendingSelection();
}

void TextArea::deleteNextWordAP(EventFlags flags) {

    EMIT_EVENT_0("delete_next_word");

    int64_t insertPos = TextDGetInsertPosition();
    int64_t lineEnd = buffer_->BufEndOfLine(insertPos);
    bool silent = flags & NoBellFlag;

    cancelDrag();
    if (checkReadOnly()) {
        return;
    }

    if (deletePendingSelection()) {
        return;
    }

    if (insertPos == lineEnd) {
        ringIfNecessary(silent);
        return;
    }

    int64_t pos = insertPos;
    while (P_delimiters.find(buffer_->BufGetCharacter(pos)) != std::string::npos && pos != lineEnd) {
        pos++;
    }

    pos = endOfWord(pos);
    buffer_->BufRemove(insertPos, pos);
    checkAutoShowInsertPos();
    callCursorMovementCBs();
}

void TextArea::endOfSelectionAP(EventFlags flags) {

    EMIT_EVENT_0("end_of_selection");

    int64_t start;
    int64_t end;
    bool isRect;
    int64_t rectStart;
    int64_t rectEnd;

    if (!buffer_->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd)) {
        return;
    }

    if (!isRect) {
        TextSetCursorPos(end);
    } else {
        TextSetCursorPos(buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(end), rectEnd));
    }
}

void TextArea::scrollUpAP(int count, ScrollUnits units, EventFlags flags) {

    EMIT_EVENT_0("scroll_up");

    int64_t topLineNum;
    int horizOffset;
    int nLines = count;

    if(units == ScrollUnits::Pages) {
        nLines *= lineStarts_.size();
    }

    TextDGetScroll(&topLineNum, &horizOffset);
    TextDSetScroll(topLineNum - nLines, horizOffset);
}

void TextArea::scrollDownAP(int count, ScrollUnits units, EventFlags flags) {

    EMIT_EVENT_0("scroll_down");

    int64_t topLineNum;
    int horizOffset;
    int nLines = count;

    if(units == ScrollUnits::Pages) {
        nLines *= lineStarts_.size();
    }

    TextDGetScroll(&topLineNum, &horizOffset);
    TextDSetScroll(topLineNum + nLines, horizOffset);
}


void TextArea::scrollLeftAP(int pixels, EventFlags flags) {
    EMIT_EVENT_0("scroll_left");
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - pixels);
}

void TextArea::scrollRightAP(int pixels, EventFlags flags) {
    EMIT_EVENT_0("scroll_right");
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + pixels);
}

void TextArea::scrollToLineAP(int line, EventFlags flags) {

    EMIT_EVENT_0("scroll_to_line");

    int64_t topLineNum;
    int horizOffset;

    TextDGetScroll(&topLineNum, &horizOffset);
    TextDSetScroll(line, horizOffset);
}
