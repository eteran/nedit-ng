
#ifndef TEXT_AREA_H_
#define TEXT_AREA_H_

#include "BlockDragTypes.h"
#include "CallTip.h"
#include "CursorStyles.h"
#include "TextBufferFwd.h"
#include "DragStates.h"
#include "StyleTableEntry.h"
#include "util/string_view.h"
#include <QAbstractScrollArea>
#include <QColor>
#include <QFlags>
#include <QFont>
#include <QPointer>
#include <QRect>
#include <QTime>
#include <memory>
#include <vector>

class CallTipWidget;
class TextArea;
class DocumentWidget;
struct DragEndEvent;
struct SmartIndentEvent;

class QMenu;
class QPoint;
class QShortcut;
class QTimer;

constexpr int NO_HINT = -1;

using unfinishedStyleCBProcEx = void (*)(const TextArea *, int, const void *);
using cursorMovedCBEx         = void (*)(TextArea *, void *);
using dragStartCBEx           = void (*)(TextArea *, void *);
using dragEndCBEx             = void (*)(TextArea *, DragEndEvent *, void *);
using smartIndentCBEx         = void (*)(TextArea *, SmartIndentEvent *, void *);

class TextArea : public QAbstractScrollArea {
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

public:
    static constexpr int DefaultVMargin = 2;
    static constexpr int DefaultHMargin = 2;

public:
    TextArea(DocumentWidget *document, TextBuffer *buffer, QFont fontStruct);
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
	void setWordDelimiters(const QString &delimiters);
	void setBacklightCharTypes(const QString &charTypes);
    void setAutoShowInsertPos(bool value);
    void setEmulateTabs(bool value);
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
    void setModifyingTabDist(int tabDist);
    void setStyleBuffer(const std::shared_ptr<TextBuffer> &buffer);

public:
    int TextDMinFontWidth(bool considerStyles) const;
    int TextDMaxFontWidth(bool considerStyles) const;
    int TextNumVisibleLines() const;
    int TextFirstVisibleLine() const;
    int TextVisibleWidth() const;    
    QColor getBackgroundPixel() const;
    QColor getForegroundPixel() const;
    QFont getFont() const;
    int getEmulateTabs() const;
    int getLineNumCols() const;
    int getBufferLinesCount() const;
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
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
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
    void beginningOfSelectionAP(EventFlags flags = NoneFlag);
	void forwardCharacterAP(EventFlags flags = NoneFlag);
	void backwardCharacterAP(EventFlags flags = NoneFlag);
	void processUpAP(EventFlags flags = NoneFlag);
	void processDownAP(EventFlags flags = NoneFlag);
	void newlineAP(EventFlags flags = NoneFlag);
	void deletePreviousCharacterAP(EventFlags flags = NoneFlag);
	void processCancelAP(EventFlags flags = NoneFlag);
	void beginningOfLineAP(EventFlags flags = NoneFlag);
	void deletePreviousWordAP(EventFlags flags = NoneFlag);
	void endOfLineAP(EventFlags flags = NoneFlag);
	void deleteNextCharacterAP(EventFlags flags = NoneFlag);
	void newlineAndIndentAP(EventFlags flags = NoneFlag);
	void newlineNoIndentAP(EventFlags flags = NoneFlag);
	void toggleOverstrikeAP(EventFlags flags = NoneFlag);
	void keySelectAP(EventFlags flags = NoneFlag);
	void processShiftDownAP(EventFlags flags = NoneFlag);
	void processShiftUpAP(EventFlags flags = NoneFlag);
	void cutClipboardAP(EventFlags flags = NoneFlag);
	void copyClipboardAP(EventFlags flags = NoneFlag);
	void pasteClipboardAP(EventFlags flags = NoneFlag);
	void copyPrimaryAP(EventFlags flags = NoneFlag);
	void beginningOfFileAP(EventFlags flags = NoneFlag);
	void endOfFileAP(EventFlags flags = NoneFlag);
	void backwardWordAP(EventFlags flags = NoneFlag);
	void forwardWordAP(EventFlags flags = NoneFlag);
	void forwardParagraphAP(EventFlags flags = NoneFlag);
	void backwardParagraphAP(EventFlags flags = NoneFlag);
	void processTabAP(EventFlags flags = NoneFlag);
	void selectAllAP(EventFlags flags = NoneFlag);
	void deselectAllAP(EventFlags flags = NoneFlag);
	void deleteToStartOfLineAP(EventFlags flags = NoneFlag);
	void deleteToEndOfLineAP(EventFlags flags = NoneFlag);
	void cutPrimaryAP(EventFlags flags = NoneFlag);
	void pageLeftAP(EventFlags flags = NoneFlag);
	void pageRightAP(EventFlags flags = NoneFlag);
	void nextPageAP(EventFlags flags = NoneFlag);
	void previousPageAP(EventFlags flags = NoneFlag);
    void deleteSelectionAP(EventFlags flags = NoneFlag);
    void deleteNextWordAP(EventFlags flags = NoneFlag);
    void endOfSelectionAP(EventFlags flags = NoneFlag);
    void scrollUpAP(int count, ScrollUnits units = ScrollUnits::Lines, EventFlags flags = NoneFlag);
    void scrollDownAP(int count, ScrollUnits units = ScrollUnits::Lines, EventFlags flags = NoneFlag);
    void scrollRightAP(int pixels, EventFlags flags = NoneFlag);
    void scrollLeftAP(int pixels, EventFlags flags = NoneFlag);
    void scrollToLineAP(int line, EventFlags flags = NoneFlag);
    void insertStringAP(const QString &string, EventFlags flags = NoneFlag);
    void selfInsertAP(const QString &string, EventFlags flags = NoneFlag);

private Q_SLOTS:
	// mouse related events
	void moveDestinationAP(QMouseEvent *event);
	void extendStartAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void extendAdjustAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void mousePanAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void copyToOrEndDragAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void copyToAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryOrDragStartAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryStartAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryOrDragAdjustAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void secondaryAdjustAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void moveToOrEndDragAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void moveToAP(QMouseEvent *event, EventFlags flags = NoneFlag);
	void exchangeAP(QMouseEvent *event, EventFlags flags = NoneFlag);

public:
    void TextDMaintainAbsLineNum(bool state);
    int TextDShowCalltip(const QString &text, bool anchored, int pos, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignStrict alignMode);
	int TextDStartOfLine(int pos) const;
	int TextDEndOfLine(int pos, bool startPosIsLineStart);
	int TextDCountBackwardNLines(int startPos, int nLines);
	void TextDRedisplayRect(int left, int top, int width, int height);
    void TextDRedisplayRect(const QRect &rect);
    int TextDCountForwardNLines(int startPos, int nLines, bool startPosIsLineStart);
    int TextDPositionToXY(int pos, int *x, int *y);
    int TextDPositionToXY(int pos, QPoint *coord);
    void TextDKillCalltip(int id);
    int TextDGetCalltipID(int id) const;
	void TextDSetColors(const QColor &textFgP, const QColor &textBgP, const QColor &selectFgP, const QColor &selectBgP, const QColor &hiliteFgP, const QColor &hiliteBgP, const QColor &lineNoFgP, const QColor &cursorFgP);
    void TextDXYToUnconstrainedPosition(const QPoint &coord, int *row, int *column);
    int TextDXYToPosition(const QPoint &coord);
	int TextDOffsetWrappedColumn(int row, int column);
	void TextDGetScroll(int *topLineNum, int *horizOffset);
    bool TextDInSelection(const QPoint &p);
    int TextGetCursorPos() const;
	int TextDGetInsertPosition() const;
	int TextDPosToLineAndCol(int pos, int *lineNum, int *column);
    void TextDSetScroll(int topLineNum, int horizOffset);
    void TextSetCursorPos(int pos);
    void TextDAttachHighlightData(const std::shared_ptr<TextBuffer> &styleBuffer, const std::vector<StyleTableEntry> &styleTable, char unfinishedStyle, unfinishedStyleCBProcEx unfinishedHighlightCB, void *user);
    int TextFirstVisiblePos() const;
    int TextLastVisiblePos() const;
    void TextDSetFont(const QFont &font);
    void TextDSetWrapMode(bool wrap, int wrapMargin);
	void textDRedisplayRange(int start, int end);
	void TextDResize(int width, int height);
	int TextDCountLines(int startPos, int endPos, int startPosIsLineStart);
	void TextDSetCursorStyle(CursorStyles style);
	void TextDUnblankCursor();
	void TextDBlankCursor();
	void TextDRedrawCalltip(int calltipID);
	void TextDSetupBGClassesEx(const QString &str);
    void TextDSetupBGClasses(const QString &s, std::vector<QColor> *pp_bgClassPixel, std::vector<uint8_t> *pp_bgClass, const QColor &bgPixelDefault);
	int TextDMoveRight();
	int TextDMoveLeft();
	int TextDMoveUp(bool absolute);
	int TextDMoveDown(bool absolute);
	void TextDSetInsertPosition(int newPos);
	void TextDMakeInsertPosVisible();
	void TextInsertAtCursorEx(view::string_view chars, bool allowPendingDelete, bool allowWrap);
	void TextDOverstrikeEx(view::string_view text);
	void TextDInsertEx(view::string_view text);
    void TextCutClipboard();
	void TextPasteClipboard();
	void TextColPasteClipboard();
	int TextDOffsetWrappedRow(int row) const;
	void TextDSetLineNumberArea(int lineNumLeft, int lineNumWidth, int textLeft);
	int TextDPreferredColumn(int *visLineNum, int *lineStartPos);
	int TextDPosOfPreferredCol(int column, int lineStartPos);
    std::string TextGetWrappedEx(int startPos, int endPos);
    int getWrapMargin() const;
    int getColumns() const;
    int TextDLineAndColToPos(int lineNum, int column);
    QRect getRect() const;
    TextBuffer *TextGetBuffer() const;

public:
	void bufPreDeleteCallback(int pos, int nDeleted);
	void bufModifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText);

