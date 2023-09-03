
#include "TextArea.h"
#include "BlockDragTypes.h"
#include "CallTipWidget.h"
#include "DocumentWidget.h"
#include "DragEndEvent.h"
#include "DragStates.h"
#include "Font.h"
#include "Highlight.h"
#include "LanguageMode.h"
#include "LineNumberArea.h"
#include "Preferences.h"
#include "RangesetTable.h"
#include "SmartIndentEvent.h"
#include "TextAreaMimeData.h"
#include "TextBuffer.h"
#include "TextEditEvent.h"
#include "X11Colors.h"

#include <QApplication>
#include <QClipboard>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPoint>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCodec>
#include <QTimer>
#include <QtDebug>
#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <QWindow>
#endif

#include <gsl/gsl_util>
#include <memory>

#define EMIT_EVENT_0(name)                                       \
	do {                                                         \
		if (!(flags & SuppressRecording)) {                       \
			TextEditEvent textEvent(QLatin1String(name), flags); \
			QApplication::sendEvent(this, &textEvent);           \
		}                                                        \
	} while (0)

#define EMIT_EVENT_1(name, arg)                                         \
	do {                                                                \
		if (!(flags & SuppressRecording)) {                              \
			TextEditEvent textEvent(QLatin1String(name), flags, (arg)); \
			QApplication::sendEvent(this, &textEvent);                  \
		}                                                               \
	} while (0)

