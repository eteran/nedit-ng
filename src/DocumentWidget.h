
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
#include "WrapStyle.h"
#include "Util/FileFormats.h"
#include "Util/string_view.h"

#include "ui_DocumentWidget.h"

#include <QPointer>
#include <QProcess>
#include <QWidget>

#include <array>

#include <gsl/span>

#include <boost/optional.hpp>

#include <sys/stat.h>

class HighlightData;
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
	DocumentWidget(std::shared_ptr<DocumentInfo> &info_ptr, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	DocumentWidget(const QString &name, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DocumentWidget() noexcept override;

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
	static DocumentWidget *EditExistingFileEx(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool background);
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
	QColor GetHighlightBGColorOfCodeEx(size_t hCode) const;
	QColor HighlightColorValueOfCodeEx(size_t hCode) const;
	QFont defaultFont() const;
	QString GetAnySelection();
	QString GetAnySelection(bool beep_on_error);
	QString GetWindowDelimitersEx() const;
	QString HighlightNameOfCodeEx(size_t hCode) const;
	QString HighlightStyleOfCodeEx(size_t hCode) const;
	QString documentDelimiters() const;
	QString filename() const;
	QString fullPath() const;
	QString path() const;
	ShowMatchingStyle showMatchingStyle() const;
	TextArea *firstPane() const;
	TextBuffer *buffer() const;
	WrapStyle wrapMode() const;
	bool GetHighlightSyntax() const;
	bool GetIncrementalBackup() const;
	bool GetMakeBackupCopy() const;
	bool GetMatchSyntaxBased() const;
	bool GetOverstrike() const;
	bool GetShowStatisticsLine() const;
	bool GetUseTabs() const;
	bool GetUserLocked() const;
	bool InSmartIndentMacros() const;
	bool ReadMacroFile(const QString &fileName, bool warnNotExist);
	bool ReadMacroString(const QString &string, const QString &errIn);
	bool checkReadOnly() const;
	bool fileChanged() const;
	bool filenameSet() const;
	bool isReadOnly() const;
	bool isTopDocument() const;
	bool modeMessageDisplayed() const;
	dev_t device() const;
	ino_t inode() const;
	int ShowTipStringEx(const QString &text, bool anchored, int pos, bool lookup, Tags::SearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode);
	int findDef(TextArea *area, const QString &value, Tags::SearchMode search_type);
	int textPanesCount() const;
	int widgetToPaneIndex(TextArea *area) const;
	int64_t StyleLengthOfCodeFromPosEx(TextCursor pos);
	int64_t highlightLengthOfCodeFromPos(TextCursor pos);
	size_t GetLanguageMode() const;
	size_t highlightCodeOfPos(TextCursor pos);
	std::unique_ptr<WindowHighlightData> createHighlightData(PatternSet *patternSet);
	std::vector<TextArea *> textPanes() const;
	void AddMarkEx(TextArea *area, QChar label);
	void DoMacro(const QString &macro, const QString &errInName);
	void FindDefCalltip(TextArea *area, const QString &tipName);
	void GotoMatchingCharacter(TextArea *area);
	void MakeSelectionVisible(TextArea *area);
	void ResumeMacroExecutionEx();
	void SelectNumberedLineEx(TextArea *area, int64_t lineNum);
	void SelectToMatchingCharacter(TextArea *area);
	void SetBacklightChars(const QString &applyBacklightTypes);
	void SetColors(const QColor &textFg, const QColor &textBg, const QColor &selectFg, const QColor &selectBg, const QColor &hiliteFg, const QColor &hiliteBg, const QColor &lineNoFg, const QColor &lineNoBg, const QColor &cursorFg);
	void SetHighlightSyntax(bool value);
	void SetIncrementalBackup(bool value);
	void SetLanguageMode(size_t mode, bool forceNewDefaults);
	void SetMakeBackupCopy(bool value);
	void SetMatchSyntaxBased(bool value);
	void SetOverstrike(bool overstrike);
	void SetShowMatching(ShowMatchingStyle state);
	void SetShowStatisticsLine(bool value);
	void SetUseTabs(bool value);
	void SetUserLocked(bool value);
	void ShowStatsLine(bool state);
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
	void setAutoWrap(WrapStyle wrapMode);
	void setEmTabDistance(int distance);
	void setFileFormat(FileFormats fileFormat);
	void setPath(const QString &pathname);
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
	std::unique_ptr<HighlightData[]> compilePatternsEx(const std::vector<HighlightPattern> &patternSrc);
	std::unique_ptr<Regex> compileRegexAndWarn(const QString &re);
	void setModeMessage(const QString &message);
	void executeModMacro(SmartIndentEvent *event);
	void executeNewlineMacro(SmartIndentEvent *event);
	MacroContinuationCode continueWorkProcEx();
	PatternSet *findPatternsForWindow(bool warn);
	QString backupFileNameEx() const;
	QString getWindowsMenuEntry() const;
	Style getHighlightInfo(TextCursor pos);
	StyleTableEntry *styleTableEntryOfCodeEx(size_t hCode) const;
	TextArea *createTextArea(TextBuffer *buffer);
	bool CloseFileAndWindow(CloseMode preResponse);
	bool MacroWindowCloseActionsEx();
	bool WriteBackupFile();
	bool compareDocumentToFile(const QString &fileName) const;
	bool doOpen(const QString &name, const QString &path, int flags);
	bool doSave();
	bool fileWasModifiedExternally() const;
	bool includeFile(const QString &name);
	bool saveDocument();
	bool saveDocumentAs(const QString &newName, bool addWrap);
	bool writeBckVersion();
	boost::optional<TextCursor> findMatchingCharEx(char toMatch, Style styleToMatch, TextCursor charPos, TextCursor startLimit, TextCursor endLimit);
	int findAllMatchesEx(TextArea *area, const QString &string);
	size_t matchLanguageMode() const;
	void AbortMacroCommand();
	void attachHighlightToWidget(TextArea *area);
	void beginLearn();
	void clearRedoList();
	void clearUndoList();
	void closeDocument();
	void DetermineLanguageMode(bool forceNewDefaults);
	void doShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, CommandSource source);
	void doShellMenuCmd(MainWindow *inWindow, TextArea *area, const MenuItem &item, CommandSource source);
	void execCursorLine(TextArea *area, CommandSource source);
	void finishLearning();
	void flashMatchingChar(TextArea *area);
	void FreeHighlightingData();
	void Redo();
	void RefreshMenuToggleStates();
	void RefreshTabState();
	void refreshWindowStates();
	void RemoveBackupFile() const;
	void replay();
	void RevertToSaved();
	void saveUndoInformation(TextCursor pos, int64_t nInserted, int64_t nDeleted, view::string_view deletedText);
	void SetWindowModified(bool modified);
	void Undo();
	void unloadLanguageModeTipsFile();
	void UpdateMarkTable(TextCursor pos, int64_t nInserted, int64_t nDeleted);
	void actionClose(CloseMode mode);
	void addRedoItem(UndoInfo &&redo);
	void addUndoItem(UndoInfo &&undo);
	void addWrapNewlines();
	void appendDeletedText(view::string_view deletedText, int64_t deletedLen, Direction direction);
	void cancelLearning();
	void createSelectMenuEx(TextArea *area, const QStringList &args);
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
	bool replaceFailed_     = false;               // flags replacements failures during multi-file replacements
	bool multiFileBusy_     = false;               // suppresses multiple beeps/dialogs during multi-file replacements
	size_t languageMode_    = PLAIN_LANGUAGE_MODE; // identifies language mode currently selected in the window

public:
	QString fontName_;                                     // names of the text fonts in use
	bool highlightSyntax_;                                 // is syntax highlighting turned on?	
	bool showStats_;                                       // is stats line supposed to be shown	
	std::shared_ptr<MacroCommandData>    macroCmdData_;    // same for macro commands
	std::shared_ptr<RangesetTable>       rangesetTable_;   // current range sets
	std::unique_ptr<WindowHighlightData> highlightData_;   // info for syntax highlighting

private:
	QMenu *contextMenu_    = nullptr;		
	size_t nMarks_         = 0;                         // number of active bookmarks	

private:
	QSplitter *splitter_;
	QFont font_;
	QString backlightCharTypes_;                        // what backlighting to use
	QString modeMessage_;                               // stats line banner content for learn and shell command executing modes
	QTimer *flashTimer_;                                // timer for getting rid of highlighted matching paren.
	bool backlightChars_;                               // is char backlighting turned on?
	std::array<Bookmark, MAX_MARKS> markTable_;         // marked locations in window
	std::unique_ptr<ShellCommandData> shellCmdData_;    // when a shell command is executing, info. about it, otherwise, nullptr	
	Ui::DocumentWidget ui;

public:
	static DocumentWidget *LastCreated;
};

#endif
