
#ifndef TEXT_AREA_H_
#define TEXT_AREA_H_

#include "BlockDragTypes.h"
#include "CallTip.h"
#include "CursorStyles.h"
#include "TextBufferFwd.h"
#include "DragStates.h"
#include "StyleTableEntry.h"
#include "Util/string_view.h"
#include "TextCursor.h"
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
#include <boost/variant.hpp>

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
        CURSOR_POS,
        CHARACTER_POS
    };

public:
    static constexpr int DefaultVMargin = 2;
    static constexpr int DefaultHMargin = 2;

public:
    TextArea(DocumentWidget *document, TextBuffer *buffer, const QFont &fontStruct);
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

public:
	// resource setters
    void setWordDelimiters(const std::string &delimiters);
	void setBacklightCharTypes(const QString &charTypes);
    void setAutoShowInsertPos(bool value);
    void setEmulateTabs(int value);
	void setWrapMargin(int value);
	void setLineNumCols(int value);
	void setReadOnly(bool value);
	void setOverstrike(bool value);
	void setCursorVPadding(int value);
    void setFont(const QFont &font);
	void setContinuousWrap(bool value);
	void setAutoWrap(bool value);
	void setAutoIndent(bool value);
	void setSmartIndent(bool value);
    void setModifyingTabDist(bool modifying);
    void setStyleBuffer(const std::shared_ptr<TextBuffer> &buffer);

public:
    int TextDMinFontWidth() const;
    int TextDMaxFontWidth() const;
    int TextNumVisibleLines() const;
    int64_t TextFirstVisibleLine() const;
    int TextVisibleWidth() const;    
    QColor getBackgroundPixel() const;
    QColor getForegroundPixel() const;
    QFont getFont() const;
    int getEmulateTabs() const;
    int getLineNumCols() const;
    int64_t getBufferLinesCount() const;
    int getFontHeight() const;
    int fontDescent() const;
    int fontAscent() const;
    QTimer *cursorBlinkTimer() const;
    const std::shared_ptr<TextBuffer> &getStyleBuffer() const;
    int getLineNumWidth() const;
    int getLineNumLeft() const;
    int getRows() const;
    int getMarginHeight() const;
    int getMarginWidth() const;

protected:
	bool focusNextPrevChild(bool next) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
	virtual void mouseTripleClickEvent(QMouseEvent *event);
	virtual void mouseQuadrupleClickEvent(QMouseEvent *event);

private Q_SLOTS:
	void cursorBlinkTimerTimeout();
	void autoScrollTimerTimeout();
	void clickTimeout();
	void verticalScrollBar_valueChanged(int value);
	void horizontalScrollBar_valueChanged(int value);

private:
	bool clickTracker(QMouseEvent *event, bool inDoubleClickHandler);

