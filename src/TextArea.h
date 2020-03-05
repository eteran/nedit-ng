
#ifndef TEXT_AREA_H_
#define TEXT_AREA_H_

#include "BlockDragTypes.h"
#include "CallTip.h"
#include "CursorStyles.h"
#include "DragStates.h"
#include "Location.h"
#include "StyleTableEntry.h"
#include "TextBufferFwd.h"
#include "TextCursor.h"
#include "Util/string_view.h"

#include <QAbstractScrollArea>
#include <QColor>
#include <QFlags>
#include <QFont>
#include <QPointer>
#include <QRect>
#include <QTime>
#include <QVector>

#include <memory>
#include <vector>

#include <boost/optional.hpp>

class CallTipWidget;
class TextArea;
class DocumentWidget;
struct DragEndEvent;
struct SmartIndentEvent;

class QMenu;
class QShortcut;
class QTimer;
class QLabel;

constexpr auto NO_HINT = TextCursor(-1);

using UnfinishedStyleCallback = void (*)(const TextArea *, TextCursor, const void *);
using CursorMovedCallback     = void (*)(TextArea *, void *);
using DragStartCallback       = void (*)(TextArea *, void *);
using DragEndCallback         = void (*)(TextArea *, const DragEndEvent *, void *);
using SmartIndentCallback     = void (*)(TextArea *, SmartIndentEvent *, void *);

class TextArea final : public QAbstractScrollArea {
	Q_OBJECT

private:
	friend class LineNumberArea;

public:
	enum EventFlag {
		NoneFlag         = 0x0000,
		AbsoluteFlag     = 0x0001,
		ColumnFlag       = 0x0002,
		CopyFlag         = 0x0004,
		DownFlag         = 0x0008,
		ExtendFlag       = 0x0010,
		LeftFlag         = 0x0020,
		NoBellFlag       = 0x0040,
		OverlayFlag      = 0x0080,
		RectFlag         = 0x0100,
		RightFlag        = 0x0200,
		ScrollbarFlag    = 0x0400,
		StutterFlag      = 0x0800,
		TailFlag         = 0x1000,
		UpFlag           = 0x2000,
		WrapFlag         = 0x4000,
		SupressRecording = 0x8000, // We use this to prevent recording of events that are triggered by events (and are thus redundant)
	};

	Q_DECLARE_FLAGS(EventFlags, EventFlag)

	enum class ScrollUnit {
		Lines,
		Pages
	};

