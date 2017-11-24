
#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include "FileFormats.h"
#include "Direction.h"
#include "SearchType.h"
#include "NewMode.h"
#include "CloseMode.h"
#include "IndentStyle.h"
#include "WrapMode.h"

#include <vector>
#include <gsl/span>

#include <QMainWindow>
#include <QPointer>

#include "ui_MainWindow.h"

class TextArea;
class DocumentWidget;
class DialogReplace;
struct MenuData;

class MainWindow : public QMainWindow {
	Q_OBJECT
	friend class DocumentWidget;
    friend class DialogReplace;

public:
    MainWindow (QWidget *parent = Q_NULLPTR, Qt::WindowFlags flags = Qt::WindowFlags());
    ~MainWindow() override = default;
	
private:
	void setupMenuGroups();
	void setupMenuStrings();
	void setupTabBar();
	void setupMenuAlternativeMenus();
    void CreateLanguageModeSubMenu();
    void setupMenuDefaults();
    void setupGlobalPrefenceDefaults();
    void setupDocumentPrefernceDefaults();
    void setupPrevOpenMenuActions();
    QMenu *createUserMenu(DocumentWidget *document, const gsl::span<MenuData> &data);

public:
    void updateLanguageModeSubmenu();
    void UpdateUserMenus(DocumentWidget *document);
    void UpdateUserMenus();

private:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

public:
    void setAutoIndent(IndentStyle state);
    void EditHighlightPatterns();
	DialogReplace *getDialogReplace() const;	
    size_t TabCount() const;
	void SortTabBar();
	void UpdateWindowReadOnly(DocumentWidget *doc);
	void UpdateWindowTitle(DocumentWidget *doc);
    DocumentWidget *currentDocument() const;
    DocumentWidget *documentAt(int index) const;
    std::vector<DocumentWidget *> openDocuments() const;
    QString PromptForExistingFileEx(const QString &path, const QString &prompt);
    QString PromptForExistingFileEx(const QString &prompt);
    QString PromptForNewFileEx(DocumentWidget *document, const QString &prompt, FileFormats *fileFormat, bool *addWrap);
    bool CheckPrefsChangesSavedEx();
    bool CloseAllDocumentInWindow();
    int updateGutterWidth();
    int updateLineNumDisp();
    void BeginISearchEx(Direction direction);
    void EndISearchEx();
    void ShowLineNumbers(bool state);
    void TempShowISearch(bool state);
    void addToGroup(QActionGroup *group, QMenu *menu);
    void openFile(DocumentWidget *document, const QString &text);
    void forceShowLineNumbers();
    void initToggleButtonsiSearch(SearchType searchType);
    void parseGeometry(QString geometry);
    void setWindowSizeDefault(int rows, int cols);
    void updatePrevOpenMenu();
    void updateTagsFileMenuEx();
    void updateTipsFileMenuEx();
    void updateWindowMenu();
    void updateWindowSizeMenu();
    void updateWindowSizeMenus();
    void SetIncrementalSearchLineMS(bool value);
    bool SearchWindowEx(DocumentWidget *document, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW);
    bool SearchAndSelectEx(DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap);
    bool SearchAndSelectIncrementalEx(DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap, bool continued);
    bool ReplaceAndSearchEx(DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWrap);
    void DoFindDlogEx(DocumentWidget *document, Direction direction, bool keepDialogs, SearchType searchType);
    bool SearchAndSelectSameEx(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
    bool SearchAndReplaceEx(DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWrap);
    bool ReplaceFindSameEx(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
    bool ReplaceSameEx(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
    void SearchForSelectedEx(DocumentWidget *document, TextArea *area, Direction direction, SearchType searchType, WrapMode searchWrap);
    void ReplaceInSelectionEx(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType);
    bool prefOrUserCancelsSubstEx(DocumentWidget *document);
    bool ReplaceAllEx(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType);
    void DoFindReplaceDlogEx(DocumentWidget *document, TextArea *area, Direction direction, bool keepDialogs, SearchType searchType);
    void iSearchRecordLastBeginPosEx(Direction direction, int initPos);
    void iSearchTryBeepOnWrapEx(Direction direction, int beginPos, int startPos);
    bool searchMatchesSelectionEx(DocumentWidget *document, const QString &searchString, SearchType searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW);
    DocumentWidget *CreateDocument(QString name);

public:
    static void updateMenuItems();
    static MainWindow *fromDocument(const DocumentWidget *document);
    static void DimPasteReplayBtns(bool enabled);
    static void ReadNEditDB();
    static void WriteNEditDB();
    static void AddToPrevOpenMenu(const QString &filename);
    static void invalidatePrevOpenMenus();
    static MainWindow *firstWindow();
    static std::vector<MainWindow *> allWindows();
    static QString UniqueUntitledNameEx();
    static DocumentWidget *FindWindowWithFile(const QString &name, const QString &path);
    static DocumentWidget *EditNewFileEx(MainWindow *window, QString geometry, bool iconic, const QString &languageMode, const QString &defaultPath);
    static void AllWindowsBusyEx(const QString &message);
    static void AllWindowsUnbusyEx();
    static void CheckCloseDimEx();
    static bool CloseAllFilesAndWindowsEx();
    static void InvalidateWindowMenus();
    static void UpdateLanguageModeMenu();
    static void updateHighlightStyleMenu();
    static void RenameHighlightPattern(const QString &oldName, const QString &newName);
    static bool LMHasHighlightPatterns(const QString &languageMode);

public:
    // internal variants of signals
    void action_Close(DocumentWidget *document, CloseMode mode = CloseMode::Prompt);
    void action_Close_Pane(DocumentWidget *document);
    void action_Delete(DocumentWidget *document);
    void action_Detach_Document(DocumentWidget *document);
    void action_Detach_Document_Dialog(DocumentWidget *document);
    void action_Execute_Command(DocumentWidget *document);
    void action_Execute_Command(DocumentWidget *document, const QString &command);
    void action_Execute_Command_Line(DocumentWidget *document);
    void action_Find_Incremental(DocumentWidget *document, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWraps, bool isContinue);
    void action_Exit(DocumentWidget *document);
    void action_Fill_Paragraph(DocumentWidget *document);
    void action_Filter_Selection(DocumentWidget *document);
    void action_Filter_Selection(DocumentWidget *document, const QString &filter);
    void action_Find_Again(DocumentWidget *document, Direction direction, WrapMode wrap);
    void action_Replace_In_Selection(DocumentWidget *document, const QString &searchString, const QString &replaceString, SearchType searchType);
    void action_Replace_Find(DocumentWidget *document, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWraps);
    void action_Find_Definition(DocumentWidget *document);
    void action_Find_Definition(DocumentWidget *document, const QString &argument);
    void action_Find_Dialog(DocumentWidget *document, Direction direction, SearchType type, bool keepDialog);
    void action_Find(DocumentWidget *document, const QString &string, Direction direction, SearchType type, WrapMode searchWrap);
    void action_Find_Selection(DocumentWidget *document, Direction direction, SearchType type, WrapMode wrap);
    void action_Goto_Line_Number(DocumentWidget *document);
    void action_Goto_Line_Number(DocumentWidget *document, const QString &s);
    void action_Goto_Mark_Dialog(DocumentWidget *document, bool extend);
    void action_Goto_Mark(DocumentWidget *document, const QString &mark, bool extend);
    void action_Goto_Matching(DocumentWidget *document);
    void action_Goto_Selected(DocumentWidget *document);
    void action_Include_File(DocumentWidget *document);
    void action_Include_File(DocumentWidget *document, const QString &filename);
    void action_Insert_Ctrl_Code(DocumentWidget *document);
    void action_Insert_Ctrl_Code(DocumentWidget *document, const QString &str);
    void action_Load_Calltips_File(DocumentWidget *document);
    void action_Load_Macro_File(DocumentWidget *document);
    void action_Load_Macro_File(DocumentWidget *document, const QString &filename);
    void action_Load_Tags_File(DocumentWidget *document);
    void action_Load_Tags_File(DocumentWidget *document, const QString &filename);
    void action_Load_Tips_File(DocumentWidget *document, const QString &filename);
    void action_Lower_case(DocumentWidget *document);
    void action_Macro_Menu_Command(DocumentWidget *document, const QString &name);
    void action_Mark(DocumentWidget *document);
    void action_Mark(DocumentWidget *document, const QString &mark);
    void action_Move_Tab_To(DocumentWidget *document);
    void action_New(DocumentWidget *document, NewMode mode = NewMode::Prefs);
    void action_Open(DocumentWidget *document);
    void action_Open(DocumentWidget *document, const QString &filename);
    void action_Open_Selected(DocumentWidget *document);
    void action_Print(DocumentWidget *document);
    void action_Print_Selection(DocumentWidget *document);
    void action_Redo(DocumentWidget *document);
    void action_Repeat(DocumentWidget *document);
    void action_Repeat_Macro(DocumentWidget *document, const QString &macro, int how);
    void action_Replace_Again(DocumentWidget *document, Direction direction, WrapMode wrap);
    void action_Replace_All(DocumentWidget *document, const QString &searchString, const QString &replaceString, SearchType type);
    void action_Replace_Dialog(DocumentWidget *document, Direction direction, SearchType type, bool keepDialog);
    void action_Replace(DocumentWidget *document, Direction direction, const QString &searchString, const QString &replaceString, SearchType type, WrapMode wrap);
    void action_Revert_to_Saved(DocumentWidget *document);
    void action_Save_As(DocumentWidget *document);
    void action_Save_As(DocumentWidget *document, const QString &filename, bool wrapped);
    void action_Save(DocumentWidget *document);
    void action_Select_All(DocumentWidget *document);
    void action_Shell_Menu_Command(DocumentWidget *document, const QString &name);
    void action_Shift_Find_Again(DocumentWidget *document);
    void action_Shift_Find(DocumentWidget *document);
    void action_Shift_Find_Incremental(DocumentWidget *document);
    void action_Shift_Find_Selection(DocumentWidget *document);
    void action_Shift_Goto_Matching(DocumentWidget *document);
    void action_Shift_Left(DocumentWidget *document);
    void action_Shift_Left_Tabs(DocumentWidget *document);
    void action_Shift_Replace(DocumentWidget *document);
    void action_Shift_Replace_Find_Again(DocumentWidget *document);
    void action_Shift_Right(DocumentWidget *document);
    void action_Shift_Right_Tabs(DocumentWidget *document);
    void action_Show_Calltip(DocumentWidget *document);
    void action_Show_Tip(DocumentWidget *document, const QString &argument);
    void action_Split_Pane(DocumentWidget *document);
    void action_Undo(DocumentWidget *document);
    void action_Unload_Tags_File(DocumentWidget *document, const QString &filename);
    void action_Unload_Tips_File(DocumentWidget *document, const QString &filename);
    void action_Upper_case(DocumentWidget *document);

public Q_SLOTS:
    // has no visual shortcut at all
    void action_Next_Document();
    void action_Prev_Document();
    void action_Last_Document();

    // These are a bit weird as they are multi-key shortcuts
    // and act a bit differently from the menu
    void action_Mark_Shortcut();
    void action_Shift_Goto_Mark_Shortcut();
    void action_Goto_Mark_Shortcut();

    // Others...
    void action_Shift_Replace_Again();
    void action_Shift_Goto_Matching();
    void action_Shift_Find();
    void action_Shift_Find_Again();
    void action_Shift_Replace();
    void action_Shift_Left_Tabs();
    void action_Shift_Right_Tabs();
    void action_Shift_Find_Selection();
    void action_Shift_Find_Incremental();

public Q_SLOTS:
    // groups
    void indentGroupTriggered(QAction *action);
    void wrapGroupTriggered(QAction *action);
    void matchingGroupTriggered(QAction *action);
    void defaultIndentGroupTriggered(QAction *action);
    void defaultWrapGroupTriggered(QAction *action);
    void defaultTagCollisionsGroupTriggered(QAction *action);
    void defaultSearchGroupTriggered(QAction *action);
    void defaultSyntaxGroupTriggered(QAction *action);
    void defaultMatchingGroupTriggered(QAction *action);
    void defaultSizeGroupTriggered(QAction *action);
    void macroTriggered(QAction *action);
    void shellTriggered(QAction *action);
#if defined(REPLACE_SCOPE)
    void defaultReplaceScopeGroupTriggered(QAction *action);
#endif

public Q_SLOTS:
    void on_tabWidget_currentChanged(int index);
    void on_tabWidget_customContextMenuRequested(const QPoint &pos);
    void on_editIFind_textChanged(const QString &text);
    void on_editIFind_returnPressed();
    void on_buttonIFind_clicked();
    void on_checkIFindCase_toggled(bool searchCaseSense);
	void on_checkIFindRegex_toggled(bool searchRegex);
	void on_checkIFindReverse_toggled(bool value);

public Q_SLOTS:
    // File Menu
    void on_action_New_triggered();
    void on_action_New_Window_triggered();
    void on_action_Open_triggered();
    void on_action_Select_All_triggered();
    void on_action_Open_Selected_triggered();
    void on_action_Close_triggered();
    void on_action_Include_File_triggered();
    void on_action_Load_Calltips_File_triggered();
    void on_action_Load_Tags_File_triggered();
    void on_action_Load_Macro_File_triggered();
    void on_action_Print_triggered();
    void on_action_Print_Selection_triggered();
    void on_action_Save_triggered();
    void on_action_Save_As_triggered();
    void on_action_Revert_to_Saved_triggered();
    void on_action_Exit_triggered();

    // Edit Menu
    void on_action_Undo_triggered();
    void on_action_Redo_triggered();
	void on_action_Cut_triggered();
	void on_action_Copy_triggered();
	void on_action_Paste_triggered();	
    void on_action_Paste_Column_triggered();
    void on_action_Delete_triggered();
    void on_action_Shift_Left_triggered();
    void on_action_Shift_Right_triggered();
    void on_action_Lower_case_triggered();
    void on_action_Upper_case_triggered();
    void on_action_Fill_Paragraph_triggered();
    void on_action_Insert_Form_Feed_triggered();
    void on_action_Insert_Ctrl_Code_triggered();

    void on_action_Goto_Line_Number_triggered();
    void on_action_Goto_Selected_triggered();
    void on_action_Find_triggered();
    void on_action_Find_Again_triggered();
    void on_action_Find_Selection_triggered();
    void on_action_Find_Incremental_triggered();
    void on_action_Replace_triggered();
    void on_action_Replace_Find_Again_triggered();
    void on_action_Replace_Again_triggered();
    void on_action_Mark_triggered();
    void on_action_Goto_Mark_triggered();
    void on_action_Goto_Matching_triggered();
    void on_action_Show_Calltip_triggered();
    void on_action_Find_Definition_triggered();
    void on_action_Execute_Command_triggered();
    void on_action_Execute_Command_Line_triggered();
    void on_action_Filter_Selection_triggered();
    void on_action_Cancel_Shell_Command_triggered();

    void on_action_Detach_Tab_triggered();
    void on_action_Split_Pane_triggered();
    void on_action_Close_Pane_triggered();
    void on_action_Move_Tab_To_triggered();

    void on_action_Statistics_Line_toggled(bool state);
    void on_action_Incremental_Search_Line_toggled(bool state);
    void on_action_Show_Line_Numbers_toggled(bool state);
    void on_action_Wrap_Margin_triggered();
    void on_action_Tab_Stops_triggered();
    void on_action_Text_Fonts_triggered();
    void on_action_Highlight_Syntax_toggled(bool state);
    void on_action_Apply_Backlighting_toggled(bool state);
    void on_action_Make_Backup_Copy_toggled(bool state);
    void on_action_Incremental_Backup_toggled(bool state);
    void on_action_Matching_Syntax_toggled(bool state);
    void on_action_Overtype_toggled(bool state);
    void on_action_Read_Only_toggled(bool state);
    void on_action_Save_Defaults_triggered();

    // Preferences Defaults
    void on_action_Default_Language_Modes_triggered();
    void on_action_Default_Program_Smart_Indent_triggered();
    void on_action_Default_Wrap_Margin_triggered();
    void on_action_Default_Command_Shell_triggered();
    void on_action_Default_Tab_Stops_triggered();
    void on_action_Default_Text_Fonts_triggered();
    void on_action_Default_Colors_triggered();
    void on_action_Default_Shell_Menu_triggered();
    void on_action_Default_Macro_Menu_triggered();
    void on_action_Default_Window_Background_Menu_triggered();
    void on_action_Default_Sort_Open_Prev_Menu_toggled(bool state);
    void on_action_Default_Show_Path_In_Windows_Menu_toggled(bool state);
    void on_action_Default_Customize_Window_Title_triggered();
    void on_action_Default_Search_Verbose_toggled(bool state);
    void on_action_Default_Search_Wrap_Around_toggled(bool state);
    void on_action_Default_Search_Beep_On_Search_Wrap_toggled(bool state);
    void on_action_Default_Search_Keep_Dialogs_Up_toggled(bool state);
    void on_action_Default_Syntax_Recognition_Patterns_triggered();
    void on_action_Default_Syntax_Text_Drawing_Styles_triggered();
    void on_action_Default_Apply_Backlighting_toggled(bool state);
    void on_action_Default_Tab_Open_File_In_New_Tab_toggled(bool state);
    void on_action_Default_Tab_Show_Tab_Bar_toggled(bool state);
    void on_action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open_toggled(bool state);
    void on_action_Default_Tab_Next_Prev_Tabs_Across_Windows_toggled(bool state);
    void on_action_Default_Tab_Sort_Tabs_Alphabetically_toggled(bool state);
    void on_action_Default_Show_Tooltips_toggled(bool state);
    void on_action_Default_Statistics_Line_toggled(bool state);
    void on_action_Default_Incremental_Search_Line_toggled(bool state);
    void on_action_Default_Show_Line_Numbers_toggled(bool state);
    void on_action_Default_Make_Backup_Copy_toggled(bool state);
    void on_action_Default_Incremental_Backup_toggled(bool state);
    void on_action_Default_Matching_Syntax_Based_toggled(bool state);
    void on_action_Default_Terminate_with_Line_Break_on_Save_toggled(bool state);
    void on_action_Default_Popups_Under_Pointer_toggled(bool state);
    void on_action_Default_Auto_Scroll_Near_Window_Top_Bottom_toggled(bool state);
    void on_action_Default_Warnings_Files_Modified_Externally_toggled(bool state);
    void on_action_Default_Warnings_Check_Modified_File_Contents_toggled(bool state);
    void on_action_Default_Warnings_On_Exit_toggled(bool state);
    void on_action_Learn_Keystrokes_triggered();
    void on_action_Finish_Learn_triggered();
    void on_action_Cancel_Learn_triggered();
    void on_action_Repeat_triggered();
    void on_action_Replay_Keystrokes_triggered();
    void on_action_About_triggered();
    void on_action_About_Qt_triggered();
    void on_action_Help_triggered();

private Q_SLOTS:
    void focusChanged(QWidget *from, QWidget *to);

public:
    QPointer<TextArea> lastFocus() const { return lastFocus_; }

public:
    int  fHistIndex_;
    int  rHistIndex_;
    bool showISearchLine_;
    bool showLineNumbers_;

private:
    QList<QAction *>   previousOpenFilesList_;
	QPointer<QDialog>  dialogFind_;
	QPointer<QDialog>  dialogReplace_;
	QPointer<TextArea> lastFocus_;    
    bool iSearchLastLiteralCase_;      // idem, for literal mode
    bool iSearchLastRegexCase_;        // idem, for regex mode in incremental search bar
    int iSearchHistIndex_;             //   find and replace dialogs
    int iSearchLastBeginPos_;          // beg. pos. last match of current i.s.
    int iSearchStartPos_;              // start pos. of current incr. search    
    bool wasSelected_;                 // last selection state (for dim/undim of selection related menu items
#if defined(REPLACE_SCOPE)
    QAction *replaceScopeInWindow_;
    QAction *replaceScopeInSelection_;
    QAction *replaceScopeSmart_;
#endif
public:
    Ui::MainWindow ui;
};

#endif
