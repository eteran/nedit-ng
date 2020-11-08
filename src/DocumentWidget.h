
#ifndef DOCUMENT_WIDGET_H_
#define DOCUMENT_WIDGET_H_

#include "Bookmark.h"
#include "CallTip.h"
#include "CloseMode.h"
#include "CommandSource.h"
#include "DocumentInfo.h"
#include "ErrorSound.h"
#include "IndentStyle.h"
#include "LanguageMode.h"
#include "LockReasons.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "RangesetTable.h"
#include "ShowMatchingStyle.h"
#include "Tags.h"
#include "TextBufferFwd.h"
#include "UndoInfo.h"
#include "Util/FileFormats.h"
#include "Util/string_view.h"
#include "Verbosity.h"
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
class Regex;
class Style;
class StyleTableEntry;
class TextArea;
class UndoInfo;
struct DragEndEvent;
struct HighlightData;
struct MacroCommandData;
struct Program;
struct ShellCommandData;
struct SmartIndentData;
struct SmartIndentEvent;
struct WindowHighlightData;

class QDir;
class QFrame;
class QLabel;
class QMenu;
class QSplitter;
class QTimer;

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
	explicit DocumentWidget(std::shared_ptr<DocumentInfo> &info_ptr, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	explicit DocumentWidget(const QString &name, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DocumentWidget() override;

Q_SIGNALS:
	void canRedoChanged(bool canUndo);
	void canUndoChanged(bool canUndo);
	void documentClosed();
	void updateStatus(DocumentWidget *document, TextArea *area);
	void updateWindowReadOnly(DocumentWidget *document);
	void updateWindowTitle(DocumentWidget *document);
	void fontChanged(DocumentWidget *document);

public:
	void dragEndCallback(TextArea *area, const DragEndEvent *event);
	void dragStartCallback(TextArea *area);
	void modifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText);
	void modifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, TextArea *area);
	void movedCallback(TextArea *area);
	void smartIndentCallback(TextArea *area, SmartIndentEvent *event);