namespace {

/**
 * @brief asciiToUnicode
 * @param chars
 * @param len
 * @return
 */
QString asciiToUnicode(view::string_view string) {
	QString s;
	s.reserve(static_cast<int>(string.size()));

	for (char c : string) {
		uint32_t ch = c & 0xff;
		if (ch < 0x80 || ch >= 0xa0) {
			s.append(QChar(ch));
		} else {
			s.append(QChar::ReplacementCharacter);
		}
	}
	return s;
}

constexpr int DefaultVMargin     = 2;
constexpr int DefaultHMargin     = 2;
constexpr int DefaultCursorWidth = 2;

constexpr int SIZE_HINT_DURATION = 1000;

constexpr int CALLTIP_EDGE_GUARD = 5;

// Length of delay in milliseconds for vertical auto-scrolling
constexpr int VERTICAL_SCROLL_DELAY = 50;

/* Masks for text drawing methods.  These are or'd together to form an
   integer which describes what drawing calls to use to draw a string */
constexpr int STYLE_LOOKUP_SHIFT = 0;
constexpr int FILL_SHIFT         = 8;
constexpr int SECONDARY_SHIFT    = 9;
constexpr int PRIMARY_SHIFT      = 10;
constexpr int HIGHLIGHT_SHIFT    = 11;
constexpr int BACKLIGHT_SHIFT    = 12;
constexpr int RANGESET_SHIFT     = 20;

constexpr uint32_t STYLE_LOOKUP_MASK = (0xff << STYLE_LOOKUP_SHIFT);
constexpr uint32_t FILL_MASK         = (1 << FILL_SHIFT);
constexpr uint32_t SECONDARY_MASK    = (1 << SECONDARY_SHIFT);
constexpr uint32_t PRIMARY_MASK      = (1 << PRIMARY_SHIFT);
constexpr uint32_t HIGHLIGHT_MASK    = (1 << HIGHLIGHT_SHIFT);
constexpr uint32_t BACKLIGHT_MASK    = (0xff << BACKLIGHT_SHIFT);
constexpr uint32_t RANGESET_MASK     = (0x3f << RANGESET_SHIFT);

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
constexpr int MAX_DISP_LINE_LEN = 1024;

/**
 * @brief offscreenV
 * @param desktop
 * @param top
 * @param height
 * @return
 */
bool offscreenV(QScreen *screen, int top, int height) {
	return (top < CALLTIP_EDGE_GUARD || top + height >= screen->geometry().height() - CALLTIP_EDGE_GUARD);
}

/*
** Returns a new string with each \t replaced with tab_width spaces or
** a pointer to text if there were no tabs.
** Note that this is dumb replacement, not smart tab-like behavior!  The goal
** is to prevent tabs from turning into squares in calltips, not to get the
** formatting just right.
*/
QString expandAllTabs(const QString &text, int tab_width) {

	// First count 'em
	const auto nTabs = static_cast<int>(std::count(text.begin(), text.end(), QLatin1Char('\t')));

	if (nTabs == 0) {
		return text;
	}

	// Allocate the new string
	int len = text.size() + (tab_width - 1) * nTabs;

	QString textCpy;
	textCpy.reserve(len);

	auto it = std::back_inserter(textCpy);

	// Now replace 'em
	for (QChar ch : text) {
		if (ch == QLatin1Char('\t')) {
			std::fill_n(it, tab_width, QLatin1Char(' '));
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
void findTextMargins(TextBuffer *buf, TextCursor start, TextCursor end, int64_t *leftMargin, int64_t *rightMargin) {

	int width    = 0;
	int maxWidth = 0;
	int minWhite = INT_MAX;
	bool inWhite = true;

	for (TextCursor pos = start; pos < end; ++pos) {
		const char ch = buf->BufGetCharacter(pos);

		if (inWhite && ch != ' ' && ch != '\t') {
			inWhite = false;
			if (width < minWhite) {
				minWhite = width;
			}
		}

		if (ch == '\n') {
			if (width > maxWidth) {
				maxWidth = width;
			}

			width   = 0;
			inWhite = true;
		} else {
			width += TextBuffer::BufCharWidth(ch, width, buf->BufGetTabDistance());
		}
	}

	if (width > maxWidth) {
		maxWidth = width;
	}

	*leftMargin  = (minWhite == INT_MAX) ? 0 : minWhite;
	*rightMargin = maxWidth;
}

/*
** Find a text position in buffer "buf" by counting forward or backward
** from a reference position with known line number
*/
TextCursor findRelativeLineStart(const TextBuffer *buf, TextCursor referencePos, int referenceLineNum, int newLineNum) {

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
void bufPreDeleteCB(TextCursor pos, int64_t nDeleted, void *arg) {
	auto area = static_cast<TextArea *>(arg);
	area->bufPreDeleteCallback(pos, nDeleted);
}

/*
** Callback attached to the text buffer to receive modification information
*/
void bufModifiedCB(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *arg) {
	auto area = static_cast<TextArea *>(arg);
	area->bufModifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText);
}

/*
** Count the number of newlines in a text string
*/
int countNewlines(view::string_view string) {
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
void trackModifyRange(TextCursor *rangeStart, TextCursor *modRangeEnd, TextCursor *unmodRangeEnd, TextCursor modPos, int64_t nInserted, int64_t nDeleted) {
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

/**
 * @brief isModifier
 * @param e
 * @return
 */
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

// I'd prefer to be able to use something like tr("Ctrl+PageUp") for the key + modifiers
struct InputHandler {
	using handler_type = void (TextArea::*)(TextArea::EventFlags flags);

	Qt::Key key;
	Qt::KeyboardModifiers modifiers;
	handler_type handler;
	TextArea::EventFlags flags; // flags to pass to the handler
};

// NOTE(eteran): a nullptr handler just means "beep"
constexpr InputHandler inputHandlers[] = {

	// Keyboard zoom support
	{Qt::Key_Equal, Qt::ControlModifier, &TextArea::zoomInAP, TextArea::NoneFlag},  // zoom-in()
	{Qt::Key_Minus, Qt::ControlModifier, &TextArea::zoomOutAP, TextArea::NoneFlag}, // zoom-out()

	{Qt::Key_PageUp, Qt::ControlModifier, &TextArea::previousDocumentAP, TextArea::NoneFlag},                                                      // previous-document()
	{Qt::Key_PageDown, Qt::ControlModifier, &TextArea::nextDocumentAP, TextArea::NoneFlag},                                                        // next-document()
	{Qt::Key_PageUp, Qt::ShiftModifier | Qt::AltModifier, &TextArea::previousPageAP, TextArea::ExtendFlag | TextArea::RectFlag},                   // previous-page(extend, rect)
	{Qt::Key_PageUp, Qt::ShiftModifier | Qt::MetaModifier, &TextArea::previousPageAP, TextArea::ExtendFlag | TextArea::RectFlag},                  // previous-page(extend, rect)
	{Qt::Key_PageUp, Qt::ShiftModifier, &TextArea::previousPageAP, TextArea::ExtendFlag},                                                          // previous-page(extend)
	{Qt::Key_PageUp, Qt::NoModifier, &TextArea::previousPageAP, TextArea::NoneFlag},                                                               // previous-page()
	{Qt::Key_PageDown, Qt::ShiftModifier | Qt::AltModifier, &TextArea::nextPageAP, TextArea::ExtendFlag | TextArea::RectFlag},                     // next-page(extend, rect)
	{Qt::Key_PageDown, Qt::ShiftModifier | Qt::MetaModifier, &TextArea::nextPageAP, TextArea::ExtendFlag | TextArea::RectFlag},                    // next-page(extend, rect)
	{Qt::Key_PageDown, Qt::ShiftModifier, &TextArea::nextPageAP, TextArea::ExtendFlag},                                                            // next-page(extend)
	{Qt::Key_PageDown, Qt::NoModifier, &TextArea::nextPageAP, TextArea::NoneFlag},                                                                 // next-page()
	{Qt::Key_PageUp, Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier, &TextArea::pageLeftAP, TextArea::ExtendFlag | TextArea::RectFlag}, // page-left(extent, rect);
	{Qt::Key_PageUp, Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier, &TextArea::pageLeftAP, TextArea::ExtendFlag | TextArea::RectFlag},  // page-left(extent, rect);
	{Qt::Key_PageUp, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::pageLeftAP, TextArea::ExtendFlag},                                        // page-left(extent);
#if 0                                                                                                                                              // NOTE(eteran): no page left/right on most keyboards, so conflict here :-/
	{ Qt::Key_PageUp,    Qt::ControlModifier,                                                               &TextArea::pageLeftAP,                       TextArea::NoneFlag }, // page-left()
	{ Qt::Key_PageDown,  Qt::ControlModifier,                                                               &TextArea::pageRightAP,                      TextArea::NoneFlag }, // page-right()
#endif
	{Qt::Key_PageDown, Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier, &TextArea::pageRightAP, TextArea::ExtendFlag | TextArea::RectFlag},     // page-right(extent, rect);
	{Qt::Key_PageDown, Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier, &TextArea::pageRightAP, TextArea::ExtendFlag | TextArea::RectFlag},      // page-right(extent, rect);
	{Qt::Key_PageDown, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::pageRightAP, TextArea::ExtendFlag},                                            // page-right(extent);
	{Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier, &TextArea::processShiftUpAP, TextArea::RectFlag},                                                   // process-shift-up(rect)
	{Qt::Key_Up, Qt::MetaModifier | Qt::ShiftModifier, &TextArea::processShiftUpAP, TextArea::RectFlag},                                                  // process-shift-up(rect)
	{Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier, &TextArea::processShiftDownAP, TextArea::RectFlag},                                               // process-shift-down(rect)
	{Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier, &TextArea::processShiftDownAP, TextArea::RectFlag},                                              // process-shift-down(rect)
	{Qt::Key_Up, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::backwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag},   // backward-paragraph(extent, rect)
	{Qt::Key_Up, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::backwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag},  // backward-paragraph(extend, rect)
	{Qt::Key_Up, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::backwardParagraphAP, TextArea::ExtendFlag},                                          // backward-paragraph(extend)
	{Qt::Key_Down, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::forwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag},  // forward-paragraph(extent, rect)
	{Qt::Key_Down, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::forwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag}, // forward-paragraph(extend, rect)
	{Qt::Key_Down, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::forwardParagraphAP, TextArea::ExtendFlag},                                         // forward-paragraph(extend)
	{Qt::Key_Right, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::forwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},      // forward-word(extend, rect)
	{Qt::Key_Right, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::forwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},     // forward-word(extend, rect)
	{Qt::Key_Right, Qt::AltModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::RightFlag | TextArea::RectFlag},                               // key-select(right, rect)
	{Qt::Key_Right, Qt::MetaModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::RightFlag | TextArea::RectFlag},                              // key-select(right, rect)
	{Qt::Key_Right, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::forwardWordAP, TextArea::ExtendFlag},                                             // forward-word(extend)
	{Qt::Key_Left, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::backwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},      // backward-word(extend, rect)
	{Qt::Key_Left, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::backwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},     // backward-word(extend, rect)
	{Qt::Key_Left, Qt::AltModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::LeftFlag | TextArea::RectFlag},                                 // key-select(left, rect)
	{Qt::Key_Left, Qt::MetaModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::LeftFlag | TextArea::RectFlag},                                // key-select(left, rect)
	{Qt::Key_Left, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::backwardWordAP, TextArea::ExtendFlag},                                             // backward-word(extend)
#if defined(Q_OS_MACOS)
	{Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::processShiftUpAP, TextArea::RectFlag},                                                   // process-shift-up(rect)
	{Qt::Key_Up, Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::processShiftUpAP, TextArea::RectFlag},                                                  // process-shift-up(rect)
	{Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::processShiftDownAP, TextArea::RectFlag},                                               // process-shift-down(rect)
	{Qt::Key_Down, Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::processShiftDownAP, TextArea::RectFlag},                                              // process-shift-down(rect)
	{Qt::Key_Up, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::backwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag},   // backward-paragraph(extent, rect)
	{Qt::Key_Up, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::backwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag},  // backward-paragraph(extend, rect)
	{Qt::Key_Up, Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::backwardParagraphAP, TextArea::ExtendFlag},                                          // backward-paragraph(extend)
	{Qt::Key_Down, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::forwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag},  // forward-paragraph(extent, rect)
	{Qt::Key_Down, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::forwardParagraphAP, TextArea::ExtendFlag | TextArea::RectFlag}, // forward-paragraph(extend, rect)
	{Qt::Key_Down, Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::forwardParagraphAP, TextArea::ExtendFlag},                                         // forward-paragraph(extend)
	{Qt::Key_Right, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::forwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},      // forward-word(extend, rect)
	{Qt::Key_Right, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::forwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},     // forward-word(extend, rect)
	{Qt::Key_Right, Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::keySelectAP, TextArea::RightFlag | TextArea::RectFlag},                               // key-select(right, rect)
	{Qt::Key_Right, Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::keySelectAP, TextArea::RightFlag | TextArea::RectFlag},                              // key-select(right, rect)
	{Qt::Key_Right, Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::forwardWordAP, TextArea::ExtendFlag},                                             // forward-word(extend)
	{Qt::Key_Left, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::backwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},      // backward-word(extend, rect)
	{Qt::Key_Left, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::backwardWordAP, TextArea::ExtendFlag | TextArea::RectFlag},     // backward-word(extend, rect)
	{Qt::Key_Left, Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::keySelectAP, TextArea::LeftFlag | TextArea::RectFlag},                                 // key-select(left, rect)
	{Qt::Key_Left, Qt::MetaModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::keySelectAP, TextArea::LeftFlag | TextArea::RectFlag},                                // key-select(left, rect)
	{Qt::Key_Left, Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::backwardWordAP, TextArea::ExtendFlag},                                             // backward-word(extend)
#endif
	{Qt::Key_End, Qt::AltModifier | Qt::ShiftModifier, &TextArea::endOfLine, TextArea::ExtendFlag | TextArea::RectFlag},                                 // end-of-line(extend, rect)
	{Qt::Key_End, Qt::MetaModifier | Qt::ShiftModifier, &TextArea::endOfLine, TextArea::ExtendFlag | TextArea::RectFlag},                                // end-of-line(extend, rect)
	{Qt::Key_End, Qt::ShiftModifier, &TextArea::endOfLine, TextArea::ExtendFlag},                                                                        // end-of-line(extend)
	{Qt::Key_End, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::endOfFileAP, TextArea::ExtendFlag | TextArea::RectFlag},         // end-of-file(extend, rect)
	{Qt::Key_End, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::endOfFileAP, TextArea::ExtendFlag | TextArea::RectFlag},        // end-of-file(extend, rect)
	{Qt::Key_End, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::endOfFileAP, TextArea::ExtendFlag},                                                // end-of-file(extend)
	{Qt::Key_Home, Qt::AltModifier | Qt::ShiftModifier, &TextArea::beginningOfLine, TextArea::ExtendFlag | TextArea::RectFlag},                          // beginning-of-line(extend, rect)
	{Qt::Key_Home, Qt::MetaModifier | Qt::ShiftModifier, &TextArea::beginningOfLine, TextArea::ExtendFlag | TextArea::RectFlag},                         // beginning-of-line(extend, rect)
	{Qt::Key_Home, Qt::ShiftModifier, &TextArea::beginningOfLine, TextArea::ExtendFlag},                                                                 // beginning-of-line(extend)
	{Qt::Key_Home, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::beginningOfFileAP, TextArea::ExtendFlag | TextArea::RectFlag},  // beginning-of-file(extend, rect)
	{Qt::Key_Home, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::beginningOfFileAP, TextArea::ExtendFlag | TextArea::RectFlag}, // beginning-of-file(extend, rect)
	{Qt::Key_Home, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::beginningOfFileAP, TextArea::ExtendFlag},                                         // beginning-of-file(extend)
	{Qt::Key_Insert, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::copyPrimaryAP, TextArea::RectFlag},                           // copy-primary(rect)
	{Qt::Key_Insert, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::copyPrimaryAP, TextArea::RectFlag},                          // copy-primary(rect)
	{Qt::Key_Delete, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::cutPrimaryAP, TextArea::RectFlag},                            // cut-primary(rect)
	{Qt::Key_Delete, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::cutPrimaryAP, TextArea::RectFlag},                           // cut-primary(rect)
	{Qt::Key_Space, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::NoneFlag},                                                // key-select()
	{Qt::Key_Space, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::RectFlag},                              // key-select(rect)
	{Qt::Key_Space, Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::RectFlag},                             // key-select(rect)
	{Qt::Key_Delete, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::cutPrimaryAP, TextArea::NoneFlag},                                              // cut-primary()
	{Qt::Key_Delete, Qt::ControlModifier, &TextArea::deleteToEndOfLineAP, TextArea::NoneFlag},                                                           // delete-to-end-of-line()
	{Qt::Key_U, Qt::ControlModifier, &TextArea::deleteToStartOfLineAP, TextArea::NoneFlag},                                                              // delete-to-start-of-line()
	{Qt::Key_Slash, Qt::ControlModifier, &TextArea::selectAllAP, TextArea::NoneFlag},                                                                    // select-all()
	{Qt::Key_Backslash, Qt::ControlModifier, &TextArea::deselectAllAP, TextArea::NoneFlag},                                                              // deselect-all()
	{Qt::Key_Tab, Qt::NoModifier, &TextArea::processTabAP, TextArea::NoneFlag},                                                                          // process-tab()
	{Qt::Key_Right, Qt::NoModifier, &TextArea::forwardCharacter, TextArea::NoneFlag},                                                                    // forward-character()
	{Qt::Key_Left, Qt::NoModifier, &TextArea::backwardCharacter, TextArea::NoneFlag},                                                                    // backward-character()
	{Qt::Key_Up, Qt::NoModifier, &TextArea::processUp, TextArea::NoneFlag},                                                                              // process-up()
	{Qt::Key_Down, Qt::NoModifier, &TextArea::processDown, TextArea::NoneFlag},                                                                          // process-down()
#if defined(Q_OS_MACOS)
	{Qt::Key_Right, Qt::KeypadModifier, &TextArea::forwardCharacter, TextArea::NoneFlag}, // forward-character()
	{Qt::Key_Left, Qt::KeypadModifier, &TextArea::backwardCharacter, TextArea::NoneFlag}, // backward-character()
	{Qt::Key_Up, Qt::KeypadModifier, &TextArea::processUp, TextArea::NoneFlag},           // process-up()
	{Qt::Key_Down, Qt::KeypadModifier, &TextArea::processDown, TextArea::NoneFlag},       // process-down()
#endif
	{Qt::Key_Return, Qt::NoModifier, &TextArea::newline, TextArea::NoneFlag},                                        // newline()
	{Qt::Key_Enter, Qt::KeypadModifier, &TextArea::newline, TextArea::NoneFlag},                                     // newline()
	{Qt::Key_Backspace, Qt::NoModifier, &TextArea::deletePreviousCharacter, TextArea::NoneFlag},                     // delete-previous-character()
	{Qt::Key_Backspace, Qt::ShiftModifier, &TextArea::deletePreviousCharacter, TextArea::NoneFlag},                  // delete-previous-character()
	{Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::deletePreviousWord, TextArea::NoneFlag}, // delete-previous-word()
	{Qt::Key_Escape, Qt::NoModifier, &TextArea::processCancel, TextArea::NoneFlag},                                  // process-cancel()
	{Qt::Key_Return, Qt::ControlModifier, &TextArea::newlineAndIndentAP, TextArea::NoneFlag},                        // newline-and-indent()
	{Qt::Key_Enter, Qt::KeypadModifier | Qt::ControlModifier, &TextArea::newlineAndIndentAP, TextArea::NoneFlag},    // newline-and-indent()
	{Qt::Key_Return, Qt::ShiftModifier, &TextArea::newlineNoIndentAP, TextArea::NoneFlag},                           // newline-no-indent()
	{Qt::Key_Enter, Qt::KeypadModifier | Qt::ShiftModifier, &TextArea::newlineNoIndentAP, TextArea::NoneFlag},       // newline-no-indent()
	{Qt::Key_Home, Qt::NoModifier, &TextArea::beginningOfLine, TextArea::NoneFlag},                                  // process-home()
	{Qt::Key_Backspace, Qt::ControlModifier, &TextArea::deletePreviousWord, TextArea::NoneFlag},                     // delete-previous-word()
	{Qt::Key_End, Qt::NoModifier, &TextArea::endOfLine, TextArea::NoneFlag},                                         // end-of-line()
	{Qt::Key_Delete, Qt::NoModifier, &TextArea::deleteNextCharacter, TextArea::NoneFlag},                            // delete-next-character()
	{Qt::Key_Insert, Qt::NoModifier, &TextArea::toggleOverstrike, TextArea::NoneFlag},                               // toggle-overstrike()
	{Qt::Key_Up, Qt::ShiftModifier, &TextArea::processShiftUpAP, TextArea::NoneFlag},                                // process-shift-up()
	{Qt::Key_Down, Qt::ShiftModifier, &TextArea::processShiftDownAP, TextArea::NoneFlag},                            // process-shift-down()
	{Qt::Key_Left, Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::LeftFlag},                                   // key-select(left)
	{Qt::Key_Right, Qt::ShiftModifier, &TextArea::keySelectAP, TextArea::RightFlag},                                 // key-select(right)
#if defined(Q_OS_MACOS)
	{Qt::Key_Up, Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::processShiftUpAP, TextArea::NoneFlag},     // process-shift-up()
	{Qt::Key_Down, Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::processShiftDownAP, TextArea::NoneFlag}, // process-shift-down()
	{Qt::Key_Left, Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::keySelectAP, TextArea::LeftFlag},        // key-select(left)
	{Qt::Key_Right, Qt::ShiftModifier | Qt::KeypadModifier, &TextArea::keySelectAP, TextArea::RightFlag},      // key-select(right)
#endif
	{Qt::Key_Delete, Qt::ShiftModifier, &TextArea::cutClipboard, TextArea::NoneFlag},                        // cut-clipboard()
	{Qt::Key_Insert, Qt::ControlModifier, &TextArea::copyClipboard, TextArea::NoneFlag},                     // copy-clipboard()
	{Qt::Key_Insert, Qt::ShiftModifier, &TextArea::pasteClipboard, TextArea::NoneFlag},                      // paste-clipboard()
	{Qt::Key_Insert, Qt::ControlModifier | Qt::ShiftModifier, &TextArea::copyPrimaryAP, TextArea::NoneFlag}, // copy-primary()
	{Qt::Key_Home, Qt::ControlModifier, &TextArea::beginningOfFileAP, TextArea::NoneFlag},                   // beginning-of-file()
	{Qt::Key_End, Qt::ControlModifier, &TextArea::endOfFileAP, TextArea::NoneFlag},                          // end-of-file()
	{Qt::Key_Left, Qt::ControlModifier, &TextArea::backwardWordAP, TextArea::NoneFlag},                      // backward-word()
	{Qt::Key_Right, Qt::ControlModifier, &TextArea::forwardWordAP, TextArea::NoneFlag},                      // forward-word()
	{Qt::Key_Up, Qt::ControlModifier, &TextArea::backwardParagraphAP, TextArea::NoneFlag},                   // backward-paragraph()
	{Qt::Key_Down, Qt::ControlModifier, &TextArea::forwardParagraphAP, TextArea::NoneFlag},                  // forward-paragraph()
#if defined(Q_OS_MACOS)
	{Qt::Key_Left, Qt::ControlModifier | Qt::KeypadModifier, &TextArea::backwardWordAP, TextArea::NoneFlag},     // backward-word()
	{Qt::Key_Right, Qt::ControlModifier | Qt::KeypadModifier, &TextArea::forwardWordAP, TextArea::NoneFlag},     // forward-word()
	{Qt::Key_Up, Qt::ControlModifier | Qt::KeypadModifier, &TextArea::backwardParagraphAP, TextArea::NoneFlag},  // backward-paragraph()
	{Qt::Key_Down, Qt::ControlModifier | Qt::KeypadModifier, &TextArea::forwardParagraphAP, TextArea::NoneFlag}, // forward-paragraph()
#endif
};

}

/**
 * @brief TextArea::TextArea
 * @param document
 * @param buffer
 * @param font
 */
TextArea::TextArea(DocumentWidget *document, TextBuffer *buffer, const QFont &font)
	: QAbstractScrollArea(document), document_(document), buffer_(buffer), font_(font) {

	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setMouseTracking(true);
	setFocusPolicy(Qt::WheelFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &TextArea::verticalScrollBar_valueChanged);
	connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &TextArea::horizontalScrollBar_valueChanged);

	autoScrollTimer_  = new QTimer(this);
	cursorBlinkTimer_ = new QTimer(this);
	clickTimer_       = new QTimer(this);
	lineNumberArea_   = new LineNumberArea(this);

	autoScrollTimer_->setSingleShot(true);
	connect(autoScrollTimer_, &QTimer::timeout, this, &TextArea::autoScrollTimerTimeout);
	connect(cursorBlinkTimer_, &QTimer::timeout, this, &TextArea::cursorBlinkTimerTimeout);

	clickTimer_->setSingleShot(true);
	connect(clickTimer_, &QTimer::timeout, this, [this]() {
		clickTimerExpired_ = true;
	});

	setWordDelimiters(Preferences::GetPrefDelimiters().toStdString());

	showTerminalSizeHint_    = Preferences::GetPrefShowResizeNotification();
	colorizeHighlightedText_ = Preferences::GetPrefColorizeHighlightedText();
	autoWrapPastedText_      = Preferences::GetPrefAutoWrapPastedText();
	heavyCursor_             = Preferences::GetPrefHeavyCursor();
	readOnly_                = document->lockReasons().isAnyLocked();
	wrapMargin_              = Preferences::GetPrefWrapMargin();
	autoIndent_              = document->autoIndentStyle() == IndentStyle::Auto;
	smartIndent_             = document->autoIndentStyle() == IndentStyle::Smart;
	autoWrap_                = document->wrapMode() == WrapStyle::Newline;
	continuousWrap_          = document->wrapMode() == WrapStyle::Continuous;
	overstrike_              = document->overstrike();
	hidePointer_             = Preferences::GetPrefTypingHidesPointer();
	smartHome_               = Preferences::GetPrefSmartHome();

	cursorBlinkTimer_->setInterval(QApplication::cursorFlashTime() / 2);

	updateFontMetrics(font);

	horizontalScrollBar()->setSingleStep(fixedFontWidth_);

	// set the default margins
	viewport()->setContentsMargins(DefaultHMargin, DefaultVMargin, 0, 0);

	/* Attach the callback to the text buffer for receiving modification
	 * information */
	if (buffer) {
		buffer->BufAddModifyCB(bufModifiedCB, this);
		buffer->BufAddPreDeleteCB(bufPreDeleteCB, this);
	}

	// Update the display to reflect the contents of the buffer
	if (buffer) {
		bufModifiedCB(buffer_->BufStartOfBuffer(), buffer->length(), 0, 0, {}, this);
	}

	// Decide if the horizontal scroll bar needs to be visible
	hideOrShowHScrollBar();

	// track when we lose ownership of the selection
	if (QApplication::clipboard()->supportsSelection()) {
		connect(QApplication::clipboard(), &QClipboard::selectionChanged, this, [this]() {
			const bool isOwner = TextAreaMimeData::isOwner(QApplication::clipboard()->mimeData(QClipboard::Selection), buffer_);
			if (!isOwner) {
				buffer_->BufUnselect();
			}
		});
	}
}

/**
 * @brief TextArea::pasteClipboard
 * @param flags
 */
void TextArea::pasteClipboard(EventFlags flags) {

	EMIT_EVENT_0("paste_clipboard");

	if (flags & RectFlag) {
		TextColPasteClipboard();
	} else {
		TextPasteClipboard();
	}
}

/**
 * @brief TextArea::cutClipboard
 * @param flags
 */
void TextArea::cutClipboard(EventFlags flags) {

	EMIT_EVENT_0("cut_clipboard");
	TextCutClipboard();
}

/**
 * @brief TextArea::toggleOverstrike
 * @param flags
 */
void TextArea::toggleOverstrike(EventFlags flags) {

	EMIT_EVENT_0("toggle_overstrike");
	setOverstrike(!overstrike_);
}

/**
 * @brief TextArea::endOfLine
 * @param flags
 */
void TextArea::endOfLine(EventFlags flags) {

	EMIT_EVENT_0("end_of_line");

	const TextCursor insertPos = cursorPos_;

	cancelDrag();

	if (flags & AbsoluteFlag) {
		setInsertPosition(buffer_->BufEndOfLine(insertPos));
	} else {
		setInsertPosition(endOfLine(insertPos, /*startPosIsLineStart=*/false));
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
	cursorPreferredCol_ = -1;
}

/**
 * @brief TextArea::deleteNextCharacter
 * @param flags
 */
void TextArea::deleteNextCharacter(EventFlags flags) {

	EMIT_EVENT_0("delete_next_character");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (deletePendingSelection()) {
		return;
	}

	if (insertPos == buffer_->BufEndOfBuffer()) {
		ringIfNecessary(silent);
		return;
	}

	buffer_->BufRemove(insertPos, insertPos + 1);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::copyClipboard
 * @param flags
 */
void TextArea::copyClipboard(EventFlags flags) {

	EMIT_EVENT_0("copy_clipboard");

	cancelDrag();
	if (!buffer_->primary.hasSelection()) {
		QApplication::beep();
		return;
	}

	CopyToClipboard();
}

/**
 * @brief TextArea::deletePreviousWord
 * @param flags
 */
void TextArea::deletePreviousWord(EventFlags flags) {

	EMIT_EVENT_0("delete_previous_word");

	const TextCursor insertPos = cursorPos_;
	const TextCursor lineStart = buffer_->BufStartOfLine(insertPos);

	const bool silent = flags & NoBellFlag;

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

	TextCursor pos = std::max(insertPos - 1, buffer_->BufStartOfBuffer());

	while (isDelimeter(buffer_->BufGetCharacter(pos)) && pos != lineStart) {
		--pos;
	}

	pos = startOfWord(pos);
	buffer_->BufRemove(pos, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::beginningOfLine
 * @param flags
 */
void TextArea::beginningOfLine(EventFlags flags) {

	EMIT_EVENT_0("beginning_of_line");

	const TextCursor insertPos = cursorPos_;

	cancelDrag();

	if (flags & AbsoluteFlag) {
		setInsertPosition(buffer_->BufStartOfLine(insertPos));
	} else {

		TextCursor lineStart = startOfLine(insertPos);

		if (smartHome_) {
			// if the user presses home, go to the first non-whitespace character
			// if they are already there, go to the actual beginning of the line
			if (boost::optional<TextCursor> p = spanForward(buffer_, lineStart, " \t", false)) {
				if (p != insertPos) {
					lineStart = *p;
				}
			}
		}
		setInsertPosition(lineStart);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
	cursorPreferredCol_ = 0;
}

/**
 * @brief TextArea::processCancel
 * @param flags
 */
void TextArea::processCancel(EventFlags flags) {

	EMIT_EVENT_0("process_cancel");

	const DragStates dragState = dragState_;

	// If there's a calltip displayed, kill it.
	TextDKillCalltip(0);

	if (dragState == PRIMARY_DRAG || dragState == PRIMARY_RECT_DRAG) {
		buffer_->BufUnselect();
	}

	cancelDrag();
}

/**
 * @brief TextArea::deletePreviousCharacter
 * @param flags
 */
void TextArea::deletePreviousCharacter(EventFlags flags) {

	EMIT_EVENT_0("delete_previous_character");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (deletePendingSelection()) {
		return;
	}

	if (insertPos == buffer_->BufStartOfBuffer()) {
		ringIfNecessary(silent);
		return;
	}

	if (deleteEmulatedTab()) {
		return;
	}

	if (overstrike_) {
		const char ch = buffer_->BufGetCharacter(insertPos - 1);
		if (ch == '\n') {
			buffer_->BufRemove(insertPos - 1, insertPos);
		} else if (ch != '\t') {
			buffer_->BufReplace(insertPos - 1, insertPos, " ");
		}
	} else {
		buffer_->BufRemove(insertPos - 1, insertPos);
	}

	setInsertPosition(insertPos - 1);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::newline
 * @param flags
 */
void TextArea::newline(EventFlags flags) {

	EMIT_EVENT_0("newline");

	if (autoIndent_ || smartIndent_) {
		newlineAndIndentAP(flags | SuppressRecording);
	} else {
		newlineNoIndentAP(flags | SuppressRecording);
	}
}

/**
 * @brief TextArea::processUp
 * @param flags
 */
void TextArea::processUp(EventFlags flags) {

	EMIT_EVENT_0("process_up");

	const bool silent          = flags & NoBellFlag;
	const bool absolute        = flags & AbsoluteFlag;
	const TextCursor insertPos = cursorPos_;

	cancelDrag();
	if (!moveUp(absolute)) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::processDown
 * @param flags
 */
void TextArea::processDown(EventFlags flags) {

	EMIT_EVENT_0("process_down");

	const bool silent          = flags & NoBellFlag;
	const bool absolute        = flags & AbsoluteFlag;
	const TextCursor insertPos = cursorPos_;

	cancelDrag();
	if (!moveDown(absolute)) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::forwardCharacter
 * @param flags
 */
void TextArea::forwardCharacter(EventFlags flags) {

	EMIT_EVENT_0("forward_character");

	const bool silent          = flags & NoBellFlag;
	const TextCursor insertPos = cursorPos_;

	cancelDrag();
	if (!moveRight()) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::backwardCharacter
 * @param flags
 */
void TextArea::backwardCharacter(EventFlags flags) {

	EMIT_EVENT_0("backward_character");

	const bool silent          = flags & NoBellFlag;
	const TextCursor insertPos = cursorPos_;

	cancelDrag();
	if (!moveLeft()) {
		ringIfNecessary(silent);
	}

	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/**
 * @brief TextArea::~TextArea
 */
TextArea::~TextArea() {
	if (buffer_) {
		buffer_->BufRemoveModifyCB(bufModifiedCB, this);
		buffer_->BufRemovePreDeleteCB(bufPreDeleteCB, this);
	}
}

/**
 * @brief TextArea::verticalScrollBar_valueChanged
 * @param value
 */
void TextArea::verticalScrollBar_valueChanged(int value) {

	setUserInteractionDetected();

	// Limit the requested scroll position to allowable values
	if (continuousWrap_) {
		if ((value > topLineNum_) && (value > (nBufferLines_ + 2 + cursorVPadding_ - nVisibleLines_))) {
			value = std::max(topLineNum_, nBufferLines_ + 2 + cursorVPadding_ - nVisibleLines_);
		}
	}

	const int lineDelta = topLineNum_ - value;

	/* If the vertical scroll position has changed, update the line
	   starts array and related counters in the text display */
	offsetLineStarts(value);

	/* Update the scroll bar ranges, note: updating the horizontal scroll bars
	 * can have the further side-effect of changing the horizontal scroll
	 * position */
	updateVScrollBarRange();
	updateHScrollBarRange();

	// NOTE(eteran): the original code seemed to do some cleverness
	//               involving copying the parts that were "moved"
	//               to avoid doing some work. For now, we'll just repaint
	//               the whole thing. It's not as fast/clever, but it'll work
	viewport()->update();

	// Refresh line number/calltip display if its up and we've scrolled vertically
	if (lineDelta != 0) {
		repaintLineNumbers();
		updateCalltip(0);
	}
}

/**
 * @brief TextArea::horizontalScrollBar_valueChanged
 * @param value
 */
void TextArea::horizontalScrollBar_valueChanged(int value) {
	Q_UNUSED(value)

	setUserInteractionDetected();

	// NOTE(eteran): the original code seemed to do some cleverness
	//               involving copying the parts that were "moved"
	//               to avoid doing some work. For now, we'll just repaint
	//               the whole thing. It's not as fast/clever, but it'll work
	viewport()->update();
}

/**
 * @brief TextArea::cursorBlinkTimerTimeout
 */
void TextArea::cursorBlinkTimerTimeout() {

	// Blink the cursor
	if (cursorOn_) {
		TextDBlankCursor();
	} else {
		unblankCursor();
	}
}

/**
 * @brief TextArea::autoScrollTimerTimeout
 */
void TextArea::autoScrollTimerTimeout() {

	const QRect viewRect    = viewport()->contentsRect();
	const int fontWidth     = fixedFontWidth_;
	const int fontHeight    = fixedFontHeight_;
	const QPoint mouseCoord = mouseCoord_;

	/* For vertical auto-scrolling just dragging the mouse outside of the top
	 * or bottom of the window is sufficient, for horizontal (non-rectangular)
	 * scrolling, see if the position where the CURSOR would go is outside */
	const TextCursor newPos = coordToPosition(mouseCoord);

	int cursorX;
	int cursorY;
	if (dragState_ == PRIMARY_RECT_DRAG) {
		cursorX = mouseCoord.x();
	} else if (!positionToXY(newPos, &cursorX, &cursorY)) {
		cursorX = mouseCoord.x();
	}

	/* Scroll away from the pointer, 1 character (horizontal), or 1 character
	 * for each fontHeight distance from the mouse to the text (vertical) */
	int topLineNum  = verticalScrollBar()->value();
	int horizOffset = horizontalScrollBar()->value();

	if (cursorX >= viewRect.right()) {
		horizOffset += fontWidth;
	} else if (mouseCoord.x() < viewRect.left()) {
		horizOffset -= fontWidth;
	}

	if (mouseCoord.y() >= viewRect.bottom()) {
		topLineNum += 1 + ((mouseCoord.y() - viewRect.bottom()) / fontHeight) + 1;
	} else if (mouseCoord.y() < viewRect.top()) {
		topLineNum -= 1 + ((viewRect.top() - mouseCoord.y()) / fontHeight);
	}

	verticalScrollBar()->setValue(topLineNum);
	horizontalScrollBar()->setValue(horizOffset);

	/* Continue the drag operation in progress.  If none is in progress
	 * (safety check) don't continue to re-establish the timer proc */
	switch (dragState_) {
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
		blockDragSelection(mouseCoord, USE_LAST);
		break;
	default:
		autoScrollTimer_->stop();
		return;
	}

	// re-establish the timer proc (this routine) to continue processing
	autoScrollTimer_->start(mouseCoord.y() >= viewRect.top() && mouseCoord.y() < viewRect.bottom()
								? (VERTICAL_SCROLL_DELAY * fontWidth) / fontHeight
								: (VERTICAL_SCROLL_DELAY));
}

/**
 * @brief TextArea::focusInEvent
 * @param event
 */
void TextArea::focusInEvent(QFocusEvent *event) {

	// If the timer is not already started, start it
	if (!cursorBlinkTimer_->isActive()) {
		cursorBlinkTimer_->start();
	}

	// Change the cursor to active style
	if (overstrike_) {
		setCursorStyle(CursorStyles::Block);
	} else {
		setCursorStyle(CursorStyles::Normal);
	}

	unblankCursor();
	QAbstractScrollArea::focusInEvent(event);
}

/**
 * @brief TextArea::focusOutEvent
 * @param event
 */
void TextArea::focusOutEvent(QFocusEvent *event) {

	cursorBlinkTimer_->stop();

	setCursorStyle(CursorStyles::Caret);
	unblankCursor();

	// If there's a calltip displayed, kill it.
	TextDKillCalltip(0);
	QAbstractScrollArea::focusOutEvent(event);

	clearUserInteractionDetected();
}

/**
 * @brief TextArea::contextMenuEvent
 * @param event
 */
void TextArea::contextMenuEvent(QContextMenuEvent *event) {

	if (event->modifiers() != Qt::ControlModifier) {
		Q_EMIT customContextMenuRequested(mapToGlobal(event->pos()));
	}

	setUserInteractionDetected();
}

/**
 * @brief TextArea::keyPressEvent
 * @param event
 */
void TextArea::keyPressEvent(QKeyEvent *event) {

	setUserInteractionDetected();

	if (isModifier(event)) {
		return;
	}

	auto it = std::find_if(std::begin(inputHandlers), std::end(inputHandlers), [event](const InputHandler &handler) {
		return handler.key == event->key() && handler.modifiers == event->modifiers();
	});

	if (it != std::end(inputHandlers)) {
		if (it->handler) {
			(this->*(it->handler))(it->flags);
		} else {
			QApplication::beep();
		}
		return;
	}

	// NOTE(eteran): these were added because Qt handles disabled shortcuts differently from Motif
	// In Motif, they are apparently still caught, but just do nothing, in Qt, it acts like
	// they don't exist, so they get sent to widgets lower in the chain (such as this one)
	// resulting in the surprising ability to type in some funny characters

	// The following characters work with ALT in nedit : vjouyq1234567890
	// The following characters work with CTRL in nedit: b34578
	if (event->modifiers() == Qt::ControlModifier) {
		switch (event->key()) {
		case Qt::Key_W:
		case Qt::Key_Z:
		case Qt::Key_C:
		case Qt::Key_B:
		case Qt::Key_X:
		case Qt::Key_D:
		case Qt::Key_R:
		case Qt::Key_Apostrophe:
		case Qt::Key_Period:
			QApplication::beep();
			return;
		}
	}

	// See above note...
	if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
		switch (event->key()) {
		case Qt::Key_Z:
		case Qt::Key_X:
			QApplication::beep();
			return;
		}
	}
	// if the user prefers, we can hide the pointer during typing
	if (hidePointer_) {
		setCursor(Qt::BlankCursor);
	}

	QString text = event->text();
	if (text.isEmpty()) {
		return;
	}

	selfInsertAP(text); // self-insert()
}

/**
 * @brief TextArea::mouseDoubleClickEvent
 * @param event
 */
void TextArea::mouseDoubleClickEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		if (!clickTracker(event, /*inDoubleClickHandler=*/true)) {
			return;
		}

		/* Indicate state for future events, PRIMARY_CLICKED indicates that
		   the proper initialization has been done for primary dragging and/or
		   multi-clicking.  Also record the timestamp for multi-click processing */
		dragState_ = PRIMARY_CLICKED;

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
		buffer_->BufSelectAll();
	}
}

/**
 * "extend_adjust", "extend_adjust('rect')", "mouse_pan"
 *
 * @brief TextArea::mouseMoveEvent
 * @param event
 */
void TextArea::mouseMoveEvent(QMouseEvent *event) {

	if (hidePointer_) {
		unsetCursor();
	}

	if (event->buttons() == Qt::LeftButton) {
		if (event->modifiers() & Qt::ControlModifier) {
			extendAdjustAP(event, RectFlag);
		} else {
			extendAdjustAP(event);
		}
	} else if (event->buttons() == Qt::RightButton) {
		mousePanAP(event);
	} else if (event->buttons() == Qt::MiddleButton) {

		if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
			secondaryOrDragAdjustAP(event, RectFlag | OverlayFlag | CopyFlag);
		} else if (event->modifiers() == (Qt::ShiftModifier)) {
			secondaryOrDragAdjustAP(event, CopyFlag);
		} else if (event->modifiers() == (Qt::ControlModifier)) {
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

	setUserInteractionDetected();

	if (event->button() == Qt::LeftButton) {
		switch (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
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
		if (!clickTracker(event, /*inDoubleClickHandler=*/false)) {
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

		int row;
		int column;
		coordToUnconstrainedPosition(event->pos(), &row, &column);

		column      = offsetWrappedColumn(row, column);
		rectAnchor_ = column;

	} else if (event->button() == Qt::RightButton) {
		if (event->modifiers() == Qt::ControlModifier) {
			mousePanAP(event);
		}
	} else if (event->button() == Qt::MiddleButton) {
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

	switch (event->button()) {
	case Qt::LeftButton:
		endDrag();
		break;
	case Qt::RightButton:
		endDrag();
		break;
	case Qt::MiddleButton:
		switch (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
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
		break;
	default:
		break;
	}
}

/**
 * @brief TextArea::paintEvent
 * @param event
 */
void TextArea::paintEvent(QPaintEvent *event) {

	const QRect viewRect = viewport()->contentsRect();
	const QRect rect     = event->rect();
	const int top        = rect.top();
	const int left       = rect.left();
	const int width      = rect.width();
	const int height     = rect.height();

	// find the line number range of the display
	const int firstLine = (top - viewRect.top() - fixedFontHeight_ + 1) / fixedFontHeight_;
	const int lastLine  = (top + height - viewRect.top()) / fixedFontHeight_;

	QPainter painter(viewport());
	{
		painter.save();
		painter.setClipRect(viewRect);

		// draw the lines of text
		for (int line = firstLine; line <= lastLine; line++) {
			redisplayLine(&painter, line, left, left + width);
		}

		painter.restore();
	}
}

/**
 * @brief TextArea::resizeEvent
 * @param event
 */
void TextArea::resizeEvent(QResizeEvent *event) {

	Q_UNUSED(event)

	// update the size sensitive variables
	handleResize(/*widthChanged=*/true);

	showResizeNotification();

	const QRect cr = contentsRect();
	lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

/**
 * @brief TextArea::bufPreDeleteCallback
 * @param pos
 * @param nDeleted
 */
void TextArea::bufPreDeleteCallback(TextCursor pos, int64_t nDeleted) {
	if (continuousWrap_ && modifyingTabDist_) {
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
void TextArea::bufModifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText) {

	const QRect viewRect = viewport()->contentsRect();
	int64_t linesInserted;
	int64_t linesDeleted;
	TextCursor endDispPos;
	const TextCursor oldFirstChar  = firstChar_;
	const TextCursor origCursorPos = cursorPos_;
	TextCursor wrapModStart        = {};
	TextCursor wrapModEnd          = {};
	bool scrolled;

	// buffer modification cancels vertical cursor motion column
	if (nInserted != 0 || nDeleted != 0) {
		cursorPreferredCol_ = -1;
	}

	/* Count the number of lines inserted and deleted, and in the case
	   of continuous wrap mode, how much has changed */
	if (continuousWrap_) {
		findWrapRange(deletedText, pos, nInserted, nDeleted, &wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);
	} else {
		linesInserted = (nInserted == 0) ? 0 : buffer_->BufCountLines(pos, pos + nInserted);
		linesDeleted  = (nDeleted == 0) ? 0 : countNewlines(deletedText);
	}

	// Update the line starts and topLineNum
	if (nInserted != 0 || nDeleted != 0) {
		if (continuousWrap_) {
			scrolled = updateLineStarts(wrapModStart, wrapModEnd - wrapModStart, nDeleted + pos - wrapModStart + (wrapModEnd - (pos + nInserted)), linesInserted, linesDeleted);
		} else {
			scrolled = updateLineStarts(pos, nInserted, nDeleted, linesInserted, linesDeleted);
		}
	} else {
		scrolled = false;
	}

	/* If we're counting non-wrapped lines as well, maintain the absolute
	   (non-wrapped) line number of the text displayed */
	if (maintainingAbsTopLineNum() && (nInserted != 0 || nDeleted != 0)) {
		if (pos + nDeleted < oldFirstChar) {
			absTopLineNum_ = absTopLineNum_ + buffer_->BufCountLines(pos, pos + nInserted) - countNewlines(deletedText);
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
		cursorPos_    = cursorToHint_;
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
		redisplayRect(viewRect);
		if (styleBuffer_) { // See comments in extendRangeForStyleMods
			styleBuffer_->primary.selected_  = false;
			styleBuffer_->primary.zeroWidth_ = false;
		}
		return;
	}

	/* If the changes didn't cause scrolling, decide the range of characters
	   that need to be re-painted.  Also if the cursor position moved, be
	   sure that the redisplay range covers the old cursor position so the
	   old cursor gets erased, and erase the bits of the cursor which extend
	   beyond the left and right edges of the text. */
	TextCursor startDispPos = continuousWrap_ ? wrapModStart : pos;
	if (origCursorPos == startDispPos && cursorPos_ != startDispPos) {
		startDispPos = std::min(startDispPos, origCursorPos - 1);
	}

	if (linesInserted == linesDeleted) {
		if (nInserted == 0 && nDeleted == 0) {
			endDispPos = pos + nRestyled;
		} else {
			endDispPos = continuousWrap_ ? wrapModEnd : buffer_->BufEndOfLine(pos + nInserted) + 1;
		}
		/* If more than one line is inserted/deleted, a line break may have
		   been inserted or removed in between, and the line numbers may
		   have changed. If only one line is altered, line numbers cannot
		   be affected (the insertion or removal of a line break always
		   results in at least two lines being redrawn). */
		if (linesInserted > 1) {
			repaintLineNumbers();
		}
	} else { // linesInserted != linesDeleted
		endDispPos = lastChar_ + 1;
		repaintLineNumbers();
	}

	/* If there is a style buffer, check if the modification caused additional
	   changes that need to be redisplayed.  (Re-displaying separately would
	   cause double-redraw on almost every modification involving styled
	   text).  Extend the redraw range to incorporate style changes */
	if (styleBuffer_) {
		extendRangeForStyleMods(&startDispPos, &endDispPos);
	}

	// Redisplay computed range
	redisplayRange(startDispPos, endDispPos);
}

/**
 * @brief TextArea::setBacklightCharTypes
 * @param charTypes
 */
void TextArea::setBacklightCharTypes(const QString &charTypes) {
	setupBGClasses(charTypes);
	viewport()->update();
}

/**
 * @brief TextArea::hideOrShowHScrollBar
 */
void TextArea::hideOrShowHScrollBar() {
	const QRect viewRect = viewport()->contentsRect();
	if (continuousWrap_ && (wrapMargin_ == 0 || (wrapMargin_ * fixedFontWidth_) < viewRect.width())) {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	} else {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	}
}

/**
 * This is a stripped-down version of the findWrapRange() function,
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
void TextArea::measureDeletedLines(TextCursor pos, int64_t nDeleted) {

	TextCursor countFrom;
	TextCursor retLineEnd;
	TextCursor retLineStart;
	TextCursor retPos;
	const int nVisLines = nVisibleLines_;
	int nLines          = 0;
	int retLines;

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
	TextCursor lineStart = countFrom;
	while (true) {
		/* advance to the next line.  If the line ended in a real newline
		   or the end of the buffer, that's far enough */
		wrappedLineCounter(buffer_, lineStart, buffer_->BufEndOfBuffer(), 1, true, &retPos, &retLines, &retLineStart, &retLineEnd);
		if (retPos >= buffer_->length()) {
			if (retPos != retLineEnd) {
				++nLines;
			}
			break;
		} else {
			lineStart = retPos;
		}

		++nLines;
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

	nLinesDeleted_  = nLines;
	suppressResync_ = true;
}

/*
** Count forward from startPos to either maxPos or maxLines (whichever is
** reached first), and return all relevant positions and line count.
** The provided TextBuffer may differ from the actual text buffer of the
** widget. In that case it must be a (partial) copy of the actual text buffer.
**
** Returned values:
**
**   retPos:        Position where counting ended.  When counting lines, the
**                  position returned is the start of the line "maxLines"
**                  lines beyond "startPos".
**   retLines:      Number of line breaks counted
**   retLineStart:  Start of the line where counting ended
**   retLineEnd:    End position of the last line traversed
*/
void TextArea::wrappedLineCounter(const TextBuffer *buf, TextCursor startPos, TextCursor maxPos, int maxLines, bool startPosIsLineStart, TextCursor *retPos, int *retLines, TextCursor *retLineStart, TextCursor *retLineEnd) const {

	const QRect viewRect = viewport()->contentsRect();
	TextCursor lineStart;
	TextCursor newLineStart = {};
	TextCursor b;
	TextCursor i;
	bool countPixels;
	bool foundBreak;
	int wrapMargin;
	int maxWidth;
	int nLines        = 0;
	const int tabDist = buffer_->BufGetTabDistance();

	/* If there's a wrap margin set, it's more efficient to measure in columns,
	 * than to count pixels.  Determine if we can count in columns
	 * (countPixels == false) or must count pixels (countPixels == true), and
	 * set the wrap target for either pixels or columns */
	if (wrapMargin_ != 0) {
		countPixels = false;
		wrapMargin  = wrapMargin_;
		maxWidth    = INT_MAX;
	} else {
		countPixels = true;
		wrapMargin  = INT_MAX;
		maxWidth    = viewRect.width();
	}

	/* Find the start of the line if the start pos is not marked as a
	   line start. */
	if (startPosIsLineStart) {
		lineStart = startPos;
	} else {
		lineStart = startOfLine(startPos);
	}

	/*
	** Loop until position exceeds maxPos or line count exceeds maxLines.
	** (actually, continues beyond maxPos to end of line containing maxPos,
	** in case later characters cause a word wrap back before maxPos)
	*/
	int colNum = 0;
	int width  = 0;
	for (TextCursor p = lineStart; p < buf->BufEndOfBuffer(); ++p) {
		const char ch = buf->BufGetCharacter(p);

		/* If the character was a newline, count the line and start over,
		   otherwise, add it to the width and column counts */
		if (ch == '\n') {
			if (p >= maxPos) {
				*retPos       = maxPos;
				*retLines     = nLines;
				*retLineStart = lineStart;
				*retLineEnd   = maxPos;
				return;
			}

			++nLines;

			if (nLines >= maxLines) {
				*retPos       = p + 1;
				*retLines     = nLines;
				*retLineStart = p + 1;
				*retLineEnd   = p;
				return;
			}

			lineStart = p + 1;
			colNum    = 0;
			width     = 0;
		} else {
			colNum += TextBuffer::BufCharWidth(ch, colNum, tabDist);
			if (countPixels) {
				width += widthInPixels(ch, colNum);
			}
		}

		/* If character exceeded wrap margin, find the break point
		   and wrap there */
		if (colNum > wrapMargin || width > maxWidth) {
			foundBreak = false;
			for (b = p; b >= lineStart; --b) {
				const char ch = buf->BufGetCharacter(b);
				if (ch == '\t' || ch == ' ') {
					newLineStart = b + 1;
					if (countPixels) {
						colNum = 0;
						width  = 0;
						for (i = b + 1; i < p + 1; ++i) {
							width += widthInPixels(buf->BufGetCharacter(i), colNum);
							++colNum;
						}
					} else {
						colNum = buf->BufCountDispChars(b + 1, p + 1);
					}
					foundBreak = true;
					break;
				}
			}

			if (!foundBreak) { // no whitespace, just break at margin
				newLineStart = std::max(p, lineStart + 1);
				colNum       = TextBuffer::BufCharWidth(ch, colNum, tabDist);
				if (countPixels) {
					width = widthInPixels(ch, colNum);
				}
			}

			if (p >= maxPos) {
				*retPos       = maxPos;
				*retLines     = maxPos < newLineStart ? nLines : nLines + 1;
				*retLineStart = maxPos < newLineStart ? lineStart : newLineStart;
				*retLineEnd   = maxPos;
				return;
			}

			++nLines;

			if (nLines >= maxLines) {
				*retPos       = foundBreak ? b + 1 : std::max(p, lineStart + 1);
				*retLines     = nLines;
				*retLineStart = lineStart;
				*retLineEnd   = foundBreak ? b : p;
				return;
			}

			lineStart = newLineStart;
		}
	}

	// reached end of buffer before reaching pos or line target
	*retPos       = buf->BufEndOfBuffer();
	*retLines     = nLines;
	*retLineStart = lineStart;
	*retLineEnd   = buf->BufEndOfBuffer();
}

/*
** Measure the width in pixels of a character "ch" at a particular column
** "colNum".
*/
int TextArea::widthInPixels(char ch, int column) const {
	return lengthToWidth(TextBuffer::BufCharWidth(ch, column, buffer_->BufGetTabDistance()));
}

/*
** Find the width of a string in the font
*/
int TextArea::lengthToWidth(int length) const noexcept {
	return fixedFontWidth_ * length;
}

/*
** Same as BufStartOfLine, but returns the character after last wrap point
** rather than the last newline.
*/
TextCursor TextArea::startOfLine(TextCursor pos) const {
	int retLines;
	TextCursor retPos;
	TextCursor retLineStart;
	TextCursor retLineEnd;

	// If we're not wrapping, use the more efficient BufStartOfLine
	if (!continuousWrap_) {
		return buffer_->BufStartOfLine(pos);
	}

	wrappedLineCounter(
		buffer_,
		buffer_->BufStartOfLine(pos),
		pos,
		INT_MAX,
		true,
		&retPos,
		&retLines,
		&retLineStart,
		&retLineEnd);

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
 * @brief TextArea::findWrapRange
 * @param deletedText
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param modRangeStart
 * @param modRangeEnd
 * @param linesInserted
 * @param linesDeleted
 */
void TextArea::findWrapRange(view::string_view deletedText, TextCursor pos, int64_t nInserted, int64_t nDeleted, TextCursor *modRangeStart, TextCursor *modRangeEnd, int64_t *linesInserted, int64_t *linesDeleted) {

	TextCursor retPos;
	TextCursor retLineStart;
	TextCursor retLineEnd;
	TextCursor countFrom;
	TextCursor countTo;
	TextCursor adjLineStart;
	int retLines;
	int visLineNum      = 0;
	int nLines          = 0;
	const int nVisLines = nVisibleLines_;

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
			countFrom  = lineStarts_[i - 1];
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
	TextCursor lineStart = countFrom;
	*modRangeStart       = countFrom;
	while (true) {

		/* advance to the next line.  If the line ended in a real newline
		   or the end of the buffer, that's far enough */
		wrappedLineCounter(buffer_, lineStart, buffer_->BufEndOfBuffer(), 1, true, &retPos, &retLines, &retLineStart, &retLineEnd);

		if (retPos >= buffer_->length()) {
			countTo      = buffer_->BufEndOfBuffer();
			*modRangeEnd = countTo;
			if (retPos != retLineEnd) {
				++nLines;
			}
			break;
		} else {
			lineStart = retPos;
		}

		++nLines;
		if (lineStart > pos + nInserted && buffer_->BufGetCharacter(lineStart - 1) == '\n') {
			countTo      = lineStart;
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
				nLines    = 0;
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
				countTo      = endOfLine(lineStart, /*startPosIsLineStart=*/true);
				*modRangeEnd = lineStart;
				break;
			}
		}
	}
	*linesInserted = nLines;

	/* Count deleted lines between countFrom and countTo as the text existed
	 * before the modification (that is, as if the text between pos and
	 * pos+nInserted were replaced by "deletedText").  This extra context is
	 * necessary because wrapping can occur outside of the modified region
	 * as a result of adding or deleting text in the region. This is done by
	 * creating a TextBuffer containing the deleted text and the necessary
	 * additional context, and calling the wrappedLineCounter on it. */
	if (suppressResync_) {
		*linesDeleted   = nLinesDeleted_;
		suppressResync_ = false;
		return;
	}

	const int64_t length = (pos - countFrom) + nDeleted + (countTo - (pos + nInserted));

	TextBuffer deletedTextBuf;
	deletedTextBuf.BufSetSyncXSelection(false);

	if (pos > countFrom) {
		deletedTextBuf.BufCopyFromBuf(buffer_, countFrom, pos, buffer_->BufStartOfBuffer());
	}

	if (nDeleted != 0) {
		deletedTextBuf.BufInsert(TextCursor(pos - countFrom), deletedText);
	}

	if (countTo > pos + nInserted) {
		deletedTextBuf.BufCopyFromBuf(buffer_, pos + nInserted, countTo, TextCursor(pos - countFrom + nDeleted));
	}

	/* Note that we need to take into account an offset for the style buffer:
	 * the deletedTextBuf can be out of sync with the style buffer. */
	wrappedLineCounter(
		&deletedTextBuf,
		buffer_->BufStartOfBuffer(),
		TextCursor(length),
		INT_MAX,
		true,
		&retPos,
		&retLines,
		&retLineStart,
		&retLineEnd);

	*linesDeleted   = retLines;
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
TextCursor TextArea::endOfLine(TextCursor pos, bool startPosIsLineStart) const {

	int retLines;
	TextCursor retPos;
	TextCursor retLineStart;
	TextCursor retLineEnd;

	// If we're not wrapping use more efficient BufEndOfLine
	if (!continuousWrap_) {
		return buffer_->BufEndOfLine(pos);
	}

	if (pos == buffer_->BufEndOfBuffer()) {
		return pos;
	}

	wrappedLineCounter(buffer_, pos, buffer_->BufEndOfBuffer(), 1, startPosIsLineStart, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retLineEnd;
}

/*
** Update the line starts array, topLineNum, firstChar and lastChar after a
** modification to the text buffer, given by the position where the change
** began "pos", and the numbers of characters and lines inserted and deleted.
*/
bool TextArea::updateLineStarts(TextCursor pos, int64_t charsInserted, int64_t charsDeleted, int64_t linesInserted, int64_t linesDeleted) {

	int lineOfPos;
	int lineOfEnd;
	const int nVisLines     = nVisibleLines_;
	const int64_t charDelta = charsInserted - charsDeleted;
	const int64_t lineDelta = linesInserted - linesDeleted;

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
		return false;
	}

	/* The change began before the beginning of the displayed text, but
	   part or all of the displayed text was deleted */
	if (pos < firstChar_) {
		// If some text remains in the window, anchor on that
		if (posToVisibleLineNum(pos + charsDeleted, &lineOfEnd) && ++lineOfEnd < nVisLines && lineStarts_[lineOfEnd] != -1) {
			topLineNum_ = std::max<int>(1, topLineNum_ + lineDelta);
			firstChar_  = countBackwardNLines(lineStarts_[lineOfEnd] + charDelta, lineOfEnd);
			// Otherwise anchor on original line number and recount everything
		} else {
			if (topLineNum_ > nBufferLines_ + lineDelta) {
				topLineNum_ = 1;
				firstChar_  = buffer_->BufStartOfBuffer();
			} else
				firstChar_ = forwardNLines(buffer_->BufStartOfBuffer(), topLineNum_ - 1, true);
		}
		calcLineStarts(0, nVisLines - 1);

		// calculate lastChar by finding the end of the last displayed line
		calcLastChar();

		return true;
	}

	/* If the change was in the middle of the displayed text (it usually is),
	   salvage as much of the line starts array as possible by moving and
	   offsetting the entries after the changed area, and re-counting the
	   added lines or the lines beyond the salvaged part of the line starts
	   array */
	if (pos <= lastChar_) {
		// find line on which the change began
		if (posToVisibleLineNum(pos, &lineOfPos)) {
			// salvage line starts after the changed area
			if (lineDelta == 0) {
				for (int i = lineOfPos + 1; i < nVisLines && lineStarts_[i] != -1; i++) {
					lineStarts_[i] += charDelta;
				}
			} else if (lineDelta > 0) {
				for (int i = nVisLines - 1; i >= lineOfPos + lineDelta + 1; i--) {
					lineStarts_[i] = lineStarts_[i - lineDelta] + (lineStarts_[i - lineDelta] == -1 ? 0 : charDelta);
				}
			} else /* (lineDelta < 0) */ {
				for (int i = std::max(0, lineOfPos + 1); i < nVisLines + lineDelta; i++) {
					lineStarts_[i] = lineStarts_[i - lineDelta] + (lineStarts_[i - lineDelta] == -1 ? 0 : charDelta);
				}
			}

			// fill in the missing line starts
			if (linesInserted >= 0) {
				calcLineStarts(lineOfPos + 1, lineOfPos + linesInserted);
			}

			if (lineDelta < 0) {
				calcLineStarts(nVisLines + lineDelta, nVisLines);
			}

			// calculate lastChar by finding the end of the last displayed line
			calcLastChar();
		}
		return false;
	}

	/* Change was past the end of the displayed text, but displayable by virtue
	   of being an insert at the end of the buffer into visible blank lines */
	if (emptyLinesVisible()) {
		if (posToVisibleLineNum(pos, &lineOfPos)) {
			calcLineStarts(lineOfPos, lineOfPos + linesInserted);
			calcLastChar();
		}
		return false;
	}

	// Change was beyond the end of the buffer and not visible, do nothing
	return false;
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

	const TextCursor bufEnd = buffer_->BufEndOfBuffer();
	TextCursor lineEnd;
	TextCursor nextLineStart;
	const int nVis = nVisibleLines_;

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
		startLine      = 1;
	}

	TextCursor startPos = lineStarts_[startLine - 1];
	int line;

	/* If the starting position is already past the end of the text,
	 * fill in -1's (means no text on line) and return */
	if (startPos == -1) {
		for (line = startLine; line <= endLine; line++) {
			lineStarts_[line] = TextCursor(-1);
		}

		return;
	}

	/* Loop searching for ends of lines and storing the positions of the
	 * start of the next line in lineStarts */
	for (line = startLine; line <= endLine; line++) {
		findLineEnd(startPos, true, &lineEnd, &nextLineStart);
		startPos = nextLineStart;
		if (startPos >= bufEnd) {
			/* If the buffer ends with a newline or line break, put
			   buf->BufGetLength() in the next line start position (instead of
			   a -1 which is the normal marker for an empty line) to
			   indicate that the cursor may safely be displayed there */
			if (line == 0 || (lineStarts_[line - 1] != bufEnd && lineEnd != nextLineStart)) {
				lineStarts_[line] = bufEnd;
				++line;
			}
			break;
		}
		lineStarts_[line] = startPos;
	}

	// Set any entries beyond the end of the text to invalid
	for (; line <= endLine; line++) {
		lineStarts_[line] = TextCursor(-1);
	}
}

/*
** Given a complete, up-to-date lineStarts array, update the lastChar entry to
** point to the last buffer position displayed.
*/
void TextArea::calcLastChar() {
	int i;

	for (i = nVisibleLines_ - 1; i > 0 && lineStarts_[i] == -1; i--) {
	}

	lastChar_ = (i < 0) ? buffer_->BufStartOfBuffer() : endOfLine(lineStarts_[i], /*startPosIsLineStart=*/true);
}

/*
** Same as BufCountBackwardNLines, but takes in to account line breaks when
** wrapping is turned on.
*/
TextCursor TextArea::countBackwardNLines(TextCursor startPos, int nLines) const {

	// If we're not wrapping, use the more efficient BufCountBackwardNLines
	if (!continuousWrap_) {
		return buffer_->BufCountBackwardNLines(startPos, nLines);
	}

	int retLines;
	TextCursor retPos;
	TextCursor retLineStart;
	TextCursor retLineEnd;
	TextCursor pos = startPos;
	while (true) {
		const TextCursor lineStart = buffer_->BufStartOfLine(pos);
		wrappedLineCounter(buffer_, lineStart, pos, INT_MAX, true, &retPos, &retLines, &retLineStart, &retLineEnd);

		if (retLines > nLines) {
			return forwardNLines(lineStart, retLines - nLines, true);
		}

		nLines -= retLines;
		pos = lineStart - 1;

		if (pos < 0) {
			return buffer_->BufStartOfBuffer();
		}

		nLines -= 1;
	}
}

/*
** Return true if a separate absolute top line number is being maintained
** (for displaying line numbers or showing in the statistics line).
*/
bool TextArea::maintainingAbsTopLineNum() const {
	return continuousWrap_ && (lineNumCols_ != 0 || needAbsTopLineNum_);
}

/*
** Count lines from the beginning of the buffer to reestablish the
** absolute (non-wrapped) top line number.  If mode is not continuous wrap,
** or the number is not being maintained, does nothing.
*/
void TextArea::resetAbsLineNum() {
	absTopLineNum_ = 1;
	offsetAbsLineNum(buffer_->BufStartOfBuffer());
}

/*
** Refresh a rectangle of the text display.  left and top are in coordinates of
** the text drawing window
*/
void TextArea::redisplayRect(const QRect &rect) {
	viewport()->update(rect);
}

/**
 * Update the minimum, maximum, page increment, and value for
 * vertical scroll bar.
 *
 * @brief TextArea::updateVScrollBarRange
 */
void TextArea::updateVScrollBarRange() {
	/* NOTE(eteran) Originally, it seemed that some special handling was needed
	 * to handle continuous wrap mode. Upong further inspection, it looks like
	 * nBufferLines_ properly tracks the number of conceptual lines in the
	 * buffer, including those that are due to wrapping. So we can just use that
	 * value regardless */
	verticalScrollBar()->setRange(1, std::max(1, nBufferLines_ - nVisibleLines_ + 2));
	verticalScrollBar()->setPageStep(std::max(1, nVisibleLines_ - 1));
}

/**
 * Same as BufCountForwardNLines, but takes in to account line breaks when
 * wrapping is turned on. If the caller knows that startPos is at a line start,
 * it can pass "startPosIsLineStart" as true to make the call more efficient by
 * avoiding the additional step of scanning back to the last newline.
 *
 * @brief TextArea::forwardNLines
 * @param startPos
 * @param nLines
 * @param startPosIsLineStart
 * @return
 */
TextCursor TextArea::forwardNLines(TextCursor startPos, int nLines, bool startPosIsLineStart) const {

	// if we're not wrapping use more efficient BufCountForwardNLines
	if (!continuousWrap_) {
		return buffer_->BufCountForwardNLines(startPos, nLines);
	}

	// wrappedLineCounter can't handle the 0 lines case
	if (nLines == 0) {
		return startPos;
	}

	// use the common line counting routine to count forward
	TextCursor retPos;
	TextCursor retLineStart;
	TextCursor retLineEnd;
	int retLines;
	wrappedLineCounter(buffer_, startPos, buffer_->BufEndOfBuffer(), nLines, startPosIsLineStart, &retPos, &retLines, &retLineStart, &retLineEnd);
	return retPos;
}

/*
** Re-calculate absolute top line number for a change in scroll position.
*/
void TextArea::offsetAbsLineNum(TextCursor oldFirstChar) {
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
void TextArea::findLineEnd(TextCursor startPos, bool startPosIsLineStart, TextCursor *lineEnd, TextCursor *nextLineStart) {

	TextCursor end = buffer_->BufEndOfBuffer();

	// if we're not wrapping use more efficient BufEndOfLine
	if (!continuousWrap_) {
		*lineEnd       = buffer_->BufEndOfLine(startPos);
		*nextLineStart = std::min(end, *lineEnd + 1);
		return;
	}

	// use the wrapped line counter routine to count forward one line
	int retLines;
	TextCursor retLineStart;
	wrappedLineCounter(
		buffer_,
		startPos,
		end,
		1,
		startPosIsLineStart,
		nextLineStart,
		&retLines,
		&retLineStart,
		lineEnd);
}

/**
 * Update the minimum, maximum, page increment, and value for the horizontal
 * scroll bar.  If scroll position is such that there is blank space to the
 * right of all lines of text, scroll back to take up the slack and position
 * the right edge of the text at the right edge of the display.
 *
 * Note, there is some cost to this routine, since it scans the whole range of
 * displayed text, particularly since it's usually called for each typed
 * character!
 *
 * @brief TextArea::updateHScrollBarRange
 * @return
 */
bool TextArea::updateHScrollBarRange() {

	if (!horizontalScrollBar()->isVisible()) {
		return false;
	}

	const QRect viewRect  = viewport()->contentsRect();
	const int origHOffset = horizontalScrollBar()->value();

	// Scan all the displayed lines to find the width of the longest line
	int maxWidth = 0;
	for (int i = 0; i < nVisibleLines_ && lineStarts_[i] != -1; i++) {
		maxWidth = std::max(measureVisLine(i), maxWidth);
	}

	horizontalScrollBar()->setRange(0, std::max(maxWidth - viewRect.width() + 1, 0));
	horizontalScrollBar()->setPageStep(std::max(viewRect.width() - 100, 10));

	// Return true if scroll position was changed
	return origHOffset != horizontalScrollBar()->value();
}

/**
 * Return true if there are lines visible with no corresponding buffer text
 *
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
bool TextArea::posToVisibleLineNum(TextCursor pos, int *lineNum) const {

	// TODO(eteran): return optional int?

	if (pos < firstChar_) {
		return false;
	}

	if (pos > lastChar_) {
		// TODO(eteran): determine if this can EVER be true, I am yet to find a circumstance where it can happen
		if (emptyLinesVisible()) {
			if (lastChar_ < buffer_->length()) {
				if (!posToVisibleLineNum(lastChar_, lineNum)) {
					qCritical("NEdit: Consistency check ptvl failed");
					return false;
				}
				return ++(*lineNum) <= nVisibleLines_ - 1;
			} else {
				return posToVisibleLineNum(std::max(lastChar_ - 1, buffer_->BufStartOfBuffer()), lineNum);
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
 * Return the width in pixels of the displayed line pointed to by "visLineNum"
 *
 * @brief TextArea::measureVisLine
 * @param visLineNum
 * @return
 */
int TextArea::measureVisLine(int visLineNum) const {

	int width                     = 0;
	int charCount                 = 0;
	const int lineLen             = visLineLength(visLineNum);
	const TextCursor lineStartPos = lineStarts_[visLineNum];

	for (int i = 0; i < lineLen; i++) {
		char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];
		const int len = buffer_->BufGetExpandedChar(lineStartPos + i, charCount, expandedChar);
		width += (fixedFontWidth_ * len);
		charCount += len;
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
int TextArea::visLineLength(int visLineNum) const {

	const TextCursor lineStartPos = lineStarts_[visLineNum];

	if (lineStartPos == -1) {
		return 0;
	}

	if (visLineNum + 1 >= nVisibleLines_) {
		return lastChar_ - lineStartPos;
	}

	const TextCursor nextLineStart = lineStarts_[visLineNum + 1];

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
bool TextArea::wrapUsesCharacter(TextCursor lineEndPos) const {

	if (!continuousWrap_ || lineEndPos == buffer_->length()) {
		return true;
	}

	const char ch = buffer_->BufGetCharacter(lineEndPos);
	return (ch == '\n') || ((ch == '\t' || ch == ' ') && lineEndPos + 1 != buffer_->length());
}

/*
** Extend the range of a redraw request (from *start to *end) with additional
** redraw requests resulting from changes to the attached style buffer (which
** contains auxiliary information for coloring or styling text).
*/
void TextArea::extendRangeForStyleMods(TextCursor *start, TextCursor *end) {
	const TextBuffer::Selection *sel = &styleBuffer_->primary;

	/* The peculiar protocol used here is that modifications to the style
	   buffer are marked by selecting them with the buffer's primary selection.
	   The style buffer is usually modified in response to a modify callback on
	   the text buffer BEFORE TextArea's modify callback, so that it can keep
	   the style buffer in step with the text buffer.  The style-update
	   callback can't just call for a redraw, because TextArea hasn't processed
	   the original text changes yet.  Anyhow, to minimize redrawing and to
	   avoid the complexity of scheduling redraws later, this simple protocol
	   tells the text display's buffer modify callback to extend it's redraw
	   range to show the text color/and font changes as well. */
	if (sel->hasSelection()) {
		if (sel->start() < *start) {
			*start = sel->start();
		}

		if (sel->end() > *end) {
			*end = sel->end();
		}
	}
}

/*
** Refresh all of the text between buffer positions "start" and "end"
** not including the character at the position "end".
** If end points beyond the end of the buffer, refresh the whole display
** after pos, including blank lines which are not technically part of
** any range of characters.
*/
void TextArea::redisplayRange(TextCursor start, TextCursor end) {

	// If the range is outside of the displayed text, just return
	if (end < firstChar_ || (start > lastChar_ && !emptyLinesVisible())) {
		return;
	}

	// Clean up the starting and ending values
	const TextCursor start_of_buffer = buffer_->BufStartOfBuffer();
	const TextCursor end_of_buffer   = buffer_->BufEndOfBuffer();

	start = qBound(start_of_buffer, start, end_of_buffer);
	end   = qBound(start_of_buffer, end, end_of_buffer);

	// Get the starting and ending lines
	if (start < firstChar_) {
		start = firstChar_;
	}

	int startLine;
	int lastLine;

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
	const int startIndex = (lineStarts_[startLine] == -1) ? 0 : start - lineStarts_[startLine];
	int endIndex;
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

	/* If the starting and ending lines are the same, redisplay the single
	   line between "start" and "end" */
	if (startLine == lastLine) {
		redisplayLine(startLine, startIndex, endIndex);
		return;
	}

	// Redisplay the first line from "start"
	redisplayLine(startLine, startIndex, INT_MAX);

	// Redisplay the lines in between at their full width
	for (int i = startLine + 1; i < lastLine; i++) {
		redisplayLine(i, 0, INT_MAX);
	}

	// Redisplay the last line to "end"
	redisplayLine(lastLine, 0, endIndex);
}

/**
 * @brief TextArea::repaintLineNumbers
 */
void TextArea::repaintLineNumbers() {
	lineNumberArea_->update();
}

/*
 * A replacement for redisplayLine. Instead of directly painting, it will
 * calculate the rect that would be repainted and trigger an update of that
 * region. We will then repaint that region during the paint event
 */
void TextArea::redisplayLine(int visLineNum, int leftCharIndex, int rightCharIndex) {

	// NOTE(eteran): the original code would only update **exactly** what was
	// needed. I haven't been able to get this quite right in the Qt port.
	// So we update the whole line (the visible portion).
	Q_UNUSED(leftCharIndex)
	Q_UNUSED(rightCharIndex)

	const QRect viewRect = viewport()->contentsRect();

	// If line is not displayed, skip it
	if (visLineNum < 0 || visLineNum >= nVisibleLines_) {
		return;
	}

	// Calculate y coordinate of the string to draw
	const int y = viewRect.top() + visLineNum * fixedFontHeight_;

	viewport()->update(QRect(viewRect.left(), y, viewRect.width(), fixedFontHeight_));
}

/*
** Redisplay the text on a single line represented by "visLineNum" (the
** number of lines down from the top of the display), limited by
** "leftClip" and "rightClip" window coordinates.
**
** The cursor is also drawn if it appears on the line.
*/
void TextArea::redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip) {

	/* Space beyond the end of the line is still counted in units of characters
	 * of a standardized character width (this is done mostly because style
	 * changes based on character position can still occur in this region due
	 * to rectangular selections).  fixedFontWidth_ must be non-zero to prevent
	 * a potential infinite loop if x does not advance */
	if (fixedFontWidth_ <= 0) {
		qWarning("NEdit: Internal Error, bad font measurement");
		return;
	}

	const QRect viewRect = viewport()->contentsRect();

	// If line is not displayed, skip it
	if (visLineNum < 0 || visLineNum >= nVisibleLines_) {
		return;
	}

	// Shrink the clipping range to the active display area
	leftClip  = std::max(viewRect.left(), leftClip);
	rightClip = std::min(rightClip, viewRect.right());

	if (leftClip > rightClip) {
		return;
	}

	// Remember where the cursor was drawn
	const int y_orig = cursor_.y();

	// Calculate y coordinate of the string to draw
	const int y = viewRect.top() + visLineNum * fixedFontHeight_;

	// get buffer position of the line to display
	const TextCursor lineStartPos = lineStarts_[visLineNum];

	// get a copy of the current line (or an empty string)
	const std::string currentLine = [&]() {
		std::string ret;
		if (lineStartPos != -1) {
			const int length = visLineLength(visLineNum);
			ret              = buffer_->BufGetRange(lineStartPos, lineStartPos + length);
		}
		return ret;
	}();

	const size_t lineSize = currentLine.size();

	/* Rectangular selections are based on "real" line starts (after a newline
	   or start of buffer).  Calculate the difference between the last newline
	   position and the line start we're using.  Since scanning back to find a
	   newline is expensive, only do so if there's actually a rectangular
	   selection which needs it */
	int64_t dispIndexOffset = 0;

	// a helper lambda for this calculation
	auto rangeTouchesRectSel = [this](TextCursor rangeStart, TextCursor rangeEnd) {
		return buffer_->primary.rangeTouchesRectSel(rangeStart, rangeEnd) ||
			   buffer_->secondary.rangeTouchesRectSel(rangeStart, rangeEnd) ||
			   buffer_->highlight.rangeTouchesRectSel(rangeStart, rangeEnd);
	};

	if (continuousWrap_ && rangeTouchesRectSel(lineStartPos, lineStartPos + currentLine.size())) {
		dispIndexOffset = buffer_->BufCountDispChars(buffer_->BufStartOfLine(lineStartPos), lineStartPos);
	}

	/* Step through character positions from the beginning of the line (even if
	 * that's off the left edge of the displayed area) to find the first
	 * character position that's not clipped, and the x coordinate for drawing
	 * that character */
	const int tabDist = buffer_->BufGetTabDistance();
	int startX        = viewRect.left() - horizontalScrollBar()->value();
	int outIndex      = 0;
	size_t startIndex = 0;
	uint32_t style    = 0;

	for (;;) {
		int charLen   = 1;
		char baseChar = '\0';
		if (startIndex < lineSize) {
			baseChar = currentLine[startIndex];
			charLen  = TextBuffer::BufCharWidth(baseChar, outIndex, tabDist);
		}

		style               = styleOfPos(lineStartPos, lineSize, startIndex, dispIndexOffset + outIndex, baseChar);
		const int charWidth = (startIndex >= lineSize) ? fixedFontWidth_ : lengthToWidth(charLen);

		if (startX + charWidth >= leftClip) {
			break;
		}

		startX += charWidth;
		outIndex += charLen;
		++startIndex;
	}

	/* Scan character positions from the beginning of the clipping range, and
	 * draw parts whenever the style changes (also note if the cursor is on
	 * this line, and where it should be drawn to take advantage of the x
	 * position which we've gone to so much trouble to calculate) */
	char buffer[MAX_DISP_LINE_LEN];
	char *outPtr = buffer;
	int x        = startX;
	size_t charIndex;
	boost::optional<int> cursorX;

	for (charIndex = startIndex;; ++charIndex) {

		// take note of where the cursor is if it's on this line...
		if (lineStartPos + charIndex == cursorPos_) {
			if (charIndex < lineSize || (charIndex == lineSize && cursorPos_ >= buffer_->BufEndOfBuffer())) {
				cursorX = x - 1;
			} else if ((charIndex == lineSize) && wrapUsesCharacter(cursorPos_)) {
				cursorX = x - 1;
			}
		}

		char expandedChar[TextBuffer::MAX_EXP_CHAR_LEN];
		char baseChar = '\0';
		int charLen   = 1;

		if (charIndex < lineSize) {
			baseChar = currentLine[charIndex];
			charLen  = TextBuffer::BufExpandCharacter(baseChar, outIndex, expandedChar, tabDist);
		}

		uint32_t charStyle = styleOfPos(lineStartPos, lineSize, charIndex, dispIndexOffset + outIndex, baseChar);

		for (int i = 0; i < charLen; ++i) {

			/* NOTE(eteran): this double check of the style is necessary to make
			 * certain types of selections work correctly
			 */
			if (i != 0 && charIndex < lineSize && currentLine[charIndex] == '\t') {
				charStyle = styleOfPos(lineStartPos, lineSize, charIndex, dispIndexOffset + outIndex, '\t');
			}

			if (charStyle != style) {

				drawString(painter, style, startX, y, x, view::string_view(buffer, outPtr - buffer));

				outPtr = buffer;
				startX = x;
				style  = charStyle;
			}

			if (charIndex < lineSize) {
				*outPtr = expandedChar[i];
			}

			++outPtr;
			++outIndex;
			x += fixedFontWidth_;
		}

		if (outPtr - buffer + TextBuffer::MAX_EXP_CHAR_LEN >= MAX_DISP_LINE_LEN || x >= rightClip) {
			break;
		}
	}

	// Draw the remaining style segment
	drawString(painter, style, startX, y, x, view::string_view(buffer, outPtr - buffer));

	/* Draw the cursor if part of it appeared on the redisplayed part of
	   this line.  Also check for the cases which are not caught as the
	   line is scanned above: when the cursor appears at the very end
	   of the redisplayed section. */
	if (cursorOn_) {
		if (cursorX) {
			drawCursor(painter, *cursorX, y);
		} else if (charIndex < lineSize && (lineStartPos + charIndex + 1 == cursorPos_) && x == rightClip) {
			if (cursorPos_ >= buffer_->length()) {
				drawCursor(painter, x - 1, y);
			} else if (wrapUsesCharacter(cursorPos_)) {
				drawCursor(painter, x - 1, y);
			}
		}
	}

	// If the y position of the cursor has changed, update the calltip location
	if (cursorX && (y_orig != cursor_.y() || y_orig != y)) {
		updateCalltip(0);
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
uint32_t TextArea::styleOfPos(TextCursor lineStartPos, size_t lineLen, size_t lineIndex, int64_t dispIndex, int thisChar) const {

	if (lineStartPos == -1 || !buffer_) {
		return FILL_MASK;
	}

	TextCursor pos = lineStartPos + std::min(lineIndex, lineLen);
	uint32_t style = 0;

	if (lineIndex >= lineLen) {
		style = FILL_MASK;
	} else if (styleBuffer_) {
		style = static_cast<uint8_t>(styleBuffer_->BufGetCharacter(pos));
		if (style == unfinishedStyle_) {
			// encountered "unfinished" style, trigger parsing
			(unfinishedHighlightCB_)(this, pos, highlightCBArg_);
			style = static_cast<uint8_t>(styleBuffer_->BufGetCharacter(pos));
		}
	}

	if (buffer_->primary.inSelection(pos, lineStartPos, dispIndex)) {
		style |= PRIMARY_MASK;
	}

	if (buffer_->highlight.inSelection(pos, lineStartPos, dispIndex)) {
		style |= HIGHLIGHT_MASK;
	}

	if (buffer_->secondary.inSelection(pos, lineStartPos, dispIndex)) {
		style |= SECONDARY_MASK;
	}

	/* store in the RANGESET_MASK portion of style the rangeset index for pos */
	if (document_->rangesetTable_) {
		size_t rangesetIndex = document_->rangesetTable_->index1ofPos(pos, true);
		style |= ((rangesetIndex << RANGESET_SHIFT) & RANGESET_MASK);
	}

	/* store in the BACKLIGHT_MASK portion of style the background color class
	   of the character thisChar */
	if (!bgClass_.empty()) {
		auto index = static_cast<size_t>(thisChar);
		if (index < bgClass_.size()) {
			style |= (bgClass_[index] << BACKLIGHT_SHIFT);
		}
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
void TextArea::drawString(QPainter *painter, uint32_t style, int x, int y, int toX, view::string_view string) {

	const QRect viewRect = viewport()->contentsRect();
	const QPalette &pal  = palette();
	QColor bground       = pal.color(QPalette::Base);
	QColor fground       = pal.color(QPalette::Text);
	QFont renderFont     = font_;
	bool underlineStyle  = false;
	bool fastPath        = true;

	enum DrawType {
		DrawStyle,
		DrawHighlight,
		DrawSelect,
		DrawPlain
	};

	const DrawType drawType = [style]() {
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
	}();

	switch (drawType) {
	case DrawHighlight:
		fground = matchFGColor_;
		bground = matchBGColor_;
		break;
	case DrawSelect:
		fground = pal.color(QPalette::HighlightedText);
		bground = pal.color(QPalette::Highlight);
		break;
	case DrawPlain:
		break;
	case DrawStyle: {
		// we have work to do
		StyleTableEntry *styleRec;
		/* Set font, color, and gc depending on style.  For normal text, GCs
			   for normal drawing, or drawing within a selection or highlight are
			   pre-allocated and pre-configured.  For syntax highlighting, GCs are
			   configured here, on the fly. */
		if (style & STYLE_LOOKUP_MASK) {
			styleRec       = &styleTable_[(style & STYLE_LOOKUP_MASK) - ASCII_A];
			underlineStyle = styleRec->isUnderlined;

			renderFont.setBold(styleRec->isBold);
			renderFont.setItalic(styleRec->isItalic);

			fastPath = !widerBold_ || (!styleRec->isBold && !styleRec->isItalic);

			fground = styleRec->color;
			// here you could pick up specific select and highlight fground
		} else {
			styleRec = nullptr;
			fground  = pal.color(QPalette::Text);
		}

		/* Background color priority order is:
		 ** 1 Primary(Selection),
		 ** 2 Highlight(Parens),
		 ** 3 Rangeset
		 ** 4 SyntaxHighlightStyle,
		 ** 5 Backlight (if NOT fill)
		 ** 6 DefaultBackground
		 */
		if (style & PRIMARY_MASK) {
			bground = pal.color(QPalette::Highlight);

			// NOTE(eteran): enabling this makes working with darker themes a lot nicer
			// basically what it does is make it so highlights are disabled inside of
			// selections. Giving the user control over the foreground color inside
			// a highlight selection.
			if (!colorizeHighlightedText_) {
				fground = pal.color(QPalette::HighlightedText);
			}

		} else if (style & HIGHLIGHT_MASK) {
			bground = matchBGColor_;
			if (!colorizeHighlightedText_) {
				fground = matchFGColor_;
			}
		} else if (style & RANGESET_MASK) {
			bground = getRangesetColor((style & RANGESET_MASK) >> RANGESET_SHIFT, bground);
		} else if (styleRec && !styleRec->bgColorName.isNull()) {
			bground = styleRec->bgColor;
		} else if ((style & BACKLIGHT_MASK) && !(style & FILL_MASK)) {
			bground = bgClassColors_[(style >> BACKLIGHT_SHIFT) & 0xff];
		} else {
			bground = pal.color(QPalette::Base);
		}

		if (fground == bground) { // B&W kludge
			fground = pal.color(QPalette::Base);
		}
	} break;
	}

	// Draw blank area rather than text, if that was the request
	if (style & FILL_MASK) {

		// wipes out to right hand edge of widget
		if (toX >= viewRect.left()) {
			const int left = std::max(x, viewRect.left());
			painter->fillRect(QRect(left, y, toX - left, fixedFontHeight_), bground);
		}

		return;
	}

	// Underline if style is secondary selection
	if ((style & SECONDARY_MASK) || underlineStyle) {
		renderFont.setUnderline(true);
	}

	const auto s = asciiToUnicode(string);
	QRect rect(x, y, toX - x, fixedFontHeight_);

	painter->save();
	painter->setFont(renderFont);
	painter->fillRect(rect, bground);

	/* NOTE(eteran): if we want to support more cursor shapes such as block
	 * cursors we need to figure out how to move at least some of the cursor
	 * rendering to here. This is because this code actually clears the
	 * background behind the rendered text */

	// TODO(eteran): Idea, use one of the free bits in the style buffer to notate
	// a location as "has cursor", this will allow us to move the rendering
	// of the cursor to this function, giving us generally a bit more flexibility.

	painter->setPen(fground);
	if (Q_LIKELY(fastPath)) {
		painter->drawText(rect, Qt::TextSingleLine | Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignLeft, s);
	} else {
		for (QChar ch : s) {
			painter->drawText(rect, Qt::TextSingleLine | Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignLeft, {ch});
			rect.adjust(fixedFontWidth_, 0, 0, 0);
		}
	}
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

	const QRect viewRect = viewport()->contentsRect();
	QPainterPath path;

	const int fontWidth  = fixedFontWidth_;
	const int fontHeight = fixedFontHeight_ - 1;

	x += 1;
	y += 1;

	const int bot = y + fontHeight - 1;

	if (x < viewRect.left() - 1 || x > viewRect.right()) {
		return;
	}

	painter->save();

	// Create segments and draw cursor
	switch (cursorStyle_) {
	case CursorStyles::Caret: {

		/* For caret cursors , make them around 2/3 of a character width,
		 * rounded to an even number of pixels so that X will draw an odd
		 * number centered on the stem at x. */

		const int midY        = bot - fontHeight / 5;
		const int cursorWidth = (fontWidth / 3) * 2;
		const int left        = x - cursorWidth / 2;
		const int right       = left + cursorWidth;

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
	case CursorStyles::Normal:
		path.moveTo(x + 1, y);
		path.lineTo(x + 1, bot);
		break;
	case CursorStyles::Block:
		path.addRect(x, y, fontWidth, fontHeight - 1);
		break;
	}

	QPen pen(cursorFGColor_);
	pen.setWidth(heavyCursor_ ? DefaultCursorWidth * 2 : DefaultCursorWidth);

	painter->setPen(pen);
	painter->drawPath(path);
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
QColor TextArea::getRangesetColor(size_t ind, QColor bground) const {

	if (ind > 0) {
		--ind;
		const std::unique_ptr<RangesetTable> &tab = document_->rangesetTable_;

		QColor color;
		int valid = tab->getColorValid(ind, &color);
		if (valid == 0) {
			const QString color_name = tab->getColorName(ind);
			if (!color_name.isNull()) {
				color = X11Colors::fromString(color_name);
			}
			tab->assignColor(ind, color);
		}

		if (color.isValid()) {
			return color;
		}
	}

	return bground;
}

/**
 * @brief TextArea::handleResize
 * @param widthChanged
 */
void TextArea::handleResize(bool widthChanged) {

	const QRect viewRect      = viewport()->contentsRect();
	const int oldVisibleLines = nVisibleLines_;
	const int newVisibleLines = (viewRect.height() / fixedFontHeight_);

	/* In continuous wrap mode, a change in width affects the total number of
	   lines in the buffer, and can leave the top line number incorrect, and
	   the top character no longer pointing at a valid line start */
	if (continuousWrap_ && wrapMargin_ == 0 && widthChanged) {
		const TextCursor oldFirstChar = firstChar_;
		const TextCursor start        = buffer_->BufStartOfBuffer();
		const TextCursor end          = buffer_->BufEndOfBuffer();

		nBufferLines_ = countLines(start, end, /*startPosIsLineStart=*/true);
		firstChar_    = startOfLine(firstChar_);
		topLineNum_   = countLines(start, firstChar_, /*startPosIsLineStart=*/true) + 1;
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

	/* if the window became taller, there may be an opportunity to display
	   more text by scrolling down */
	if (oldVisibleLines < newVisibleLines && topLineNum_ + nVisibleLines_ > nBufferLines_) {
		verticalScrollBar()->setValue(std::max(1, nBufferLines_ - nVisibleLines_ + 2 + cursorVPadding_));
	}

	/* Update the scroll bar bar parameters.
	 * If updating the horizontal range caused scrolling, redraw */
	updateVScrollBarRange();
	updateHScrollBarRange();

	hideOrShowHScrollBar();

	updateCalltip(0);
}

/*
** Same as BufCountLines, but takes in to account wrapping if wrapping is
** turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as true to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
*/
int TextArea::countLines(TextCursor startPos, TextCursor endPos, bool startPosIsLineStart) {

	// If we're not wrapping use simple (and more efficient) BufCountLines
	if (!continuousWrap_) {
		return buffer_->BufCountLines(startPos, endPos);
	}

	int retLines;
	TextCursor retPos;
	TextCursor retLineStart;
	TextCursor retLineEnd;

	wrappedLineCounter(
		buffer_,
		startPos,
		endPos,
		INT_MAX,
		startPosIsLineStart,
		&retPos,
		&retLines,
		&retLineStart,
		&retLineEnd);

	return retLines;
}

/**
 * @brief TextArea::setCursorStyle
 * @param style
 */
void TextArea::setCursorStyle(CursorStyles style) {
	cursorStyle_ = style;
	if (cursorOn_) {
		redisplayRange(cursorPos_ - 1, cursorPos_ + 1);
	}
}

/**
 * @brief TextArea::TextDBlankCursor
 */
void TextArea::TextDBlankCursor() {
	if (cursorOn_) {
		cursorOn_ = false;
		redisplayRange(cursorPos_ - 1, cursorPos_ + 1);
	}
}

/**
 * @brief TextArea::TextDUnblankCursor
 */
void TextArea::unblankCursor() {
	if (!cursorOn_) {
		cursorOn_ = true;
		redisplayRange(cursorPos_ - 1, cursorPos_ + 1);
	}
}

/*
** Offset the line starts array, topLineNum, firstChar and lastChar, for a new
** vertical scroll position given by newTopLineNum.  If any currently displayed
** lines will still be visible, salvage the line starts values, otherwise,
** count lines from the nearest known line start (start or end of buffer, or
** the closest value in the lineStarts array)
*/
void TextArea::offsetLineStarts(int newTopLineNum) {

	const TextCursor oldFirstChar = firstChar_;
	const int oldTopLineNum       = topLineNum_;
	const int lineDelta           = newTopLineNum - oldTopLineNum;
	const int nVisLines           = nVisibleLines_;

	// If there was no offset, nothing needs to be changed
	if (lineDelta == 0) {
		return;
	}

	/* Find the new value for firstChar by counting lines from the nearest
	   known line start (start or end of buffer, or the closest value in the
	   lineStarts array) */
	const int lastLineNum = oldTopLineNum + nVisLines - 1;

	if (newTopLineNum < oldTopLineNum && newTopLineNum < -lineDelta) {
		firstChar_ = forwardNLines(buffer_->BufStartOfBuffer(), newTopLineNum - 1, true);
	} else if (newTopLineNum < oldTopLineNum) {
		firstChar_ = countBackwardNLines(firstChar_, -lineDelta);
	} else if (newTopLineNum < lastLineNum) {
		firstChar_ = lineStarts_[newTopLineNum - oldTopLineNum];
	} else if (newTopLineNum - lastLineNum < nBufferLines_ - newTopLineNum) {
		firstChar_ = forwardNLines(lineStarts_[nVisLines - 1], newTopLineNum - lastLineNum, true);
	} else {
		firstChar_ = countBackwardNLines(buffer_->BufEndOfBuffer(), nBufferLines_ - newTopLineNum + 1);
	}

	// Fill in the line starts array
	if (lineDelta < 0 && -lineDelta < nVisLines) {
		for (int i = nVisLines - 1; i >= -lineDelta; i--) {
			lineStarts_[i] = lineStarts_[i + lineDelta];
		}

		calcLineStarts(0, -lineDelta);
	} else if (lineDelta > 0 && lineDelta < nVisLines) {
		for (int i = 0; i < nVisLines - lineDelta; i++) {
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
** Update the position of the current calltip if one exists, else do nothing
*/
void TextArea::updateCalltip(int calltipID) {

	const QRect viewRect = viewport()->contentsRect();

	if (calltip_.ID == 0) {
		return;
	}

	if (calltipID != 0 && calltipID != calltip_.ID) {
		return;
	}

	if (!calltipWidget_) {
		return;
	}

	int rel_x;
	int rel_y;

	if (calltip_.anchored) {
		// Put it at the anchor position
		if (!positionToXY(boost::get<TextCursor>(calltip_.pos), &rel_x, &rel_y)) {
			if (calltip_.alignMode == TipAlignMode::Strict) {
				TextDKillCalltip(calltip_.ID);
			}
			return;
		}
	} else {
		if (boost::get<int>(calltip_.pos) < 0) {
			// First display of tip with cursor offscreen (detected in ShowCalltip)
			calltip_.pos    = viewRect.width() / 2;
			calltip_.hAlign = TipHAlignMode::Center;
			rel_y           = viewRect.height() / 3;
		} else if (!positionToXY(cursorPos_, &rel_x, &rel_y)) {
			// Window has scrolled and tip is now offscreen
			if (calltip_.alignMode == TipAlignMode::Strict) {
				TextDKillCalltip(calltip_.ID);
			}
			return;
		}
		rel_x = boost::get<int>(calltip_.pos);
	}

	const int lineHeight = fixedFontHeight_;
	const int tipWidth   = calltipWidget_->width();
	const int tipHeight  = calltipWidget_->height();

	constexpr int BorderWidth = 1;
	rel_x += BorderWidth;
	rel_y += lineHeight / 2 + BorderWidth;

	// Adjust rel_x for horizontal alignment modes
	switch (calltip_.hAlign) {
	case TipHAlignMode::Center:
		rel_x -= tipWidth / 2;
		break;
	case TipHAlignMode::Right:
		rel_x -= tipWidth;
		break;
	default:
		break;
	}

	int flip_delta;

	// Adjust rel_y for vertical alignment modes
	switch (calltip_.vAlign) {
	case TipVAlignMode::Above:
		flip_delta = tipHeight + lineHeight + (2 * BorderWidth);
		rel_y -= flip_delta;
		break;
	default:
		flip_delta = -(tipHeight + lineHeight + (2 * BorderWidth));
		break;
	}

	QPoint abs = mapToGlobal(QPoint(rel_x, rel_y));

	// If we're not in strict mode try to keep the tip on-screen
	if (calltip_.alignMode == TipAlignMode::Sloppy) {

		QScreen *currentScreen = QGuiApplication::primaryScreen();
		QRect screenGeometry   = currentScreen->geometry();

		// make sure tip doesn't run off right or left side of screen
		if (abs.x() + tipWidth >= screenGeometry.width() - CALLTIP_EDGE_GUARD) {
			abs.setX(screenGeometry.width() - tipWidth - CALLTIP_EDGE_GUARD);
		}

		if (abs.x() < CALLTIP_EDGE_GUARD) {
			abs.setX(CALLTIP_EDGE_GUARD);
		}

		// Try to keep the tip onscreen vertically if possible
		if (screenGeometry.height() > tipHeight && offscreenV(currentScreen, abs.y(), tipHeight)) {
			// Maybe flipping from below to above (or vice-versa) will help
			if (!offscreenV(currentScreen, abs.y() + flip_delta, tipHeight)) {
				abs.setY(abs.y() + flip_delta);
			}

			// Make sure the tip doesn't end up *totally* offscreen
			else if (abs.y() + tipHeight < 0) {
				abs.setY(CALLTIP_EDGE_GUARD);
			} else if (abs.y() >= screenGeometry.height()) {
				abs.setY(screenGeometry.height() - tipHeight - CALLTIP_EDGE_GUARD);
			}
			// If no case applied, just go with the default placement.
		}
	}

	calltipWidget_->move(abs);
	calltipWidget_->show();
}

/*
** Read the background color class specification string in str, allocating the
** necessary colors.
** Note: the allocation of class numbers could be more intelligent: there can
** never be more than 256 of these (one per character); but I don't think
** there'll be a pressing need. I suppose the scanning of the specification
** could be better too, but then, who cares!
*/
void TextArea::setupBGClasses(const QString &str) {

	const QColor bgColorDefault = palette().color(QPalette::Base);

	bgClassColors_.clear();
	bgClass_.clear();

	if (str.isEmpty()) {
		return;
	}

	std::array<uint8_t, 256> bgClass      = {};
	std::array<QColor, 256> bgClassColors = {};

	// default for all chars is class number zero, for standard background
	bgClassColors[0] = bgColorDefault;

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
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	QStringList formats = str.split(QLatin1Char(';'), Qt::SkipEmptyParts);
#else
	QStringList formats = str.split(QLatin1Char(';'), QString::SkipEmptyParts);
#endif
	for (const QString &format : formats) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
		QStringList s1 = format.split(QLatin1Char(':'), Qt::SkipEmptyParts);
#else
		QStringList s1 = format.split(QLatin1Char(':'), QString::SkipEmptyParts);
#endif
		if (s1.size() == 2) {
			QString ranges = s1[0];
			QString color  = s1[1];

			if (class_no > UINT8_MAX) {
				break;
			}

			// NOTE(eteran): the original code started at index 1
			// by incrementing before using it. This code post increments
			// starting the classes at 0, which allows NUL characters
			// to be styled correctly. I am not aware of any negative
			// side effects of this.
			const auto nextClass = static_cast<uint8_t>(class_no++);

			QColor pix               = X11Colors::fromString(color);
			bgClassColors[nextClass] = pix;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
			QStringList rangeList = ranges.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
			QStringList rangeList = ranges.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
			for (const QString &range : rangeList) {
				QRegExp regex(QLatin1String("([0-9]+)(?:-([0-9]+))?"));
				if (regex.exactMatch(range)) {

					const QString lo = regex.cap(1);
					const QString hi = regex.cap(2);

					bool loOK;
					bool hiOK;
					int lowerBound = lo.toInt(&loOK);
					int upperBound = hi.toInt(&hiOK);

					if (loOK) {
						if (!hiOK) {
							upperBound = lowerBound;
						}

						for (int i = lowerBound; i <= upperBound; ++i) {
							bgClass[i] = nextClass;
						}
					}
				}
			}
		}
	}

	std::vector<uint8_t> backgroundClass;
	backgroundClass.reserve(256);
	std::copy(bgClass.begin(), bgClass.end(), std::back_inserter(backgroundClass));

	std::vector<QColor> backgroundColor;
	backgroundColor.reserve(class_no);
	std::copy_n(bgClassColors.begin(), class_no, std::back_inserter(backgroundColor));

	bgClass_       = std::move(backgroundClass);
	bgClassColors_ = std::move(backgroundColor);
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** false if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
bool TextArea::positionToXY(TextCursor pos, QPoint *coord) const {
	int x;
	int y;
	if (positionToXY(pos, &x, &y)) {
		*coord = QPoint(x, y);
		return true;
	}
	return false;
}

bool TextArea::positionToXY(TextCursor pos, int *x, int *y) const {

	// TODO(eteran): return optional pair?
	const QRect viewRect = viewport()->contentsRect();
	int visLineNum       = 0;

	// If position is not displayed, return false
	if (pos < firstChar_ || (pos > lastChar_ && !emptyLinesVisible())) {
		return false;
	}

	// Calculate y coordinate
	if (!posToVisibleLineNum(pos, &visLineNum)) {
		return false;
	}

	*y = viewRect.top() + visLineNum * fixedFontHeight_ + fixedFontHeight_ / 2;

	/* Get the text, length, and  buffer position of the line. If the position
	   is beyond the end of the buffer and should be at the first position on
	   the first empty line, don't try to get or scan the text  */
	const TextCursor lineStartPos = lineStarts_[visLineNum];
	if (lineStartPos == -1) {
		*x = viewRect.left() - horizontalScrollBar()->value();
		return true;
	}

	const int lineLen         = visLineLength(visLineNum);
	const std::string lineStr = buffer_->BufGetRange(lineStartPos, lineStartPos + lineLen);

	/* Step through character positions from the beginning of the line
	   to "pos" to calculate the x coordinate */
	int xStep             = viewRect.left() - horizontalScrollBar()->value();
	int outIndex          = 0;
	const int tabDistance = buffer_->BufGetTabDistance();
	for (int charIndex = 0; charIndex < pos - lineStartPos; charIndex++) {

		const int charLen = TextBuffer::BufCharWidth(
			lineStr[static_cast<size_t>(charIndex)],
			outIndex,
			tabDistance);

		xStep += lengthToWidth(charLen);
		outIndex += charLen;
	}

	*x = xStep;
	return true;
}

/**
 * Change the (non syntax-highlight) colors
 *
 * @brief TextArea::setColors
 * @param textFG
 * @param textBG
 * @param selectionFG
 * @param selectionBG
 * @param matchFG
 * @param matchBG
 * @param lineNumberFG
 * @param lineNumberBG
 * @param cursorFG
 */
void TextArea::setColors(const QColor &textFG, const QColor &textBG, const QColor &selectionFG, const QColor &selectionBG, const QColor &matchFG, const QColor &matchBG, const QColor &lineNumberFG, const QColor &lineNumberBG, const QColor &cursorFG) {

	const QRect viewRect = viewport()->contentsRect();
	QPalette pal         = palette();
	pal.setColor(QPalette::Text, textFG);                 // foreground color
	pal.setColor(QPalette::Base, textBG);                 // background
	pal.setColor(QPalette::Highlight, selectionBG);       // highlight background
	pal.setColor(QPalette::HighlightedText, selectionFG); // highlight foreground
	setPalette(pal);

	matchFGColor_   = matchFG;
	matchBGColor_   = matchBG;
	lineNumFGColor_ = lineNumberFG;
	lineNumBGColor_ = lineNumberBG;
	cursorFGColor_  = cursorFG;

	// Redisplay
	redisplayRect(viewRect);
	repaintLineNumbers();
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

	switch (dragState) {
	case SECONDARY_DRAG:
	case SECONDARY_RECT_DRAG:
		buffer_->BufSecondaryUnselect();
		break;
	case PRIMARY_BLOCK_DRAG:
		cancelBlockDrag();
		break;
	case MOUSE_PAN:
		viewport()->setCursor(Qt::ArrowCursor);
		break;
	case NOT_CLICKED:
		break;
	default:
		if (dragState != NOT_CLICKED) {
			dragState_ = DRAG_CANCELED;
		}
		break;
	}
}

/*
** Set the position of the text insertion cursor
*/
void TextArea::setInsertPosition(TextCursor newPos) {
	// make sure new position is ok, do nothing if it hasn't changed
	if (newPos == cursorPos_) {
		return;
	}

	newPos = qBound(buffer_->BufStartOfBuffer(), newPos, buffer_->BufEndOfBuffer());

	// cursor movement cancels vertical cursor motion column
	cursorPreferredCol_ = -1;

	// erase the cursor at it's previous position
	TextDBlankCursor();

	// draw it at its new position
	cursorPos_ = newPos;
	cursorOn_  = true;
	redisplayRange(cursorPos_ - 1, cursorPos_ + 1);
}

void TextArea::checkAutoShowInsertPos() {

	if (autoShowInsertPos_) {
		TextDMakeInsertPosVisible();
	}
}

/*
** Scroll the display to bring insertion cursor into view.
**
** Note: it would be nice to be able to do this without counting lines twice
** (scrolling counts them too) and/or to count from the most efficient
** starting point, but the efficiency of this routine is not as important to
** the overall performance of the text display.
*/
void TextArea::TextDMakeInsertPosVisible() {

	const QRect viewRect       = viewport()->contentsRect();
	const TextCursor cursorPos = cursorPos_;
	const int cursorVPadding   = cursorVPadding_;
	int linesFromTop           = 0;
	int topLine                = topLineNum_;

	// Don't do padding if this is a mouse operation
	const bool do_padding = ((dragState_ == NOT_CLICKED) && (cursorVPadding > 0));

	// Find the new top line number
	if (cursorPos < firstChar_) {
		topLine -= countLines(cursorPos, firstChar_, /*startPosIsLineStart=*/false);
		// linesFromTop = 0;
	} else if (cursorPos > lastChar_ && !emptyLinesVisible()) {
		topLine += countLines(lastChar_ - (wrapUsesCharacter(lastChar_) ? 0 : 1), cursorPos, /*startPosIsLineStart=*/false);
		linesFromTop = nVisibleLines_ - 1;
	} else if (cursorPos == lastChar_ && !emptyLinesVisible() && !wrapUsesCharacter(lastChar_)) {
		++topLine;
		linesFromTop = nVisibleLines_ - 1;
	} else {
		// Avoid extra counting if cursorVPadding is disabled
		if (do_padding) {
			linesFromTop = countLines(firstChar_, cursorPos, /*startPosIsLineStart=*/true);
		}
	}
	if (topLine < 1) {
		qCritical("NEdit: internal consistency check tl1 failed");
		topLine = 1;
	}

	if (do_padding) {
		// Keep the cursor away from the top or bottom of screen.
		if (nVisibleLines_ <= 2 * cursorVPadding) {
			topLine += (linesFromTop - nVisibleLines_ / 2);
			topLine = std::max(topLine, 1);
		} else if (linesFromTop < cursorVPadding) {
			topLine -= (cursorVPadding - linesFromTop);
			topLine = std::max(topLine, 1);
		} else if (linesFromTop > nVisibleLines_ - cursorVPadding - 1) {
			topLine += (linesFromTop - (nVisibleLines_ - cursorVPadding - 1));
		}
	}

	/* Find the new setting for horizontal offset (this is a bit ungraceful).
	   If the line is visible, just use positionToXY to get the position
	   to scroll to, otherwise, do the vertical scrolling first, then the
	   horizontal */
	QPoint p;
	if (!positionToXY(cursorPos, &p)) {
		verticalScrollBar()->setValue(topLine);

		if (!positionToXY(cursorPos, &p)) {
			return; // Give up, it's not worth it (but why does it fail?)
		}
	}

	int horizOffset = horizontalScrollBar()->value();

	if (p.x() > viewRect.right()) {
		horizOffset += p.x() - (viewRect.right());
	} else if (p.x() < viewRect.left()) {
		horizOffset += p.x() - viewRect.left();
	}

	// Do the scroll
	verticalScrollBar()->setValue(topLine);
	horizontalScrollBar()->setValue(horizOffset);
}

/*
** Cancel a block drag operation
*/
void TextArea::cancelBlockDrag() {

	std::shared_ptr<TextBuffer> origBuf  = std::move(dragOrigBuf_);
	const TextBuffer::Selection *origSel = &origBuf->primary;
	auto modRangeStart                   = TextCursor(-1);
	TextCursor origModRangeEnd;
	TextCursor bufModRangeEnd;

	/* If the operation was a move, make the modify range reflect the
	   removal of the text from the starting position */
	if (dragSourceDeleted_ != 0) {
		trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragSourceDeletePos_, dragSourceInserted_, dragSourceDeleted_);
	}

	/* Include the insert being undone from the last step in the modified
	   range. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragInsertPos_, dragInserted_, dragDeleted_);

	// Make the changes in the buffer
	const std::string repText = origBuf->BufGetRange(modRangeStart, origModRangeEnd);
	buffer_->BufReplace(modRangeStart, bufModRangeEnd, repText);

	// Reset the selection and cursor position
	if (origSel->isRectangular()) {
		buffer_->BufRectSelect(origSel->start(), origSel->end(), origSel->rectStart(), origSel->rectEnd());
	} else {
		buffer_->BufSelect(origSel->start(), origSel->end());
	}

	setInsertPosition(buffer_->BufCursorPosHint());

	callMovedCBs();

	emTabsBeforeCursor_ = 0;

	// Indicate end of drag
	dragState_ = DRAG_CANCELED;

	// Call finish-drag callback
	constexpr DragEndEvent endStruct = {
		TextCursor(), 0, 0, {}};

	for (auto &c : dragEndCallbacks_) {
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
void TextArea::checkMoveSelectionChange(EventFlags flags, TextCursor startPos) {

	const bool extend = flags & ExtendFlag;
	if (extend) {
		const bool rect = flags & RectFlag;
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
void TextArea::keyMoveExtendSelection(TextCursor origPos, bool rectangular) {

	const TextBuffer::Selection *sel = &buffer_->primary;
	const TextCursor newPos          = cursorPos_;

	if ((sel->hasSelection() || sel->isZeroWidth()) && sel->isRectangular() && rectangular) {

		// rect -> rect
		const int newCol          = buffer_->BufCountDispChars(buffer_->BufStartOfLine(newPos), newPos);
		const int startCol        = std::min(rectAnchor_, newCol);
		const int endCol          = std::max(rectAnchor_, newCol);
		const TextCursor startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		const TextCursor endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));

		buffer_->BufRectSelect(startPos, endPos, startCol, endCol);

	} else if (sel->hasSelection() && rectangular) { // plain -> rect

		const int newCol = buffer_->BufCountDispChars(buffer_->BufStartOfLine(newPos), newPos);
		TextCursor anchor;

		if (std::abs(newPos - sel->start()) < std::abs(newPos - sel->end())) {
			anchor = sel->end();
		} else {
			anchor = sel->start();
		}

		const TextCursor anchorLineStart = buffer_->BufStartOfLine(anchor);
		const int rectAnchor             = buffer_->BufCountDispChars(anchorLineStart, anchor);

		anchor_     = anchor;
		rectAnchor_ = rectAnchor;

		buffer_->BufRectSelect(
			buffer_->BufStartOfLine(std::min(anchor, newPos)),
			buffer_->BufEndOfLine(std::max(anchor, newPos)),
			std::min(rectAnchor, newCol),
			std::max(rectAnchor, newCol));

	} else if (sel->hasSelection() && sel->isRectangular()) { // rect -> plain

		const TextCursor startPos = buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(sel->start()), sel->rectStart());
		const TextCursor endPos   = buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(sel->end()), sel->rectEnd());
		TextCursor anchor;

		if (std::abs(origPos - startPos) < std::abs(origPos - endPos)) {
			anchor = endPos;
		} else {
			anchor = startPos;
		}

		buffer_->BufSelect(anchor, newPos);

	} else if (sel->hasSelection()) { // plain -> plain

		TextCursor anchor;

		if (std::abs(origPos - sel->start()) < std::abs(origPos - sel->end())) {
			anchor = sel->end();
		} else {
			anchor = sel->start();
		}

		buffer_->BufSelect(anchor, newPos);

	} else if (rectangular) { // no sel -> rect

		const int origCol         = buffer_->BufCountDispChars(buffer_->BufStartOfLine(origPos), origPos);
		const int newCol          = buffer_->BufCountDispChars(buffer_->BufStartOfLine(newPos), newPos);
		const int startCol        = std::min(newCol, origCol);
		const int endCol          = std::max(newCol, origCol);
		const TextCursor startPos = buffer_->BufStartOfLine(std::min(origPos, newPos));
		const TextCursor endPos   = buffer_->BufEndOfLine(std::max(origPos, newPos));

		anchor_     = origPos;
		rectAnchor_ = origCol;
		buffer_->BufRectSelect(startPos, endPos, startCol, endCol);

	} else { // no sel -> plain

		anchor_     = origPos;
		rectAnchor_ = buffer_->BufCountDispChars(buffer_->BufStartOfLine(origPos), origPos);
		buffer_->BufSelect(anchor_, newPos);
	}
}

/**
 * @brief TextArea::moveRight
 * @return
 */
bool TextArea::moveRight() {
	if (cursorPos_ >= buffer_->BufEndOfBuffer()) {
		return false;
	}

	setInsertPosition(cursorPos_ + 1);
	return true;
}

/**
 * @brief TextArea::moveLeft
 * @return
 */
bool TextArea::moveLeft() {
	if (cursorPos_ <= buffer_->BufStartOfBuffer()) {
		return false;
	}

	setInsertPosition(cursorPos_ - 1);
	return true;
}

/**
 * @brief TextArea::moveUp
 * @param absolute
 * @return
 */
bool TextArea::moveUp(bool absolute) {
	TextCursor lineStartPos;
	TextCursor prevLineStartPos;
	int visLineNum;

	/* Find the position of the start of the line.  Use the line starts array
	   if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (absolute) {
		lineStartPos = buffer_->BufStartOfLine(cursorPos_);
		visLineNum   = -1;
	} else if (posToVisibleLineNum(cursorPos_, &visLineNum)) {
		Q_ASSERT(visLineNum >= 0);
		lineStartPos = lineStarts_[visLineNum];
	} else {
		lineStartPos = startOfLine(cursorPos_);
		visLineNum   = -1;
	}

	if (lineStartPos == 0) {
		return false;
	}

	// Decide what column to move to, if there's a preferred column use that
	const int64_t column = cursorPreferredCol_ >= 0 ? cursorPreferredCol_ : buffer_->BufCountDispChars(lineStartPos, cursorPos_);

	// count forward from the start of the previous line to reach the column
	if (absolute) {
		prevLineStartPos = buffer_->BufCountBackwardNLines(lineStartPos, 1);
	} else if (visLineNum != -1 && visLineNum != 0) {
		prevLineStartPos = lineStarts_[visLineNum - 1];
	} else {
		prevLineStartPos = countBackwardNLines(lineStartPos, 1);
	}

	TextCursor newPos = buffer_->BufCountForwardDispChars(prevLineStartPos, column);
	if (continuousWrap_ && !absolute) {
		newPos = std::min(newPos, endOfLine(prevLineStartPos, /*startPosIsLineStart=*/true));
	}

	// move the cursor
	setInsertPosition(newPos);

	// if a preferred column wasn't already established, establish it
	cursorPreferredCol_ = column;
	return true;
}

/**
 * @brief TextArea::moveDown
 * @param absolute
 * @return
 */
bool TextArea::moveDown(bool absolute) {
	TextCursor lineStartPos;
	TextCursor nextLineStartPos;
	int visLineNum;

	if (cursorPos_ == buffer_->length()) {
		return false;
	}

	if (absolute) {
		lineStartPos = buffer_->BufStartOfLine(cursorPos_);
	} else if (posToVisibleLineNum(cursorPos_, &visLineNum)) {
		lineStartPos = lineStarts_[visLineNum];
	} else {
		lineStartPos = startOfLine(cursorPos_);
	}

	const int64_t column = (cursorPreferredCol_ >= 0) ? cursorPreferredCol_ : buffer_->BufCountDispChars(lineStartPos, cursorPos_);

	if (absolute) {
		nextLineStartPos = buffer_->BufCountForwardNLines(lineStartPos, 1);
	} else {
		nextLineStartPos = forwardNLines(lineStartPos, 1, true);
	}

	TextCursor newPos = buffer_->BufCountForwardDispChars(nextLineStartPos, column);

	if (continuousWrap_ && !absolute) {
		newPos = std::min(newPos, endOfLine(nextLineStartPos, /*startPosIsLineStart=*/true));
	}

	setInsertPosition(newPos);
	cursorPreferredCol_ = column;
	return true;
}

/**
 * @brief TextArea::checkReadOnly
 * @return
 */
bool TextArea::checkReadOnly() const {
	if (readOnly_) {
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
void TextArea::TextInsertAtCursor(view::string_view chars, bool allowPendingDelete, bool allowWrap) {

	const QRect viewRect = viewport()->contentsRect();
	const int fontWidth  = fixedFontWidth_;

	// Don't wrap if auto-wrap is off or suppressed, or it's just a newline
	if (!allowWrap || !autoWrap_ || chars.compare("\n") == 0) {
		simpleInsertAtCursor(chars, allowPendingDelete);
		return;
	}

	/* If this is going to be a pending delete operation, the real insert
	   position is the start of the selection.  This will make rectangular
	   selections wrap strangely, but this routine should rarely be used for
	   them, and even more rarely when they need to be wrapped. */
	const bool replaceSel      = allowPendingDelete && pendingSelection();
	const TextCursor cursorPos = replaceSel ? buffer_->primary.start() : cursorPos_;

	/* If the text is only one line and doesn't need to be wrapped, just insert
	   it and be done (for efficiency only, this routine is called for each
	   character typed). (Of course, it may not be significantly more efficient
	   than the more general code below it, so it may be a waste of time!) */
	const int wrapMargin          = (wrapMargin_ != 0) ? wrapMargin_ : (viewRect.width() / fontWidth);
	const TextCursor lineStartPos = buffer_->BufStartOfLine(cursorPos);

	int64_t colNum = buffer_->BufCountDispChars(lineStartPos, cursorPos);

	auto it = chars.begin();
	for (; it != chars.end() && *it != '\n'; it++) {
		colNum += TextBuffer::BufCharWidth(*it, colNum, buffer_->BufGetTabDistance());
	}

	const bool singleLine = (it == chars.end());
	if (colNum < wrapMargin && singleLine) {
		simpleInsertAtCursor(chars, true);
		return;
	}

	// Wrap the text
	int64_t breakAt                 = 0;
	const std::string lineStartText = buffer_->BufGetRange(lineStartPos, cursorPos);
	const std::string wrappedText   = wrapText(lineStartText, chars, to_integer(lineStartPos), wrapMargin, replaceSel ? nullptr : &breakAt);

	/* Insert the text.  Where possible, use TextDInsert which is optimized
	   for less redraw. */
	if (replaceSel) {
		buffer_->BufReplaceSelected(wrappedText);
		setInsertPosition(buffer_->BufCursorPosHint());
	} else if (overstrike_) {
		if (breakAt == 0 && singleLine)
			TextDOverstrike(wrappedText);
		else {
			buffer_->BufReplace(cursorPos - breakAt, cursorPos, wrappedText);
			setInsertPosition(buffer_->BufCursorPosHint());
		}
	} else {
		if (breakAt == 0) {
			insertText(wrappedText);
		} else {
			buffer_->BufReplace(cursorPos - breakAt, cursorPos, wrappedText);
			setInsertPosition(buffer_->BufCursorPosHint());
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
	const TextBuffer::Selection *sel = &buffer_->primary;
	const TextCursor pos             = cursorPos_;

	return pendingDelete_ && sel->hasSelection() && pos >= sel->start() && pos <= sel->end();
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
std::string TextArea::wrapText(view::string_view startLine, view::string_view text, int64_t bufOffset, int wrapMargin, int64_t *breakBefore) {

	// Create a temporary text buffer and load it with the strings
	TextBuffer wrapBuf;
	wrapBuf.BufSetSyncXSelection(false);
	wrapBuf.BufInsert(buffer_->BufStartOfBuffer(), startLine);
	wrapBuf.BufAppend(text);

	const auto startLineEnd = TextCursor(startLine.size());
	const int tabDist       = buffer_->BufGetTabDistance();

	/* Scan the buffer for long lines and apply wrapLine when wrapMargin is
	   exceeded.  limitPos enforces no breaks in the "startLine" part of the
	   string (if requested), and prevents re-scanning of long unbreakable
	   lines for each character beyond the margin */
	int64_t colNum          = 0;
	TextCursor pos          = wrapBuf.BufStartOfBuffer();
	TextCursor lineStartPos = {};
	TextCursor limitPos     = (breakBefore == nullptr) ? startLineEnd : buffer_->BufStartOfBuffer();
	auto firstBreak         = TextCursor(-1);

	while (pos < wrapBuf.BufEndOfBuffer()) {
		const char c = wrapBuf.BufGetCharacter(pos);
		if (c == '\n') {
			lineStartPos = limitPos = pos + 1;
			colNum                  = 0;
		} else {
			colNum += TextBuffer::BufCharWidth(c, colNum, tabDist);
			if (colNum > wrapMargin) {

				int64_t charsAdded;
				TextCursor breakAt;

				if (!wrapLine(&wrapBuf, bufOffset, lineStartPos, pos, limitPos, &breakAt, &charsAdded)) {
					limitPos = std::max(pos, limitPos);
				} else {
					lineStartPos = limitPos = breakAt + 1;
					pos += charsAdded;
					colNum = wrapBuf.BufCountDispChars(lineStartPos, pos + 1);
					if (firstBreak == -1) {
						firstBreak = breakAt;
					}
				}
			}
		}
		++pos;
	}

	// Return the wrapped text, possibly including part of startLine
	std::string wrappedText;

	if (!breakBefore) {
		wrappedText = wrapBuf.BufGetRange(startLineEnd, TextCursor(wrapBuf.length()));
	} else {
		*breakBefore = firstBreak != -1 && firstBreak < startLineEnd ? startLineEnd - firstBreak : 0;
		wrappedText  = wrapBuf.BufGetRange(startLineEnd - *breakBefore, TextCursor(wrapBuf.length()));
	}
	return wrappedText;
}

/*
** Insert "text" (which must not contain newlines), over-striking the current
** cursor location.
*/
void TextArea::TextDOverstrike(view::string_view text) {

	const TextCursor startPos  = cursorPos_;
	const TextCursor lineStart = buffer_->BufStartOfLine(startPos);
	const auto textLen         = gsl::narrow<int64_t>(text.size());
	TextCursor p;

	std::string paddedText;
	bool paddedTextSet = false;

	// determine how many displayed character positions are covered
	const int64_t startIndent = buffer_->BufCountDispChars(lineStart, startPos);
	int64_t indent            = startIndent;
	for (char ch : text) {
		indent += TextBuffer::BufCharWidth(ch, indent, buffer_->BufGetTabDistance());
	}
	const int64_t endIndent = indent;

	/* find which characters to remove, and if necessary generate additional
	   padding to make up for removed control characters at the end */
	indent = startIndent;
	for (p = startPos;; ++p) {
		if (p == buffer_->length()) {
			break;
		}

		const char ch = buffer_->BufGetCharacter(p);
		if (ch == '\n') {
			break;
		}

		indent += TextBuffer::BufCharWidth(ch, indent, buffer_->BufGetTabDistance());
		if (indent == endIndent) {
			++p;
			break;
		} else if (indent > endIndent) {
			if (ch != '\t') {
				++p;

				std::string padded;
				padded.reserve(text.size() + static_cast<size_t>(indent - endIndent));

				padded.append(text.begin(), text.end());
				padded.append(static_cast<size_t>(indent - endIndent), ' ');
				paddedText    = std::move(padded);
				paddedTextSet = true;
			}
			break;
		}
	}
	const TextCursor endPos = p;

	cursorToHint_ = startPos + textLen;
	buffer_->BufReplace(startPos, endPos, !paddedTextSet ? text : paddedText);
	cursorToHint_ = NO_HINT;
}

/*
** Insert "text" at the current cursor location.  This has the same
** effect as inserting the text into the buffer using BufInsertEx and
** then moving the insert position after the newly inserted text, except
** that it's optimized to do less redrawing.
*/
void TextArea::insertText(view::string_view text) {

	const TextCursor pos = cursorPos_;

	cursorToHint_ = pos + static_cast<int64_t>(text.size());
	buffer_->BufInsert(pos, text);
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
** the number of characters added to achieve the auto-indent.  wrapMargin is
** used to decide whether auto-indent should be skipped because the indent
** string itself would exceed the wrap margin.
*/
bool TextArea::wrapLine(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, TextCursor limitPos, TextCursor *breakAt, int64_t *charsAdded) {

	// TODO(eteran): return optional structure?
	TextCursor p;
	int column;

	/* Scan backward for whitespace or BOL.  If BOL, return false, no
	   whitespace in line at which to wrap */
	for (p = lineEndPos;; --p) {
		if (p < lineStartPos || p < limitPos) {
			return false;
		}

		const char c = buf->BufGetCharacter(p);
		if (c == '\t' || c == ' ') {
			break;
		}
	}

	/* Create an auto-indent string to insert to do wrap.  If the auto
	   indent string reaches the wrap position, slice the auto-indent
	   back off and return to the left margin */
	std::string indentStr;
	if (autoIndent_ || smartIndent_) {
		indentStr = createIndentString(buf, bufOffset, lineStartPos, lineEndPos, &column);
		if (column >= p - lineStartPos) {
			indentStr.resize(1);
		}
	} else {
		indentStr = "\n";
	}

	/* Replace the whitespace character with the auto-indent string
	   and return the stats */
	buf->BufReplace(p, p + 1, indentStr);

	*breakAt    = p;
	*charsAdded = indentStr.size() - 1;
	return true;
}

/*
** Create and return an auto-indent string to add a newline at lineEndPos to a
** line starting at lineStartPos in buf.  "buf" may or may not be the real
** text buffer for the widget.  If it is not the widget's text buffer it's
** offset position from the real buffer must be specified in "bufOffset" to
** allow the smart-indent routines to scan back as far as necessary.
** the indent column is returned in "column" (if non nullptr).
*/
std::string TextArea::createIndentString(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, int *column) {

	int indent         = -1;
	const int tabDist  = buffer_->BufGetTabDistance();
	const bool useTabs = buffer_->BufGetUseTabs();

	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (smartIndent_ && (lineStartPos == 0 || buf == buffer_)) {
		SmartIndentEvent smartIndent;
		smartIndent.reason     = NEWLINE_INDENT_NEEDED;
		smartIndent.pos        = lineEndPos + bufOffset;
		smartIndent.request    = 0;
		smartIndent.charsTyped = view::string_view();

		for (auto &c : smartIndentCallbacks_) {
			c.first(this, &smartIndent, c.second);
		}

		indent = smartIndent.request;
	}

	// If smart indent wasn't used, measure the indent distance of the line
	if (indent == -1) {
		indent = 0;
		for (TextCursor pos = lineStartPos; pos < lineEndPos; ++pos) {
			const char c = buf->BufGetCharacter(pos);
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

	std::string indentStr;
	indentStr.reserve(static_cast<size_t>(indent));

	auto indentPtr = std::back_inserter(indentStr);

	*indentPtr++ = '\n';
	if (useTabs) {
		for (int i = 0; i < indent / tabDist; i++) {
			*indentPtr++ = '\t';
		}

		for (int i = 0; i < indent % tabDist; i++) {
			*indentPtr++ = ' ';
		}
	} else {
		for (int i = 0; i < indent; i++) {
			*indentPtr++ = ' ';
		}
	}

	// Return any requested stats
	if (column) {
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

	simpleInsertAtCursor("\n", true);
	buffer_->BufUnselect();
}

void TextArea::newlineAndIndentAP(EventFlags flags) {

	EMIT_EVENT_0("newline_and_indent");

	if (checkReadOnly()) {
		return;
	}

	cancelDrag();

	/* Create a string containing a newline followed by auto or smart
	   indent string */
	int column;
	const TextCursor cursorPos    = cursorPos_;
	const TextCursor lineStartPos = buffer_->BufStartOfLine(cursorPos);
	const std::string indentStr   = createIndentString(buffer_, 0, lineStartPos, cursorPos, &column);

	// Insert it at the cursor
	simpleInsertAtCursor(indentStr, true);

	if (emulateTabs_ > 0) {
		/*  If emulated tabs are on, make the inserted indent deletable by
			tab. Round this up by faking the column a bit to the right to
			let the user delete half-tabs with one keypress.  */
		column += emulateTabs_ - 1;
		emTabsBeforeCursor_ = column / emulateTabs_;
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
bool TextArea::deleteEmulatedTab() {

	const int emTabDist          = emulateTabs_;
	const int emTabsBeforeCursor = emTabsBeforeCursor_;

	if (emTabDist <= 0 || emTabsBeforeCursor <= 0) {
		return false;
	}

	// Find the position of the previous tab stop
	const TextCursor insertPos = cursorPos_;
	const TextCursor lineStart = buffer_->BufStartOfLine(insertPos);
	const int64_t startIndent  = buffer_->BufCountDispChars(lineStart, insertPos);
	const int64_t toIndent     = (startIndent - 1) - ((startIndent - 1) % emTabDist);

	/* Find the position at which to begin deleting (stop at non-whitespace
	   characters) */
	int startPosIndent  = 0;
	int indent          = 0;
	TextCursor startPos = lineStart;

	for (TextCursor pos = lineStart; pos < insertPos; ++pos) {
		const char ch = buffer_->BufGetCharacter(pos);
		indent += TextBuffer::BufCharWidth(ch, indent, buffer_->BufGetTabDistance());
		if (indent > toIndent) {
			break;
		}
		startPosIndent = indent;
		startPos       = pos + 1;
	}

	// Just to make sure, check that we're not deleting any non-white chars
	for (TextCursor pos = insertPos - 1; pos >= startPos; --pos) {
		const char ch = buffer_->BufGetCharacter(pos);
		if (ch != ' ' && ch != '\t') {
			startPos = pos + 1;
			break;
		}
	}

	/* Do the text replacement and reposition the cursor.  If any spaces need
	   to be inserted to make up for a deleted tab, do a BufReplaceEx, otherwise,
	   do a BufRemove. */
	if (startPosIndent < toIndent) {

		const std::string spaceString(static_cast<size_t>(toIndent - startPosIndent), ' ');

		buffer_->BufReplace(startPos, insertPos, spaceString);
		setInsertPosition(startPos + toIndent - startPosIndent);
	} else {
		buffer_->BufRemove(startPos, insertPos);
		setInsertPosition(startPos);
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

	if (buffer_->primary.hasSelection()) {
		buffer_->BufRemoveSelected();
		setInsertPosition(buffer_->BufCursorPosHint());
		checkAutoShowInsertPos();
		callCursorMovementCBs();
		return true;
	} else {
		return false;
	}
}

TextCursor TextArea::startOfWord(TextCursor pos) const {

	if (buffer_->BufIsEmpty()) {
		return buffer_->BufStartOfBuffer();
	}

	const char ch = buffer_->BufGetCharacter(pos);

	boost::optional<TextCursor> startPos;

	if (ch == ' ' || ch == '\t') {
		startPos = spanBackward(buffer_, pos, view::string_view(" \t", 2), false);
	} else if (isDelimeter(ch)) {
		startPos = spanBackward(buffer_, pos, delimiters_, true);
	} else {
		startPos = buffer_->searchBackward(pos, delimiters_);
	}

	if (!startPos) {
		return buffer_->BufStartOfBuffer();
	}

	return std::min(pos, *startPos + 1);
}

TextCursor TextArea::endOfWord(TextCursor pos) const {

	if (buffer_->BufIsEmpty()) {
		return buffer_->BufStartOfBuffer();
	}

	const char ch = buffer_->BufGetCharacter(pos);

	boost::optional<TextCursor> endPos;

	if (ch == ' ' || ch == '\t') {
		endPos = spanForward(buffer_, pos, view::string_view(" \t", 2), false);
	} else if (isDelimeter(ch)) {
		endPos = spanForward(buffer_, pos, delimiters_, true);
	} else {
		endPos = buffer_->searchForward(pos, delimiters_);
	}

	if (!endPos) {
		return buffer_->BufEndOfBuffer();
	}

	return *endPos;
}

/*
** Search backwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character BEFORE "startPos". If
** ignoreSpace is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
boost::optional<TextCursor> TextArea::spanBackward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const {

	if (startPos == 0) {
		return boost::none;
	}

	TextCursor pos = startPos - 1;
	while (pos >= 0) {

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

		--pos;
	}

	return boost::none;
}

/*
** Search forwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character "startPos". If ignoreSpace
** is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
boost::optional<TextCursor> TextArea::spanForward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const {

	TextCursor pos = startPos;
	while (pos < buf->length()) {

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
		++pos;
	}

	return boost::none;
}

void TextArea::processShiftUpAP(EventFlags flags) {

	EMIT_EVENT_0("process_shift_up");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;
	const bool absolute        = flags & AbsoluteFlag;

	cancelDrag();
	if (!moveUp(absolute)) {
		ringIfNecessary(silent);
	}

	keyMoveExtendSelection(insertPos, flags & RectFlag);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::processShiftDownAP(EventFlags flags) {

	EMIT_EVENT_0("process_shift_down");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;
	const bool absolute        = flags & AbsoluteFlag;

	cancelDrag();
	if (!moveDown(absolute)) {
		ringIfNecessary(silent);
	}

	keyMoveExtendSelection(insertPos, flags & RectFlag);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::keySelectAP(EventFlags flags) {

	EMIT_EVENT_0("key_select");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;

	cancelDrag();

	bool stat;
	if (flags & LeftFlag) {
		stat = moveLeft();
	} else if (flags & RightFlag) {
		stat = moveRight();
	} else if (flags & UpFlag) {
		stat = moveUp(false);
	} else if (flags & DownFlag) {
		stat = moveDown(false);
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

	if (!buffer_->primary.hasSelection()) {
		QApplication::beep();
		return;
	}

	CopyToClipboard();
	buffer_->BufRemoveSelected();
	setInsertPosition(buffer_->BufCursorPosHint());
	checkAutoShowInsertPos();
}

/*
** Copy the primary selection to the clipboard
*/
void TextArea::CopyToClipboard() {

	// Get the selected text, if there's no selection, do nothing
	const std::string text = buffer_->BufGetSelectionText();
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

	insertClipboard(/*isColumnar=*/false);
	callCursorMovementCBs();
}

void TextArea::TextColPasteClipboard() {
	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	insertClipboard(/*isColumnar=*/true);
	callCursorMovementCBs();
}

/*
** Insert the X CLIPBOARD selection at the cursor position.  If isColumnar,
** do an BufInsertCol for a columnar paste instead of BufInsert
*/
void TextArea::insertClipboard(bool isColumnar) {

	const QMimeData *const mimeData = QApplication::clipboard()->mimeData(QClipboard::Clipboard);
	if (!mimeData->hasText()) {
		return;
	}

	const std::string contents = mimeData->text().toStdString();

	// Insert it in the text widget
	if (isColumnar && !buffer_->primary.hasSelection()) {
		const TextCursor cursorPos       = cursorPos_;
		const TextCursor cursorLineStart = buffer_->BufStartOfLine(cursorPos);
		const int64_t column             = buffer_->BufCountDispChars(cursorLineStart, cursorPos);

		if (overstrike_) {
			buffer_->BufOverlayRect(cursorLineStart, column, -1, contents, nullptr, nullptr);
		} else {
			buffer_->BufInsertCol(column, cursorLineStart, contents, nullptr, nullptr);
		}

		setInsertPosition(buffer_->BufCountForwardDispChars(cursorLineStart, column));
		if (autoShowInsertPos_) {
			TextDMakeInsertPosVisible();
		}
	} else {
		TextInsertAtCursor(contents, true, autoWrapPastedText_);
	}
}

/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursorEx, but without the complicated auto-wrap
** scanning and re-formatting.
*/
void TextArea::simpleInsertAtCursor(view::string_view chars, bool allowPendingDelete) {

	if (allowPendingDelete && pendingSelection()) {
		buffer_->BufReplaceSelected(chars);
		setInsertPosition(buffer_->BufCursorPosHint());
	} else if (overstrike_) {

		const size_t index = chars.find('\n');
		if (index != view::string_view::npos) {
			insertText(chars);
		} else {
			TextDOverstrike(chars);
		}
	} else {
		insertText(chars);
	}

	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::copyPrimaryAP(EventFlags flags) {

	EMIT_EVENT_0("copy_primary");

	const TextBuffer::Selection &primary = buffer_->primary;
	const bool rectangular               = flags & RectFlag;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (primary.hasSelection() && rectangular) {
		const std::string textToCopy = buffer_->BufGetSelectionText();
		const TextCursor insertPos   = cursorPos_;
		const int64_t column         = buffer_->BufCountDispChars(buffer_->BufStartOfLine(insertPos), insertPos);

		buffer_->BufInsertCol(column, insertPos, textToCopy, nullptr, nullptr);
		setInsertPosition(buffer_->BufCursorPosHint());

		checkAutoShowInsertPos();
	} else if (primary.hasSelection()) {
		const std::string textToCopy = buffer_->BufGetSelectionText();
		const TextCursor insertPos   = cursorPos_;

		buffer_->BufInsert(insertPos, textToCopy);
		setInsertPosition(insertPos + textToCopy.size());

		checkAutoShowInsertPos();
	} else if (rectangular) {
		if (!positionToXY(cursorPos_, &btnDownCoord_)) {
			return; // shouldn't happen
		}

		insertPrimarySelection(/*isColumnar=*/true);
	} else {
		insertPrimarySelection(/*isColumnar=*/false);
	}
}

/*
** Insert the X PRIMARY selection (from whatever window currently owns it)
** at the cursor position.
*/
void TextArea::insertPrimarySelection(bool isColumnar) {

	if (!QApplication::clipboard()->supportsSelection()) {
		QApplication::beep();
		return;
	}

	const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
	if (!mimeData->hasText()) {
		return;
	}

	std::string string = mimeData->text().toStdString();

	// Insert it in the text widget
	if (isColumnar) {
		const TextCursor cursorPos       = cursorPos_;
		const TextCursor cursorLineStart = buffer_->BufStartOfLine(cursorPos);

		int column;
		int row;
		coordToUnconstrainedPosition(
			btnDownCoord_,
			&row,
			&column);

		buffer_->BufInsertCol(column, cursorLineStart, string, nullptr, nullptr);
		setInsertPosition(buffer_->BufCursorPosHint());
	} else {
		TextInsertAtCursor(string, false, autoWrapPastedText_);
	}
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.
*/
void TextArea::coordToUnconstrainedPosition(const QPoint &coord, int *row, int *column) const {
	xyToUnconstrainedPos(coord.x(), coord.y(), row, column, PositionType::Cursor);
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.
**
** The parameter posType specifies how to interpret the position:
** CURSOR_POS means translate the coordinates to the nearest position between
** characters, and CHARACTER_POS means translate the position to the nearest
** character cell.
*/
void TextArea::xyToUnconstrainedPos(const QPoint &pos, int *row, int *column, PositionType posType) const {
	xyToUnconstrainedPos(pos.x(), pos.y(), row, column, posType);
}

void TextArea::xyToUnconstrainedPos(int x, int y, int *row, int *column, PositionType posType) const {

	const QRect viewRect = viewport()->contentsRect();
	const int fontHeight = fixedFontHeight_;
	const int fontWidth  = fixedFontWidth_;

	// Find the visible line number corresponding to the y coordinate
	*row = (y - viewRect.top()) / fontHeight;

	if (*row < 0) {
		*row = 0;
	}

	if (*row >= nVisibleLines_) {
		*row = nVisibleLines_ - 1;
	}

	*column = ((x - viewRect.left()) + horizontalScrollBar()->value() + (posType == PositionType::Cursor ? fontWidth / 2 : 0)) / fontWidth;

	if (*column < 0) {
		*column = 0;
	}
}

void TextArea::beginningOfFileAP(EventFlags flags) {

	EMIT_EVENT_0("beginning_of_file");

	const TextCursor insertPos = cursorPos_;

	cancelDrag();
	if (flags & ScrollbarFlag) {
		if (topLineNum_ != 1) {
			verticalScrollBar()->setValue(1);
		}
	} else {
		setInsertPosition(buffer_->BufStartOfBuffer());
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::endOfFileAP(EventFlags flags) {

	EMIT_EVENT_0("end_of_file");

	const TextCursor insertPos = cursorPos_;

	cancelDrag();
	if (flags & ScrollbarFlag) {
		const int lastTopLine = std::max(1, nBufferLines_ - (nVisibleLines_ - 2) + cursorVPadding_);
		if (lastTopLine != topLineNum_) {
			verticalScrollBar()->setValue(lastTopLine);
		}
	} else {
		setInsertPosition(buffer_->BufEndOfBuffer());
		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::backwardWordAP(EventFlags flags) {

	EMIT_EVENT_0("backward_word");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}

	TextCursor pos = std::max(insertPos - 1, buffer_->BufStartOfBuffer());
	while (isDelimeter(buffer_->BufGetCharacter(pos)) && pos > 0) {
		--pos;
	}

	pos = startOfWord(pos);

	setInsertPosition(pos);
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::forwardWordAP(EventFlags flags) {

	EMIT_EVENT_0("forward_word");

	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == buffer_->length()) {
		ringIfNecessary(silent);
		return;
	}

	TextCursor pos = insertPos;

	if (flags & TailFlag) {
		for (; pos < buffer_->length(); ++pos) {
			if (!isDelimeter(buffer_->BufGetCharacter(pos))) {
				break;
			}
		}
		if (!isDelimeter(buffer_->BufGetCharacter(pos))) {
			pos = endOfWord(pos);
		}
	} else {
		if (!isDelimeter(buffer_->BufGetCharacter(pos))) {
			pos = endOfWord(pos);
		}
		for (; pos < buffer_->length(); ++pos) {
			if (!isDelimeter(buffer_->BufGetCharacter(pos))) {
				break;
			}
		}
	}

	setInsertPosition(pos);
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::forwardParagraphAP(EventFlags flags) {

	EMIT_EVENT_0("forward_paragraph");

	const TextCursor insertPos     = cursorPos_;
	static const char whiteChars[] = " \t";
	const bool silent              = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == buffer_->length()) {
		ringIfNecessary(silent);
		return;
	}

	TextCursor pos = std::min(buffer_->BufEndOfLine(insertPos) + 1, buffer_->BufEndOfBuffer());
	while (pos < buffer_->length()) {
		char c = buffer_->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (::strchr(whiteChars, c) != nullptr)
			++pos;
		else
			pos = std::min(buffer_->BufEndOfLine(pos) + 1, buffer_->BufEndOfBuffer());
	}

	setInsertPosition(std::min(pos + 1, buffer_->BufEndOfBuffer()));
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::backwardParagraphAP(EventFlags flags) {

	EMIT_EVENT_0("backward_paragraph");

	const TextCursor insertPos     = cursorPos_;
	static const char whiteChars[] = " \t";
	const bool silent              = flags & NoBellFlag;

	cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}

	TextCursor parStart = buffer_->BufStartOfLine(std::max(insertPos - 1, buffer_->BufStartOfBuffer()));
	TextCursor pos      = std::max(parStart - 2, buffer_->BufStartOfBuffer());

	while (pos > 0) {
		char c = buffer_->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (::strchr(whiteChars, c) != nullptr)
			--pos;
		else {
			parStart = buffer_->BufStartOfLine(pos);
			pos      = std::max(parStart - 2, buffer_->BufStartOfBuffer());
		}
	}
	setInsertPosition(parStart);
	checkMoveSelectionChange(flags, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::processTabAP(EventFlags flags) {

	EMIT_EVENT_0("process_tab");

	const TextBuffer::Selection &sel = buffer_->primary;
	const int emTabDist              = emulateTabs_;
	const int emTabsBeforeCursor     = emTabsBeforeCursor_;

	if (checkReadOnly()) {
		return;
	}

	cancelDrag();

	// If emulated tabs are off, just insert a tab
	if (emTabDist <= 0) {
		TextInsertAtCursor("\t", true, true);
		return;
	}

	/* Find the starting and ending indentation.  If the tab is to
	   replace an existing selection, use the start of the selection
	   instead of the cursor position as the indent.  When replacing
	   rectangular selections, tabs are automatically recalculated as
	   if the inserted text began at the start of the line */
	TextCursor insertPos = pendingSelection() ? sel.start() : cursorPos_;
	TextCursor lineStart = buffer_->BufStartOfLine(insertPos);

	if (pendingSelection() && sel.isRectangular()) {
		insertPos = buffer_->BufCountForwardDispChars(lineStart, sel.rectStart());
	}

	int64_t startIndent = buffer_->BufCountDispChars(lineStart, insertPos);
	int64_t toIndent    = startIndent + emTabDist - (startIndent % emTabDist);
	if (pendingSelection() && sel.isRectangular()) {
		toIndent -= startIndent;
		startIndent = 0;
	}

	// Allocate a buffer assuming all the inserted characters will be spaces
	std::string outStr;
	outStr.reserve(static_cast<size_t>(toIndent - startIndent));

	// Add spaces and tabs to outStr until it reaches toIndent
	auto outPtr = std::back_inserter(outStr);

	int64_t indent = startIndent;
	while (indent < toIndent) {
		const int tabWidth = TextBuffer::BufCharWidth('\t', indent, buffer_->BufGetTabDistance());
		if (buffer_->BufGetUseTabs() && tabWidth > 1 && indent + tabWidth <= toIndent) {
			*outPtr++ = '\t';
			indent += tabWidth;
		} else {
			*outPtr++ = ' ';
			indent++;
		}
	}

	// Insert the emulated tab
	TextInsertAtCursor(outStr, true, true);

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

	TextCursor insertPos = cursorPos_;

	QPoint p;
	if (positionToXY(insertPos, &p)) {
		if (pointerX < p.x() && insertPos > 0 && buffer_->BufGetCharacter(insertPos - 1) != '\n') {
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

	const TextCursor insertPos = cursorPos_;

	TextCursor endPos   = buffer_->BufEndOfLine(insertPos);
	TextCursor startPos = buffer_->BufStartOfLine(insertPos);

	buffer_->BufSelect(startPos, std::min(endPos + 1, buffer_->BufEndOfBuffer()));
	setInsertPosition(endPos);
}

void TextArea::moveDestinationAP(QMouseEvent *event) {

	// Move the cursor
	setInsertPosition(coordToPosition(event->pos()));
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

/*
** Translate window coordinates to the nearest text cursor position.
*/
TextCursor TextArea::coordToPosition(const QPoint &coord) const {
	return xyToPos(coord, PositionType::Cursor);
}

/*
** Translate window coordinates to the nearest (insert cursor or character
** cell) text position.  The parameter posType specifies how to interpret the
** position: CURSOR_POS means translate the coordinates to the nearest cursor
** position, and CHARACTER_POS means return the position of the character
** closest to (x, y).
*/
TextCursor TextArea::xyToPos(const QPoint &pos, PositionType posType) const {
	return xyToPos(pos.x(), pos.y(), posType);
}

TextCursor TextArea::xyToPos(int x, int y, PositionType posType) const {

	// Find the visible line number corresponding to the y coordinate
	const QRect viewRect = viewport()->contentsRect();
	int visLineNum       = (y - viewRect.top()) / fixedFontHeight_;

	if (visLineNum < 0) {
		return firstChar_;
	}

	if (visLineNum >= nVisibleLines_) {
		visLineNum = nVisibleLines_ - 1;
	}

	// Find the position at the start of the line
	TextCursor lineStart = lineStarts_[visLineNum];

	// If the line start was empty, return the last position in the buffer
	if (lineStart == -1) {
		return buffer_->BufEndOfBuffer();
	}

	// Get the line text and its length
	int64_t lineLen     = visLineLength(visLineNum);
	std::string lineStr = buffer_->BufGetRange(lineStart, lineStart + lineLen);

	/* Step through character positions from the beginning of the line
	   to find the character position corresponding to the x coordinate */
	int64_t xStep         = viewRect.left() - horizontalScrollBar()->value();
	int outIndex          = 0;
	const int tabDistance = buffer_->BufGetTabDistance();
	for (int64_t charIndex = 0; charIndex < lineLen; charIndex++) {

		const int charLen       = TextBuffer::BufCharWidth(lineStr[charIndex], outIndex, tabDistance);
		const int64_t charWidth = lengthToWidth(charLen);

		if (x < xStep + (posType == PositionType::Cursor ? charWidth / 2 : charWidth)) {
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
** involves character re-counting.
*/
int TextArea::offsetWrappedColumn(int row, int column) const {

	if (!continuousWrap_ || row < 0 || row > nVisibleLines_) {
		return column;
	}

	const TextCursor dispLineStart = lineStarts_[row];
	if (dispLineStart == -1) {
		return column;
	}

	const TextCursor lineStart = buffer_->BufStartOfLine(dispLineStart);
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

	setUserInteractionDetected();

	if (clickCount_ < 3 && clickPos_ == event->pos() && !clickTimerExpired_) {
		clickCount_++;
	} else {
		clickCount_ = 0;
	}

	clickPos_ = event->pos();

	clickTimerExpired_ = false;
	clickTimer_->start(QApplication::doubleClickInterval());

	switch (clickCount_) {
	case 0:
		return true;
	case 1:
		if (inDoubleClickHandler) {
			return true;
		} else {
			mouseDoubleClickEvent(event);
			return false;
		}
	case 2:
		mouseTripleClickEvent(event);
		return false;
	case 3:
		mouseQuadrupleClickEvent(event);
		return false;
	}

	return true;
}

/*
 * @brief TextArea::clearUserInteractionDetected
*/
void TextArea::clearUserInteractionDetected() {
	document_->clearUserInteractionDetected();
}

/*
 * @brief TextArea::setUserInteractionDetected
*/
void TextArea::setUserInteractionDetected() {
	document_->setUserInteractionDetected();
}

/**
 * @brief TextArea::selectAllAP
 * @param flags
 */
void TextArea::selectAllAP(EventFlags flags) {

	EMIT_EVENT_0("select_all");

	cancelDrag();
	buffer_->BufSelectAll();
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

	const TextBuffer::Selection *sel = &buffer_->primary;
	TextCursor anchor;
	int rectAnchor;
	TextCursor anchorLineStart;
	int row;
	int column;

	// Find the new anchor point for the rest of this drag operation
	TextCursor newPos = coordToPosition(event->pos());
	coordToUnconstrainedPosition(event->pos(), &row, &column);
	column = offsetWrappedColumn(row, column);
	if (sel->hasSelection()) {
		if (sel->isRectangular()) {

			rectAnchor      = column < (sel->rectEnd() + sel->rectStart()) / 2 ? sel->rectEnd() : sel->rectStart();
			anchorLineStart = buffer_->BufStartOfLine(newPos < (sel->end() + to_integer(sel->start())) / 2 ? sel->end() : sel->start());
			anchor          = buffer_->BufCountForwardDispChars(anchorLineStart, rectAnchor);

		} else {
			if (std::abs(newPos - sel->start()) < std::abs(newPos - sel->end())) {
				anchor = sel->end();
			} else {
				anchor = sel->start();
			}

			anchorLineStart = buffer_->BufStartOfLine(anchor);
			rectAnchor      = buffer_->BufCountDispChars(anchorLineStart, anchor);
		}
	} else {
		anchor          = cursorPos_;
		anchorLineStart = buffer_->BufStartOfLine(anchor);
		rectAnchor      = buffer_->BufCountDispChars(anchorLineStart, anchor);
	}
	anchor_     = anchor;
	rectAnchor_ = rectAnchor;

	// Make the new selection
	if (flags & RectFlag) {
		buffer_->BufRectSelect(
			buffer_->BufStartOfLine(std::min(anchor, newPos)),
			buffer_->BufEndOfLine(std::max(anchor, newPos)),
			std::min(rectAnchor, column),
			std::max(rectAnchor, column));
	} else {
		buffer_->BufSelect(std::minmax(anchor, newPos));
	}

	/* Never mind the motion threshold, go right to dragging since
	   extend-start is unambiguously the start of a selection */
	dragState_ = PRIMARY_DRAG;

	// Don't do by-word or by-line adjustment, just by character
	clickCount_ = 0;

	// Move the cursor
	setInsertPosition(newPos);
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
	const bool rectDrag  = flags & RectFlag;

	// Make sure the proper initialization was done on mouse down
	if (dragState != PRIMARY_DRAG && dragState != PRIMARY_CLICKED && dragState != PRIMARY_RECT_DRAG) {
		return;
	}

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (dragState_ == PRIMARY_CLICKED) {

		const QPoint point = event->pos() - btnDownCoord_;
		if (point.manhattanLength() > QApplication::startDragDistance()) {
			dragState_ = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
		} else {
			return;
		}
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	dragState_ = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;

	/* Record the new position for the auto-scrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(event->pos());

	// Adjust the selection and move the cursor
	adjustSelection(event->pos());
}

/*
** Adjust the selection as the mouse is dragged to position: (x, y).
*/
void TextArea::adjustSelection(const QPoint &coord) {

	TextCursor newPos = coordToPosition(coord);

	// Adjust the selection
	if (dragState_ == PRIMARY_RECT_DRAG) {
		int row;
		int column;

		coordToUnconstrainedPosition(coord, &row, &column);
		column                    = offsetWrappedColumn(row, column);
		const int startCol        = std::min(rectAnchor_, column);
		const int endCol          = std::max(rectAnchor_, column);
		const TextCursor startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		const TextCursor endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));
		buffer_->BufRectSelect(startPos, endPos, startCol, endCol);

	} else if (clickCount_ == 1) { // multiClickState_ == ONE_CLICK) {
		const TextCursor startPos = startOfWord(std::min(anchor_, newPos));
		const TextCursor endPos   = endOfWord(std::max(anchor_, newPos));
		buffer_->BufSelect(startPos, endPos);
		newPos = (newPos < anchor_) ? startPos : endPos;

	} else if (clickCount_ == 2) { // multiClickState_ == TWO_CLICKS) {
		const TextCursor startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		const TextCursor endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));
		buffer_->BufSelect(startPos, std::min(endPos + 1, buffer_->BufEndOfBuffer()));
		newPos = newPos < anchor_ ? startPos : endPos;
	} else {
		buffer_->BufSelect(anchor_, newPos);
	}

	// Move the cursor
	setInsertPosition(newPos);
	callCursorMovementCBs();
}

/*
** Given a new mouse pointer location, pass the position on to the
** auto-scroll timer routine, and make sure the timer is on when it's
** needed and off when it's not.
*/
void TextArea::checkAutoScroll(const QPoint &coord) {

	const QRect viewRect = viewport()->contentsRect();
	const bool inWindow  = viewRect.contains(coord);

	if (inWindow) {
		autoScrollTimer_->stop();
		return;
	}

	// If the timer is not already started, start it
	autoScrollTimer_->start(0);

	// Pass on the newest mouse location to the auto-scroll routine
	mouseCoord_ = coord;
}

void TextArea::deleteToStartOfLineAP(EventFlags flags) {

	EMIT_EVENT_0("delete_to_start_of_line");

	const TextCursor insertPos = cursorPos_;
	TextCursor lineStart;

	bool silent = (flags & NoBellFlag);

	if (flags & WrapFlag) {
		lineStart = startOfLine(insertPos);
	} else {
		lineStart = buffer_->BufStartOfLine(insertPos);
	}

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

	buffer_->BufRemove(lineStart, insertPos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::mousePanAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("mouse_pan");

	const int lineHeight = fixedFontHeight_;

	switch (dragState_) {
	case MOUSE_PAN:
		verticalScrollBar()->setValue((btnDownCoord_.y() - event->y() + lineHeight / 2) / lineHeight);
		horizontalScrollBar()->setValue(btnDownCoord_.x() - event->x());
		break;
	case NOT_CLICKED: {
		const int topLineNum  = verticalScrollBar()->value();
		const int horizOffset = horizontalScrollBar()->value();

		btnDownCoord_ = QPoint(event->x() + horizOffset, event->y() + topLineNum * lineHeight);
		dragState_    = MOUSE_PAN;

		viewport()->setCursor(Qt::SizeAllCursor);
		break;
	}
	default:
		cancelDrag();
		break;
	}
}

void TextArea::copyToOrEndDragAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("copy_to_end_drag");

	DragStates dragState = dragState_;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		copyToAP(event, flags | SuppressRecording);
		return;
	}

	finishBlockDrag();
}

void TextArea::copyToAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("copy_to");

	DragStates dragState                   = dragState_;
	const TextBuffer::Selection &secondary = buffer_->secondary;
	const TextBuffer::Selection &primary   = buffer_->primary;
	bool rectangular                       = secondary.isRectangular();

	endDrag();
	if (!((dragState == SECONDARY_DRAG && secondary.hasSelection()) || (dragState == SECONDARY_RECT_DRAG && secondary.hasSelection()) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED)) {
		return;
	}

	if (checkReadOnly()) {
		buffer_->BufSecondaryUnselect();
		return;
	}

	if (secondary.hasSelection()) {

		TextDBlankCursor();
		std::string textToCopy = buffer_->BufGetSecSelectText();
		if (primary.hasSelection() && rectangular) {
			buffer_->BufReplaceSelected(textToCopy);
			setInsertPosition(buffer_->BufCursorPosHint());
		} else if (rectangular) {
			const TextCursor insertPos = cursorPos_;
			const TextCursor lineStart = buffer_->BufStartOfLine(insertPos);
			const int64_t column       = buffer_->BufCountDispChars(lineStart, insertPos);
			buffer_->BufInsertCol(column, lineStart, textToCopy, nullptr, nullptr);
			setInsertPosition(buffer_->BufCursorPosHint());
		} else {
			TextInsertAtCursor(textToCopy, true, autoWrapPastedText_);
		}

		buffer_->BufSecondaryUnselect();
		unblankCursor();

	} else if (primary.hasSelection()) {
		std::string textToCopy = buffer_->BufGetSelectionText();
		setInsertPosition(coordToPosition(event->pos()));
		TextInsertAtCursor(textToCopy, false, autoWrapPastedText_);
	} else {
		setInsertPosition(coordToPosition(event->pos()));
		insertPrimarySelection(false);
	}
}

/*
** Complete a block text drag operation
*/
void TextArea::finishBlockDrag() {

	auto modRangeStart = TextCursor(-1);
	TextCursor origModRangeEnd;
	TextCursor bufModRangeEnd;

	/* Find the changed region of the buffer, covering both the deletion
	   of the selected text at the drag start position, and insertion at
	   the drag destination */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragSourceDeletePos_, dragSourceInserted_, dragSourceDeleted_);
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd, dragInsertPos_, dragInserted_, dragDeleted_);

	// Get the original (pre-modified) range of text from saved backup buffer
	std::string deletedText = dragOrigBuf_->BufGetRange(modRangeStart, origModRangeEnd);

	// Free the backup buffer
	dragOrigBuf_ = nullptr;

	// Return to normal drag state
	dragState_ = NOT_CLICKED;

	// Call finish-drag callback
	DragEndEvent endStruct;
	endStruct.startPos       = modRangeStart;
	endStruct.nCharsDeleted  = origModRangeEnd - modRangeStart;
	endStruct.nCharsInserted = bufModRangeEnd - modRangeStart;
	endStruct.deletedText    = deletedText;

	for (auto &c : dragEndCallbacks_) {
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
	if (!buffer_->primary.hasSelection() || !inSelection(event->pos())) {
		secondaryStartAP(event, flags | SuppressRecording);
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
bool TextArea::inSelection(const QPoint &p) const {

	TextCursor pos = xyToPos(p, PositionType::Character);

	int row;
	int column;
	xyToUnconstrainedPos(p, &row, &column, PositionType::Character);

	if (buffer_->primary.rangeTouchesRectSel(firstChar_, lastChar_)) {
		column = offsetWrappedColumn(row, column);
	}

	return buffer_->primary.inSelection(pos, buffer_->BufStartOfLine(pos), column);
}

/**
 * @brief TextArea::secondaryStartAP
 * @param event
 * @param flags
 */
void TextArea::secondaryStartAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("secondary_start");

	const TextBuffer::Selection &sel = buffer_->secondary;

	// Find the new anchor point and make the new selection
	TextCursor pos = coordToPosition(event->pos());
	if (sel.hasSelection()) {
		TextCursor anchor;
		if (std::abs(pos - sel.start()) < std::abs(pos - sel.end())) {
			anchor = sel.end();
		} else {
			anchor = sel.start();
		}
		buffer_->BufSecondarySelect(anchor, pos);
	} else {
		TextCursor anchor = pos;
		Q_UNUSED(anchor)
	}

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   selection, (and where the selection began) */
	btnDownCoord_ = event->pos();
	anchor_       = pos;

	int row;
	int column;
	coordToUnconstrainedPosition(event->pos(), &row, &column);

	column      = offsetWrappedColumn(row, column);
	rectAnchor_ = column;
	dragState_  = SECONDARY_CLICKED;
}

/**
 * @brief TextArea::secondaryOrDragAdjustAP
 * @param event
 * @param flags
 */
void TextArea::secondaryOrDragAdjustAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("secondary_or_drag_adjust");

	const DragStates dragState = dragState_;

	/* Only dragging of blocks of text is handled in this action proc.
	   Otherwise, defer to secondaryAdjust to handle the rest */
	if (dragState != CLICKED_IN_SELECTION && dragState != PRIMARY_BLOCK_DRAG) {
		secondaryAdjustAP(event, flags | SuppressRecording);
		return;
	}

	/* Decide whether the mouse has moved far enough from the
	   initial mouse down to be considered a drag */
	if (dragState_ == CLICKED_IN_SELECTION) {
		const QPoint point = event->pos() - btnDownCoord_;
		if (point.manhattanLength() > QApplication::startDragDistance()) {
			beginBlockDrag();
		} else {
			return;
		}
	}

	/* Record the new position for the auto-scrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(event->pos());

	// Adjust the selection
	blockDragSelection(
		event->pos(),
		(flags & OverlayFlag) ? ((flags & CopyFlag) ? DRAG_OVERLAY_COPY : DRAG_OVERLAY_MOVE) : ((flags & CopyFlag) ? DRAG_COPY : DRAG_MOVE));
}

void TextArea::secondaryAdjustAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("secondary_adjust");

	const DragStates dragState = dragState_;
	const bool rectDrag        = flags & RectFlag;

	// Make sure the proper initialization was done on mouse down
	if (dragState != SECONDARY_DRAG && dragState != SECONDARY_RECT_DRAG && dragState != SECONDARY_CLICKED) {
		return;
	}

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (dragState_ == SECONDARY_CLICKED) {
		const QPoint point = event->pos() - btnDownCoord_;
		if (point.manhattanLength() > QApplication::startDragDistance()) {
			dragState_ = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;
		} else {
			return;
		}
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	dragState_ = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;

	/* Record the new position for the auto-scrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(event->pos());

	// Adjust the selection
	adjustSecondarySelection(event->pos());
}

/*
** Start the process of dragging the current primary-selected text across
** the window (move by dragging, as opposed to dragging to create the
** selection)
*/
void TextArea::beginBlockDrag() {

	const QRect viewRect             = viewport()->contentsRect();
	const int fontHeight             = fixedFontHeight_;
	const int fontWidth              = fixedFontWidth_;
	const TextBuffer::Selection &sel = buffer_->primary;

	/* Save a copy of the whole text buffer as a backup, and for
	   deriving changes */
	dragOrigBuf_ = std::make_unique<TextBuffer>();
	dragOrigBuf_->BufSetSyncXSelection(false);
	dragOrigBuf_->BufSetTabDistance(buffer_->BufGetTabDistance(), true);
	dragOrigBuf_->BufSetUseTabs(buffer_->BufGetUseTabs());

	dragOrigBuf_->BufSetAll(buffer_->BufGetAll());

	if (sel.isRectangular()) {
		dragOrigBuf_->BufRectSelect(sel.start(), sel.end(), sel.rectStart(), sel.rectEnd());
	} else {
		dragOrigBuf_->BufSelect(sel.start(), sel.end());
	}

	/* Record the mouse pointer offsets from the top left corner of the
	   selection (the position where text will actually be inserted In dragging
	   non-rectangular selections)  */
	if (sel.isRectangular()) {
		dragXOffset_ = btnDownCoord_.x() + horizontalScrollBar()->value() - viewRect.left() - sel.rectStart() * fontWidth;
	} else {
		int x;
		int y;
		if (!positionToXY(sel.start(), &x, &y)) {
			x = buffer_->BufCountDispChars(startOfLine(sel.start()), sel.start()) * fontWidth + viewRect.left() - horizontalScrollBar()->value();
		}
		dragXOffset_ = btnDownCoord_.x() - x;
	}

	const TextCursor mousePos = coordToPosition(btnDownCoord_);
	const int64_t nLines      = buffer_->BufCountLines(sel.start(), mousePos);

	dragYOffset_ = nLines * fontHeight + (((btnDownCoord_.y() - viewRect.top()) % fontHeight) - fontHeight / 2);
	dragNLines_  = buffer_->BufCountLines(sel.start(), sel.end());

	/* Record the current drag insert position and the information for
	   undoing the fictional insert of the selection in its new position */
	dragInsertPos_ = sel.start();
	dragInserted_  = sel.end() - sel.start();
	if (sel.isRectangular()) {
		TextBuffer testBuf;
		testBuf.BufSetSyncXSelection(false);

		std::string testText = buffer_->BufGetRange(sel.start(), sel.end());
		testBuf.BufSetTabDistance(buffer_->BufGetTabDistance(), true);
		testBuf.BufSetUseTabs(buffer_->BufGetUseTabs());
		testBuf.BufSetAll(testText);

		testBuf.BufRemoveRect(buffer_->BufStartOfBuffer(), buffer_->BufStartOfBuffer() + (sel.end() - sel.start()), sel.rectStart(), sel.rectEnd());
		dragDeleted_   = testBuf.length();
		dragRectStart_ = sel.rectStart();
	} else {
		dragDeleted_   = 0;
		dragRectStart_ = 0;
	}

	dragType_            = DRAG_MOVE;
	dragSourceDeletePos_ = sel.start();
	dragSourceInserted_  = dragDeleted_;
	dragSourceDeleted_   = dragInserted_;

	/* For non-rectangular selections, fill in the rectangular information in
	   the selection for overlay mode drags which are done rectangularly */
	if (!sel.isRectangular()) {
		TextCursor lineStart = buffer_->BufStartOfLine(sel.start());
		if (dragNLines_ == 0) {
			dragOrigBuf_->primary.rectStart_ = buffer_->BufCountDispChars(lineStart, sel.start());
			dragOrigBuf_->primary.rectEnd_   = buffer_->BufCountDispChars(lineStart, sel.end());
		} else {
			TextCursor lineEnd = buffer_->BufGetCharacter(sel.end() - 1) == '\n' ? sel.end() - 1 : sel.end();
			findTextMargins(buffer_, lineStart, lineEnd, &dragOrigBuf_->primary.rectStart_, &dragOrigBuf_->primary.rectEnd_);
		}
	}

	// Set the drag state to announce an ongoing block-drag
	dragState_ = PRIMARY_BLOCK_DRAG;

	// Call the callback announcing the start of a block drag
	for (auto &c : dragStartCallbacks_) {
		c.first(this, c.second);
	}
}

/*
** Reposition the primary-selected text that is being dragged as a block
** for a new mouse position of (x, y)
*/
void TextArea::blockDragSelection(const QPoint &pos, BlockDragTypes dragType) {

	const int fontHeight                 = fixedFontHeight_;
	const int fontWidth                  = fixedFontWidth_;
	auto &origBuf                        = dragOrigBuf_;
	int dragXOffset                      = dragXOffset_;
	const TextBuffer::Selection &origSel = origBuf->primary;
	bool rectangular                     = origSel.isRectangular();
	BlockDragTypes oldDragType           = dragType_;
	int64_t nLines                       = dragNLines_;
	auto modRangeStart                   = TextCursor(-1);
	auto tempModRangeEnd                 = TextCursor(-1);
	auto bufModRangeEnd                  = TextCursor(-1);
	TextCursor insStart;
	TextCursor referencePos;
	TextCursor origSelLineEnd;
	TextCursor sourceDeletePos;
	int64_t insertInserted;
	int64_t insertDeleted;
	int64_t sourceInserted;
	int64_t sourceDeleted;
	int64_t insRectStart;
	int insLineNum;
	int referenceLine;

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
	   operation will do everything.  This is done using a routine called
	   trackModifyRange which tracks expanding ranges of changes in the two
	   buffers in modRangeStart, tempModRangeEnd, and bufModRangeEnd. */

	/* Create a temporary buffer for accumulating changes which will
	   eventually be replaced in the real buffer.  Load the buffer with the
	   range of characters which might be modified in this drag step
	   (this could be tighter, but hopefully it's not too slow) */
	TextBuffer tempBuf;
	tempBuf.BufSetSyncXSelection(false);
	tempBuf.BufSetTabDistance(buffer_->BufGetTabDistance(), false);
	tempBuf.BufSetUseTabs(buffer_->BufGetUseTabs());

	const TextCursor tempStart = std::min({dragInsertPos_, origSel.start(), buffer_->BufCountBackwardNLines(firstChar_, nLines + 2)});
	const TextCursor tempEnd   = buffer_->BufCountForwardNLines(std::max({dragInsertPos_, origSel.start(), lastChar_}), nLines + 2) + (origSel.end() - origSel.start());

	const std::string text = origBuf->BufGetRange(tempStart, tempEnd);
	tempBuf.BufSetAll(text);

	// If the drag type is USE_LAST, use the last dragType applied
	if (dragType == USE_LAST) {
		dragType = dragType_;
	}

	const bool overlay = (dragType == DRAG_OVERLAY_MOVE) || (dragType == DRAG_OVERLAY_COPY);

	/* Overlay mode uses rectangular selections whether or not the original
	   was rectangular.  To use a plain selection as if it were rectangular,
	   the start and end positions need to be moved to the line boundaries
	   and trailing newlines must be excluded */
	TextCursor origSelLineStart = origBuf->BufStartOfLine(origSel.start());

	if (!rectangular && origBuf->BufGetCharacter(origSel.end() - 1) == '\n') {
		origSelLineEnd = origSel.end() - 1;
	} else {
		origSelLineEnd = origBuf->BufEndOfLine(origSel.end());
	}

	if (!rectangular && overlay && nLines != 0) {
		dragXOffset -= fontWidth * (origSel.rectStart() - (origSel.start() - origSelLineStart));
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
			const int64_t prevLen    = tempBuf.length();
			const int64_t origSelLen = origSelLineEnd - origSelLineStart;

			if (overlay) {
				tempBuf.BufClearRect(TextCursor(origSelLineStart - tempStart), TextCursor(origSelLineEnd - tempStart), origSel.rectStart(), origSel.rectEnd());
			} else {
				tempBuf.BufRemoveRect(TextCursor(origSelLineStart - tempStart), TextCursor(origSelLineEnd - tempStart), origSel.rectStart(), origSel.rectEnd());
			}

			sourceDeletePos = origSelLineStart;
			sourceInserted  = origSelLen - prevLen + tempBuf.length();
			sourceDeleted   = origSelLen;
		} else {
			tempBuf.BufRemove(TextCursor(origSel.start() - tempStart), TextCursor(origSel.end() - tempStart));
			sourceDeletePos = origSel.start();
			sourceInserted  = 0;
			sourceDeleted   = origSel.end() - origSel.start();
		}

		if (dragType != oldDragType) {
			trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, sourceDeletePos, sourceInserted, sourceDeleted);
		}

	} else {
		sourceDeletePos = buffer_->BufStartOfBuffer();
		sourceInserted  = 0;
		sourceDeleted   = 0;
	}

	/* Expand the modified-range to include undoing the insert from the last
	   call. */
	trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd, dragInsertPos_, dragInserted_, dragDeleted_);

	/* Find the line number and column of the insert position.  Note that in
	   continuous wrap mode, these must be calculated as if the text were
	   not wrapped */
	int row;
	int column;
	coordToUnconstrainedPosition(
		QPoint(
			std::max(0, pos.x() - dragXOffset),
			std::max(0, pos.y() - (dragYOffset_ % fontHeight))),
		&row,
		&column);

	column     = offsetWrappedColumn(row, column);
	row        = offsetWrappedRow(row);
	insLineNum = row + topLineNum_ - dragYOffset_ / fontHeight;

	/* find a common point of reference between the two buffers, from which
	   the insert position line number can be translated to a position */
	if (firstChar_ > modRangeStart) {
		referenceLine = topLineNum_ - buffer_->BufCountLines(modRangeStart, firstChar_);
		referencePos  = modRangeStart;
	} else {
		referencePos  = firstChar_;
		referenceLine = topLineNum_;
	}

	/* find the position associated with the start of the new line in the
	   temporary buffer */
	TextCursor insLineStart = findRelativeLineStart(&tempBuf, TextCursor(referencePos - tempStart), referenceLine, insLineNum) + to_integer(tempStart);

	if (insLineStart - tempStart == tempBuf.length()) {
		insLineStart = tempBuf.BufStartOfLine(TextCursor(insLineStart - tempStart)) + to_integer(tempStart);
	}

	// Find the actual insert position
	if (rectangular || overlay) {
		insStart     = insLineStart;
		insRectStart = column;
	} else {
		insStart     = tempBuf.BufCountForwardDispChars(TextCursor(insLineStart - tempStart), column) + to_integer(tempStart);
		insRectStart = 0;
	}

	/* If the position is the same as last time, don't bother drawing (it
	   would be nice if this decision could be made earlier) */
	if (insStart == dragInsertPos_ && insRectStart == dragRectStart_ && dragType == oldDragType) {
		return;
	}

	// Do the insert in the temporary buffer
	if (rectangular || overlay) {

		std::string insText = origBuf->BufGetTextInRect(origSelLineStart, origSelLineEnd, origSel.rectStart(), origSel.rectEnd());
		if (overlay) {
			tempBuf.BufOverlayRect(TextCursor(insStart - tempStart), insRectStart, insRectStart + origSel.rectEnd() - origSel.rectStart(), insText, &insertInserted, &insertDeleted);
		} else {
			tempBuf.BufInsertCol(insRectStart, TextCursor(insStart - tempStart), insText, &insertInserted, &insertDeleted);
		}
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, insertInserted, insertDeleted);

	} else {
		std::string insText = origBuf->BufGetSelectionText();
		tempBuf.BufInsert(TextCursor(insStart - tempStart), insText);
		trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd, insStart, origSel.end() - origSel.start(), 0);
		insertInserted = origSel.end() - origSel.start();
		insertDeleted  = 0;
	}

	// Make the changes in the real buffer
	std::string repText = tempBuf.BufGetRange(TextCursor(modRangeStart - tempStart), TextCursor(tempModRangeEnd - tempStart));

	TextDBlankCursor();
	buffer_->BufReplace(modRangeStart, bufModRangeEnd, repText);

	// Store the necessary information for undoing this step
	dragInsertPos_       = insStart;
	dragRectStart_       = insRectStart;
	dragInserted_        = insertInserted;
	dragDeleted_         = insertDeleted;
	dragSourceDeletePos_ = sourceDeletePos;
	dragSourceInserted_  = sourceInserted;
	dragSourceDeleted_   = sourceDeleted;
	dragType_            = dragType;

	// as an optimization, if we are moving a block but not combining it with
	// the new location's content, then the selected text is functionally the
	// same, so we don't need to constantly synchronize the X selection.
	bool prev = true;
	if (dragType == DRAG_OVERLAY_MOVE) {
		prev = buffer_->BufSetSyncXSelection(false);
	}

	// Reset the selection and cursor position
	if (rectangular || overlay) {
		int64_t insRectEnd = insRectStart + origSel.rectEnd() - origSel.rectStart();
		buffer_->BufRectSelect(insStart, insStart + insertInserted, insRectStart, insRectEnd);
		setInsertPosition(buffer_->BufCountForwardDispChars(buffer_->BufCountForwardNLines(insStart, dragNLines_), insRectEnd));
	} else {
		buffer_->BufSelect(insStart, insStart + (origSel.end() - origSel.start()));
		setInsertPosition(insStart + (origSel.end() - origSel.start()));
	}

	if (dragType == DRAG_OVERLAY_MOVE) {
		buffer_->BufSetSyncXSelection(prev);
	}

	unblankCursor();
	callMovedCBs();
	emTabsBeforeCursor_ = 0;
}

void TextArea::callMovedCBs() {
	for (auto &c : movedCallbacks_) {
		c.first(this, c.second);
	}
}

void TextArea::adjustSecondarySelection(const QPoint &coord) {

	TextCursor newPos = coordToPosition(coord);

	if (dragState_ == SECONDARY_RECT_DRAG) {

		int row;
		int column;
		coordToUnconstrainedPosition(coord, &row, &column);
		column                    = offsetWrappedColumn(row, column);
		const int startCol        = std::min(rectAnchor_, column);
		const int endCol          = std::max(rectAnchor_, column);
		const TextCursor startPos = buffer_->BufStartOfLine(std::min(anchor_, newPos));
		const TextCursor endPos   = buffer_->BufEndOfLine(std::max(anchor_, newPos));
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
int TextArea::offsetWrappedRow(int row) const {
	if (!continuousWrap_ || row < 0 || row > nVisibleLines_) {
		return row;
	}

	return buffer_->BufCountLines(firstChar_, lineStarts_[row]);
}

void TextArea::setWordDelimiters(const std::string &delimiters) {

	// add mandatory delimiters blank, tab, and newline to the list
	delimiters_ = " \t\n";
	delimiters_ += delimiters;
}

void TextArea::setAutoShowInsertPos(bool value) {
	autoShowInsertPos_ = value;
}

void TextArea::setEmulateTabs(int value) {
	emulateTabs_ = value;
}

void TextArea::setWrapMargin(int value) {
	setWrapMode(continuousWrap_, value);
}

void TextArea::setLineNumCols(int value) {
	lineNumCols_ = value;
	resetAbsLineNum();

	if (value == 0) {
		setLineNumberAreaWidth(0);
	} else {
		setLineNumberAreaWidth((fixedFontWidth_ * lineNumCols_) + (LineNumberArea::Padding * 2));
	}
}

/**
 * @brief TextArea::lineNumberAreaWidth
 * @return
 */
int TextArea::lineNumberAreaWidth() const {
	return lineNumberArea_->width();
}

/*
** Define area for drawing line numbers. A width of 0 disables line
** number drawing.
*/
void TextArea::setLineNumberAreaWidth(int lineNumWidth) {
	lineNumberArea_->resize(lineNumWidth, lineNumberArea_->height());
	setViewportMargins(lineNumberArea_->width(), 0, 0, 0);
	lineNumberArea_->update();
}

void TextArea::setWrapMode(bool wrap, int wrapMargin) {

	continuousWrap_ = wrap;
	wrapMargin_     = wrapMargin;

	// wrapping can change change the total number of lines, re-count
	nBufferLines_ = countLines(buffer_->BufStartOfBuffer(), buffer_->BufEndOfBuffer(), /*startPosIsLineStart=*/true);

	/* changing wrap margins wrap or changing from wrapped mode to non-wrapped
	 * can leave the character at the top no longer at a line start, and/or
	 * change the line number */
	firstChar_  = startOfLine(firstChar_);
	topLineNum_ = countLines(buffer_->BufStartOfBuffer(), firstChar_, /*startPosIsLineStart=*/true) + 1;
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
	viewport()->update();
}

void TextArea::deleteToEndOfLineAP(EventFlags flags) {

	EMIT_EVENT_0("delete_to_end_of_line");

	const TextCursor insertPos = cursorPos_;
	TextCursor lineEnd;

	const bool silent = flags & NoBellFlag;
	if (flags & AbsoluteFlag) {
		lineEnd = buffer_->BufEndOfLine(insertPos);
	} else {
		lineEnd = endOfLine(insertPos, /*startPosIsLineStart=*/false);
	}

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
	buffer_->BufRemove(insertPos, lineEnd);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::cutPrimaryAP(EventFlags flags) {

	EMIT_EVENT_0("cut_primary");

	const TextBuffer::Selection &primary = buffer_->primary;

	const bool rectangular = flags & RectFlag;
	TextCursor insertPos;

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (primary.hasSelection() && rectangular) {
		const std::string textToCopy = buffer_->BufGetSelectionText();
		insertPos                    = cursorPos_;
		const int64_t col            = buffer_->BufCountDispChars(buffer_->BufStartOfLine(insertPos), insertPos);
		buffer_->BufInsertCol(col, insertPos, textToCopy, nullptr, nullptr);
		setInsertPosition(buffer_->BufCursorPosHint());

		buffer_->BufRemoveSelected();
		checkAutoShowInsertPos();
	} else if (primary.hasSelection()) {
		const std::string textToCopy = buffer_->BufGetSelectionText();
		insertPos                    = cursorPos_;
		buffer_->BufInsert(insertPos, textToCopy);
		setInsertPosition(insertPos + textToCopy.size());

		buffer_->BufRemoveSelected();
		checkAutoShowInsertPos();
	} else if (rectangular) {
		if (!positionToXY(cursorPos_, &btnDownCoord_)) {
			return; // shouldn't happen
		}
		movePrimarySelection(true);
	} else {
		movePrimarySelection(false);
	}
}

/*
** Insert the contents of the PRIMARY selection at the cursor position and
** deletes the contents of the selection in its current owner
** (if the selection owner supports DELETE targets).
*/
void TextArea::movePrimarySelection(bool isColumnar) {
	Q_UNUSED(isColumnar)

	// TODO(eteran): implement MovePrimarySelection
	qWarning("MovePrimarySelection is not implemented");
}

void TextArea::moveToOrEndDragAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("move_to_or_end_drag");

	DragStates dragState = dragState_;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		moveToAP(event, flags | SuppressRecording);
		return;
	}

	finishBlockDrag();
}

void TextArea::moveToAP(QMouseEvent *event, EventFlags flags) {

	EMIT_EVENT_0("move_to");

	DragStates dragState = dragState_;

	const TextBuffer::Selection &secondary = buffer_->secondary;
	const TextBuffer::Selection &primary   = buffer_->primary;
	const bool rectangular                 = secondary.isRectangular();

	endDrag();

	if (!((dragState == SECONDARY_DRAG && secondary.hasSelection()) || (dragState == SECONDARY_RECT_DRAG && secondary.hasSelection()) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED)) {
		return;
	}

	if (checkReadOnly()) {
		buffer_->BufSecondaryUnselect();
		return;
	}

	if (secondary.hasSelection()) {

		const std::string textToCopy = buffer_->BufGetSecSelectText();
		if (primary.hasSelection() && rectangular) {
			buffer_->BufReplaceSelected(textToCopy);
			setInsertPosition(buffer_->BufCursorPosHint());
		} else if (rectangular) {
			const TextCursor insertPos = cursorPos_;
			const TextCursor lineStart = buffer_->BufStartOfLine(insertPos);
			const int64_t column       = buffer_->BufCountDispChars(lineStart, insertPos);

			buffer_->BufInsertCol(column, lineStart, textToCopy, nullptr, nullptr);
			setInsertPosition(buffer_->BufCursorPosHint());
		} else {
			TextInsertAtCursor(textToCopy, true, autoWrapPastedText_);
		}

		buffer_->BufRemoveSecSelect();
		buffer_->BufSecondaryUnselect();

	} else if (primary.hasSelection()) {
		const std::string textToCopy = buffer_->BufGetRange(primary.start(), primary.end());
		setInsertPosition(coordToPosition(event->pos()));
		TextInsertAtCursor(textToCopy, false, autoWrapPastedText_);

		buffer_->BufRemoveSelected();
		buffer_->BufUnselect();
	} else {
		setInsertPosition(coordToPosition(event->pos()));
		movePrimarySelection(false);
	}
}

void TextArea::exchangeAP(QMouseEvent *event, EventFlags flags) {

	Q_UNUSED(event)
	EMIT_EVENT_0("exchange");

	const TextBuffer::Selection secondary = buffer_->secondary;
	const TextBuffer::Selection primary   = buffer_->primary;

	DragStates dragState = dragState_; // save before endDrag
	const bool silent    = flags & NoBellFlag;

	endDrag();
	if (checkReadOnly())
		return;

	/* If there's no secondary selection here, or the primary and secondary
	   selection overlap, just beep and return */
	if (!secondary.hasSelection() || (primary.hasSelection() && ((primary.start() <= secondary.start() && primary.end() > secondary.start()) || (secondary.start() <= primary.start() && secondary.end() > primary.start())))) {
		buffer_->BufSecondaryUnselect();
		ringIfNecessary(silent);
		/* If there's no secondary selection, but the primary selection is
		   being dragged, we must not forget to finish the dragging.
		   Otherwise, modifications aren't recorded. */
		if (dragState == PRIMARY_BLOCK_DRAG) {
			finishBlockDrag();
		}
		return;
	}

	// if the primary selection is in another widget, use selection routines
	if (!primary.hasSelection()) {
		exchangeSelections();
		return;
	}

	// Both primary and secondary are in this widget, do the exchange here
	std::string primaryText = buffer_->BufGetSelectionText();
	std::string secText     = buffer_->BufGetSecSelectText();

	const bool secWasRect = secondary.isRectangular();
	buffer_->BufReplaceSecSelect(primaryText);

	const TextCursor newPrimaryStart = primary.start();
	buffer_->BufReplaceSelected(secText);

	const TextCursor newPrimaryEnd = newPrimaryStart + secText.size();

	buffer_->BufSecondaryUnselect();
	if (secWasRect) {
		setInsertPosition(buffer_->BufCursorPosHint());
	} else {
		buffer_->BufSelect(newPrimaryStart, newPrimaryEnd);
		setInsertPosition(newPrimaryEnd);
	}
	checkAutoShowInsertPos();
}

/*
** Exchange Primary and secondary selections (to be called by the widget
** with the secondary selection)
*/
void TextArea::exchangeSelections() {

	if (!buffer_->secondary.hasSelection()) {
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
int TextArea::getAbsTopLineNum() const {

	if (!continuousWrap_) {
		return topLineNum_;
	}

	if (maintainingAbsTopLineNum()) {
		return absTopLineNum_;
	}

	return 0;
}

QColor TextArea::getForegroundColor() const {
	return palette().color(QPalette::Text);
}

QColor TextArea::getBackgroundColor() const {
	return palette().color(QPalette::Base);
}

void TextArea::setReadOnly(bool value) {
	readOnly_ = value;
}

void TextArea::setOverstrike(bool value) {

	// Only need to do anything if the value of overstrike has changed
	if (overstrike_ != value) {
		overstrike_ = value;

		switch (cursorStyle_) {
		case CursorStyles::Block:
			setCursorStyle(CursorStyles::Normal);
			break;
		case CursorStyles::Normal:
			setCursorStyle(CursorStyles::Block);
			break;
		default:
			break;
		}
	}
}

void TextArea::setCursorVPadding(int value) {
	cursorVPadding_ = value;
}

void TextArea::setFont(const QFont &font) {

	font_ = font;
	updateFontMetrics(font);

	// force recalculation of font related parameters
	handleResize(/*widthChanged=*/false);

	// force a recalc of the line numbers
	setLineNumCols(getLineNumCols());

	horizontalScrollBar()->setSingleStep(fixedFontWidth_);

	viewport()->update();
}

int TextArea::getLineNumCols() const {
	return lineNumCols_;
}

void TextArea::setAutoWrap(bool value) {
	autoWrap_ = value;
}

void TextArea::setContinuousWrap(bool value) {
	setWrapMode(value, wrapMargin_);
}

void TextArea::setAutoIndent(bool value) {
	autoIndent_ = value;
}

void TextArea::setSmartIndent(bool value) {
	smartIndent_ = value;
}

void TextArea::pageLeftAP(EventFlags flags) {

	EMIT_EVENT_0("page_left");

	const QRect viewRect       = viewport()->contentsRect();
	const TextCursor insertPos = cursorPos_;
	const bool silent          = flags & NoBellFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) {
		if (horizontalScrollBar()->value() == 0) {
			ringIfNecessary(silent);
			return;
		}

		horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
	} else {
		TextCursor lineStartPos = buffer_->BufStartOfLine(insertPos);
		if (insertPos == lineStartPos && horizontalScrollBar()->value() == 0) {
			ringIfNecessary(silent);
			return;
		}
		const int indent = buffer_->BufCountDispChars(lineStartPos, insertPos);
		TextCursor pos   = buffer_->BufCountForwardDispChars(lineStartPos, std::max(0, indent - viewRect.width() / fixedFontWidth_));
		setInsertPosition(pos);

		horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);

		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::pageRightAP(EventFlags flags) {

	EMIT_EVENT_0("page_right");

	const QRect viewRect     = viewport()->contentsRect();
	TextCursor insertPos     = cursorPos_;
	const int oldHorizOffset = horizontalScrollBar()->value();
	const bool silent        = flags & NoBellFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) {

		horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);

		if (horizontalScrollBar()->value() == oldHorizOffset) {
			ringIfNecessary(silent);
		}

	} else {
		TextCursor lineStartPos = buffer_->BufStartOfLine(insertPos);
		int64_t indent          = buffer_->BufCountDispChars(lineStartPos, insertPos);
		TextCursor pos          = buffer_->BufCountForwardDispChars(lineStartPos, indent + viewRect.width() / fixedFontWidth_);

		setInsertPosition(pos);

		horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);

		if (horizontalScrollBar()->value() == oldHorizOffset && insertPos == pos) {
			ringIfNecessary(silent);
		}

		checkMoveSelectionChange(flags, insertPos);
		checkAutoShowInsertPos();
		callCursorMovementCBs();
	}
}

void TextArea::nextPageAP(EventFlags flags) {

	EMIT_EVENT_0("next_page");

	int lastTopLine            = std::max(1, nBufferLines_ - (nVisibleLines_ - 2) + cursorVPadding_);
	const TextCursor insertPos = cursorPos_;
	int column                 = 0;
	int visLineNum;
	TextCursor lineStartPos;
	int targetLine;
	int pageForwardCount = std::max(1, nVisibleLines_ - 1);

	const bool silent         = flags & NoBellFlag;
	const bool maintainColumn = flags & ColumnFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) { // scrollbar only
		targetLine = std::min(topLineNum_ + pageForwardCount, lastTopLine);

		if (targetLine == topLineNum_) {
			ringIfNecessary(silent);
			return;
		}

		verticalScrollBar()->setValue(targetLine);

	} else if (flags & StutterFlag) { // Mac style
		// move to bottom line of visible area
		// if already there, page down maintaining preferred column
		targetLine = std::max(std::min(nVisibleLines_ - 1, nBufferLines_), 0);
		column     = preferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == lineStarts_[targetLine]) {
			if (insertPos >= buffer_->length() || topLineNum_ == lastTopLine) {
				ringIfNecessary(silent);
				return;
			}

			targetLine     = std::min(topLineNum_ + pageForwardCount, lastTopLine);
			TextCursor pos = forwardNLines(insertPos, pageForwardCount, false);

			if (maintainColumn) {
				pos = preferredColumnPos(column, pos);
			}

			setInsertPosition(pos);

			verticalScrollBar()->setValue(targetLine);
		} else {
			TextCursor pos = lineStarts_[targetLine];

			while (targetLine > 0 && pos == -1) {
				--targetLine;
				pos = lineStarts_[targetLine];
			}

			if (lineStartPos == pos) {
				ringIfNecessary(silent);
				return;
			}

			if (maintainColumn) {
				pos = preferredColumnPos(column, pos);
			}

			setInsertPosition(pos);
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
		if (insertPos >= buffer_->length() && topLineNum_ == lastTopLine) {
			ringIfNecessary(silent);
			return;
		}

		if (maintainColumn) {
			column = preferredColumn(&visLineNum, &lineStartPos);
		}

		targetLine = topLineNum_ + nVisibleLines_ - 1;
		targetLine = qBound(1, targetLine, lastTopLine);

		TextCursor pos = forwardNLines(insertPos, nVisibleLines_ - 1, false);
		if (maintainColumn) {
			pos = preferredColumnPos(column, pos);
		}

		setInsertPosition(pos);

		verticalScrollBar()->setValue(targetLine);

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

	const TextCursor insertPos = cursorPos_;
	int visLineNum;
	TextCursor lineStartPos;
	const int pageBackwardCount = std::max(1, nVisibleLines_ - 1);

	const bool silent         = flags & NoBellFlag;
	const bool maintainColumn = flags & ColumnFlag;

	cancelDrag();
	if (flags & ScrollbarFlag) { // scrollbar only
		const int targetLine = std::max(topLineNum_ - pageBackwardCount, 1);

		if (targetLine == topLineNum_) {
			ringIfNecessary(silent);
			return;
		}

		verticalScrollBar()->setValue(targetLine);

	} else if (flags & StutterFlag) { // Mac style
		// move to top line of visible area
		// if already there, page up maintaining preferred column if required
		int targetLine   = 0;
		const int column = preferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == lineStarts_[targetLine]) {
			if (topLineNum_ == 1 && (maintainColumn || column == 0)) {
				ringIfNecessary(silent);
				return;
			}
			targetLine = std::max(topLineNum_ - pageBackwardCount, 1);

			TextCursor pos = countBackwardNLines(insertPos, pageBackwardCount);
			if (maintainColumn) {
				pos = preferredColumnPos(column, pos);
			}

			setInsertPosition(pos);

			verticalScrollBar()->setValue(targetLine);
		} else {
			TextCursor pos = lineStarts_[targetLine];
			if (maintainColumn) {
				pos = preferredColumnPos(column, pos);
			}
			setInsertPosition(pos);
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

		int column = 0;
		if (maintainColumn) {
			column = preferredColumn(&visLineNum, &lineStartPos);
		}

		int targetLine = topLineNum_ - (nVisibleLines_ - 1);
		if (targetLine < 1) {
			targetLine = 1;
		}

		TextCursor pos = countBackwardNLines(insertPos, nVisibleLines_ - 1);
		if (maintainColumn) {
			pos = preferredColumnPos(column, pos);
		}

		setInsertPosition(pos);

		verticalScrollBar()->setValue(targetLine);

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
int TextArea::preferredColumn(int *visLineNum, TextCursor *lineStartPos) {

	/* Find the position of the start of the line.  Use the line starts array
	if possible, to avoid unbounded line-counting in continuous wrap mode */
	if (posToVisibleLineNum(cursorPos_, visLineNum)) {
		*lineStartPos = lineStarts_[*visLineNum];
	} else {
		*lineStartPos = startOfLine(cursorPos_);
		*visLineNum   = -1;
	}

	// Decide what column to move to, if there's a preferred column use that
	return (cursorPreferredCol_ >= 0) ? cursorPreferredCol_ : buffer_->BufCountDispChars(*lineStartPos, cursorPos_);
}

/*
** Return the insert position of the requested column given
** the lineStartPos.
*/
TextCursor TextArea::preferredColumnPos(int column, TextCursor lineStartPos) {

	TextCursor newPos = buffer_->BufCountForwardDispChars(lineStartPos, column);
	if (continuousWrap_) {
		newPos = std::min(newPos, endOfLine(lineStartPos, /*startPosIsLineStart=*/true));
	}

	return newPos;
}

/**
 * @brief TextArea::cursorPos
 * @return
 */
TextCursor TextArea::cursorPos() const {
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
boost::optional<Location> TextArea::positionToLineAndCol(TextCursor pos) const {
	/* In continuous wrap mode, the absolute (non-wrapped) line count is
	   maintained separately, as needed.  Only return it if we're actually
	   keeping track of it and pos is in the displayed text */
	if (continuousWrap_) {
		if (!maintainingAbsTopLineNum() || pos < firstChar_ || pos > lastChar_) {
			return boost::none;
		}

		Location loc;
		loc.line   = absTopLineNum_ + buffer_->BufCountLines(firstChar_, pos);
		loc.column = buffer_->BufCountDispChars(buffer_->BufStartOfLine(pos), pos);
		return loc;
	}

	// Only return the data if pos is within the displayed text
	int visLineNum;
	if (!posToVisibleLineNum(pos, &visLineNum)) {
		return boost::none;
	}

	Location loc;
	loc.column = buffer_->BufCountDispChars(lineStarts_[visLineNum], pos);
	loc.line   = visLineNum + topLineNum_;
	return loc;
}

void TextArea::addCursorMovementCallback(CursorMovedCallback callback, void *arg) {
	movedCallbacks_.emplace_back(callback, arg);
}

void TextArea::addDragStartCallback(DragStartCallback callback, void *arg) {
	dragStartCallbacks_.emplace_back(callback, arg);
}

void TextArea::addDragEndCallback(DragEndCallback callback, void *arg) {
	dragEndCallbacks_.emplace_back(callback, arg);
}

void TextArea::addSmartIndentCallback(SmartIndentCallback callback, void *arg) {
	smartIndentCallbacks_.emplace_back(callback, arg);
}

/*
**  Sets the caret to on or off and restart the caret blink timer.
**  This could be used by other modules to modify the caret's blinking.
*/
void TextArea::resetCursorBlink(bool startsBlanked)
{
	if (cursorBlinkTimer_->isActive()) {
		//  Start blinking the caret again.
		cursorBlinkTimer_->start();

		if (startsBlanked) {
			TextDBlankCursor();
		} else {
			unblankCursor();
		}
	}
}

bool TextArea::focusNextPrevChild(bool next) {

	// Prevent tab from changing focus
	Q_UNUSED(next)
	return false;
}

int TextArea::getEmulateTabs() const {
	return emulateTabs_;
}

/*
** Set the cursor position
*/
void TextArea::TextSetCursorPos(TextCursor pos) {
	setInsertPosition(pos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::setModifyingTabDist(bool modifying) {
	modifyingTabDist_ = modifying;
}

int64_t TextArea::getBufferLinesCount() const {
	return nBufferLines_;
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
*/
void TextArea::attachHighlightData(TextBuffer *styleBuffer, const std::vector<StyleTableEntry> &styleTable, uint32_t unfinishedStyle, UnfinishedStyleCallback unfinishedHighlightCB, void *user) {
	styleBuffer_           = styleBuffer;
	styleTable_            = styleTable;
	unfinishedStyle_       = unfinishedStyle;
	unfinishedHighlightCB_ = unfinishedHighlightCB;
	highlightCBArg_        = user;
	viewport()->update();
}

QTimer *TextArea::cursorBlinkTimer() const {
	return cursorBlinkTimer_;
}

TextCursor TextArea::firstVisiblePos() const {
	return firstChar_;
}

TextCursor TextArea::TextLastVisiblePos() const {
	return lastChar_;
}

TextBuffer *TextArea::styleBuffer() const {
	return styleBuffer_;
}

/**
 * @brief TextArea::updateFontMetrics
 * @param font
 */
void TextArea::updateFontMetrics(const QFont &font) {
	QFontInfo fi(font);

	if (!fi.fixedPitch()) {
		qWarning("NEdit: a variable width font has been specified. This is not supported, and will result in unexpected results");
	}

	QFontMetrics fm(font);
	fixedFontWidth_          = Font::maxWidth(fm);
	const int standardHeight = fm.ascent() + fm.descent();

	QFont boldFont = font;
	boldFont.setWeight(QFont::Bold);
	QFontMetrics fmb(boldFont);
	const int boldHeight = fmb.ascent() + fmb.descent();

	fixedFontHeight_ = std::max(standardHeight, boldHeight);

	widerBold_ = Font::maxWidth(fm) != Font::maxWidth(fmb);
}

int TextArea::getLineNumWidth() const {
	return lineNumberAreaWidth();
}

int TextArea::getRows() const {
	const QRect viewRect = viewport()->contentsRect();
	return viewRect.height() / fixedFontHeight_;
}

QMargins TextArea::getMargins() const {
	return viewport()->contentsMargins();
}

/*
** Fetch text from the widget's buffer, adding wrapping newlines to emulate
** effect achieved by wrapping in the text display in continuous wrap mode.
*/
std::string TextArea::TextGetWrapped(TextCursor startPos, TextCursor endPos) {

	if (!continuousWrap_ || startPos == endPos) {
		return buffer_->BufGetRange(startPos, endPos);
	}

	/* Create a text buffer with a good estimate of the size that adding
	   newlines will expand it to.  Since it's a text buffer, if we guess
	   wrong, it will fail softly, and simply expand the size */
	TextBuffer outBuf((endPos - startPos) + (endPos - startPos) / 5);
	outBuf.BufSetSyncXSelection(false);

	TextCursor outPos;

	/* Go (displayed) line by line through the buffer, adding newlines where
	   the text is wrapped at some character other than an existing newline */
	TextCursor fromPos = startPos;
	TextCursor toPos   = forwardNLines(startPos, 1, false);

	while (toPos < endPos) {
		outBuf.BufCopyFromBuf(buffer_, fromPos, toPos, outPos);
		outPos += toPos - fromPos;
		char c = outBuf.BufGetCharacter(outPos - 1);
		if (c == ' ' || c == '\t') {
			outBuf.BufReplace(outPos - 1, outPos, "\n");
		} else if (c != '\n') {
			outBuf.BufInsert(outPos, '\n');
			++outPos;
		}
		fromPos = toPos;
		toPos   = forwardNLines(fromPos, 1, true);
	}

	outBuf.BufCopyFromBuf(buffer_, fromPos, endPos, outPos);

	// return the contents of the output buffer as a string
	return outBuf.BufGetAll();
}

void TextArea::setStyleBuffer(TextBuffer *buffer) {
	styleBuffer_ = buffer;
}

int TextArea::getWrapMargin() const {
	return wrapMargin_;
}

int TextArea::getColumns() const {
	const QRect viewRect = viewport()->contentsRect();
	return viewRect.width() / fixedFontWidth_;
}

/**
 * @brief TextArea::selfInsertAP
 * @param string
 * @param flags
 */
void TextArea::selfInsertAP(const QString &string, EventFlags flags) {
	insertStringAP(string, flags);
}

void TextArea::insertStringAP(const QString &string, EventFlags flags) {

	EMIT_EVENT_1("insert_string", string);

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	std::string str = string.toStdString();

	if (smartIndent_) {
		SmartIndentEvent smartIndent;
		smartIndent.reason     = CHAR_TYPED;
		smartIndent.pos        = cursorPos_;
		smartIndent.request    = 0;
		smartIndent.charsTyped = str;

		for (auto &c : smartIndentCallbacks_) {
			c.first(this, &smartIndent, c.second);
		}
	}

	TextInsertAtCursor(str, /*allowPendingDelete=*/true, /*allowWrap=*/true);
	buffer_->BufUnselect();
}

/*
** Translate line and column to the nearest row and column number for
** positioning the cursor.
*/
TextCursor TextArea::lineAndColToPosition(Location loc) const {
	int i;
	TextCursor lineStart = {};

	// Count lines
	if (loc.line < 1) {
		loc.line = 1;
	}

	auto lineEnd = TextCursor(-1);
	for (i = 1; i <= loc.line && lineEnd < buffer_->length(); i++) {
		lineStart = lineEnd + 1;
		lineEnd   = buffer_->BufEndOfLine(lineStart);
	}

	// If line is beyond end of buffer, position at last character in buffer
	if (loc.line >= i) {
		return lineEnd;
	}

	// Start character index at zero
	int charIndex = 0;

	// Only have to count columns if column isn't zero (or negative)
	if (loc.column > 0) {

		int charLen = 0;

		// Count columns, expanding each character
		const std::string lineStr = buffer_->BufGetRange(lineStart, lineEnd);
		int outIndex              = 0;
		for (TextCursor cur = lineStart; cur < lineEnd; ++cur, ++charIndex) {

			charLen = TextBuffer::BufCharWidth(lineStr[charIndex], outIndex, buffer_->BufGetTabDistance());

			if (outIndex + charLen >= loc.column) {
				break;
			}

			outIndex += charLen;
		}

		/* If the column is in the middle of an expanded character, put cursor
		 * in front of character if in first half of character, and behind
		 * character if in last half of character
		 */
		if (loc.column >= outIndex + (charLen / 2)) {
			++charIndex;
		}

		// If we are beyond the end of the line, back up one space
		if ((i >= lineEnd) && (charIndex > 0)) {
			--charIndex;
		}
	}

	// Position is the start of the line plus the index into line buffer
	return lineStart + charIndex;
}

/**
 * @brief TextArea::lineAndColToPosition
 * @param line
 * @param column
 * @return
 */
TextCursor TextArea::lineAndColToPosition(int line, int column) {

	Location loc;
	loc.line   = line;
	loc.column = column;
	return lineAndColToPosition(loc);
}

/**
 * @brief TextArea::TextDGetCalltipID
 * @param id
 * @return
 */
int TextArea::TextDGetCalltipID(int id) const {

	if (id == 0) {
		return calltip_.ID;
	} else if (id == calltip_.ID) {
		return id;
	} else {
		return 0;
	}
}

/**
 * @brief TextArea::TextDKillCalltip
 * @param id
 */
void TextArea::TextDKillCalltip(int id) {
	if (calltip_.ID == 0) {
		return;
	}

	if (id == 0 || id == calltip_.ID) {
		if (calltipWidget_) {
			calltipWidget_->hide();
		}

		calltip_.ID = 0;
	}
}

int TextArea::TextDShowCalltip(const QString &text, bool anchored, CallTipPosition pos, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode) {

	static int StaticCalltipID = 1;

	// Destroy any previous calltip
	TextDKillCalltip(0);

	if (!calltipWidget_) {
		calltipWidget_ = new CallTipWidget(this, Qt::Tool | Qt::FramelessWindowHint);
	}

	// Figure out where to put the tip
	if (anchored) {
		// Put it at the specified position
		// If position is not displayed, return 0
		if (boost::get<TextCursor>(pos) < firstChar_ || boost::get<TextCursor>(pos) > lastChar_) {
			QApplication::beep();
			return 0;
		}
		calltip_.pos = pos;
	} else {
		/* Put it next to the cursor, or in the center of the window if the
			cursor is offscreen and mode != strict */
		QPoint rel;
		if (!positionToXY(cursorPos(), &rel)) {
			if (alignMode == TipAlignMode::Strict) {
				QApplication::beep();
				return 0;
			}
			calltip_.pos = -1;
		} else
			// Store the x-offset for use when redrawing
			calltip_.pos = rel.x();
	}

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

	// Expand any tabs in the calltip and set the calltip's text
	calltipWidget_->setText(expandAllTabs(text, buffer_->BufGetTabDistance()));
	updateCalltip(0);

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

/**
 * @brief TextArea::buffer
 * @return
 */
TextBuffer *TextArea::buffer() const {
	return buffer_;
}

/**
 * @brief TextArea::minimumFontWidth
 * @return
 */
int TextArea::minimumFontWidth() const {
	return fixedFontWidth_;
}

/**
 * @brief TextArea::maximumFontWidth
 * @return
 */
int TextArea::maximumFontWidth() const {
	return fixedFontWidth_;
}

/**
 * @brief TextArea::TextNumVisibleLines
 * @return
 */
int TextArea::TextNumVisibleLines() const {
	return nVisibleLines_;
}

/**
 * @brief TextArea::TextFirstVisibleLine
 * @return
 */
int64_t TextArea::TextFirstVisibleLine() const {
	return topLineNum_;
}

/**
 * @brief TextArea::TextVisibleWidth
 * @return
 */
int TextArea::TextVisibleWidth() const {
	const QRect viewRect = viewport()->contentsRect();
	return viewRect.width();
}

/**
 * @brief TextArea::beginningOfSelectionAP
 * @param flags
 */
void TextArea::beginningOfSelectionAP(EventFlags flags) {

	EMIT_EVENT_0("beginning_of_selection");

	const boost::optional<SelectionPos> pos = buffer_->BufGetSelectionPos();
	if (!pos) {
		return;
	}

	if (!pos->isRect) {
		TextSetCursorPos(pos->start);
	} else {
		TextSetCursorPos(buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(pos->start), pos->rectStart));
	}
}

/**
 * @brief TextArea::deleteSelectionAP
 * @param flags
 */
void TextArea::deleteSelectionAP(EventFlags flags) {

	EMIT_EVENT_0("delete_selection");

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	deletePendingSelection();
}

/**
 * @brief TextArea::isDelimeter
 * @param ch
 * @return
 */
bool TextArea::isDelimeter(char ch) const {
	return delimiters_.find(ch) != std::string::npos;
}

void TextArea::deleteNextWordAP(EventFlags flags) {

	EMIT_EVENT_0("delete_next_word");

	cancelDrag();
	if (checkReadOnly()) {
		return;
	}

	if (deletePendingSelection()) {
		return;
	}

	const TextCursor insertPos = cursorPos_;
	const TextCursor lineEnd   = buffer_->BufEndOfLine(insertPos);
	const bool silent          = flags & NoBellFlag;

	if (insertPos == lineEnd) {
		ringIfNecessary(silent);
		return;
	}

	TextCursor pos = insertPos;
	while (isDelimeter(buffer_->BufGetCharacter(pos)) && pos != lineEnd) {
		++pos;
	}

	pos = endOfWord(pos);
	buffer_->BufRemove(insertPos, pos);
	checkAutoShowInsertPos();
	callCursorMovementCBs();
}

void TextArea::endOfSelectionAP(EventFlags flags) {

	EMIT_EVENT_0("end_of_selection");

	const boost::optional<SelectionPos> pos = buffer_->BufGetSelectionPos();
	if (!pos) {
		return;
	}

	if (!pos->isRect) {
		TextSetCursorPos(pos->end);
	} else {
		TextSetCursorPos(buffer_->BufCountForwardDispChars(buffer_->BufStartOfLine(pos->end), pos->rectEnd));
	}
}

void TextArea::scrollUpAP(int count, ScrollUnit units, EventFlags flags) {

	EMIT_EVENT_0("scroll_up");

	int nLines = count;
	if (units == ScrollUnit::Pages) {
		nLines *= nVisibleLines_;
	}

	verticalScrollBar()->setValue(verticalScrollBar()->value() - nLines);
}

void TextArea::scrollDownAP(int count, ScrollUnit units, EventFlags flags) {

	EMIT_EVENT_0("scroll_down");

	int nLines = count;
	if (units == ScrollUnit::Pages) {
		nLines *= nVisibleLines_;
	}

	verticalScrollBar()->setValue(verticalScrollBar()->value() + nLines);
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
	verticalScrollBar()->setValue(line);
}

void TextArea::previousDocumentAP(EventFlags flags) {
	EMIT_EVENT_0("previous_document");
	// handled at higher layer, this is a placeholder
}

void TextArea::nextDocumentAP(EventFlags flags) {
	EMIT_EVENT_0("next_document");
	// handled at higher layer, this is a placeholder
}

/*
** Remove style information from a text widget and redisplay it.
*/
void TextArea::removeWidgetHighlight() {
	attachHighlightData(nullptr, {}, UNFINISHED_STYLE, nullptr, nullptr);
}

/**
 * @brief TextArea::showResizeNotification
 * Shows the size of the widget in rows/columns.
 * Lifted from Konsole's TerminalDisplay::showResizeNotification
 */
void TextArea::showResizeNotification() {
	if (showTerminalSizeHint_ && isVisible()) {
		if (!resizeWidget_) {
			resizeWidget_ = new QLabel(tr("Size: XXX x XXX"), this);
			resizeWidget_->setAttribute(Qt::WA_TransparentForMouseEvents);
			resizeWidget_->setMinimumWidth(Font::stringWidth(resizeWidget_->fontMetrics(), tr("Size: XXX x XXX")));
			resizeWidget_->setMinimumHeight(resizeWidget_->sizeHint().height());
			resizeWidget_->setAlignment(Qt::AlignCenter);

			resizeWidget_->setStyleSheet(QStringLiteral("background-color:palette(window);border-style:solid;border-width:1px;border-color:palette(dark)"));

			resizeTimer_ = new QTimer(this);
			resizeTimer_->setInterval(SIZE_HINT_DURATION);
			resizeTimer_->setSingleShot(true);
			connect(resizeTimer_, &QTimer::timeout, resizeWidget_, &QLabel::hide);
		}

		auto sizeStr = tr("Size: %1 x %2").arg(getColumns()).arg(getRows());
		resizeWidget_->setText(sizeStr);
		resizeWidget_->move((width() - resizeWidget_->width()) / 2,
							(height() - resizeWidget_->height()) / 2 + 20);
		resizeWidget_->show();
		resizeTimer_->start();
	}
}

/*
** If the selection (or cursor position if there's no selection) is not
** fully shown, scroll to bring it in to view.  Note that as written,
** this won't work well with multi-line selections.  Modest re-write
** of the horizontal scrolling part would be quite easy to make it work
** well with rectangular selections.
*/
void TextArea::makeSelectionVisible() {

	const QRect viewRect = viewport()->contentsRect();
	bool isRect;
	TextCursor left;
	int64_t rectEnd;
	int64_t rectStart;
	TextCursor right;

	const TextCursor topChar  = firstVisiblePos();
	const TextCursor lastChar = TextLastVisiblePos();

	// find out where the selection is
	if (!buffer_->BufGetSelectionPos(&left, &right, &isRect, &rectStart, &rectEnd)) {
		left = right = cursorPos();
		isRect       = false;
	}

	/* Check vertical positioning unless the selection is already shown or
	   already covers the display.  If the end of the selection is below
	   bottom, scroll it in to view until the end selection is scrollOffset
	   lines from the bottom of the display or the start of the selection
	   scrollOffset lines from the top.  Calculate a pleasing distance from the
	   top or bottom of the window, to scroll the selection to (if scrolling is
	   necessary), around 1/3 of the height of the window */
	if (!((left >= topChar && right <= lastChar) || (left <= topChar && right >= lastChar))) {

		const int rows         = getRows();
		const int scrollOffset = rows / 3;

		const int topLineNum = verticalScrollBar()->value();

		if (right > lastChar) {
			// End of sel. is below bottom of screen
			const int leftLineNum   = topLineNum + countLines(topChar, left, /*startPosIsLineStart=*/false);
			const int targetLineNum = topLineNum + scrollOffset;

			if (leftLineNum >= targetLineNum) {
				// Start of sel. is not between top & target
				int linesToScroll = countLines(lastChar, right, /*startPosIsLineStart=*/false) + scrollOffset;
				if (leftLineNum - linesToScroll < targetLineNum) {
					linesToScroll = leftLineNum - targetLineNum;
				}

				// Scroll start of selection to the target line
				verticalScrollBar()->setValue(topLineNum + linesToScroll);
			}
		} else if (left < topChar) {
			// Start of sel. is above top of screen
			const int lastLineNum   = topLineNum + rows;
			const int rightLineNum  = lastLineNum - countLines(right, lastChar, /*startPosIsLineStart=*/false);
			const int targetLineNum = lastLineNum - scrollOffset;

			if (rightLineNum <= targetLineNum) {
				// End of sel. is not between bottom & target
				int linesToScroll = countLines(left, topChar, /*startPosIsLineStart=*/false) + scrollOffset;
				if (rightLineNum + linesToScroll > targetLineNum) {
					linesToScroll = targetLineNum - rightLineNum;
				}

				// Scroll end of selection to the target line
				verticalScrollBar()->setValue(topLineNum - linesToScroll);
			}
		}
	}

	/* If either end of the selection off screen horizontally, try to bring it
	   in view, by making sure both end-points are visible.  Using only end
	   points of a multi-line selection is not a great idea, and disaster for
	   rectangular selections, so this part of the routine should be re-written
	   if it is to be used much with either.  Note also that this is a second
	   scrolling operation, causing the display to jump twice.  It's done after
	   vertical scrolling to take advantage of TextDPosToXY which requires it's
	   requested position to be vertically on screen) */
	int leftX;
	int rightX;
	int dummy;
	if (positionToXY(left, &leftX, &dummy) && positionToXY(right, &rightX, &dummy) && leftX <= rightX) {

		int horizOffset = horizontalScrollBar()->value();

		if (leftX < viewRect.left()) {
			horizOffset -= viewRect.left() - leftX;
		} else if (rightX > viewRect.right()) {
			horizOffset += rightX - viewRect.right();
		}

		horizontalScrollBar()->setValue(horizOffset);
	}
}

/**
 * @brief TextArea::zoomOutAP
 * @param flags
 */
void TextArea::zoomOutAP(TextArea::EventFlags flags) {
	Q_UNUSED(flags)
	const QList<int> sizes = Font::pointSizes(font_);

	int currentSize = font_.pointSize();
	int index       = sizes.indexOf(currentSize);
	if (index != 0) {
		font_.setPointSize(sizes[index - 1]);
		document_->action_Set_Fonts(font_.toString());
	} else {
		QApplication::beep();
	}
}

/**
 * @brief TextArea::zoomInAP
 * @param flags
 */
void TextArea::zoomInAP(TextArea::EventFlags flags) {
	Q_UNUSED(flags)
	const QList<int> sizes = Font::pointSizes(font_);

	int currentSize = font_.pointSize();
	int index       = sizes.indexOf(currentSize);
	if (index != sizes.size() - 1) {
		font_.setPointSize(sizes[index + 1]);
		document_->action_Set_Fonts(font_.toString());
	} else {
		QApplication::beep();
	}
}

/**
 * @brief TextArea::wheelEvent
 * @param event
 */
void TextArea::wheelEvent(QWheelEvent *event) {
	if (event->modifiers() == Qt::ControlModifier) {
		if (event->angleDelta().y() > 0) {
			zoomInAP();
		} else {
			zoomOutAP();
		}
	} else {
		QAbstractScrollArea::wheelEvent(event);
	}

	setUserInteractionDetected();
}

/**
 * @brief TextArea::visibleLineContainsCursor
 * @param visLine
 * @param cursor
 * @return
 */
bool TextArea::visibleLineContainsCursor(int visLine, TextCursor cursor) const {
	const TextCursor lineStart = lineStarts_[visLine];

	if (lineStart != TextCursor(-1)) {
		const TextCursor lineEnd = endOfLine(lineStart, /*startPosIsLineStart=*/true);
		if (cursor >= lineStart && cursor <= lineEnd) {
			return true;
		}
	}

	return false;
}

/**
 * @brief TextArea::document
 * @return
 */
DocumentWidget *TextArea::document() const {
	return document_;
}

/**
 * @brief TextArea::fixedFontHeight
 * @return
 */
int TextArea::fixedFontHeight() const {
	return fixedFontHeight_;
}

/**
 * @brief TextArea::fixedFontWidth
 * @return
 */
int TextArea::fixedFontWidth() const {
	return fixedFontWidth_;
}
