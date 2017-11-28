
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
class RangesetTable;
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
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

public Q_SLOTS:
    int findDef(TextArea *area, const QString &value, TagSearchMode search_type);
    void action_Set_Language_Mode(const QString &languageMode);
    void AddMarkEx(TextArea *area, QChar label);
    void bannerTimeoutProc();
    void BeginSmartIndentEx(int warn);
    void closePane();
    void execAP(TextArea *area, const QString &command);
    void ExecShellCommandEx(TextArea *area, const QString &command, bool fromMacro);
    void FindDefCalltip(TextArea *area, const QString &tipName);
    void findDefinitionHelper(TextArea *area, const QString &arg, TagSearchMode search_type);
    void FindDefinition(TextArea *area, const QString &tagName);
    void gotoAP(TextArea *area, int lineNum, int column);
    void gotoMark(TextArea *area, QChar label, bool extendSel);
    void GotoMatchingCharacter(TextArea *area);
    void moveDocument(MainWindow *fromWindow);
    void open(const QString &fullpath);
    void PrintStringEx(const std::string &string, const QString &jobName);
    void PrintWindow(TextArea *area, bool selectedOnly);
    void SelectNumberedLineEx(TextArea *area, int lineNum);
    void SelectToMatchingCharacter(TextArea *area);
    void SetColors(const QString &textFg, const QString &textBg, const QString &selectFg, const QString &selectBg, const QString &hiliteFg, const QString &hiliteBg, const QString &lineNoFg, const QString &cursorFg);
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
    static DocumentWidget *EditExistingFileEx(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool bgOpen);
    static std::vector<DocumentWidget *> allDocuments();

public:
    bool CheckReadOnly() const;
    bool InSmartIndentMacrosEx() const;
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
    QFont FontOfNamedStyleEx(const QString &styleName) const;
    QString FullPath() const;
    QString GetAnySelectionEx();
    QString GetWindowDelimiters() const;
    QString GetWindowDelimitersEx() const;
    QString HighlightNameOfCodeEx(int hCode) const;
    QString HighlightStyleOfCodeEx(int hCode) const;
    std::unique_ptr<WindowHighlightData> createHighlightDataEx(PatternSet *patSet);
    std::vector<TextArea *> textPanes() const;
    TextArea *firstPane() const;
    void AbortShellCommandEx();
    void CancelMacroOrLearnEx();
    void CheckForChangesToFileEx();
    void ClearModeMessageEx();
    void DoMacroEx(const QString &macro, const QString &errInName);
    void EndSmartIndentEx();
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
    void action_Set_Fonts(const QString &fontName, const QString &italicName, const QString &boldName, const QString &boldItalicName);
    void SetHighlightSyntax(bool value);
    void SetIncrementalBackup(bool value);
    void SetLanguageMode(size_t mode, bool forceNewDefaults);
    void action_Set_Language_Mode(const QString &languageMode, bool forceNewDefaults);
    void SetUserLocked(bool value);
    bool GetUserLocked() const;
    void SetOverstrike(bool overstrike);
    void SetShowMatching(ShowMatchingStyle state);
    void SetTabDist(int tabDist);
    void SetUseTabs(bool value);
    bool GetUseTabs() const;
    void setWrapMargin(int margin);
    void ShellCmdToMacroStringEx(const QString &command, const QString &input);
    void StartHighlightingEx(bool warn);
    void StopHighlightingEx();
    void UpdateHighlightStylesEx();
    bool GetHighlightSyntax() const;
    bool GetIncrementalBackup() const;
    bool GetMakeBackupCopy() const;
    void SetMakeBackupCopy(bool value);
    bool GetOverstrike() const;
    bool GetMatchSyntaxBased() const;
    void SetMatchSyntaxBased(bool value);
    void SetShowStatisticsLine(bool value);
    bool GetShowStatisticsLine() const;
    bool modeMessageDisplayed() const;


public:
#if defined(REPLACE_SCOPE)
    bool selectionSpansMultipleLines();
#endif

