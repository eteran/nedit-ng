
#ifndef TEXT_AREA_H_
#define TEXT_AREA_H_

#include "BlockDragTypes.h"
#include "CallTip.h"
#include "CursorStyles.h"
#include "DragStates.h"
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

using unfinishedStyleCBProcEx = void (*)(const TextArea *, TextCursor, const void *);
using cursorMovedCBEx         = void (*)(TextArea *, void *);
using dragStartCBEx           = void (*)(TextArea *, void *);
using dragEndCBEx             = void (*)(TextArea *, const DragEndEvent *, void *);
using smartIndentCBEx         = void (*)(TextArea *, SmartIndentEvent *, void *);

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

	enum class ScrollUnits {
		Lines,
		Pages
	};

	enum class PositionTypes {
		Cursor,
		Character
	};

public:
	static constexpr int DefaultVMargin = 2;
	static constexpr int DefaultHMargin = 2;

public:
	TextArea(DocumentWidget *document, TextBuffer *buffer, const QFont &font);
	TextArea(const TextArea &other) = delete;
	TextArea& operator=(const TextArea &) = delete;
	~TextArea() noexcept override;

public:
	// NOTE(eteran): if these aren't expected to have side effects, then some
	// of them may be able to be replaced with signals
	void addCursorMovementCallback(cursorMovedCBEx callback, void *arg);
	void addDragStartCallback(dragStartCBEx callback, void *arg);
	void addDragEndCallback(dragEndCBEx callback, void *arg);
	void addSmartIndentCallback(smartIndentCBEx callback, void *arg);

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

private Q_SLOTS:
	void cursorBlinkTimerTimeout();
	void autoScrollTimerTimeout();
	void clickTimeout();
	void verticalScrollBar_valueChanged(int value);
	void horizontalScrollBar_valueChanged(int value);

private:
	bool clickTracker(QMouseEvent *event, bool inDoubleClickHandler);

public Q_SLOTS:
	void ZoomOutAP(TextArea::EventFlags flags = NoneFlag);
	void ZoomInAP(TextArea::EventFlags flags = NoneFlag);
	void previousDocumentAP(TextArea::EventFlags flags = NoneFlag);
	void nextDocumentAP(TextArea::EventFlags flags = NoneFlag);
	void beginningOfSelectionAP(TextArea::EventFlags flags = NoneFlag);
	void forwardCharacterAP(TextArea::EventFlags flags = NoneFlag);
	void backwardCharacterAP(TextArea::EventFlags flags = NoneFlag);
	void processUpAP(TextArea::EventFlags flags = NoneFlag);
	void processDownAP(TextArea::EventFlags flags = NoneFlag);
	void newlineAP(TextArea::EventFlags flags = NoneFlag);
	void deletePreviousCharacterAP(TextArea::EventFlags flags = NoneFlag);
	void processCancelAP(TextArea::EventFlags flags = NoneFlag);
	void beginningOfLineAP(TextArea::EventFlags flags = NoneFlag);
	void deletePreviousWordAP(TextArea::EventFlags flags = NoneFlag);
	void endOfLineAP(TextArea::EventFlags flags = NoneFlag);
	void deleteNextCharacterAP(TextArea::EventFlags flags = NoneFlag);
	void newlineAndIndentAP(TextArea::EventFlags flags = NoneFlag);
	void newlineNoIndentAP(TextArea::EventFlags flags = NoneFlag);
	void toggleOverstrikeAP(TextArea::EventFlags flags = NoneFlag);
	void keySelectAP(TextArea::EventFlags flags = NoneFlag);
	void processShiftDownAP(TextArea::EventFlags flags = NoneFlag);
	void processShiftUpAP(TextArea::EventFlags flags = NoneFlag);
	void cutClipboardAP(TextArea::EventFlags flags = NoneFlag);
	void copyClipboardAP(TextArea::EventFlags flags = NoneFlag);
	void pasteClipboardAP(TextArea::EventFlags flags = NoneFlag);
	void copyPrimaryAP(TextArea::EventFlags flags = NoneFlag);
	void beginningOfFileAP(TextArea::EventFlags flags = NoneFlag);
	void endOfFileAP(TextArea::EventFlags flags = NoneFlag);
	void backwardWordAP(TextArea::EventFlags flags = NoneFlag);
	void forwardWordAP(TextArea::EventFlags flags = NoneFlag);
	void forwardParagraphAP(TextArea::EventFlags flags = NoneFlag);
	void backwardParagraphAP(TextArea::EventFlags flags = NoneFlag);
	void processTabAP(TextArea::EventFlags flags = NoneFlag);
	void selectAllAP(TextArea::EventFlags flags = NoneFlag);
	void deselectAllAP(TextArea::EventFlags flags = NoneFlag);
	void deleteToStartOfLineAP(TextArea::EventFlags flags = NoneFlag);
	void deleteToEndOfLineAP(TextArea::EventFlags flags = NoneFlag);
	void cutPrimaryAP(TextArea::EventFlags flags = NoneFlag);
	void pageLeftAP(TextArea::EventFlags flags = NoneFlag);
	void pageRightAP(TextArea::EventFlags flags = NoneFlag);
	void nextPageAP(TextArea::EventFlags flags = NoneFlag);
	void previousPageAP(TextArea::EventFlags flags = NoneFlag);
	void deleteSelectionAP(TextArea::EventFlags flags = NoneFlag);
	void deleteNextWordAP(TextArea::EventFlags flags = NoneFlag);
	void endOfSelectionAP(TextArea::EventFlags flags = NoneFlag);
	void scrollUpAP(int count, TextArea::ScrollUnits units = ScrollUnits::Lines, TextArea::EventFlags flags = NoneFlag);
	void scrollDownAP(int count, TextArea::ScrollUnits units = ScrollUnits::Lines, TextArea::EventFlags flags = NoneFlag);
	void scrollRightAP(int pixels, TextArea::EventFlags flags = NoneFlag);
	void scrollLeftAP(int pixels, TextArea::EventFlags flags = NoneFlag);
	void scrollToLineAP(int line, TextArea::EventFlags flags = NoneFlag);
	void insertStringAP(const QString &string, TextArea::EventFlags flags = NoneFlag);
	void selfInsertAP(const QString &string, TextArea::EventFlags flags = NoneFlag);

