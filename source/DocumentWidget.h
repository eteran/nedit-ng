
#ifndef DOCUMENT_WIDGET_H_
#define DOCUMENT_WIDGET_H_

#include "Bookmark.h"
#include "CallTip.h"
#include "CloseMode.h"
#include "CommandSource.h"
#include "Util/FileFormats.h"
#include "IndentStyle.h"
#include "LockReasons.h"
#include "MenuItem.h"
#include "ShowMatchingStyle.h"
#include "TextBufferFwd.h"
#include "Util/string_view.h"
#include "tags.h"
#include "UndoInfo.h"
#include "MenuData.h"
#include "WrapStyle.h"

#include "ui_DocumentWidget.h"

#include <QPointer>
#include <QProcess>
#include <QWidget>

#include <gsl/span>
#include <array>
#include <boost/optional.hpp>

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
class WindowHighlightData;
struct DragEndEvent;
struct MacroCommandData;
struct Program;
struct ShellCommandData;
struct SmartIndentData;
struct SmartIndentEvent;


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
    DocumentWidget(const QString &name, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DocumentWidget() noexcept override;

Q_SIGNALS:
    void documentClosed();

public:
	void movedCallback(TextArea *area);
	void dragStartCallback(TextArea *area);
    void dragEndCallback(TextArea *area, DragEndEvent *data);
	void smartIndentCallback(TextArea *area, SmartIndentEvent *data);
    void modifiedCallback(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText);

public:
    static DocumentWidget *fromArea(TextArea *area);
    static DocumentWidget *EditExistingFileEx(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool bgOpen);
    static std::vector<DocumentWidget *> allDocuments();

public:
    void action_Set_Fonts(const QString &fontName, const QString &italicName, const QString &boldName, const QString &boldItalicName);
    void action_Set_Language_Mode(const QString &languageMode);
    void action_Set_Language_Mode(const QString &languageMode, bool forceNewDefaults);

public:
    bool CheckReadOnly() const;
    bool GetHighlightSyntax() const;
    bool GetIncrementalBackup() const;
    bool GetMakeBackupCopy() const;
    bool GetMatchSyntaxBased() const;
    bool GetOverstrike() const;
    bool GetShowStatisticsLine() const;
    bool GetUserLocked() const;
    bool GetUseTabs() const;
    bool InSmartIndentMacrosEx() const;
    bool IsTopDocument() const;
    bool modeMessageDisplayed() const;
    bool ReadMacroFileEx(const QString &fileName, bool warnNotExist);
    HighlightPattern *FindPatternOfWindowEx(const QString &name) const;
    int findDef(TextArea *area, const QString &value, TagSearchMode search_type);
    size_t HighlightCodeOfPosEx(int pos);
    int HighlightLengthOfCodeFromPosEx(int pos, size_t *checkCode);
    int ReadMacroStringEx(const QString &string, const QString &errIn);
    int StyleLengthOfCodeFromPosEx(int pos);
    int textPanesCount() const;
    int WidgetToPaneIndex(TextArea *area) const;
    LockReasons lockReasons() const;
    QColor GetHighlightBGColorOfCodeEx(size_t hCode) const;
    QColor HighlightColorValueOfCodeEx(size_t hCode) const;
    QFont FontOfNamedStyleEx(const QString &styleName) const;
    QString FileName() const;
    QString FullPath() const;
    QString GetAnySelectionEx();
    QString GetWindowDelimiters() const;
    QString GetWindowDelimitersEx() const;
    QString HighlightNameOfCodeEx(size_t hCode) const;
    QString HighlightStyleOfCodeEx(size_t hCode) const;
    size_t GetLanguageMode() const;
    std::unique_ptr<WindowHighlightData> createHighlightDataEx(PatternSet *patSet);
    std::vector<TextArea *> textPanes() const;
    TextArea *firstPane() const;
    void AbortShellCommandEx();
    void AddMarkEx(TextArea *area, QChar label);
    void BeginSmartIndentEx(bool warn);
    void CancelMacroOrLearnEx();
    void CheckForChangesToFileEx();
    void ClearModeMessageEx();
    void closePane();
    void DoMacroEx(const QString &macro, const QString &errInName);
    void EndSmartIndent();
    void execAP(TextArea *area, const QString &command);
    void ExecShellCommandEx(TextArea *area, const QString &command, CommandSource source);
    void FindDefCalltip(TextArea *area, const QString &tipName);
    void findDefinitionHelper(TextArea *area, const QString &arg, TagSearchMode search_type);
    void FindDefinition(TextArea *area, const QString &tagName);
    void finishMacroCmdExecutionEx();
    void gotoAP(TextArea *area, int64_t lineNum, int64_t column);
    void gotoMark(TextArea *area, QChar label, bool extendSel);
    void GotoMatchingCharacter(TextArea *area);
    void handleUnparsedRegionEx(const std::shared_ptr<TextBuffer> &styleBuf, int64_t pos) const;
    void macroBannerTimeoutProc();
    void MakeSelectionVisible(TextArea *area);
    void moveDocument(MainWindow *fromWindow);
    void open(const QString &fullpath);
    void PrintStringEx(const std::string &string, const QString &jobName);
    void PrintWindow(TextArea *area, bool selectedOnly);
    void RaiseDocument();
    void RaiseDocumentWindow();
    void RaiseFocusDocumentWindow(bool focus);
    void ReadMacroInitFileEx();
    void repeatMacro(const QString &macro, int how);
    void ResumeMacroExecutionEx();
    void runMacroEx(const std::shared_ptr<Program> &prog);
    void SelectNumberedLineEx(TextArea *area, int64_t lineNum);
    void SelectToMatchingCharacter(TextArea *area);
    void SetAutoIndent(IndentStyle state);
    void SetAutoScroll(int margin);
    void SetAutoWrap(WrapStyle state);
    void SetColors(const QString &textFg, const QString &textBg, const QString &selectFg, const QString &selectBg, const QString &hiliteFg, const QString &hiliteBg, const QString &lineNoFg, const QString &cursorFg);
    void SetEmTabDist(int emTabDist);
    void SetHighlightSyntax(bool value);
    void SetIncrementalBackup(bool value);
    void SetLanguageMode(size_t mode, bool forceNewDefaults);
    void SetMakeBackupCopy(bool value);
    void SetMatchSyntaxBased(bool value);
    void SetOverstrike(bool overstrike);
    void SetShowMatching(ShowMatchingStyle state);
    void SetShowStatisticsLine(bool value);
    void SetTabDist(int tabDist);
    void SetUserLocked(bool value);
    void SetUseTabs(bool value);
    void setWrapMargin(int margin);
    void shellBannerTimeoutProc();
    void ShellCmdToMacroStringEx(const QString &command, const QString &input);
    void ShowStatsLine(bool state);
    void splitPane();
    void StartHighlightingEx(bool warn);
    void StopHighlightingEx();
    void UpdateHighlightStylesEx();
    int ShowTipStringEx(const QString &text, bool anchored, int pos, bool lookup, TagSearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignStrict alignMode);
    void editTaggedLocationEx(TextArea *area, int i);
    void SetBacklightChars(const QString &applyBacklightTypes);

private:
    void createSelectMenuEx(TextArea *area, const QStringList &args);
    int findAllMatchesEx(TextArea *area, const QString &string);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    bool bckError(const QString &errString, const QString &file);
    bool doOpen(const QString &name, const QString &path, int flags);
    bool doSave();
    boost::optional<int64_t> findMatchingCharEx(char toMatch, Style styleToMatch, int64_t charPos, int64_t startLimit, int64_t endLimit);
    bool includeFile(const QString &name);
    bool writeBckVersion();
    HighlightData *compilePatternsEx(const gsl::span<HighlightPattern> &patternSrc);
    int CloseFileAndWindow(CloseMode preResponse);
    bool cmpWinAgainstFile(const QString &fileName) const;
    int fileWasModifiedExternally() const;
    int MacroWindowCloseActionsEx();
    size_t matchLanguageMode() const;
    int SaveWindow();
    bool SaveWindowAs(const QString &newName, bool addWrap);
    int WriteBackupFile();
    MacroContinuationCode continueWorkProcEx();
    PatternSet *findPatternsForWindowEx(bool warn);
    QString backupFileNameEx() const;
    QString getWindowsMenuEntry() const;
    std::shared_ptr<Regex> compileREAndWarnEx(const QString &re);
    Style GetHighlightInfoEx(int64_t pos);
    StyleTableEntry *styleTableEntryOfCodeEx(size_t hCode) const;
    TextArea *createTextArea(TextBuffer *buffer);
    void AbortMacroCommandEx();
    void actionClose(CloseMode mode);
    void addRedoItem(UndoInfo &&redo);
    void addUndoItem(UndoInfo &&undo);
    void addWrapNewlines();
    void appendDeletedText(view::string_view deletedText, int64_t deletedLen, Direction direction);
    void AttachHighlightToWidgetEx(TextArea *area);
    void BeginLearnEx();
    void cancelLearnEx();
    void ClearRedoList();
    void ClearUndoList();
    void CloseDocument();
    void DetermineLanguageMode(bool forceNewDefaults);
    void dimSelDepItemsInMenu(QMenu *menuPane, const gsl::span<MenuData> &menuList, bool enabled);
    void DimSelectionDepUserMenuItems(bool enabled);
    void documentRaised();
    void DoShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, CommandSource source);
    void eraseFlashEx();
    void ExecCursorLineEx(TextArea *area, CommandSource source);
    void executeModMacroEx(SmartIndentEvent *cbInfo);
    void executeNewlineMacroEx(SmartIndentEvent *cbInfo);
    void FilterSelection(const QString &command, CommandSource source);
    void filterSelection(const QString &filterText);
    void FinishLearnEx();
    void FlashMatchingEx(TextArea *area);
    void FreeHighlightingDataEx();
    void issueCommandEx(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, int64_t replaceLeft, int64_t replaceRight, CommandSource source);
    void reapplyLanguageMode(size_t mode, bool forceDefaults);
    void Redo();
    void refreshMenuBar();
    void RefreshMenuToggleStates();
    void RefreshTabState();
    void RefreshWindowStates();
    void RemoveBackupFile() const;
    void removeRedoItem();
    void removeUndoItem();
    void ReplayEx();
    void RevertToSaved();
    void SaveUndoInformation(int64_t pos, int64_t nInserted, int64_t nDeleted, view::string_view deletedText);
    void SetModeMessageEx(const QString &message);
    void SetWindowModified(bool modified);
    void trimUndoList(size_t maxLength);
    void Undo();
    void UnloadLanguageModeTipsFileEx();
    void UpdateMarkTable(int64_t pos, int64_t nInserted, int64_t nDeleted);
    void UpdateStatsLine(TextArea *area);    

