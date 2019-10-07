
#ifndef DOCUMENT_WIDGET_H_
#define DOCUMENT_WIDGET_H_

#include "Bookmark.h"
#include "CallTip.h"
#include "CloseMode.h"
#include "CommandSource.h"
#include "DocumentInfo.h"
#include "IndentStyle.h"
#include "LanguageMode.h"
#include "LockReasons.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "ShowMatchingStyle.h"
#include "Tags.h"
#include "TextBufferFwd.h"
#include "UndoInfo.h"
#include "Util/FileFormats.h"
#include "Util/string_view.h"
#include "WrapStyle.h"

#include "ui_DocumentWidget.h"

#include <QPointer>
#include <QProcess>
#include <QWidget>

#include <gsl/span>

#include <boost/optional.hpp>

#include <sys/stat.h>

class HighlightPattern;
class MainWindow;
class PatternSet;
class RangesetTable;
class Regex;
class Style;
class StyleTableEntry;
class TextArea;
class UndoInfo;
struct DragEndEvent;
struct MacroCommandData;
struct Program;
struct ShellCommandData;
struct SmartIndentData;
struct SmartIndentEvent;
struct WindowHighlightData;
struct HighlightData;

class QFrame;
class QLabel;
class QMenu;
class QSplitter;
class QTimer;
class QDir;

enum class Direction : uint8_t;