private:
    void callMovedCBs();
    void updateFontHeightMetrics(const QFont &font);
    void updateFontWidthMetrics(const QFont &font);
	void measureDeletedLines(int pos, int nDeleted);
    void wrappedLineCounter(const TextBuffer *buf, int startPos, int maxPos, int maxLines, bool startPosIsLineStart, int styleBufOffset, int *retPos, int *retLines, int *retLineStart, int *retLineEnd) const;
    int measurePropChar(char ch, int colNum, int pos) const;
    int stringWidth(const char *string, int length, int style) const;
	void findWrapRangeEx(view::string_view deletedText, int pos, int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd, int *linesInserted, int *linesDeleted);
	void updateLineStarts(int pos, int charsInserted, int charsDeleted, int linesInserted, int linesDeleted, int *scrolled);
	void hideOrShowHScrollBar();
	void calcLineStarts(int startLine, int endLine);
	void calcLastChar();
	bool maintainingAbsTopLineNum() const;
	void resetAbsLineNum();
	void updateVScrollBarRange();
	void offsetAbsLineNum(int oldFirstChar);
	void findLineEnd(int startPos, int startPosIsLineStart, int *lineEnd, int *nextLineStart);
    bool updateHScrollBarRange();
    bool emptyLinesVisible() const;
	int posToVisibleLineNum(int pos, int *lineNum);
	void blankCursorProtrusions();
	int measureVisLine(int visLineNum);
	int visLineLength(int visLineNum);
	int wrapUsesCharacter(int lineEndPos);
	void extendRangeForStyleMods(int *start, int *end);
    void redrawLineNumbers(QPainter *painter);
    void redrawLineNumbersEx();
    void redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex);
	void redisplayLineEx(int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex);
	int styleOfPos(int lineStartPos, int lineLen, int lineIndex, int dispIndex, int thisChar);
    void drawString(QPainter *painter, int style, int x, int y, int toX, char *string, long nChars);
	void drawCursor(QPainter *painter, int x, int y);
	QColor getRangesetColor(int ind, QColor bground);
	void setScroll(int topLineNum, int horizOffset, bool updateVScrollBar, bool updateHScrollBar);
	void offsetLineStarts(int newTopLineNum);
	void cancelDrag();
	void checkAutoShowInsertPos();
	void CancelBlockDrag();
	void callCursorMovementCBs();
	void checkMoveSelectionChange(EventFlags flags, int startPos);
	void keyMoveExtendSelection(int origPos, bool rectangular);
	bool checkReadOnly() const;
	void simpleInsertAtCursorEx(view::string_view chars, bool allowPendingDelete);
	int pendingSelection();
	std::string wrapTextEx(view::string_view startLine, view::string_view text, int bufOffset, int wrapMargin, int *breakBefore);
	int wrapLine(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int limitPos, int *breakAt, int *charsAdded);
	std::string createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *column);
	int deleteEmulatedTab();
	bool deletePendingSelection();
    int startOfWord(int pos) const;
    int endOfWord(int pos) const;
    bool spanBackward(TextBuffer *buf, int startPos, view::string_view searchChars, bool ignoreSpace, int *foundPos) const;
    bool spanForward(TextBuffer *buf, int startPos, view::string_view searchChars, bool ignoreSpace, int *foundPos) const;
	QShortcut *createShortcut(const QString &name, const QKeySequence &keySequence, const char *member);
	void CopyToClipboard();
	void InsertClipboard(bool isColumnar);
	void InsertPrimarySelection(bool isColumnar);
	void xyToUnconstrainedPos(int x, int y, int *row, int *column, int posType);
    void xyToUnconstrainedPos(const QPoint &pos, int *row, int *column, int posType);
	void selectWord(int pointerX);
	void selectLine();
	int xyToPos(int x, int y, int posType);
    int xyToPos(const QPoint &pos, int posType);
	void endDrag();
    void adjustSelection(const QPoint &coord);
    void checkAutoScroll(const QPoint &coord);
	void FinishBlockDrag();
	void BeginBlockDrag();
    void BlockDragSelection(const QPoint &pos, BlockDragTypes dragType);
    void adjustSecondarySelection(const QPoint &coord);
	void MovePrimarySelection(bool isColumnar);
	void ExchangeSelections();
	int getAbsTopLineNum();
	CursorStyles getCursorStyle() const;

    bool cursorOn_               = false;
    bool needAbsTopLineNum_      = false;          // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
    bool pointerHidden_          = false;          // true if the mouse pointer is hidden
    bool suppressResync_         = false;          // Suppress resynchronization of line starts during buffer updates
    CallTip calltip_;                              // The info for the calltip itself
    CursorStyles cursorStyle_    = CursorStyles::Normal;  // One of enum cursorStyles above
    DragStates dragState_        = NOT_CLICKED;    // Why is the mouse being dragged and what is being acquired
    int absTopLineNum_           = 1;              // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
    int clickCount_              = 0;
    int cursorPos_               = 0;
    int cursorPreferredCol_      = -1;             // Column for vert. cursor movement
    int cursorToHint_            = NO_HINT;        // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
    int emTabsBeforeCursor_      = 0;              // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
    int firstChar_               = 0;              // Buffer positions of first and last displayed character (lastChar points either to a newline or one character beyond the end of the buffer)
    int horizOffset_             = 0;              // Horizontal scroll pos. in pixels
    int lastChar_                = 0;
    int lineNumLeft_             = 0;
    int lineNumWidth_            = 0;
    int modifyingTabDist_        = 0;              // Whether tab distance is being modified
    int nBufferLines_            = 0;              // # of newlines in the buffer
    int nLinesDeleted_           = 0;              // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
    int nVisibleLines_           = 1;              // # of visible (displayed) lines
    int topLineNum_              = 1;              // Line number of top displayed line of file (first line of file is 1)