public Q_SLOTS:
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
    void RemoveWidgetHighlightEx();
    void TextDMaintainAbsLineNum(bool state);
    int TextDShowCalltip(const QString &text, bool anchored, CallTipPosition pos, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode);
    TextCursor TextDStartOfLine(TextCursor pos) const;
    TextCursor TextDEndOfLine(TextCursor pos, bool startPosIsLineStart) const;
    TextCursor TextDCountBackwardNLines(TextCursor startPos, int64_t nLines) const;
	void TextDRedisplayRect(int left, int top, int width, int height);
    void TextDRedisplayRect(const QRect &rect);
    TextCursor TextDCountForwardNLines(TextCursor startPos, int64_t nLines, bool startPosIsLineStart) const;
    bool TextDPositionToXY(TextCursor pos, int *x, int *y) const;
    bool TextDPositionToXY(TextCursor pos, QPoint *coord) const;
    void TextDKillCalltip(int id);
    int TextDGetCalltipID(int id) const;
	void TextDSetColors(const QColor &textFgP, const QColor &textBgP, const QColor &selectFgP, const QColor &selectBgP, const QColor &hiliteFgP, const QColor &hiliteBgP, const QColor &lineNoFgP, const QColor &cursorFgP);
    void TextDXYToUnconstrainedPosition(const QPoint &coord, int64_t *row, int64_t *column) const;
    TextCursor TextDXYToPosition(const QPoint &coord) const;
    int64_t TextDOffsetWrappedColumn(int64_t row, int64_t column) const;
    void TextDGetScroll(int64_t *topLineNum, int *horizOffset);
    bool TextDInSelection(const QPoint &p) const;
    TextCursor TextGetCursorPos() const;
    bool TextDPosToLineAndCol(TextCursor pos, int64_t *lineNum, int64_t *column);
    void TextDSetScroll(int64_t topLineNum, int horizOffset);
    void TextSetCursorPos(TextCursor pos);
    void TextDAttachHighlightData(const std::shared_ptr<TextBuffer> &styleBuffer, const std::vector<StyleTableEntry> &styleTable, char unfinishedStyle, unfinishedStyleCBProcEx unfinishedHighlightCB, void *user);
    TextCursor TextFirstVisiblePos() const;
    TextCursor TextLastVisiblePos() const;
    void TextDSetFont(const QFont &font);
    void TextDSetWrapMode(bool wrap, int wrapMargin);
    void textDRedisplayRange(TextCursor start, TextCursor end);
	void TextDResize(int width, int height);
    int64_t TextDCountLines(TextCursor startPos, TextCursor endPos, bool startPosIsLineStart);
	void TextDSetCursorStyle(CursorStyles style);
	void TextDUnblankCursor();
	void TextDBlankCursor();
	void TextDRedrawCalltip(int calltipID);
    void TextDSetupBGClassesEx(const QString &str);
    bool TextDMoveRight();
    bool TextDMoveLeft();
    bool TextDMoveUp(bool absolute);
    bool TextDMoveDown(bool absolute);
    void TextDSetInsertPosition(TextCursor newPos);
	void TextDMakeInsertPosVisible();
	void TextDMakeSelectionVisible();
	void TextInsertAtCursorEx(view::string_view chars, bool allowPendingDelete, bool allowWrap);
	void TextDOverstrikeEx(view::string_view text);
	void TextDInsertEx(view::string_view text);
    void TextCutClipboard();
	void TextPasteClipboard();
	void TextColPasteClipboard();
    int64_t TextDOffsetWrappedRow(int row) const;
	void TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft);
    int64_t TextDPreferredColumn(int *visLineNum, TextCursor *lineStartPos);
    TextCursor TextDPosOfPreferredCol(int64_t column, TextCursor lineStartPos);
    std::string TextGetWrapped(TextCursor startPos, TextCursor endPos);
    int getWrapMargin() const;
    int getColumns() const;
    TextCursor TextDLineAndColToPos(int64_t lineNum, int64_t column);
    QRect getRect() const;
    TextBuffer *TextGetBuffer() const;

public:
    void bufPreDeleteCallback(TextCursor pos, int64_t nDeleted);
    void bufModifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText);