public:
    bool multiFileBusy_     = false;             // suppresses multiple beeps/dialogs during multi-file replacements
    bool filenameSet_       = false;             // is the window still "Untitled"?
    bool fileChanged_       = false;             // has window been modified?
    bool overstrike_        = false;             // is overstrike mode turned on ?
    FileFormats fileFormat_ = FileFormats::Unix; // whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks

public:
    QString path_;                                         // path component of file being edited
    QString filename_;                                     // name component of file being edited
    QString fontName_;                                     // names of the text fonts in use
    QString boldFontName_;
    QString italicFontName_;
    QString boldItalicFontName_;
    LockReasons lockReasons_;                              // all ways a file can be locked
    TextBuffer *buffer_;                                   // holds the text being edited
    bool replaceFailed_ = false;                           // flags replacements failures during multi-file replacements
    bool highlightSyntax_;                                 // is syntax highlighting turned on?
    bool showStats_;                                       // is stats line supposed to be shown
    bool saveOldVersion_;                                  // keep old version in filename.bc
    bool autoSave_;                                        // is autosave turned on?
    size_t languageMode_;                                  // identifies language mode currently selected in the window
    int matchSyntaxBased_;                                 // Use syntax info to show matching
    ShowMatchingStyle showMatchingStyle_;                  // How to show matching parens: None, Delimeter, or Range
    IndentStyle indentStyle_;                              // whether/how to auto indent
    WrapStyle wrapMode_;                                   // line wrap style: None, Newline or Continuous
    std::unique_ptr<WindowHighlightData> highlightData_;   // info for syntax highlighting
    std::shared_ptr<MacroCommandData>    macroCmdData_;    // same for macro commands
    std::shared_ptr<RangesetTable>       rangesetTable_;   // current range sets