	enum class PositionType {
		Cursor,
		Character
	};

public:
	TextArea(DocumentWidget *document, TextBuffer *buffer, const QFont &font);
	TextArea(const TextArea &other) = delete;
	TextArea &operator=(const TextArea &) = delete;
	~TextArea() override;

public:
	// NOTE(eteran): if these aren't expected to have side effects, then some
	// of them may be able to be replaced with signals
	void addCursorMovementCallback(CursorMovedCallback callback, void *arg);
	void addDragStartCallback(DragStartCallback callback, void *arg);
	void addDragEndCallback(DragEndCallback callback, void *arg);
	void addSmartIndentCallback(SmartIndentCallback callback, void *arg);

protected:
	bool focusNextPrevChild(bool next) override;
	virtual void mouseQuadrupleClickEvent(QMouseEvent *event);
	virtual void mouseTripleClickEvent(QMouseEvent *event);
	void contextMenuEvent(QContextMenuEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private:
	void cursorBlinkTimerTimeout();
	void autoScrollTimerTimeout();
	void verticalScrollBar_valueChanged(int value);
	void horizontalScrollBar_valueChanged(int value);

private:
	bool clickTracker(QMouseEvent *event, bool inDoubleClickHandler);

public Q_SLOTS:
	void backwardCharacter(EventFlags flags = NoneFlag);
	void backwardParagraphAP(EventFlags flags = NoneFlag);
	void backwardWordAP(EventFlags flags = NoneFlag);
	void beginningOfFileAP(EventFlags flags = NoneFlag);
	void beginningOfLine(EventFlags flags = NoneFlag);
	void beginningOfSelectionAP(EventFlags flags = NoneFlag);
	void copyClipboard(EventFlags flags = NoneFlag);
	void copyPrimaryAP(EventFlags flags = NoneFlag);
	void cutClipboard(EventFlags flags = NoneFlag);
	void cutPrimaryAP(EventFlags flags = NoneFlag);
	void deleteNextCharacter(EventFlags flags = NoneFlag);
	void deleteNextWordAP(EventFlags flags = NoneFlag);
	void deletePreviousCharacter(EventFlags flags = NoneFlag);
	void deletePreviousWord(EventFlags flags = NoneFlag);
	void deleteSelectionAP(EventFlags flags = NoneFlag);
	void deleteToEndOfLineAP(EventFlags flags = NoneFlag);
	void deleteToStartOfLineAP(EventFlags flags = NoneFlag);
	void deselectAllAP(EventFlags flags = NoneFlag);
	void endOfFileAP(EventFlags flags = NoneFlag);
	void endOfLine(EventFlags flags = NoneFlag);
	void endOfSelectionAP(EventFlags flags = NoneFlag);
	void forwardCharacter(EventFlags flags = NoneFlag);
	void forwardParagraphAP(EventFlags flags = NoneFlag);
	void forwardWordAP(EventFlags flags = NoneFlag);
	void insertStringAP(const QString &string, EventFlags flags = NoneFlag);
	void keySelectAP(EventFlags flags = NoneFlag);
	void newline(EventFlags flags = NoneFlag);
	void newlineAndIndentAP(EventFlags flags = NoneFlag);
	void newlineNoIndentAP(EventFlags flags = NoneFlag);
	void nextDocumentAP(EventFlags flags = NoneFlag);
	void nextPageAP(EventFlags flags = NoneFlag);
	void pageLeftAP(EventFlags flags = NoneFlag);
	void pageRightAP(EventFlags flags = NoneFlag);
	void pasteClipboard(EventFlags flags = NoneFlag);
	void previousDocumentAP(EventFlags flags = NoneFlag);
	void previousPageAP(EventFlags flags = NoneFlag);
	void processCancel(EventFlags flags = NoneFlag);
	void processDown(EventFlags flags = NoneFlag);
	void processShiftDownAP(EventFlags flags = NoneFlag);
	void processShiftUpAP(EventFlags flags = NoneFlag);
	void processTabAP(EventFlags flags = NoneFlag);
	void processUp(EventFlags flags = NoneFlag);
	void scrollDownAP(int count, ScrollUnit units = ScrollUnit::Lines, EventFlags flags = NoneFlag);
	void scrollLeftAP(int pixels, EventFlags flags = NoneFlag);
	void scrollRightAP(int pixels, EventFlags flags = NoneFlag);
	void scrollToLineAP(int line, EventFlags flags = NoneFlag);
	void scrollUpAP(int count, ScrollUnit units = ScrollUnit::Lines, EventFlags flags = NoneFlag);
	void selectAllAP(EventFlags flags = NoneFlag);
	void selfInsertAP(const QString &string, EventFlags flags = NoneFlag);
	void toggleOverstrike(EventFlags flags = NoneFlag);
	void zoomInAP(EventFlags flags = NoneFlag);
	void zoomOutAP(EventFlags flags = NoneFlag);

private Q_SLOTS:
	// mouse related events
	void copyToAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void copyToOrEndDragAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void exchangeAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void extendAdjustAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void extendStartAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void mousePanAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void moveDestinationAP(QMouseEvent *event);
	void moveToAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void moveToOrEndDragAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryAdjustAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryOrDragAdjustAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryOrDragStartAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryStartAP(QMouseEvent *event, EventFlags flags = NoneFlag);

public:
	DocumentWidget *document() const;
	QColor getBackgroundColor() const;
	QColor getForegroundColor() const;
	QMargins getMargins() const;
	QTimer *cursorBlinkTimer() const;
	TextBuffer *buffer() const;
	TextCursor lineAndColToPosition(int line, int column);
	TextCursor firstVisiblePos() const;
	TextCursor cursorPos() const;
	TextCursor TextLastVisiblePos() const;
	boost::optional<Location> positionToLineAndCol(TextCursor pos) const;
	TextCursor lineAndColToPosition(Location loc) const;
	TextBuffer *styleBuffer() const;
	int TextDGetCalltipID(int id) const;
	int TextDMaxFontWidth() const;
	int TextDMinFontWidth() const;
	int TextDShowCalltip(const QString &text, bool anchored, CallTipPosition pos, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode);
	int TextNumVisibleLines() const;
	int TextVisibleWidth() const;
	int getColumns() const;
	int getEmulateTabs() const;
	int getLineNumCols() const;
	int getRows() const;
	int getWrapMargin() const;
	int lineNumberAreaWidth() const;
	int64_t TextFirstVisibleLine() const;
	int64_t getBufferLinesCount() const;
	std::string TextGetWrapped(TextCursor startPos, TextCursor endPos);
	void removeWidgetHighlight();
	void attachHighlightData(TextBuffer *styleBuffer, const std::vector<StyleTableEntry> &styleTable, uint32_t unfinishedStyle, UnfinishedStyleCallback unfinishedHighlightCB, void *user);
	void TextDKillCalltip(int id);
	void TextDMaintainAbsLineNum(bool state);
	void TextSetCursorPos(TextCursor pos);
	void makeSelectionVisible();
	void setAutoIndent(bool value);
	void setAutoShowInsertPos(bool value);
	void setAutoWrap(bool value);
	void setBacklightCharTypes(const QString &charTypes);
	void setColors(const QColor &textFG, const QColor &textBG, const QColor &selectionFG, const QColor &selectionBG, const QColor &matchFG, const QColor &matchBG, const QColor &lineNumberFG, const QColor &lineNumberBG, const QColor &cursorFG);
	void setContinuousWrap(bool value);
	void setCursorVPadding(int value);
	void setEmulateTabs(int value);
	void setFont(const QFont &font);
	void setLineNumCols(int value);
	void setModifyingTabDist(bool modifying);
	void setOverstrike(bool value);
	void setReadOnly(bool value);
	void setSmartIndent(bool value);
	void setStyleBuffer(TextBuffer *buffer);
	void setWordDelimiters(const std::string &delimiters);
	void setWrapMargin(int value);

public:
	void bufPreDeleteCallback(TextCursor pos, int64_t nDeleted);
	void bufModifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText);

private:
	QColor getRangesetColor(size_t ind, QColor bground) const;
	QShortcut *createShortcut(const QString &name, const QKeySequence &keySequence, const char *member);
	TextCursor preferredColumnPos(int column, TextCursor lineStartPos);
	TextCursor coordToPosition(const QPoint &coord) const;
	TextCursor countBackwardNLines(TextCursor startPos, int nLines) const;
	TextCursor endOfLine(TextCursor pos, bool startPosIsLineStart) const;
	TextCursor endOfWord(TextCursor pos) const;
	TextCursor forwardNLines(TextCursor startPos, int nLines, bool startPosIsLineStart) const;
	TextCursor startOfLine(TextCursor pos) const;
	TextCursor startOfWord(TextCursor pos) const;
	TextCursor xyToPos(const QPoint &pos, PositionType posType) const;
	TextCursor xyToPos(int x, int y, PositionType posType) const;
	bool moveDown(bool absolute);
	bool moveLeft();
	bool moveRight();
	bool moveUp(bool absolute);
	bool checkReadOnly() const;
	bool deleteEmulatedTab();
	bool deletePendingSelection();
	bool emptyLinesVisible() const;
	bool inSelection(const QPoint &p) const;
	bool isDelimeter(char ch) const;
	bool maintainingAbsTopLineNum() const;
	bool pendingSelection() const;
	bool posToVisibleLineNum(TextCursor pos, int *lineNum) const;
	bool positionToXY(TextCursor pos, QPoint *coord) const;
	bool positionToXY(TextCursor pos, int *x, int *y) const;
	bool updateHScrollBarRange();
	bool updateLineStarts(TextCursor pos, int64_t charsInserted, int64_t charsDeleted, int64_t linesInserted, int64_t linesDeleted);
	bool visibleLineContainsCursor(int visLine, TextCursor cursor) const;
	bool wrapLine(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, TextCursor limitPos, TextCursor *breakAt, int64_t *charsAdded);
	bool wrapUsesCharacter(TextCursor lineEndPos) const;
	boost::optional<TextCursor> spanBackward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const;
	boost::optional<TextCursor> spanForward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const;
	int offsetWrappedColumn(int row, int column) const;
	int offsetWrappedRow(int row) const;
	int preferredColumn(int *visLineNum, TextCursor *lineStartPos);
	int countLines(TextCursor startPos, TextCursor endPos, bool startPosIsLineStart);
	int getAbsTopLineNum() const;
	int getLineNumWidth() const;
	int lengthToWidth(int length) const noexcept;
	int measureVisLine(int visLineNum) const;
	int visLineLength(int visLineNum) const;
	int widthInPixels(char ch, int column) const;
	std::string createIndentString(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, int *column);
	std::string wrapText(view::string_view startLine, view::string_view text, int64_t bufOffset, int wrapMargin, int64_t *breakBefore);
	uint32_t styleOfPos(TextCursor lineStartPos, size_t lineLen, size_t lineIndex, int64_t dispIndex, int thisChar) const;
	void beginBlockDrag();
	void blockDragSelection(const QPoint &pos, BlockDragTypes dragType);
	void CopyToClipboard();
	void exchangeSelections();
	void finishBlockDrag();
	void insertPrimarySelection(bool isColumnar);
	void movePrimarySelection(bool isColumnar);
	void TextColPasteClipboard();
	void TextCutClipboard();
	void TextDBlankCursor();
	void TextDMakeInsertPosVisible();
	void TextDOverstrike(view::string_view text);
	void setWrapMode(bool wrap, int wrapMargin);
	void coordToUnconstrainedPosition(const QPoint &coord, int *row, int *column) const;
	void TextInsertAtCursor(view::string_view chars, bool allowPendingDelete, bool allowWrap);
	void TextPasteClipboard();
	void adjustSecondarySelection(const QPoint &coord);
	void adjustSelection(const QPoint &coord);
	void calcLastChar();
	void calcLineStarts(int startLine, int endLine);
	void callCursorMovementCBs();
	void callMovedCBs();
	void cancelBlockDrag();
	void cancelDrag();
	void checkAutoScroll(const QPoint &coord);
	void checkAutoShowInsertPos();
	void checkMoveSelectionChange(EventFlags flags, TextCursor startPos);
	void drawCursor(QPainter *painter, int x, int y);
	void drawString(QPainter *painter, uint32_t style, int x, int y, int toX, view::string_view string);
	void endDrag();
	void extendRangeForStyleMods(TextCursor *start, TextCursor *end);
	void findLineEnd(TextCursor startPos, bool startPosIsLineStart, TextCursor *lineEnd, TextCursor *nextLineStart);
	void findWrapRange(view::string_view deletedText, TextCursor pos, int64_t nInserted, int64_t nDeleted, TextCursor *modRangeStart, TextCursor *modRangeEnd, int64_t *linesInserted, int64_t *linesDeleted);
	void handleResize(bool widthChanged);
	void hideOrShowHScrollBar();
	void insertClipboard(bool isColumnar);
	void insertText(view::string_view text);
	void keyMoveExtendSelection(TextCursor origPos, bool rectangular);
	void measureDeletedLines(TextCursor pos, int64_t nDeleted);
	void offsetAbsLineNum(TextCursor oldFirstChar);
	void offsetLineStarts(int newTopLineNum);
	void redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip);
	void redisplayLine(int visLineNum, int leftCharIndex, int rightCharIndex);
	void redisplayRange(TextCursor start, TextCursor end);
	void redisplayRect(const QRect &rect);
	void repaintLineNumbers();
	void resetAbsLineNum();
	void selectLine();
	void selectWord(int pointerX);
	void setCursorStyle(CursorStyles style);
	void setInsertPosition(TextCursor newPos);
	void setLineNumberAreaWidth(int lineNumWidth);
	void setupBGClasses(const QString &str);
	void showResizeNotification();
	void simpleInsertAtCursor(view::string_view chars, bool allowPendingDelete);
	void unblankCursor();
	void updateCalltip(int calltipID);
	void updateFontMetrics(const QFont &font);
	void updateVScrollBarRange();
	void wrappedLineCounter(const TextBuffer *buf, TextCursor startPos, TextCursor maxPos, int maxLines, bool startPosIsLineStart, TextCursor *retPos, int *retLines, TextCursor *retLineStart, TextCursor *retLineEnd) const;
	void xyToUnconstrainedPos(const QPoint &pos, int *row, int *column, PositionType posType) const;
	void xyToUnconstrainedPos(int x, int y, int *row, int *column, PositionType posType) const;