private Q_SLOTS:
	// mouse related events
	void moveDestinationAP(QMouseEvent *event);
	void extendStartAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void extendAdjustAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void mousePanAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void copyToOrEndDragAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void copyToAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void secondaryOrDragStartAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void secondaryStartAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void secondaryOrDragAdjustAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void secondaryAdjustAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void moveToOrEndDragAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void moveToAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);
	void exchangeAP(QMouseEvent *event, TextArea::EventFlags flags = NoneFlag);

public:
	QColor getBackgroundColor() const;
	QColor getForegroundColor() const;
	QTimer *cursorBlinkTimer() const;
	TextBuffer *TextGetBuffer() const;
	TextCursor TextDLineAndColToPos(int64_t lineNum, int64_t column);
	TextCursor TextFirstVisiblePos() const;
	TextCursor TextGetCursorPos() const;
	TextCursor TextLastVisiblePos() const;
	bool TextDPosToLineAndCol(TextCursor pos, int64_t *lineNum, int64_t *column);
	const std::shared_ptr<TextBuffer> &getStyleBuffer() const;
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
	QMargins getMargins() const;
	int64_t TextFirstVisibleLine() const;
	int64_t getBufferLinesCount() const;
	std::string TextGetWrapped(TextCursor startPos, TextCursor endPos);
	void RemoveWidgetHighlightEx();
	void TextDAttachHighlightData(const std::shared_ptr<TextBuffer> &styleBuffer, const std::vector<StyleTableEntry> &styleTable, uint32_t unfinishedStyle, unfinishedStyleCBProcEx unfinishedHighlightCB, void *user);	
	void TextDKillCalltip(int id);
	void TextDMaintainAbsLineNum(bool state);
	void TextDMakeSelectionVisible();
	void TextDSetColors(const QColor &textFgP, const QColor &textBgP, const QColor &selectFgP, const QColor &selectBgP, const QColor &hiliteFgP, const QColor &hiliteBgP, const QColor &lineNoFgP, const QColor &cursorFgP);
	void TextSetCursorPos(TextCursor pos);
	void setAutoIndent(bool value);
	void setAutoShowInsertPos(bool value);
	void setAutoWrap(bool value);
	void setBacklightCharTypes(const QString &charTypes);
	void setContinuousWrap(bool value);
	void setCursorVPadding(int value);
	void setEmulateTabs(int value);
	void setFont(const QFont &font);
	void setLineNumCols(int value);
	void setModifyingTabDist(bool modifying);
	void setOverstrike(bool value);
	void setReadOnly(bool value);
	void setSmartIndent(bool value);
	void setStyleBuffer(const std::shared_ptr<TextBuffer> &buffer);
	void setWordDelimiters(const std::string &delimiters);
	void setWrapMargin(int value);

