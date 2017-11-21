
#ifndef DOCUMENT_WIDGET_H_
#define DOCUMENT_WIDGET_H_


#include "Bookmark.h"
#include "IndentStyle.h"
#include "LockReasons.h"
#include "MenuItem.h"
#include "Direction.h"
#include "Style.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "smartIndent.h"
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

#include <vector>
#include <deque>

#include <gsl/span>

#include "ui_DocumentWidget.h"

class MainWindow;
class QFrame;
class QLabel;
class QMenu;
class QSplitter;
class QTimer;
class TextArea;
class UndoInfo;
class HighlightData;
class WindowHighlightData;
class Program;
struct DragEndEvent;
struct SmartIndentEvent;
struct SmartIndentData;
struct MacroCommandData;
struct ShellCommandData;
class PatternSet;
class HighlightPattern;
class StyleTableEntry;
class regexp;

template <class Ch, class Tr>
class BasicTextBuffer;

using TextBuffer = BasicTextBuffer<char, std::char_traits<char>>;

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
    DocumentWidget(const QString &name, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DocumentWidget() noexcept override;

Q_SIGNALS:
    void documentClosed();

private Q_SLOTS:
    void flashTimerTimeout();
    void customContextMenuRequested(const QPoint &pos);
    void mergedReadProc();
    void stdoutReadProc();
    void stderrReadProc();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

public Q_SLOTS:
    int findDef(TextArea *area, const QString &value, TagSearchMode search_type);
    void bannerTimeoutProc();
    void BeginSmartIndentEx(int warn);
    void closePane();
    void execAP(TextArea *area, const QString &command);
    void ExecShellCommandEx(TextArea *area, const QString &command, bool fromMacro);
    void findAP(const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWraps);
    void FindDefCalltip(TextArea *area, const QString &tipName);
    void findDefinitionHelper(TextArea *area, const QString &arg, TagSearchMode search_type);
    void FindDefinition(TextArea *area, const QString &tagName);
    void findIncrAP(const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWraps, bool isContinue);
    void gotoAP(TextArea *area, const QString &arg1, const QString &arg2);
    void gotoAP(TextArea *area, const QString &args);
    void gotoAP(TextArea *area, int lineNum, int column);
    void gotoMarkAP(QChar label, bool extendSel);
    void GotoMatchingCharacter(TextArea *area);
    void markAP(QChar ch);
    void moveDocument(MainWindow *fromWindow);
    void open(const QString &fullpath);
    void PrintStringEx(const std::string &string, const QString &jobName);
    void PrintWindow(TextArea *area, bool selectedOnly);
    void replaceAllAP(const QString &searchString, const QString &replaceString, SearchType searchType);
    void replaceAP(const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWraps);
    void replaceFindAP(const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWraps);
    void replaceInSelAP(const QString &searchString, const QString &replaceString, SearchType searchType);
    void SelectToMatchingCharacter(TextArea *area);
    void SetColors(const QString &textFg, const QString &textBg, const QString &selectFg, const QString &selectBg, const QString &hiliteFg, const QString &hiliteBg, const QString &lineNoFg, const QString &cursorFg);
    void setLanguageMode(const QString &mode);
    void ShowStatsLine(bool state);
    void splitPane();

public:
	void movedCallback(TextArea *area);
	void dragStartCallback(TextArea *area);
    void dragEndCallback(TextArea *area, DragEndEvent *data);
	void smartIndentCallback(TextArea *area, SmartIndentEvent *data);
    void modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText);

public:
    static DocumentWidget *fromArea(TextArea *area);
    static DocumentWidget *EditExistingFileEx(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, int iconic, const QString &languageMode, bool tabbed, bool bgOpen);
    static std::vector<DocumentWidget *> allDocuments();