public:
	static DocumentWidget *editExistingFile(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool background);
	static DocumentWidget *fromArea(TextArea *area);
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
	QString backlightCharTypes() const;
	QString documentDelimiters() const;
	QString filename() const;
	QString fullPath() const;
	QString getAnySelection() const;
	QString getAnySelection(ErrorSound errorSound) const;
	QString getWindowDelimiters() const;
	QString highlightNameOfCode(size_t hCode) const;
	QString highlightStyleOfCode(size_t hCode) const;
	QString path() const;
	ShowMatchingStyle showMatchingStyle() const;
	TextArea *firstPane() const;
	TextBuffer *buffer() const;
	WrapStyle wrapMode() const;
	bool backlightChars() const;
	bool checkReadOnly() const;
	bool fileChanged() const;
	bool filenameSet() const;
	bool highlightSyntax() const;
	bool inSmartIndentMacros() const;
	bool incrementalBackup() const;
	bool isReadOnly() const;
	bool isTopDocument() const;
	bool makeBackupCopy() const;
	bool matchSyntaxBased() const;
	bool modeMessageDisplayed() const;
	bool overstrike() const;
	bool readMacroFile(const QString &fileName, bool warnNotExist);
	bool readMacroString(const QString &string, const QString &errIn);
	bool showStatisticsLine() const;
	bool useTabs() const;
	bool userLocked() const;
	dev_t device() const;
	ino_t inode() const;
	int findDefinitionHelperCommon(TextArea *area, const QString &value, Tags::SearchMode search_type);
	int showTipString(const QString &text, bool anchored, int pos, bool lookup, Tags::SearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode);
	int textPanesCount() const;
	int widgetToPaneIndex(TextArea *area) const;
	int64_t highlightLengthOfCodeFromPos(TextCursor pos) const;
	int64_t styleLengthOfCodeFromPos(TextCursor pos) const;
	size_t getLanguageMode() const;
	size_t highlightCodeOfPos(TextCursor pos) const;
	std::unique_ptr<WindowHighlightData> createHighlightData(PatternSet *patternSet);
	std::vector<TextArea *> textPanes() const;
	void abortShellCommand();
	void addMark(TextArea *area, QChar label);
	void beginSmartIndent(Verbosity verbosity);
	void cancelMacroOrLearn();
	void checkForChangesToFile();
	void clearModeMessage();
	void closePane();
	void doMacro(const QString &macro, const QString &errInName);
	void editTaggedLocation(TextArea *area, int i);
	void endSmartIndent();
	void execAP(TextArea *area, const QString &command);
	void executeShellCommand(TextArea *area, const QString &command, CommandSource source);
	void findDefinition(TextArea *area, const QString &tagName);
	void findDefinitionCalltip(TextArea *area, const QString &tipName);
	void findDefinitionHelper(TextArea *area, const QString &arg, Tags::SearchMode search_type);
	void finishMacroCmdExecution();
	void gotoAP(TextArea *area, int lineNum, int column);
	void gotoMark(TextArea *area, QChar label, bool extendSel);
	void gotoMatchingCharacter(TextArea *area, bool select);
	void handleUnparsedRegion(const std::shared_ptr<TextBuffer> &styleBuf, TextCursor pos) const;
	void handleUnparsedRegion(TextBuffer *styleBuf, TextCursor pos) const;
	void macroBannerTimeoutProc();
	void makeSelectionVisible(TextArea *area);
	void moveDocument(MainWindow *fromWindow);
	void printString(const std::string &string, const QString &jobname);
	void printWindow(TextArea *area, bool selectedOnly);
	void raiseDocument();
	void raiseDocumentWindow();
	void raiseFocusDocumentWindow(bool focus);
	void readMacroInitFile();
	void repeatMacro(const QString &macro, int how);
	void resumeMacroExecution();
	void runMacro(Program *prog);
	void selectNumberedLine(TextArea *area, int64_t lineNum);
	void setAutoIndent(IndentStyle indentStyle);
	void setAutoScroll(int margin);
	void setAutoWrap(WrapStyle wrapStyle);
	void setBacklightChars(const QString &applyBacklightTypes);
	void setColors(const QColor &textFg, const QColor &textBg, const QColor &selectFg, const QColor &selectBg, const QColor &hiliteFg, const QColor &hiliteBg, const QColor &lineNoFg, const QColor &lineNoBg, const QColor &cursorFg);
	void setEmTabDistance(int distance);
	void setInsertTabs(bool value);
	void setFileFormat(FileFormats fileFormat);
	void setFilename(const QString &filename);
	void setHighlightSyntax(bool value);
	void setIncrementalBackup(bool value);
	void setLanguageMode(size_t mode, bool forceNewDefaults);
	void setMakeBackupCopy(bool value);
	void setMatchSyntaxBased(bool value);
	void setOverstrike(bool overstrike);
	void setPath(const QDir &path);
	void setPath(const QString &pathname);
	void setShowMatching(ShowMatchingStyle state);
	void setShowStatisticsLine(bool value);
	void setTabDistance(int distance);
	void setUseTabs(bool value);
	void setUserLocked(bool value);
	void setWrapMargin(int margin);
	void shellBannerTimeoutProc();
	void shellCmdToMacroString(const QString &command, const QString &input);
	void showStatsLine(bool state);
	void splitPane();
	void startHighlighting(Verbosity verbosity);
	void stopHighlighting();
	void updateHighlightStyles();
	void updateSignals(MainWindow *from, MainWindow *to);

