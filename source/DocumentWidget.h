
#ifndef DOCUMENT_WIDGET_H_
#define DOCUMENT_WIDGET_H_


#include "Bookmark.h"
#include "IndentStyle.h"
#include "LockReasons.h"
#include "MenuItem.h"
#include "SearchDirection.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "string_view.h"
#include "tags.h"
#include "UndoInfo.h"
#include "CloseMode.h"
#include "WrapStyle.h"
#include "WrapMode.h"
#include "userCmds.h"
#include "util/FileFormats.h"

#include <QDialog>
#include <QPointer>
#include <QProcess>
#include <QWidget>

#include "ui_DocumentWidget.h"

class MainWindow;
class QFrame;
class QLabel;
class QMenu;
class QSplitter;
class QTimer;
class TextArea;
class TextBuffer;
class UndoInfo;
class HighlightData;
class WindowHighlightData;
struct DragEndEvent;
struct SmartIndentEvent;
struct SmartIndentData;
struct MacroCommandData;
struct ShellCommandData;

enum class Direction;

class DocumentWidget : public QWidget {
	Q_OBJECT
	friend class MainWindow;
	
public:
    DocumentWidget(const QString &name, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DocumentWidget() noexcept override;

private Q_SLOTS:
    void flashTimerTimeout();
    void customContextMenuRequested(const QPoint &pos);
    void mergedReadProc();
    void stdoutReadProc();
    void stderrReadProc();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

public Q_SLOTS:
	int findDef(TextArea *area, const QString &value, Mode search_type);
    void BeginSmartIndentEx(int warn);
    void ExecShellCommandEx(TextArea *area, const QString &command, bool fromMacro);
    void FindDefCalltip(TextArea *area, const QString &tipName);
    void FindDefinition(TextArea *area, const QString &tagName);
    void GotoMatchingCharacter(TextArea *area);
    void PrintStringEx(const std::string &string, const QString &jobName);
    void PrintWindow(TextArea *area, bool selectedOnly);
    void SelectToMatchingCharacter(TextArea *area);
    void SetColors(const QString &textFg, const QString &textBg, const QString &selectFg, const QString &selectBg, const QString &hiliteFg, const QString &hiliteBg, const QString &lineNoFg, const QString &cursorFg);
    void ShowStatsLine(bool state);
    void bannerTimeoutProc();
    void closePane();
    void execAP(TextArea *area, const QString &command);
    void findAP(const QString &searchString, SearchDirection direction, SearchType searchType, WrapMode searchWraps);
	void findDefinitionHelper(TextArea *area, const QString &arg, Mode search_type);
    void findIncrAP(const QString &searchString, SearchDirection direction, SearchType searchType, WrapMode searchWraps, bool isContinue);
	void gotoAP(TextArea *area, const QString &args);
	void gotoAP(TextArea *area, const QString &arg1, const QString &arg2);
    void gotoAP(TextArea *area, int lineNum, int column);
    void gotoMarkAP(QChar label, bool extendSel);
    void markAP(QChar ch);
    void moveDocument(MainWindow *fromWindow);
    void open(const QString &fullpath);
    void replaceAP(const QString &searchString, const QString &replaceString, SearchDirection direction, SearchType searchType, WrapMode searchWraps);
    void replaceAllAP(const QString &searchString, const QString &replaceString, SearchType searchType);
    void replaceFindAP(const QString &searchString, const QString &replaceString, SearchDirection direction, SearchType searchType, WrapMode searchWraps);
    void replaceInSelAP(const QString &searchString, const QString &replaceString, SearchType searchType);
    void setLanguageMode(const QString &mode);
    void splitPane();

public:
	void movedCallback(TextArea *area);
	void dragStartCallback(TextArea *area);
    void dragEndCallback(TextArea *area, DragEndEvent *data);
	void smartIndentCallback(TextArea *area, SmartIndentEvent *data);
    void modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText);

public:
    static DocumentWidget *documentFrom(TextArea *area);

public:
	void BeginLearnEx();
	int ReadMacroFileEx(const QString &fileName, bool warnNotExist);
	int matchLanguageMode();
	void DetermineLanguageMode(bool forceNewDefaults);
	void RefreshTabState();
	void SetLanguageMode(int mode, bool forceNewDefaults);
	void SetWindowModified(bool modified);
	void UpdateStatsLine(TextArea *area);
    MainWindow *toWindow() const;
    QList<TextArea *> textPanes() const;
    QString FullPath() const;
    QString GetWindowDelimiters() const;
    QString backupFileNameEx();
    QString getWindowsMenuEntry();
    TextArea *createTextArea(TextBuffer *buffer);
    TextArea *firstPane() const;
    bool CheckReadOnly() const;
    bool DoNamedBGMenuCmd(TextArea *area, const QString &name, bool fromMacro);
    bool DoNamedMacroMenuCmd(TextArea *area, const QString &name, bool fromMacro);
    bool DoNamedShellMenuCmd(TextArea *area, const QString &name, bool fromMacro);
    bool IsTopDocument() const;
    bool bckError(const QString &errString, const QString &file);
    bool doOpen(const QString &name, const QString &path, int flags);
    bool doSave();
    bool findMatchingCharEx(char toMatch, void *styleToMatch, int charPos, int startLimit, int endLimit, int *matchPos);
    bool includeFile(const QString &name);
    bool writeBckVersion();
    int CloseFileAndWindow(CloseMode preResponse);
    int SaveWindow();
    int SaveWindowAs(const QString &newName, bool addWrap);
    int WidgetToPaneIndex(TextArea *area) const;
    int WriteBackupFile();
    int cmpWinAgainstFile(const QString &fileName);
    int fileWasModifiedExternally();
    int textPanesCount() const;
    void AbortShellCommandEx();
    void CheckForChangesToFileEx();
    void ClearModeMessageEx();
    void ClearRedoList();
    void ClearUndoList();
    void CloseWindow();
    void DimSelectionDepUserMenuItems(bool enabled);
    void DoShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, bool fromMacro);
    void ExecCursorLineEx(TextArea *area, bool fromMacro);
    void FilterSelection(const QString &command, bool fromMacro);
	void MakeSelectionVisible(TextArea *area);
    void RaiseDocument();
    void RaiseDocumentWindow();
    void RaiseFocusDocumentWindow(bool focus);
    void Redo();
    void RefreshMenuToggleStates();
    void RefreshWindowStates();
    void RemoveBackupFile();
    void RevertToSaved();
    void SaveUndoInformation(int pos, int nInserted, int nDeleted, view::string_view deletedText);
    void SetAutoIndent(IndentStyle state);
    void SetAutoScroll(int margin);
    void SetAutoWrap(WrapStyle state);
    void SetBacklightChars(const QString &applyBacklightTypes);
    void SetEmTabDist(int emTabDist);
    void SetFonts(const QString &fontName, const QString &italicName, const QString &boldName, const QString &boldItalicName);
    void SetModeMessageEx(const QString &message);
    void SetOverstrike(bool overstrike);
    void SetShowMatching(ShowMatchingStyle state);
    void SetTabDist(int tabDist);
    void ShellCmdToMacroStringEx(const QString &command, const QString &input);
    void StopHighlightingEx();
    void Undo();
    void UnloadLanguageModeTipsFileEx();
    void UpdateMarkTable(int pos, int nInserted, int nDeleted);
    void actionClose(CloseMode mode);
    void addRedoItem(const UndoInfo &redo);
    void addUndoItem(const UndoInfo &undo);
    void addWrapNewlines();
    void appendDeletedText(view::string_view deletedText, int deletedLen, Direction direction);
    void dimSelDepItemsInMenu(QMenu *menuPane, const QVector<MenuData> &menuList, bool enabled);
    void documentRaised();
    void executeModMacroEx(SmartIndentEvent *cbInfo);
    void executeNewlineMacroEx(SmartIndentEvent *cbInfo);
    void filterSelection(const QString &filterText);
    void freeHighlightData(WindowHighlightData *hd);
    void freePatterns(HighlightData *patterns);
    void issueCommandEx(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, int replaceLeft, int replaceRight, bool fromMacro);
    void reapplyLanguageMode(int mode, bool forceDefaults);
    void refreshMenuBar();
    void removeRedoItem();
    void removeUndoItem();
    void repeatMacro(const QString &macro, int how);
    void safeCloseEx();
	void setWrapMargin(int margin);
    void trimUndoList(int maxLength);