public:
	void bufPreDeleteCallback(TextCursor pos, int64_t nDeleted);
	void bufModifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText);

private:
	void TextDSetScroll(int64_t topLineNum, int horizOffset);
	void TextDGetScroll(int64_t *topLineNum, int *horizOffset);
	CursorStyles getCursorStyle() const;
	QColor getRangesetColor(size_t ind, QColor bground) const;
	QFont getFont() const;
	QShortcut *createShortcut(const QString &name, const QKeySequence &keySequence, const char *member);
	TextCursor TextDCountBackwardNLines(TextCursor startPos, int64_t nLines) const;
	TextCursor TextDCountForwardNLines(TextCursor startPos, int64_t nLines, bool startPosIsLineStart) const;
	TextCursor TextDEndOfLine(TextCursor pos, bool startPosIsLineStart) const;
	TextCursor TextDPosOfPreferredCol(int64_t column, TextCursor lineStartPos);
	TextCursor TextDStartOfLine(TextCursor pos) const;
	TextCursor TextDXYToPosition(const QPoint &coord) const;
	TextCursor endOfWord(TextCursor pos) const;
	TextCursor startOfWord(TextCursor pos) const;
	TextCursor xyToPos(const QPoint &pos, PositionTypes posType) const;
	TextCursor xyToPos(int x, int y, PositionTypes posType) const;
	bool visibleLineContainsCursor(int visLine, TextCursor cursor) const;
	bool TextDInSelection(const QPoint &p) const;
	bool TextDMoveDown(bool absolute);
	bool TextDMoveLeft();
	bool TextDMoveRight();
	bool TextDMoveUp(bool absolute);
	bool TextDPositionToXY(TextCursor pos, QPoint *coord) const;
	bool TextDPositionToXY(TextCursor pos, int *x, int *y) const;
	bool checkReadOnly() const;
	bool deleteEmulatedTab();
	bool deletePendingSelection();
	bool emptyLinesVisible() const;
	bool maintainingAbsTopLineNum() const;
	bool pendingSelection() const;
	bool posToVisibleLineNum(TextCursor pos, int *lineNum) const;
	bool updateHScrollBarRange();
	bool wrapLine(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, TextCursor limitPos, TextCursor *breakAt, int64_t *charsAdded);
	bool wrapUsesCharacter(TextCursor lineEndPos) const;
	boost::optional<TextCursor> spanBackward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const;
	boost::optional<TextCursor> spanForward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const;
	int getLineNumWidth() const;
	int lineNumberAreaWidth() const;
	int stringWidth(int length) const;
	int64_t TextDCountLines(TextCursor startPos, TextCursor endPos, bool startPosIsLineStart);
	int64_t TextDOffsetWrappedColumn(int64_t row, int64_t column) const;
	int64_t TextDOffsetWrappedRow(int64_t row) const;
	int64_t TextDPreferredColumn(int *visLineNum, TextCursor *lineStartPos);
	int64_t getAbsTopLineNum() const;
	int64_t measureVisLine(int visLineNum) const;
	int64_t visLineLength(int visLineNum) const;
	int64_t widthInPixels(char ch, int64_t colNum) const;
	std::string createIndentStringEx(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, int *column);
	std::string wrapTextEx(view::string_view startLine, view::string_view text, int64_t bufOffset, int wrapMargin, int64_t *breakBefore);
	uint32_t styleOfPos(TextCursor lineStartPos, int64_t lineLen, int64_t lineIndex, int64_t dispIndex, int thisChar) const;
	void BeginBlockDrag();
	void BlockDragSelection(const QPoint &pos, BlockDragTypes dragType);
	void CancelBlockDrag();
	void CopyToClipboard();
	void ExchangeSelections();
	void FinishBlockDrag();
	void InsertClipboard(bool isColumnar);
	void InsertPrimarySelection(bool isColumnar);
	void MovePrimarySelection(bool isColumnar);
	void TextColPasteClipboard();
	void TextCutClipboard();
	void TextDBlankCursor();
	void TextDInsertEx(view::string_view text);
	void TextDMakeInsertPosVisible();
	void TextDOverstrikeEx(view::string_view text);
	void TextDRedisplayRect(const QRect &rect);
	void TextDRedisplayRect(int left, int top, int width, int height);
	void TextDRedrawCalltip(int calltipID);
	void TextDResize(bool widthChanged);
	void TextDSetCursorStyle(CursorStyles style);
	void TextDSetInsertPosition(TextCursor newPos);
	void TextDSetLineNumberArea(int lineNumWidth);
	void TextDSetWrapMode(bool wrap, int wrapMargin);
	void TextDSetupBGClassesEx(const QString &str);
	void TextDUnblankCursor();
	void TextDXYToUnconstrainedPosition(const QPoint &coord, int64_t *row, int64_t *column) const;
	void TextInsertAtCursorEx(view::string_view chars, bool allowPendingDelete, bool allowWrap);
	void TextPasteClipboard();
	void adjustSecondarySelection(const QPoint &coord);
	void adjustSelection(const QPoint &coord);
	void calcLastChar();
	void calcLineStarts(int startLine, int endLine);
	void callCursorMovementCBs();
	void callMovedCBs();
	void cancelDrag();
	void checkAutoScroll(const QPoint &coord);
	void checkAutoShowInsertPos();
	void checkMoveSelectionChange(EventFlags flags, TextCursor startPos);
	void drawCursor(QPainter *painter, int x, int y);
	void drawString(QPainter *painter, uint32_t style, int64_t x, int y, int64_t toX, const char *string, long nChars);
	void endDrag();
	void extendRangeForStyleMods(TextCursor *start, TextCursor *end);
	void findLineEnd(TextCursor startPos, bool startPosIsLineStart, TextCursor *lineEnd, TextCursor *nextLineStart);
	void findWrapRangeEx(view::string_view deletedText, TextCursor pos, int64_t nInserted, int64_t nDeleted, TextCursor *modRangeStart, TextCursor *modRangeEnd, int64_t *linesInserted, int64_t *linesDeleted);
	void hideOrShowHScrollBar();
	void keyMoveExtendSelection(TextCursor origPos, bool rectangular);
	void measureDeletedLines(TextCursor pos, int64_t nDeleted);
	void offsetAbsLineNum(TextCursor oldFirstChar);
	void offsetLineStarts(int64_t newTopLineNum);
	void redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip);
	void redisplayLineEx(int visLineNum, int64_t leftCharIndex, int64_t rightCharIndex);
	void repaintLineNumbers();
	void resetAbsLineNum();
	void selectLine();
	void selectWord(int pointerX);
	void setScroll(int64_t topLineNum, int horizOffset);
	void showResizeNotification();
	void simpleInsertAtCursorEx(view::string_view chars, bool allowPendingDelete);
	void textDRedisplayRange(TextCursor start, TextCursor end);
	void updateFontMetrics(const QFont &font);
	void updateLineStarts(TextCursor pos, int64_t charsInserted, int64_t charsDeleted, int64_t linesInserted, int64_t linesDeleted, bool *scrolled);
	void updateVScrollBarRange();
	void wrappedLineCounter(const TextBuffer *buf, TextCursor startPos, TextCursor maxPos, int64_t maxLines, bool startPosIsLineStart, TextCursor *retPos, int64_t *retLines, TextCursor *retLineStart, TextCursor *retLineEnd) const;
	void xyToUnconstrainedPos(const QPoint &pos, int64_t *row, int64_t *column, PositionTypes posType) const;
	void xyToUnconstrainedPos(int x, int y, int64_t *row, int64_t *column, PositionTypes posType) const;