public:
    bool CheckReadOnly() const;
    bool DoNamedBGMenuCmd(TextArea *area, const QString &name, bool fromMacro);
    bool IsTopDocument() const;
    bool ReadMacroFileEx(const QString &fileName, bool warnNotExist);
    HighlightPattern *FindPatternOfWindowEx(const QString &name) const;
    int HighlightCodeOfPosEx(int pos);
    int HighlightLengthOfCodeFromPosEx(int pos, int *checkCode);
    int ReadMacroStringEx(const QString &string, const QString &errIn);
    int StyleLengthOfCodeFromPosEx(int pos);
    int textPanesCount() const;
    int WidgetToPaneIndex(TextArea *area) const;
    QColor GetHighlightBGColorOfCodeEx(int hCode) const;
    QColor HighlightColorValueOfCodeEx(int hCode) const;
    std::vector<TextArea *> textPanes() const;
    QString FullPath() const;
    QString GetWindowDelimiters() const;
    QString HighlightNameOfCodeEx(int hCode) const;
    QString HighlightStyleOfCodeEx(int hCode) const;
    std::unique_ptr<WindowHighlightData> createHighlightDataEx(PatternSet *patSet);
    TextArea *firstPane() const;
    void AbortShellCommandEx();
    void CancelMacroOrLearnEx();
    void CheckForChangesToFileEx();
    void ClearModeMessageEx();
    void DoMacroEx(const QString &macro, const QString &errInName);
    void finishMacroCmdExecutionEx();
    void handleUnparsedRegionEx(const std::shared_ptr<TextBuffer> &styleBuf, int pos) const;
    void MakeSelectionVisible(TextArea *area);
    void RaiseDocument();
    void RaiseDocumentWindow();
    void RaiseFocusDocumentWindow(bool focus);
    void ReadMacroInitFileEx();
    void repeatMacro(const QString &macro, int how);
    void ResumeMacroExecutionEx();
    void runMacroEx(Program *prog);
    void SetAutoIndent(IndentStyle state);
    void SetAutoScroll(int margin);
    void SetAutoWrap(WrapStyle state);
    void SetEmTabDist(int emTabDist);
    void SetFonts(const QString &fontName, const QString &italicName, const QString &boldName, const QString &boldItalicName);
    void SetLanguageMode(int mode, bool forceNewDefaults);
    void SetOverstrike(bool overstrike);
    void SetShowMatching(ShowMatchingStyle state);
    void SetTabDist(int tabDist);
    void setWrapMargin(int margin);
    void ShellCmdToMacroStringEx(const QString &command, const QString &input);
    void StartHighlightingEx(bool warn);
    void StopHighlightingEx();
    void UpdateHighlightStylesEx();
    void EndSmartIndentEx();
    bool InSmartIndentMacrosEx() const;
    QString GetAnySelectionEx();
    QFont FontOfNamedStyleEx(const QString &styleName) const;
    QString GetWindowDelimitersEx() const;