private:    
    bool bckError(const QString &errString, const QString &file);
    bool doOpen(const QString &name, const QString &path, int flags);
    bool doSave();
    bool findMatchingCharEx(char toMatch, Style styleToMatch, int charPos, int startLimit, int endLimit, int *matchPos);
    bool includeFile(const QString &name);
    bool writeBckVersion();
    HighlightData *compilePatternsEx(const gsl::span<HighlightPattern> &patternSrc);
    int CloseFileAndWindow(CloseMode preResponse);
    bool cmpWinAgainstFile(const QString &fileName) const;
    int fileWasModifiedExternally() const;
    int MacroWindowCloseActionsEx();
    size_t matchLanguageMode() const;
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
    void dimSelDepItemsInMenu(QMenu *menuPane, const gsl::span<MenuData> &menuList, bool enabled);
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
    void reapplyLanguageMode(size_t mode, bool forceDefaults);
    void Redo();
    void refreshMenuBar();
    void RefreshMenuToggleStates();
    void RefreshTabState();
    void RefreshWindowStates();
    void RemoveBackupFile() const;
    void removeRedoItem();
    void removeUndoItem();
    void RepeatMacroEx(const QString &command, int how);
    void ReplayEx();
    void RevertToSaved();
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
    // TODO(eteran): 2.0, in nedit, these are owned per-document. But the effect all
    // open documents, so they shoudl be at the very least, per-window if not global
    QPointer<QDialog> dialogColors_;
    QPointer<QDialog> dialogFonts_;

public:
    QString fontName_;                 // names of the text fonts in use
    QString path_;                     // path component of file being edited
    QString filename_;                 // name component of file being edited
    QString boldFontName_;
    QString italicFontName_;
    QString boldItalicFontName_;
    LockReasons lockReasons_;          // all ways a file can be locked
    TextBuffer *buffer_;               // holds the text being edited
    bool multiFileBusy_;               // suppresses multiple beeps/dialogs during multi-file replacements
    bool multiFileReplSelected_;       // selected during last multi-window replacement operation (history)
    bool filenameSet_;                 // is the window still "Untitled"?
    bool replaceFailed_;               // flags replacements failures during multi-file replacements
    bool highlightSyntax_;             // is syntax highlighting turned on?
    bool fileChanged_;                 // has window been modified?
    bool showStats_;                   // is stats line supposed to be shown
    bool overstrike_;                  // is overstrike mode turned on ?
    bool saveOldVersion_;              // keep old version in filename.bc
    bool autoSave_;                    // is autosave turned on?
    size_t languageMode_;              // identifies language mode currently selected in the window
    int matchSyntaxBased_;             // Use syntax info to show matching
    ShowMatchingStyle showMatchingStyle_;           // How to show matching parens: None, Delimeter, or Range
    IndentStyle indentStyle_;          // whether/how to auto indent
    WrapStyle wrapMode_;               // line wrap style: None, Newline or Continuous
    FileFormats fileFormat_;           // whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks
    std::unique_ptr<WindowHighlightData> highlightData_;              // info for syntax highlighting
    std::shared_ptr<MacroCommandData>    macroCmdData_;               // same for macro commands
    std::shared_ptr<RangesetTable>       rangesetTable_;   // current range sets

private:
	Bookmark markTable_[MAX_MARKS];    // marked locations in window	
	QString backlightCharTypes_;       // what backlighting to use
    QFont fontStruct_;
    QFont boldFontStruct_;
    QFont boldItalicFontStruct_;
    QFont italicFontStruct_;
    QTimer *flashTimer_;               // timer for getting rid of highlighted matching paren.
    QMenu *contextMenu_;	
	bool backlightChars_;              // is char backlighting turned on?
	bool fileMissing_;                 // is the window's file gone?		
	bool ignoreModify_;                // ignore modifications to text area
    QString modeMessage_;              // stats line banner content for learn and shell command executing modes
    dev_t dev_;                        // device where the file resides
    gid_t gid_;                        // last recorded group id of the file
    ino_t ino_;                        // file's inode
    uid_t uid_;                        // last recorded user id of the file
    time_t lastModTime_;               // time of last modification to file
    mode_t mode_;                      // permissions of file being edited
	int autoSaveCharCount_;            // count of single characters typed since last backup file generated
	int autoSaveOpCount_;              // count of editing operations ""
	int flashPos_;                     // position saved for erasing matching paren highlight (if one is drawn)	
	int nMarks_;                       // number of active bookmarks
	int undoMemUsed_;                  // amount of memory (in bytes) dedicated to the undo list
    std::deque<UndoInfo> redo_;        // info for redoing last undone op
    std::deque<UndoInfo> undo_;        // info for undoing last operation
    std::unique_ptr<ShellCommandData> shellCmdData_;               // when a shell command is executing, info. about it, otherwise, nullptr
    std::unique_ptr<SmartIndentData>  smartIndentData_;            // compiled macros for smart indent

private:
	QSplitter *splitter_;
	Ui::DocumentWidget ui;
};

#endif