private:	
	CursorStyles cursorStyle_       = CursorStyles::Normal;
	DragStates dragState_           = NOT_CLICKED;    // Why is the mouse being dragged and what is being acquired
	QColor cursorFGColor_           = Qt::black;
	QColor highlightBGColor_        = Qt::red;
	QColor highlightFGColor_        = Qt::white;      // Highlight colors are used when flashing matching parens
	QColor lineNumFGColor_          = Qt::black;      // Color for drawing line numbers
	QLabel *resizeWidget_           = nullptr;
	QTimer *autoScrollTimer_        = nullptr;
	QTimer *clickTimer_             = nullptr;
	QTimer *cursorBlinkTimer_       = nullptr;
	QTimer *resizeTimer_            = nullptr;
	QWidget *lineNumberArea_        = nullptr;
	QPoint cursor_                  = { -100, -100 }; // X pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
	QVector<TextCursor> lineStarts_ = { TextCursor() };
	TextCursor cursorToHint_        = NO_HINT;        // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
	int64_t absTopLineNum_          = 1;              // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
	int64_t cursorPreferredCol_     = -1;             // Column for vert. cursor movement
	int64_t topLineNum_             = 1;              // Line number of top displayed line of file (first line of file is 1)
	int64_t nBufferLines_           = 0;              // # of newlines in the buffer
	int clickCount_                 = 0;
	int emTabsBeforeCursor_         = 0;              // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
	int horizOffset_                = 0;              // Horizontal scroll pos. in pixels	
	int lineNumCols_                = 0;
	int dragXOffset_                = 0;              // offsets between cursor location and actual insertion point in drag
	int dragYOffset_                = 0;              // offsets between cursor location and actual insertion point in drag
	int nLinesDeleted_              = 0;              // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
	int nVisibleLines_              = 1;              // # of visible (displayed) lines
	bool cursorOn_                  = false;
	bool modifyingTabDist_          = false;          // Whether tab distance is being modified
	bool needAbsTopLineNum_         = false;          // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
	bool suppressResync_            = false;          // Suppress resynchronization of line starts during buffer updates
	bool showTerminalSizeHint_      = false;
	bool autoShowInsertPos_         = true;
	bool pendingDelete_             = true;