private:
	CursorStyles cursorStyle_       = CursorStyles::Normal;
	DragStates dragState_           = NOT_CLICKED; // Why is the mouse being dragged and what is being acquired
	QColor cursorFGColor_           = Qt::black;
	QColor matchBGColor_            = Qt::red;
	QColor matchFGColor_            = Qt::white; // Highlight colors are used when flashing matching parens
	QColor lineNumFGColor_          = Qt::black; // Color for drawing line numbers
	QColor lineNumBGColor_          = Qt::white; // Color for drawing line numbers
	QLabel *resizeWidget_           = nullptr;
	QTimer *autoScrollTimer_        = nullptr;
	QTimer *clickTimer_             = nullptr;
	QTimer *cursorBlinkTimer_       = nullptr;
	QTimer *resizeTimer_            = nullptr;
	QWidget *lineNumberArea_        = nullptr;
	QPoint cursor_                  = {-100, -100}; // X pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
	QVector<TextCursor> lineStarts_ = {TextCursor()};
	TextCursor cursorToHint_        = NO_HINT; // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
	int nBufferLines_               = 0;       // # of newlines in the buffer
	int clickCount_                 = 0;
	int emTabsBeforeCursor_         = 0; // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
	int absTopLineNum_              = 1; // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
	int topLineNum_                 = 1; // Line number of top displayed line of file (first line of file is 1)
	int lineNumCols_                = 0;
	int dragXOffset_                = 0;  // offsets between cursor location and actual insertion point in drag
	int dragYOffset_                = 0;  // offsets between cursor location and actual insertion point in drag
	int nLinesDeleted_              = 0;  // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
	int nVisibleLines_              = 1;  // # of visible (displayed) lines
	int cursorPreferredCol_         = -1; // Column for vert. cursor movement
	bool clickTimerExpired_         = false;
	bool cursorOn_                  = false;
	bool modifyingTabDist_          = false; // Whether tab distance is being modified
	bool needAbsTopLineNum_         = false; // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
	bool suppressResync_            = false; // Suppress resynchronization of line starts during buffer updates
	bool showTerminalSizeHint_      = false;
	bool autoShowInsertPos_         = true;
	bool pendingDelete_             = true;