private:
	MacroContinuationCode continueWorkProc();
	PatternSet *findPatternsForWindow(Verbosity verbosity);
	QString backupFileName() const;
	QString getWindowsMenuEntry() const;
	Style getHighlightInfo(TextCursor pos);
	StyleTableEntry *styleTableEntryOfCode(size_t hCode) const;
	TextArea *createTextArea(const std::shared_ptr<TextBuffer> &buffer);
	bool closeFileAndWindow(CloseMode preResponse);
	bool compareDocumentToFile(const QString &fileName) const;
	bool doOpen(const QString &name, const QString &path, int flags);
	bool doSave();
	bool fileWasModifiedExternally() const;
	bool includeFile(const QString &name);
	bool macroWindowCloseActions();
	bool saveDocument();
	bool saveDocumentAs(const QString &newName, bool addWrap);
	bool writeBackupFile();
	bool writeBckVersion();
	boost::optional<TextCursor> findMatchingChar(char toMatch, Style styleToMatch, TextCursor charPos, TextCursor startLimit, TextCursor endLimit);
	int findAllMatches(TextArea *area, const QString &string);
	size_t matchLanguageMode() const;
	std::unique_ptr<HighlightData[]> compilePatterns(const std::vector<HighlightPattern> &patternSrc);
	std::unique_ptr<Regex> compileRegexAndWarn(const QString &re);
	void abortMacroCommand();
	void actionClose(CloseMode mode);
	void addRedoItem(UndoInfo &&redo);
	void addUndoItem(UndoInfo &&undo);
	void addWrapNewlines();
	void appendDeletedText(view::string_view deletedText, int64_t deletedLen, Direction direction);
	void attachHighlightToWidget(TextArea *area);
	void beginLearn();
	void cancelLearning();
	void clearRedoList();
	void clearUndoList();
	void closeDocument();
	void createSelectMenu(TextArea *area, const QStringList &args);
	void determineLanguageMode(bool forceNewDefaults);
	void doShellMenuCmd(MainWindow *inWindow, TextArea *area, const MenuItem &item, CommandSource source);
	void doShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, CommandSource source);
	void documentRaised();
	void eraseFlash();
	void execCursorLine(TextArea *area, CommandSource source);
	void executeModMacro(SmartIndentEvent *event);
	void executeNewlineMacro(SmartIndentEvent *event);
	void filterSelection(const QString &command, CommandSource source);
	void finishLearning();
	void flashMatchingChar(TextArea *area);
	void freeHighlightingData();
	void issueCommand(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, TextCursor replaceLeft, TextCursor replaceRight, CommandSource source);
	void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void reapplyLanguageMode(size_t mode, bool forceDefaults);
	void redo();
	void refreshMenuBar();
	void refreshMenuToggleStates();
	void refreshTabState();
	void refreshWindowStates();
	void removeBackupFile() const;
	void removeRedoItem();
	void removeUndoItem();
	void replay();
	void revertToSaved();
	void saveUndoInformation(TextCursor pos, int64_t nInserted, int64_t nDeleted, view::string_view deletedText);
	void setModeMessage(const QString &message);
	void setWindowModified(bool modified);
	void trimUndoList(size_t maxLength);
	void undo();
	void unloadLanguageModeTipsFile();
	void updateMarkTable(TextCursor pos, int64_t nInserted, int64_t nDeleted);
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
	std::unique_ptr<RangesetTable> rangesetTable_;       // current range sets
	std::unique_ptr<WindowHighlightData> highlightData_; // info for syntax highlighting

private:
	QMenu *contextMenu_ = nullptr;

private:
	QSplitter *splitter_;
	QFont font_;
	QString backlightCharTypes_; // what backlighting to use
	QString modeMessage_;        // stats line banner content for learn and shell command executing modes
	QTimer *flashTimer_;         // timer for getting rid of highlighted matching paren.
	bool backlightChars_;        // is char backlighting turned on?
	std::map<QChar, Bookmark> markTable_;
	std::unique_ptr<ShellCommandData> shellCmdData_; // when a shell command is executing, info. about it, otherwise, nullptr
	Ui::DocumentWidget ui;

public:
	static DocumentWidget *LastCreated;
};

#endif