public:
    static DocumentWidget *EditExistingFileEx(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, int iconic, const QString &languageMode, bool tabbed, bool bgOpen);
    static QList<DocumentWidget *> allDocuments();

private:
	// TODO(eteran): are these dialog's per window or per text document?
	QPointer<QDialog> dialogColors_;
	QPointer<QDialog> dialogFonts_; /* nullptr, unless font dialog is up */

public:
	Bookmark markTable_[MAX_MARKS];    // marked locations in window
	FileFormats fileFormat_;           // whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks
	LockReasons lockReasons_;          // all ways a file can be locked
	QString backlightCharTypes_;       // what backlighting to use
	QString boldFontName_;
	QString boldItalicFontName_;
	QString filename_;                 // name component of file being edited
	QString fontName_;                 // names of the text fonts in use
	QString italicFontName_;
	QString path_;                     // path component of file being edited
	TextBuffer *buffer_;               // holds the text being edited

    QFont fontStruct_;
    QFont boldFontStruct_;
    QFont boldItalicFontStruct_;
    QFont italicFontStruct_;    // fontStructs for highlighting fonts

    QTimer *flashTimer_;               // timer for getting rid of highlighted matching paren.
    QMenu *contextMenu_;

	bool autoSave_;                    // is autosave turned on?
	bool backlightChars_;              // is char backlighting turned on?
	bool fileChanged_;                 // has window been modified?
	bool fileMissing_;                 // is the window's file gone?
	bool filenameSet_;                 // is the window still "Untitled"?
	bool highlightSyntax_;             // is syntax highlighting turned on?
	bool ignoreModify_;                // ignore modifications to text area
	bool modeMessageDisplayed_;        // special stats line banner for learn and shell command executing modes
	bool multiFileBusy_;               // suppresses multiple beeps/dialogs during multi-file replacements
	bool multiFileReplSelected_;       // selected during last multi-window replacement operation (history)
	bool overstrike_;                  // is overstrike mode turned on ?
	bool replaceFailed_;               // flags replacements failures during multi-file replacements
    bool saveOldVersion_;              // keep old version in filename.bc
    QString modeMessage_;              // stats line banner content for learn and shell command executing modes
    IndentStyle indentStyle_;          // whether/how to auto indent
    int matchSyntaxBased_;            // Use syntax info to show matching
    ShowMatchingStyle showMatchingStyle_;           // How to show matching parens: NO_FLASH, FLASH_DELIMIT, or FLASH_RANGE
    WrapStyle wrapMode_;                    // line wrap style: NO_WRAP, NEWLINE_WRAP or CONTINUOUS_WRAP
	dev_t device_;                     // device where the file resides
	gid_t fileGid_;                    // last recorded group id of the file
	ino_t inode_;                      // file's inode
	int autoSaveCharCount_;            // count of single characters typed since last backup file generated
	int autoSaveOpCount_;              // count of editing operations ""
	int flashPos_;                     // position saved for erasing matching paren highlight (if one is drawn)
	int languageMode_;                 // identifies language mode currently selected in the window
	int nMarks_;                       // number of active bookmarks
	int undoMemUsed_;                  // amount of memory (in bytes) dedicated to the undo list
    QList<UndoInfo> redo_;             // info for redoing last undone op
    QList<UndoInfo> undo_;             // info for undoing last operation
	time_t lastModTime_;               // time of last modification to file
	uid_t fileUid_;                    // last recorded user id of the file
	unsigned fileMode_;                // permissions of file being edited
    WindowHighlightData *highlightData_;              // info for syntax highlighting
    std::shared_ptr<MacroCommandData> macroCmdData_;               // same for macro commands
    std::shared_ptr<ShellCommandData> shellCmdData_;               // when a shell command is executing, info. about it, otherwise, nullptr
	SmartIndentData *smartIndentData_;            // compiled macros for smart indent
    bool showStats_;                  // is stats line supposed to be shown

private:
	QSplitter *splitter_;
	Ui::DocumentWidget ui;
};

#endif