private:
	BlockDragTypes dragType_; // style of block drag operation
	CallTip calltip_;
	DocumentWidget *document_;
	QFont font_;
	QPoint btnDownCoord_; // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
	QPoint clickPos_;
	QPoint mouseCoord_; // Last known mouse position in drag operation (for autoscroll)
	QPointer<CallTipWidget> calltipWidget_;
	TextBuffer *buffer_; // Contains text to be displayed
	TextCursor anchor_;  // Anchor for drag operations
	TextCursor cursorPos_;
	TextCursor dragInsertPos_;       // location where text being block dragged was last inserted
	TextCursor dragSourceDeletePos_; // location from which move source text was removed at start of drag
	TextCursor firstChar_;           // Buffer positions of first and last displayed character (lastChar_ points either to a newline or one character beyond the end of the buffer)
	TextCursor lastChar_;
	bool autoIndent_;
	bool autoWrapPastedText_;
	bool autoWrap_;
	bool colorizeHighlightedText_;
	bool continuousWrap_;
	bool heavyCursor_;
	bool hidePointer_;
	bool overstrike_;
	bool readOnly_;
	bool smartIndent_;
	bool smartHome_;
	int cursorBlinkRate_;
	int cursorVPadding_;
	int emulateTabs_;
	int fixedFontWidth_; // Font width if all current fonts are fixed and match in width
	int fixedFontHeight_;
	int wrapMargin_;
	int rectAnchor_;       // Anchor for rectangular drag operations
	int64_t dragDeleted_;  // # of characters deleted at drag destination in last drag position
	int64_t dragInserted_; // # of characters inserted at drag destination in last drag position
	int64_t dragNLines_;   // # of newlines in text being drag'd
	int64_t dragRectStart_;
	int64_t dragSourceDeleted_;         // # of chars. deleted when move source text was deleted
	int64_t dragSourceInserted_;        // # of chars. inserted when move source text was inserted
	TextBuffer *styleBuffer_ = nullptr; // Optional parallel buffer containing color and font information
	std::string delimiters_;
	std::shared_ptr<TextBuffer> dragOrigBuf_;       // backup buffer copy used during block dragging of selections
	std::vector<QColor> bgClassColors_;             // table of colors for each BG class
	std::vector<StyleTableEntry> styleTable_;       // Table of fonts and colors for coloring/syntax-highlighting
	std::vector<uint8_t> bgClass_;                  // obtains index into bgClassColors_
	uint32_t unfinishedStyle_;                      // Style buffer entry which triggers on-the-fly reparsing of region
	UnfinishedStyleCallback unfinishedHighlightCB_; // Callback to parse "unfinished" regions
	void *highlightCBArg_;                          // Arg to unfinishedHighlightCB

private:
	std::vector<std::pair<CursorMovedCallback, void *>> movedCallbacks_;
	std::vector<std::pair<DragStartCallback, void *>> dragStartCallbacks_;
	std::vector<std::pair<DragEndCallback, void *>> dragEndCallbacks_;
	std::vector<std::pair<SmartIndentCallback, void *>> smartIndentCallbacks_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextArea::EventFlags)

#endif