private:    
    bool bckError(const QString &errString, const QString &file);
    bool DoNamedMacroMenuCmd(TextArea *area, const QString &name, bool fromMacro);
    bool DoNamedShellMenuCmd(TextArea *area, const QString &name, bool fromMacro);
    bool doOpen(const QString &name, const QString &path, int flags);
    bool doSave();
    bool findMatchingCharEx(char toMatch, Style styleToMatch, int charPos, int startLimit, int endLimit, int *matchPos);
    bool includeFile(const QString &name);
    bool writeBckVersion();
    HighlightData *compilePatternsEx(const gsl::span<HighlightPattern> &patternSrc);
    int CloseFileAndWindow(CloseMode preResponse);
    bool cmpWinAgainstFile(const QString &fileName) const;
    int fileWasModifiedExternally();
    int MacroWindowCloseActionsEx();
    int matchLanguageMode();
    int SaveWindow();
    int SaveWindowAs(const QString &newName, bool addWrap);
    int WriteBackupFile();
    MacroContinuationCode continueWorkProcEx();
    PatternSet *findPatternsForWindowEx(bool warn);
    QString backupFileNameEx() const;
    QString getWindowsMenuEntry() const;
    std::shared_ptr<regexp> compileREAndWarnEx(const QString &re);
    Style GetHighlightInfoEx(int pos);
    StyleTableEntry *styleTableEntryOfCodeEx(int hCode) const;
    TextArea *createTextArea(TextBuffer *buffer);
    void AbortMacroCommandEx();
    void actionClose(CloseMode mode);
    void addRedoItem(const UndoInfo &redo);
    void addUndoItem(const UndoInfo &undo);
    void addWrapNewlines();
    void appendDeletedText(view::string_view deletedText, int deletedLen, Direction direction);
    void AttachHighlightToWidgetEx(TextArea *area);
    void BeginLearnEx();
    void cancelLearnEx();
    void ClearRedoList();
    void ClearUndoList();
    void CloseWindow();
    void DetermineLanguageMode(bool forceNewDefaults);
    void dimSelDepItemsInMenu(QMenu *menuPane, const QVector<MenuData> &menuList, bool enabled);
    void DimSelectionDepUserMenuItems(bool enabled);
    void documentRaised();
    void DoShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, bool fromMacro);
    void eraseFlashEx();
    void ExecCursorLineEx(TextArea *area, bool fromMacro);
    void executeModMacroEx(SmartIndentEvent *cbInfo);
    void executeNewlineMacroEx(SmartIndentEvent *cbInfo);
    void FilterSelection(const QString &command, bool fromMacro);
    void filterSelection(const QString &filterText);
    void FinishLearnEx();
    void FlashMatchingEx(TextArea *area);
    void FreeHighlightingDataEx();
    void issueCommandEx(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, int replaceLeft, int replaceRight, bool fromMacro);
    void reapplyLanguageMode(int mode, bool forceDefaults);
    void Redo();
    void refreshMenuBar();
    void RefreshMenuToggleStates();
    void RefreshTabState();
    void RefreshWindowStates();
    void RemoveBackupFile();
    void removeRedoItem();
    void removeUndoItem();
    void RepeatMacroEx(const QString &command, int how);
    void ReplayEx();
    void RevertToSaved();
    void safeCloseEx();
    void SaveUndoInformation(int pos, int nInserted, int nDeleted, view::string_view deletedText);
    void SetBacklightChars(const QString &applyBacklightTypes);
    void SetModeMessageEx(const QString &message);
    void SetWindowModified(bool modified);
    void trimUndoList(int maxLength);
    void Undo();
    void UnloadLanguageModeTipsFileEx();
    void UpdateMarkTable(int pos, int nInserted, int nDeleted);
    void UpdateStatsLine(TextArea *area);

private:
	// TODO(eteran): are these dialog's per window or per text document?
	QPointer<QDialog> dialogColors_;
    QPointer<QDialog> dialogFonts_;

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
    ShowMatchingStyle showMatchingStyle_;           // How to show matching parens: None, Delimeter, or Range
    WrapStyle wrapMode_;                    // line wrap style: None, Newline or Continuous
	dev_t device_;                     // device where the file resides
	gid_t fileGid_;                    // last recorded group id of the file
	ino_t inode_;                      // file's inode
	int autoSaveCharCount_;            // count of single characters typed since last backup file generated
	int autoSaveOpCount_;              // count of editing operations ""
	int flashPos_;                     // position saved for erasing matching paren highlight (if one is drawn)
	int languageMode_;                 // identifies language mode currently selected in the window
	int nMarks_;                       // number of active bookmarks
	int undoMemUsed_;                  // amount of memory (in bytes) dedicated to the undo list
    std::deque<UndoInfo> redo_;             // info for redoing last undone op
    std::deque<UndoInfo> undo_;             // info for undoing last operation
    time_t lastModTime_;               // time of last modification to file
	uid_t fileUid_;                    // last recorded user id of the file
	unsigned fileMode_;                // permissions of file being edited
    std::unique_ptr<WindowHighlightData> highlightData_;              // info for syntax highlighting
    std::shared_ptr<MacroCommandData>    macroCmdData_;               // same for macro commands
    std::unique_ptr<ShellCommandData>    shellCmdData_;               // when a shell command is executing, info. about it, otherwise, nullptr
    std::unique_ptr<SmartIndentData>     smartIndentData_;            // compiled macros for smart indent
    bool showStats_;                  // is stats line supposed to be shown

private:
	QSplitter *splitter_;
	Ui::DocumentWidget ui;
};

#endif
