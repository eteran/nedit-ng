
#ifndef TEXT_AREA_H_
#define TEXT_AREA_H_

#include "BlockDragTypes.h"
#include "CallTip.h"
#include "CursorStyles.h"
#include "DragStates.h"
#include "StyleTableEntry.h"
#include "string_view.h"
#include <QAbstractScrollArea>
#include <QColor>
#include <QFlags>
#include <QFont>
#include <QPointer>
#include <QRect>
#include <QTime>
#include <QVector>
#include <memory>

class CallTipWidget;
class QMenu;
class QPoint;
class QShortcut;
class QTimer;
class TextArea;
class TextBuffer;
class DocumentWidget;
struct DragEndEvent;
struct SmartIndentEvent;

typedef void (*unfinishedStyleCBProcEx)(const TextArea *, int, const void *);
typedef void (*cursorMovedCBEx)(TextArea *, void *);
typedef void (*dragStartCBEx)(TextArea *, void *);
typedef void (*dragEndCBEx)(TextArea *, DragEndEvent *, void *);
typedef void (*smartIndentCBEx)(TextArea *, SmartIndentEvent *, void *);

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
    TextArea(DocumentWidget *document,
             QWidget *parent,
            int left,
            int top,
            int width,
            int height,
            int lineNumLeft,
            int lineNumWidth,
            TextBuffer *buffer,
            QFont fontStruct);

    TextArea(const TextArea &other) = delete;
	TextArea& operator=(const TextArea &) = delete;
    virtual ~TextArea() noexcept;

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
	void setEmulateTabs(int value);
	void setWrapMargin(int value);
	void setLineNumCols(int value);
	void setForegroundPixel(const QColor &pixel);
    void setBackgroundPixel(const QColor &pixel);
	void setReadOnly(bool value);
	void setOverstrike(bool value);
	void setCursorVPadding(int value);
    void setFont(const QFont &font);
	void setContinuousWrap(bool value);
	void setAutoWrap(bool value);
	void setAutoIndent(bool value);
	void setSmartIndent(bool value);
    void setModifyingTabDist(int tabDist);
    void setStyleBuffer(TextBuffer *buffer);

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
    TextBuffer *getStyleBuffer() const;
    int getLineNumWidth() const;
    int getLineNumLeft() const;
    int getRows() const;
    int getMarginHeight() const;
    int getMarginWidth() const;

protected:
	virtual bool focusNextPrevChild(bool next) override;
	virtual void contextMenuEvent(QContextMenuEvent *event) override;
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void focusInEvent(QFocusEvent *event) override;
	virtual void focusOutEvent(QFocusEvent *event) override;
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
    int TextDShowCalltip(const QString &text, bool anchored, int pos, int hAlign, int vAlign, int alignMode);
	int TextDStartOfLine(int pos) const;
	int TextDEndOfLine(int pos, bool startPosIsLineStart);
	int TextDCountBackwardNLines(int startPos, int nLines);
	void TextDRedisplayRect(int left, int top, int width, int height);
    void TextDRedisplayRect(const QRect &rect);
	int TextDCountForwardNLines(const int startPos, const unsigned nLines, bool startPosIsLineStart);
	void TextSetBuffer(TextBuffer *buffer);
    int TextDPositionToXY(int pos, int *x, int *y);
    int TextDPositionToXY(int pos, QPoint *coord);
	void TextDKillCalltip(int calltipID);
    int TextDGetCalltipID(int calltipID) const;
	void TextDSetColors(const QColor &textFgP, const QColor &textBgP, const QColor &selectFgP, const QColor &selectBgP, const QColor &hiliteFgP, const QColor &hiliteBgP, const QColor &lineNoFgP, const QColor &cursorFgP);
    void TextDXYToUnconstrainedPosition(const QPoint &coord, int *row, int *column);
    int TextDXYToPosition(const QPoint &coord);
	int TextDOffsetWrappedColumn(int row, int column);
	void TextDGetScroll(int *topLineNum, int *horizOffset);
    int TextDInSelection(const QPoint &p);
    int TextGetCursorPos() const;
	int TextDGetInsertPosition() const;
	int TextDPosToLineAndCol(int pos, int *lineNum, int *column);
    void TextDSetScroll(int topLineNum, int horizOffset);
    void TextSetCursorPos(int pos);
    void TextDAttachHighlightData(TextBuffer *styleBuffer, StyleTableEntry *styleTable, int nStyles, char unfinishedStyle, unfinishedStyleCBProcEx unfinishedHighlightCB, void *cbArg);
    int TextFirstVisiblePos() const;
    int TextLastVisiblePos() const;
    void TextDSetFont(const QFont &font);
	void TextDSetWrapMode(int wrap, int wrapMargin);
	void textDRedisplayRange(int start, int end);
	void TextDResize(int width, int height);
	int TextDCountLines(int startPos, int endPos, int startPosIsLineStart);
	void TextDSetBuffer(TextBuffer *buffer);
	void TextDSetCursorStyle(CursorStyles style);
	void TextDUnblankCursor();
	void TextDBlankCursor();
	void TextDRedrawCalltip(int calltipID);
	void TextDSetupBGClassesEx(const QString &str);
	void TextDSetupBGClasses(const QString &s, QVector<QColor> *pp_bgClassPixel, QVector<uint8_t> *pp_bgClass, const QColor &bgPixelDefault);
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
	void modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);