class DocumentWidget : public QWidget {
	Q_OBJECT
	friend class MainWindow;

public:
	enum MacroContinuationCode {
		Continue,
		Stop
	};

public:
	DocumentWidget(std::shared_ptr<DocumentInfo> &info_ptr, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	DocumentWidget(const QString &name, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DocumentWidget() override;

Q_SIGNALS:
	void documentClosed();
	void canUndoChanged(bool canUndo);
	void canRedoChanged(bool canUndo);
	void updateStatus(DocumentWidget *document, TextArea *area);
	void updateWindowReadOnly(DocumentWidget *document);
	void updateWindowTitle(DocumentWidget *document);

public:
	void movedCallback(TextArea *area);
	void dragStartCallback(TextArea *area);
	void dragEndCallback(TextArea *area, const DragEndEvent *event);
	void smartIndentCallback(TextArea *area, SmartIndentEvent *event);
	void modifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText);
	void modifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, TextArea *area);

public:
	static DocumentWidget *fromArea(TextArea *area);
	static DocumentWidget *editExistingFile(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool background);
	static std::vector<DocumentWidget *> allDocuments();

public:
	void action_Set_Fonts(const QString &fontName);
	void action_Set_Language_Mode(const QString &languageMode);
	void action_Set_Language_Mode(const QString &languageMode, bool forceNewDefaults);

public:
	DocumentWidget *open(const QString &fullpath);
	FileFormats fileFormat() const;
	HighlightPattern *findPatternOfWindow(const QString &name) const;
	IndentStyle autoIndentStyle() const;
	LockReasons lockReasons() const;
	QColor highlightBGColorOfCode(size_t hCode) const;
	QColor highlightColorValueOfCode(size_t hCode) const;
	QFont defaultFont() const;
	QString getAnySelection();
	QString getAnySelection(bool beep_on_error);
	QString getWindowDelimiters() const;
	QString highlightNameOfCode(size_t hCode) const;
	QString highlightStyleOfCode(size_t hCode) const;
	QString documentDelimiters() const;
	QString filename() const;
	QString fullPath() const;
	QString path() const;
	void setFilename(const QString &filename);
	ShowMatchingStyle showMatchingStyle() const;
	TextArea *firstPane() const;
	TextBuffer *buffer() const;
	WrapStyle wrapMode() const;
	bool highlightSyntax() const;
	bool incrementalBackup() const;
	bool makeBackupCopy() const;
	bool matchSyntaxBased() const;
	bool overstrike() const;
	bool showStatisticsLine() const;
	bool useTabs() const;
	bool userLocked() const;
	bool inSmartIndentMacros() const;
	bool readMacroFile(const QString &fileName, bool warnNotExist);
	bool readMacroString(const QString &string, const QString &errIn);
	bool checkReadOnly() const;
	bool fileChanged() const;
	bool filenameSet() const;
	bool isReadOnly() const;
	bool isTopDocument() const;
	bool modeMessageDisplayed() const;
	dev_t device() const;
	ino_t inode() const;
	int showTipString(const QString &text, bool anchored, int pos, bool lookup, Tags::SearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode);
	int findDefinitionHelperCommon(TextArea *area, const QString &value, Tags::SearchMode search_type);
	int textPanesCount() const;
	int widgetToPaneIndex(TextArea *area) const;
	int64_t styleLengthOfCodeFromPos(TextCursor pos);
	int64_t highlightLengthOfCodeFromPos(TextCursor pos);
	size_t getLanguageMode() const;
	size_t highlightCodeOfPos(TextCursor pos);
	std::unique_ptr<WindowHighlightData> createHighlightData(PatternSet *patternSet);
	std::vector<TextArea *> textPanes() const;
	void addMark(TextArea *area, QChar label);
	void doMacro(const QString &macro, const QString &errInName);
	void findDefinitionCalltip(TextArea *area, const QString &tipName);
	void gotoMatchingCharacter(TextArea *area);
	void makeSelectionVisible(TextArea *area);
	void resumeMacroExecution();
	void selectNumberedLine(TextArea *area, int64_t lineNum);
	void selectToMatchingCharacter(TextArea *area);
	void setBacklightChars(const QString &applyBacklightTypes);
	void setColors(const QColor &textFg, const QColor &textBg, const QColor &selectFg, const QColor &selectBg, const QColor &hiliteFg, const QColor &hiliteBg, const QColor &lineNoFg, const QColor &lineNoBg, const QColor &cursorFg);
	void setHighlightSyntax(bool value);
	void setIncrementalBackup(bool value);
	void setLanguageMode(size_t mode, bool forceNewDefaults);
	void setMakeBackupCopy(bool value);
	void setMatchSyntaxBased(bool value);
	void setOverstrike(bool overstrike);
	void setShowMatching(ShowMatchingStyle state);
	void setShowStatisticsLine(bool value);
	void setUseTabs(bool value);
	void setUserLocked(bool value);
	void showStatsLine(bool state);
	void abortShellCommand();
	void beginSmartIndent(bool warn);
	void cancelMacroOrLearn();
	void checkForChangesToFile();
	void clearModeMessage();
	void closePane();
	void editTaggedLocation(TextArea *area, int i);
	void endSmartIndent();
	void execAP(TextArea *area, const QString &command);
	void executeShellCommand(TextArea *area, const QString &command, CommandSource source);
	void findDefinition(TextArea *area, const QString &tagName);
	void findDefinitionHelper(TextArea *area, const QString &arg, Tags::SearchMode search_type);
	void finishMacroCmdExecution();
	void gotoAP(TextArea *area, int lineNum, int column);
	void gotoMark(TextArea *area, QChar label, bool extendSel);
	void handleUnparsedRegion(const std::shared_ptr<TextBuffer> &styleBuf, TextCursor pos) const;
	void macroBannerTimeoutProc();
	void moveDocument(MainWindow *fromWindow);
	void printString(const std::string &string, const QString &jobname);
	void printWindow(TextArea *area, bool selectedOnly);
	void raiseDocument();
	void raiseDocumentWindow();
	void raiseFocusDocumentWindow(bool focus);
	void readMacroInitFile();
	void repeatMacro(const QString &macro, int how);
	void runMacro(Program *prog);
	void setAutoIndent(IndentStyle indentStyle);
	void setAutoScroll(int margin);
	void setAutoWrap(WrapStyle wrapStyle);
	void setEmTabDistance(int distance);
	void setFileFormat(FileFormats fileFormat);
	void setPath(const QString &pathname);
	void setPath(const QDir &path);
	void setTabDistance(int distance);
	void setWrapMargin(int margin);
	void shellBannerTimeoutProc();
	void shellCmdToMacroString(const QString &command, const QString &input);
	void splitPane();
	void startHighlighting(bool warn);
	void stopHighlighting();
	void updateHighlightStyles();
	void updateSignals(MainWindow *from, MainWindow *to);

private:
	std::unique_ptr<HighlightData[]> compilePatterns(const std::vector<HighlightPattern> &patternSrc);
	std::unique_ptr<Regex> compileRegexAndWarn(const QString &re);
	void setModeMessage(const QString &message);
	void executeModMacro(SmartIndentEvent *event);
	void executeNewlineMacro(SmartIndentEvent *event);
	MacroContinuationCode continueWorkProc();
	PatternSet *findPatternsForWindow(bool warn);
	QString backupFileName() const;
	QString getWindowsMenuEntry() const;
	Style getHighlightInfo(TextCursor pos);
	StyleTableEntry *styleTableEntryOfCode(size_t hCode) const;
	TextArea *createTextArea(TextBuffer *buffer);
	bool closeFileAndWindow(CloseMode preResponse);
	bool macroWindowCloseActions();
	bool writeBackupFile();
	bool compareDocumentToFile(const QString &fileName) const;
	bool doOpen(const QString &name, const QString &path, int flags);
	bool doSave();
	bool fileWasModifiedExternally() const;
	bool includeFile(const QString &name);
	bool saveDocument();
	bool saveDocumentAs(const QString &newName, bool addWrap);
	bool writeBckVersion();
	boost::optional<TextCursor> findMatchingChar(char toMatch, Style styleToMatch, TextCursor charPos, TextCursor startLimit, TextCursor endLimit);
	int findAllMatches(TextArea *area, const QString &string);
	size_t matchLanguageMode() const;
	void abortMacroCommand();
	void attachHighlightToWidget(TextArea *area);
	void beginLearn();
	void clearRedoList();
	void clearUndoList();
	void closeDocument();
	void determineLanguageMode(bool forceNewDefaults);
	void doShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, CommandSource source);
	void doShellMenuCmd(MainWindow *inWindow, TextArea *area, const MenuItem &item, CommandSource source);
	void execCursorLine(TextArea *area, CommandSource source);
	void finishLearning();
	void flashMatchingChar(TextArea *area);
	void freeHighlightingData();
	void redo();
	void refreshMenuToggleStates();
	void refreshTabState();
	void refreshWindowStates();
	void removeBackupFile() const;
	void replay();
	void revertToSaved();
	void saveUndoInformation(TextCursor pos, int64_t nInserted, int64_t nDeleted, view::string_view deletedText);
	void setWindowModified(bool modified);
	void undo();
	void unloadLanguageModeTipsFile();
	void updateMarkTable(TextCursor pos, int64_t nInserted, int64_t nDeleted);
	void actionClose(CloseMode mode);
	void addRedoItem(UndoInfo &&redo);
	void addUndoItem(UndoInfo &&undo);
	void addWrapNewlines();
	void appendDeletedText(view::string_view deletedText, int64_t deletedLen, Direction direction);
	void cancelLearning();
	void createSelectMenu(TextArea *area, const QStringList &args);
	void documentRaised();
	void eraseFlash();
	void filterSelection(const QString &command, CommandSource source);
	void issueCommand(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, TextCursor replaceLeft, TextCursor replaceRight, CommandSource source);
	void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void reapplyLanguageMode(size_t mode, bool forceDefaults);
	void refreshMenuBar();
	void removeRedoItem();
	void removeUndoItem();
	void trimUndoList(size_t maxLength);
	void updateSelectionSensitiveMenu(QMenu *menu, const gsl::span<MenuData> &menuList, bool enabled);
	void updateSelectionSensitiveMenus(bool enabled);