private:
    QMenu *contextMenu_    = nullptr;
    bool fileMissing_      = true;                      // is the window's file gone?
    bool ignoreModify_     = false;                     // ignore modifications to text area
    dev_t dev_             = 0;                         // device where the file resides
    gid_t gid_             = 0;                         // last recorded group id of the file
    ino_t ino_             = 0;                         // file's inode
    int autoSaveCharCount_ = 0;                         // count of single characters typed since last backup file generated
    int autoSaveOpCount_   = 0;                         // count of editing operations ""
    size_t nMarks_         = 0;                         // number of active bookmarks
    mode_t mode_           = 0;                         // permissions of file being edited
    time_t lastModTime_    = 0;                         // time of last modification to file
    uid_t uid_             = 0;                         // last recorded user id of the file

private:
    std::array<Bookmark, MAX_MARKS> markTable_;         // marked locations in window
    bool backlightChars_;                               // is char backlighting turned on?
    QFont boldFontStruct_;
    QFont boldItalicFontStruct_;
    QFont fontStruct_;
    QFont italicFontStruct_;
    QString backlightCharTypes_;                        // what backlighting to use
    QString modeMessage_;                               // stats line banner content for learn and shell command executing modes
    QTimer *flashTimer_;                                // timer for getting rid of highlighted matching paren.
    std::deque<UndoInfo> redo_;                         // info for redoing last undone op
    std::deque<UndoInfo> undo_;                         // info for undoing last operation
    std::unique_ptr<ShellCommandData> shellCmdData_;    // when a shell command is executing, info. about it, otherwise, nullptr
    std::unique_ptr<SmartIndentData>  smartIndentData_; // compiled macros for smart indent
	QSplitter *splitter_;
	Ui::DocumentWidget ui;

public:
    static DocumentWidget *LastCreated;
};

#endif