private:
    QColor cursorFGPixel_        = Qt::black;
    QColor highlightBGPixel_     = Qt::red;
    QColor highlightFGPixel_     = Qt::white;      // Highlight colors are used when flashing matching parens
    QColor lineNumFGPixel_       = Qt::black;      // Color for drawing line numbers
    QFont  font_;
    QMenu *bgMenu_               = nullptr;
    QPoint btnDownCoord_;                          // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
    QPoint clickPos_;
    QPoint cursor_               = { -100, -100 }; // X pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
    QPointer<CallTipWidget> calltipWidget_;
    QPoint mouseCoord_;                            // Last known mouse position in drag operation (for autoscroll)
    std::shared_ptr<TextBuffer> styleBuffer_;      // Optional parallel buffer containing color and font information
    std::unique_ptr<TextBuffer> dragOrigBuf_;      // backup buffer copy used during block dragging of selections
    QVector<int> lineStarts_ = { 0 };
    std::vector<QColor> bgClassPixel_;             // table of colors for each BG class
    std::vector<uint8_t> bgClass_;                 // obtains index into bgClassPixel[]
    std::vector<StyleTableEntry> styleTable_;        // Table of fonts and colors for coloring/syntax-highlighting

private:
    BlockDragTypes dragType_;                       // style of block drag operation
    char unfinishedStyle_;                          // Style buffer entry which triggers on-the-fly reparsing of region
    DocumentWidget *document_;
    int anchor_;                                    // Anchor for drag operations
    int ascent_;                                    // Composite ascent and descent for primary font + all-highlight fonts
    int descent_;
    int dragDeleted_;                               // # of characters deleted ""
    int dragInserted_;                              // # of characters inserted at drag destination in last drag position
    int dragInsertPos_;                             // location where text being block dragged was last inserted
    int dragNLines_;                                // # of newlines in text being drag'd
    int dragRectStart_;                             // rect. offset ""
    int dragSourceDeleted_;                         // # of chars. deleted ""
    int dragSourceDeletePos_;                       // location from which move source text was removed at start of drag
    int dragSourceInserted_;                        // # of chars. inserted when move source text was deleted
    int dragXOffset_;                               // offsets between cursor location and actual insertion point in drag
    int dragYOffset_;                               // offsets between cursor location and actual insertion point in drag
    int fixedFontWidth_;                            // Font width if all current fonts are fixed and match in width, else -1    
    int rectAnchor_;                                // Anchor for rectangular drag operations
    QRect rect_;
    QTimer *autoScrollTimer_;
    QTimer *clickTimer_;
    QTimer *cursorBlinkTimer_;
    TextBuffer *buffer_;                            // Contains text to be displayed
    unfinishedStyleCBProcEx unfinishedHighlightCB_; // Callback to parse "unfinished" regions
    void *highlightCBArg_;                          // Arg to unfinishedHighlightCB

private:
    bool        P_autoShowInsertPos  = true;
    bool        P_autoWrapPastedText = false;
    bool        P_heavyCursor        = false;
    bool        P_pendingDelete      = true;
    int         P_lineNumCols        = 0;
    int         P_marginHeight       = DefaultVMargin;
    int         P_marginWidth        = DefaultHMargin;
    QSize       size_;
    std::string P_delimiters;

private:
    bool        P_colorizeHighlightedText;
	bool        P_autoWrap;
	bool        P_continuousWrap;
	bool        P_autoIndent;
	bool        P_smartIndent;
	bool        P_overstrike;
	bool        P_readOnly;
	bool        P_hidePointer;
	int         P_rows;
	int         P_columns;
	int         P_cursorBlinkRate;
	int         P_wrapMargin;
    int         P_emulateTabs;
	int         P_cursorVPadding;

private:
    std::vector<QPair<cursorMovedCBEx, void *>> movedCallbacks_;
    std::vector<QPair<dragStartCBEx, void *>>   dragStartCallbacks_;
    std::vector<QPair<dragEndCBEx, void *>>     dragEndCallbacks_;
    std::vector<QPair<smartIndentCBEx, void *>> smartIndentCallbacks_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextArea::EventFlags)

#endif