private:
	void measureDeletedLines(int pos, int nDeleted);
    void wrappedLineCounter(const TextBuffer *buf, int startPos, int maxPos, int maxLines, bool startPosIsLineStart, int styleBufOffset, int *retPos, int *retLines, int *retLineStart, int *retLineEnd) const;
    int measurePropChar(const char c, int colNum, int pos) const;
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
	int emptyLinesVisible() const;
	int posToVisibleLineNum(int pos, int *lineNum);
	void blankCursorProtrusions();
	int measureVisLine(int visLineNum);
	int visLineLength(int visLineNum);
	int wrapUsesCharacter(int lineEndPos);
	void extendRangeForStyleMods(int *start, int *end);
	void redrawLineNumbers(QPainter *painter, bool clearAll);
	void redrawLineNumbersEx(bool clearAll);
	void redisplayLine(QPainter *painter, int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex);
	void redisplayLineEx(int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex);
	int styleOfPos(int lineStartPos, int lineLen, int lineIndex, int dispIndex, int thisChar);
    void drawString(QPainter *painter, int style, int x, int y, int toX, char *string, int nChars);
	void drawCursor(QPainter *painter, int x, int y);
	QColor getRangesetColor(int ind, QColor bground);
	void setScroll(int topLineNum, int horizOffset, bool updateVScrollBar, bool updateHScrollBar);
	void StopHandlingXSelections();
	void offsetLineStarts(int newTopLineNum);
	void cancelDrag();
	void checkAutoShowInsertPos();
	void CancelBlockDrag();
	void callCursorMovementCBs();
	void checkMoveSelectionChange(EventFlags flags, int startPos);
	void keyMoveExtendSelection(int origPos, bool rectangular);
    void TakeMotifDestination();
	bool checkReadOnly() const;
	void simpleInsertAtCursorEx(view::string_view chars, bool allowPendingDelete);
	int pendingSelection();
	std::string wrapTextEx(view::string_view startLine, view::string_view text, int bufOffset, int wrapMargin, int *breakBefore);
	int wrapLine(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int limitPos, int *breakAt, int *charsAdded);
	std::string createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *length, int *column);
	std::string createIndentStringEx(TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *column);
	int deleteEmulatedTab();
	bool deletePendingSelection();
	int startOfWord(int pos);
	int endOfWord(int pos);
    bool spanBackward(TextBuffer *buf, int startPos, const QByteArray &searchChars, bool ignoreSpace, int *foundPos);
    bool spanForward(TextBuffer *buf, int startPos, const QByteArray &searchChars, bool ignoreSpace, int *foundPos);
	QShortcut *createShortcut(const QString &name, const QKeySequence &keySequence, const char *member);
	void CopyToClipboard();
	void InsertClipboard(bool isColumnar);
	void InsertPrimarySelection(bool isColumnar);
	void xyToUnconstrainedPos(int x, int y, int *row, int *column, int posType);
	void selectWord(int pointerX);
	void selectLine();
	int xyToPos(int x, int y, int posType);
	void endDrag();
    void adjustSelection(const QPoint &coord);
    void checkAutoScroll(const QPoint &coord);
	void FinishBlockDrag();
	void SendSecondarySelection(bool removeAfter);
	void BeginBlockDrag();
    void BlockDragSelection(const QPoint &pos, BlockDragTypes dragType);
    void adjustSecondarySelection(const QPoint &coord);
	void MovePrimarySelection(bool isColumnar);
	void ExchangeSelections();
	int getAbsTopLineNum();
	CursorStyles getCursorStyle() const;