private:
    void callMovedCBs();
    void updateFontHeightMetrics(const QFont &font);
    void updateFontWidthMetrics(const QFont &font);
    void measureDeletedLines(TextCursor pos, int64_t nDeleted);
    void wrappedLineCounter(const TextBuffer *buf, TextCursor startPos, TextCursor maxPos, int64_t maxLines, bool startPosIsLineStart, TextCursor *retPos, int64_t *retLines, TextCursor *retLineStart, TextCursor *retLineEnd) const;
    int64_t widthInPixels(char ch, int64_t colNum) const;
    int stringWidth(int length) const;
    void findWrapRangeEx(view::string_view deletedText, TextCursor pos, int64_t nInserted, int64_t nDeleted, TextCursor *modRangeStart, TextCursor *modRangeEnd, int64_t *linesInserted, int64_t *linesDeleted);
    void updateLineStarts(TextCursor pos, int64_t charsInserted, int64_t charsDeleted, int64_t linesInserted, int64_t linesDeleted, bool *scrolled);
	void hideOrShowHScrollBar();
    void calcLineStarts(int startLine, int endLine);
	void calcLastChar();
	bool maintainingAbsTopLineNum() const;
	void resetAbsLineNum();
	void updateVScrollBarRange();
    void offsetAbsLineNum(TextCursor oldFirstChar);
    void findLineEnd(TextCursor startPos, int64_t startPosIsLineStart, TextCursor *lineEnd, TextCursor *nextLineStart);
    bool updateHScrollBarRange();
    bool emptyLinesVisible() const;
    bool posToVisibleLineNum(TextCursor pos, int *lineNum) const;
    int64_t measureVisLine(int visLineNum) const;
    int64_t visLineLength(int visLineNum) const;
    bool wrapUsesCharacter(TextCursor lineEndPos) const;
    void extendRangeForStyleMods(TextCursor *start, TextCursor *end);
    void redrawLineNumbers(QPainter *painter);
    void redrawLineNumbersEx();
    void redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip);
    void redisplayLineEx(int visLineNum, int64_t leftCharIndex, int64_t rightCharIndex);
    uint32_t styleOfPos(TextCursor lineStartPos, int64_t lineLen, int64_t lineIndex, int64_t dispIndex, int thisChar) const;
    void drawString(QPainter *painter, uint32_t style, int64_t x, int y, int64_t toX, const char *string, long nChars);
    void drawCursor(QPainter *painter, int x, int y);
    QColor getRangesetColor(int ind, QColor bground) const;
    void setScroll(int64_t topLineNum, int horizOffset, bool updateVScrollBar, bool updateHScrollBar);
    void offsetLineStarts(int64_t newTopLineNum);
	void cancelDrag();
	void checkAutoShowInsertPos();
	void CancelBlockDrag();
	void callCursorMovementCBs();
    void checkMoveSelectionChange(EventFlags flags, TextCursor startPos);
    void keyMoveExtendSelection(TextCursor origPos, bool rectangular);
	bool checkReadOnly() const;
	void simpleInsertAtCursorEx(view::string_view chars, bool allowPendingDelete);
    bool pendingSelection() const;
    std::string wrapTextEx(view::string_view startLine, view::string_view text, int64_t bufOffset, int wrapMargin, int64_t *breakBefore);
    int wrapLine(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, TextCursor limitPos, TextCursor *breakAt, int64_t *charsAdded);
    std::string createIndentStringEx(TextBuffer *buf, int64_t bufOffset, TextCursor lineStartPos, TextCursor lineEndPos, int *column);
    bool deleteEmulatedTab();
	bool deletePendingSelection();
    TextCursor startOfWord(TextCursor pos) const;
    TextCursor endOfWord(TextCursor pos) const;
    boost::optional<TextCursor> spanBackward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const;
    boost::optional<TextCursor> spanForward(TextBuffer *buf, TextCursor startPos, view::string_view searchChars, bool ignoreSpace) const;
	QShortcut *createShortcut(const QString &name, const QKeySequence &keySequence, const char *member);
	void CopyToClipboard();
	void InsertClipboard(bool isColumnar);
	void InsertPrimarySelection(bool isColumnar);
    void xyToUnconstrainedPos(int x, int y, int64_t *row, int64_t *column, PositionTypes posType) const;
    void xyToUnconstrainedPos(const QPoint &pos, int64_t *row, int64_t *column, PositionTypes posType) const;
	void selectWord(int pointerX);
	void selectLine();
    TextCursor xyToPos(int x, int y, PositionTypes posType) const;
    TextCursor xyToPos(const QPoint &pos, PositionTypes posType) const;
	void endDrag();
    void adjustSelection(const QPoint &coord);
    void checkAutoScroll(const QPoint &coord);
	void FinishBlockDrag();
	void BeginBlockDrag();
    void BlockDragSelection(const QPoint &pos, BlockDragTypes dragType);
    void adjustSecondarySelection(const QPoint &coord);
	void MovePrimarySelection(bool isColumnar);
	void ExchangeSelections();
    int64_t getAbsTopLineNum() const;
	CursorStyles getCursorStyle() const;
    void showResizeNotification();

private:
    CursorStyles cursorStyle_       = CursorStyles::Normal;
    DragStates dragState_           = NOT_CLICKED;    // Why is the mouse being dragged and what is being acquired
    QColor cursorFGPixel_           = Qt::black;
    QColor highlightBGPixel_        = Qt::red;
    QColor highlightFGPixel_        = Qt::white;      // Highlight colors are used when flashing matching parens
    QColor lineNumFGPixel_          = Qt::black;      // Color for drawing line numbers
    QLabel *resizeWidget_           = nullptr;
    QMenu *bgMenu_                  = nullptr;
    QPoint cursor_                  = { -100, -100 }; // X pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
    QTimer *resizeTimer_            = nullptr;
    QVector<TextCursor> lineStarts_ = { TextCursor() };
    TextCursor cursorPos_           = {};
    TextCursor cursorToHint_        = NO_HINT;        // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
    TextCursor firstChar_           = {};             // Buffer positions of first and last displayed character (lastChar points either to a newline or one character beyond the end of the buffer)
    TextCursor lastChar_            = {};
    int64_t absTopLineNum_          = 1;              // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
    int64_t cursorPreferredCol_     = -1;             // Column for vert. cursor movement
    int64_t nBufferLines_           = 0;              // # of newlines in the buffer
    int64_t topLineNum_             = 1;              // Line number of top displayed line of file (first line of file is 1)
    int clickCount_                 = 0;
    int emTabsBeforeCursor_         = 0;              // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
    int horizOffset_                = 0;              // Horizontal scroll pos. in pixels
    int lineNumLeft_                = 0;
    int lineNumWidth_               = 0;
    int nLinesDeleted_              = 0;              // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
    int P_lineNumCols               = 0;
    int P_marginHeight              = DefaultVMargin;
    int P_marginWidth               = DefaultHMargin;
    int nVisibleLines_              = 1;              // # of visible (displayed) lines
    bool cursorOn_                  = false;
    bool modifyingTabDist_          = false;          // Whether tab distance is being modified
    bool needAbsTopLineNum_         = false;          // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
    bool pointerHidden_             = false;          // true if the mouse pointer is hidden
    bool suppressResync_            = false;          // Suppress resynchronization of line starts during buffer updates
    bool P_autoShowInsertPos        = true;
    bool P_pendingDelete            = true;
    bool showTerminalSizeHint_      = true;