private:
	BlockDragTypes dragType_;                       // style of block drag operation
	CallTip calltip_;
	DocumentWidget *document_;
	QFont font_;
	QPoint btnDownCoord_;                           // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
	QPoint clickPos_;
	QPoint mouseCoord_;                             // Last known mouse position in drag operation (for autoscroll)
	QPointer<CallTipWidget> calltipWidget_;
	TextBuffer *buffer_;                            // Contains text to be displayed
	TextCursor anchor_;                             // Anchor for drag operations
	TextCursor cursorPos_;
	TextCursor dragInsertPos_;                      // location where text being block dragged was last inserted
	TextCursor dragSourceDeletePos_;                // location from which move source text was removed at start of drag
	TextCursor firstChar_;                          // Buffer positions of first and last displayed character (lastChar_ points either to a newline or one character beyond the end of the buffer)
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
	int ascent_;                                    // Composite ascent and descent for primary font
	int cursorBlinkRate_;
	int cursorVPadding_;
	int descent_;
	int emulateTabs_;
	int fixedFontWidth_;                            // Font width if all current fonts are fixed and match in width
	int wrapMargin_;
	int64_t dragDeleted_;                           // # of characters deleted at drag destination in last drag position
	int64_t dragInserted_;                          // # of characters inserted at drag destination in last drag position
	int64_t dragNLines_;                            // # of newlines in text being drag'd
	int64_t dragRectStart_;
	int64_t dragSourceDeleted_;                     // # of chars. deleted when move source text was deleted
	int64_t dragSourceInserted_;                    // # of chars. inserted when move source text was inserted
	int64_t rectAnchor_;                            // Anchor for rectangular drag operations
	std::shared_ptr<TextBuffer> styleBuffer_;       // Optional parallel buffer containing color and font information
	std::string delimiters_;
	std::unique_ptr<TextBuffer> dragOrigBuf_;       // backup buffer copy used during block dragging of selections
	std::vector<QColor> bgClassColors_;             // table of colors for each BG class
	std::vector<StyleTableEntry> styleTable_;       // Table of fonts and colors for coloring/syntax-highlighting
	std::vector<uint8_t> bgClass_;                  // obtains index into bgClassColors_
	uint32_t unfinishedStyle_;                      // Style buffer entry which triggers on-the-fly reparsing of region
	unfinishedStyleCBProcEx unfinishedHighlightCB_; // Callback to parse "unfinished" regions
	void *highlightCBArg_;                          // Arg to unfinishedHighlightCB

private:
	std::vector<std::pair<cursorMovedCBEx, void *>> movedCallbacks_;
	std::vector<std::pair<dragStartCBEx,   void *>> dragStartCallbacks_;
	std::vector<std::pair<dragEndCBEx,     void *>> dragEndCallbacks_;
	std::vector<std::pair<smartIndentCBEx, void *>> smartIndentCallbacks_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextArea::EventFlags)

#endif