private:
	QVector<QColor> bgClassPixel_;                 // table of colors for each BG class
	QVector<uint8_t> bgClass_;                    // obtains index into bgClassPixel[]
    QRect rect_;
	int lineNumLeft_;
	int lineNumWidth_;
	int cursorPos_;
	int cursorOn_;
    QPoint cursor_;                               // X pos. of last drawn cursor Note: these are used for *drawing* and are not generally reliable for finding the insert position's x/y coordinates!
	int cursorToHint_;                            // Tells the buffer modified callback where to move the cursor, to reduce the number of redraw calls
	CursorStyles cursorStyle_;                    // One of enum cursorStyles above
	int cursorPreferredCol_;                      // Column for vert. cursor movement
	int nVisibleLines_;                           // # of visible (displayed) lines
	int nBufferLines_;                            // # of newlines in the buffer
	TextBuffer *buffer_;                          // Contains text to be displayed
	TextBuffer *styleBuffer_;                     // Optional parallel buffer containing color and font information
	int firstChar_;                               // Buffer positions of first and last displayed character (lastChar points either to a newline or one character beyond the end of the buffer)
	int lastChar_;
    QVector<int> lineStarts_;
	int topLineNum_;                              // Line number of top displayed line of file (first line of file is 1)
	int absTopLineNum_;                           // In continuous wrap mode, the line number of the top line if the text were not wrapped (note that this is only maintained as needed).
	int needAbsTopLineNum_;                       // Externally settable flag to continue maintaining absTopLineNum even if it isn't needed for line # display
	int horizOffset_;                             // Horizontal scroll pos. in pixels
	int nStyles_;                                 // Number of entries in styleTable
	StyleTableEntry *styleTable_;                 // Table of fonts and colors for coloring/syntax-highlighting
	char unfinishedStyle_;                        // Style buffer entry which triggers on-the-fly reparsing of region
	unfinishedStyleCBProcEx unfinishedHighlightCB_; // Callback to parse "unfinished" regions
	void *highlightCBArg_;                        // Arg to unfinishedHighlightCB
	int ascent_;                                  // Composite ascent and descent for primary font + all-highlight fonts
	int descent_;
	int fixedFontWidth_;                          // Font width if all current fonts are fixed and match in width, else -1
    QPointer<CallTipWidget> calltipWidget_;
	CallTip calltip_;                       // The info for the calltip itself
	bool suppressResync_;                          // Suppress resynchronization of line starts during buffer updates
	int nLinesDeleted_;                           // Number of lines deleted during buffer modification (only used when resynchronization is suppressed)
	int modifyingTabDist_;                        // Whether tab distance is being modified
	bool pointerHidden_;                          // true if the mouse pointer is hidden

    // moved from textP
    std::unique_ptr<TextBuffer> dragOrigBuf_;        // backup buffer copy used during block dragging of selections
	int dragXOffset_;                // offsets between cursor location and actual insertion point in drag
	int dragYOffset_;                // offsets between cursor location and actual insertion point in drag
	BlockDragTypes dragType_;        // style of block drag operation
	int dragInsertPos_;              // location where text being block dragged was last inserted
	int dragRectStart_;              // rect. offset ""
	int dragInserted_;               // # of characters inserted at drag destination in last drag position
	int dragDeleted_;                // # of characters deleted ""
	int dragSourceDeletePos_;        // location from which move source text was removed at start of drag
	int dragSourceInserted_;         // # of chars. inserted when move source text was deleted
	int dragSourceDeleted_;          // # of chars. deleted ""
	int dragNLines_;                 // # of newlines in text being drag'd

	int anchor_;                     // Anchor for drag operations
	int rectAnchor_;                 // Anchor for rectangular drag operations
	DragStates dragState_;           // Why is the mouse being dragged and what is being acquired
    QPoint btnDownCoord_;             // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
    QPoint mouseCoord_;               // Last known mouse position in drag operation (for autoscroll)
	bool selectionOwner_;            // True if widget owns the selection
	bool motifDestOwner_;            // " "            owns the motif destination
	QTimer *autoScrollTimer_;
	QTimer *cursorBlinkTimer_;
	QTimer *clickTimer_;
	int clickCount_;
	QPoint clickPos_;
	int            emTabsBeforeCursor_; // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
    QMenu *bgMenu_;
private:
    QColor highlightFGPixel_;   // Highlight colors are used when flashing matching parens
    QColor highlightBGPixel_;
    QColor lineNumFGPixel_;     // Color for drawing line numbers
    QColor cursorFGPixel_;
    QFont  font_;

private:
	bool        P_pendingDelete;
	bool        P_autoShowInsertPos;
	bool        P_autoWrap;
	bool        P_autoWrapPastedText;
	bool        P_continuousWrap;
	bool        P_autoIndent;
	bool        P_smartIndent;
	bool        P_overstrike;
	bool        P_heavyCursor;
	bool        P_readOnly;
	bool        P_hidePointer;
	int         P_rows;
	int         P_columns;
	int         P_marginWidth;
	int         P_marginHeight;
	int         P_cursorBlinkRate;
	int         P_wrapMargin;
	int         P_emulateTabs;
	int         P_lineNumCols;
    QString     P_delimiters;
	int         P_cursorVPadding;
    QSize       size_;

private:
	QVector<QPair<cursorMovedCBEx, void *>> movedCallbacks_;
	QVector<QPair<dragStartCBEx, void *>>   dragStartCallbacks_;
	QVector<QPair<dragEndCBEx, void *>>     dragEndCallbacks_;
	QVector<QPair<smartIndentCBEx, void *>> smartIndentCallbacks_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextArea::EventFlags)

#endif