private:    
    CallTip calltip_;
    QFont  font_;
    QPoint btnDownCoord_;                          // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
    QPoint clickPos_;    
    QPointer<CallTipWidget> calltipWidget_;
    QPoint mouseCoord_;                            // Last known mouse position in drag operation (for autoscroll)
    std::shared_ptr<TextBuffer> styleBuffer_;      // Optional parallel buffer containing color and font information
    std::unique_ptr<TextBuffer> dragOrigBuf_;      // backup buffer copy used during block dragging of selections    
    std::vector<QColor> bgClassPixel_;             // table of colors for each BG class
    std::vector<uint8_t> bgClass_;                 // obtains index into bgClassPixel[]
    std::vector<StyleTableEntry> styleTable_;      // Table of fonts and colors for coloring/syntax-highlighting
    BlockDragTypes dragType_;                       // style of block drag operation    
    DocumentWidget *document_;
    TextCursor anchor_;                             // Anchor for drag operations
    TextCursor dragInsertPos_;                      // location where text being block dragged was last inserted
    TextCursor dragSourceDeletePos_;                // location from which move source text was removed at start of drag
    int ascent_;                                    // Composite ascent and descent for primary font + all-highlight fonts
    int descent_;
    int dragXOffset_;                               // offsets between cursor location and actual insertion point in drag
    int dragYOffset_;                               // offsets between cursor location and actual insertion point in drag
    int fixedFontWidth_;                            // Font width if all current fonts are fixed and match in width, else -1
    int64_t dragDeleted_;                           // # of characters deleted ""
    int64_t dragInserted_;                          // # of characters inserted at drag destination in last drag position
    int64_t dragNLines_;                            // # of newlines in text being drag'd
    int64_t dragRectStart_;                         // rect. offset ""
    int64_t dragSourceDeleted_;                     // # of chars. deleted ""
    int64_t dragSourceInserted_;                    // # of chars. inserted when move source text was deleted
    int64_t rectAnchor_;                            // Anchor for rectangular drag operations
    uint32_t unfinishedStyle_;                      // Style buffer entry which triggers on-the-fly reparsing of region

    QSize       size_;
    std::string P_delimiters;

    // TODO(eteran): maybe use QWidget::contentsMargins
    QRect rect_;
    QTimer *autoScrollTimer_;
    QTimer *clickTimer_;
    QTimer *cursorBlinkTimer_;
    TextBuffer *buffer_;                            // Contains text to be displayed
    unfinishedStyleCBProcEx unfinishedHighlightCB_; // Callback to parse "unfinished" regions
    void *highlightCBArg_;                          // Arg to unfinishedHighlightCB

private:
    bool P_heavyCursor;
    bool P_autoWrapPastedText;
    bool P_colorizeHighlightedText;
    bool P_autoWrap;
    bool P_continuousWrap;
    bool P_autoIndent;
    bool P_smartIndent;
    bool P_overstrike;
    bool P_readOnly;
    bool P_hidePointer;
    int P_rows;
    int P_columns;
    int P_cursorBlinkRate;
    int P_wrapMargin;
    int P_emulateTabs;
    int P_cursorVPadding;

private:
    std::vector<QPair<cursorMovedCBEx, void *>> movedCallbacks_;
    std::vector<QPair<dragStartCBEx, void *>>   dragStartCallbacks_;
    std::vector<QPair<dragEndCBEx, void *>>     dragEndCallbacks_;
    std::vector<QPair<smartIndentCBEx, void *>> smartIndentCallbacks_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextArea::EventFlags)

#endif