public:
	std::shared_ptr<DocumentInfo> info_;

public:
	bool replaceFailed_  = false;               // flags replacements failures during multi-file replacements
	bool multiFileBusy_  = false;               // suppresses multiple beeps/dialogs during multi-file replacements
	size_t languageMode_ = PLAIN_LANGUAGE_MODE; // identifies language mode currently selected in the window

public:
	QString fontName_;                                   // names of the text fonts in use
	bool highlightSyntax_;                               // is syntax highlighting turned on?
	bool showStats_;                                     // is stats line supposed to be shown
	std::shared_ptr<MacroCommandData> macroCmdData_;     // same for macro commands
	std::shared_ptr<RangesetTable> rangesetTable_;       // current range sets
	std::unique_ptr<WindowHighlightData> highlightData_; // info for syntax highlighting

private:
	QMenu *contextMenu_ = nullptr;

private:
	QSplitter *splitter_;
	QFont font_;
	QString backlightCharTypes_;                     // what backlighting to use
	QString modeMessage_;                            // stats line banner content for learn and shell command executing modes
	QTimer *flashTimer_;                             // timer for getting rid of highlighted matching paren.
	bool backlightChars_;                            // is char backlighting turned on?
	std::map<QChar, Bookmark> markTable_;
	std::unique_ptr<ShellCommandData> shellCmdData_; // when a shell command is executing, info. about it, otherwise, nullptr
	Ui::DocumentWidget ui;

public:
	static DocumentWidget *LastCreated;
};

#endif
