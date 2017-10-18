
#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include "FileFormats.h"
#include "SearchDirection.h"
#include "SearchType.h"
#include "SmartIndentEvent.h"
#include "NewMode.h"
#include "CloseMode.h"
#include "WrapMode.h"
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
    MainWindow (QWidget *parent = 0, Qt::WindowFlags flags = 0);
	virtual ~MainWindow() = default;
	
private:
	void setupMenuGroups();
	void setupMenuStrings();
	void setupTabBar();
	void setupMenuAlternativeMenus();
    void CreateLanguageModeSubMenu();
    void setupMenuDefaults();
    void setupPrevOpenMenuActions();
    QMenu *createUserMenu(DocumentWidget *document, const QVector<MenuData> &data);

public:
    void updateLanguageModeSubmenu();
    void UpdateUserMenus(DocumentWidget *document);

private:
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;
    virtual bool eventFilter(QObject *object, QEvent *event) override;

public:
	DialogReplace *getDialogReplace() const;	
	int TabCount();    
	void SortTabBar();
	void UpdateWindowReadOnly(DocumentWidget *doc);
	void UpdateWindowTitle(DocumentWidget *doc);
    DocumentWidget *currentDocument() const;
    DocumentWidget *documentAt(int index) const;
    QList<DocumentWidget *> openDocuments() const;
    QString PromptForExistingFileEx(const QString &path, const QString &prompt);
    QString PromptForExistingFileEx(const QString &prompt);
    QString PromptForNewFileEx(DocumentWidget *document, const QString &prompt, FileFormats *fileFormat, bool *addWrap);
    bool CheckPrefsChangesSavedEx();
    bool CloseAllDocumentInWindow();
    int updateGutterWidth();
    int updateLineNumDisp();
    void BeginISearchEx(SearchDirection direction);
    void EndISearchEx();
    void ShowLineNumbers(bool state);
    void TempShowISearch(bool state);
    void addToGroup(QActionGroup *group, QMenu *menu);
    void fileCB(DocumentWidget *window, const QString &text);
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

public:
    static void DimPasteReplayBtns(bool enabled);
    static void ReadNEditDB();
    static void WriteNEditDB();
    static void AddToPrevOpenMenu(const QString &filename);
    static void invalidatePrevOpenMenus();
    static MainWindow *firstWindow();
    static QList<MainWindow *> allWindows();    
    static QString UniqueUntitledNameEx();
    static DocumentWidget *FindWindowWithFile(const QString &name, const QString &path);
    static DocumentWidget *EditNewFileEx(MainWindow *inWindow, QString geometry, bool iconic, const QString &languageMode, const QString &defaultPath);
    static void AllWindowsBusyEx(const QString &message);
    static void AllWindowsUnbusyEx();
    static void BusyWaitEx();
    static void CheckCloseDimEx();
    static bool CloseAllFilesAndWindowsEx();
    static void InvalidateWindowMenus();

public:
	DocumentWidget *CreateDocument(QString name);

public Q_SLOTS:
	// internal variants
    void action_New(NewMode mode = NewMode::Prefs);
    void action_Open(const QString &filename);
	void action_Save_As(const QString &filename, bool wrapped);
    void action_Close(CloseMode mode = CloseMode::Prompt);
    void action_Load_Macro_File(const QString &filename);
    void action_Load_Tags_File(const QString &filename);
    void action_Unload_Tags_File(const QString &filename);
    void action_Load_Tips_File(const QString &filename);
    void action_Unload_Tips_File(const QString &filename);
    void action_Find(const QString &string, SearchDirection direction, SearchType type, WrapMode searchWrap);
    void action_Find_Dialog(SearchDirection direction, SearchType type, bool keepDialog);
    void action_Find_Again(SearchDirection direction, WrapMode wrap);
    void action_Find_Selection(SearchDirection direction, SearchType type, WrapMode wrap);
    void action_Replace(SearchDirection direction, const QString &searchString, const QString &replaceString, SearchType type, WrapMode wrap);
    void action_Replace_Dialog(SearchDirection direction, SearchType type, bool keepDialog);
    void action_Replace_All(const QString &searchString, const QString &replaceString, SearchType type);
    void action_Replace_Again(SearchDirection direction, WrapMode wrap);
    void action_Mark(const QString &mark);
    void action_Goto_Mark(const QString &mark, bool extend);
    void action_Goto_Mark_Dialog(bool extend);
    void action_Find_Definition(const QString &argument);
    void action_Show_Tip(const QString &argument);
    void action_Filter_Selection(const QString &filter);
    void action_Execute_Command(const QString &command);
    void action_Shell_Menu_Command(const QString &name);
    void action_Macro_Menu_Command(const QString &name);
    void action_Repeat_Macro(const QString &macro, int how);
    void action_Detach_Document(DocumentWidget *document);

    void action_Shift_Left_Tabs();
    void action_Shift_Right_Tabs();
    void action_Shift_Find();
    void action_Shift_Find_Again();
    void action_Shift_Find_Selection_triggered();
    void action_Shift_Find_Incremental_triggered();
    void action_Shift_Replace_triggered();
    void action_Shift_Replace_Find_Again_triggered();
    void action_Shift_Replace_Again_triggered();
    void action_Shift_Goto_Matching_triggered();

    // These are a bit weird as they are multi-key shortcuts
    // and act a bit differently from the menu
    void action_Mark_Shortcut_triggered();
    void action_Shift_Goto_Mark_Shortcut_triggered();
    void action_Goto_Mark_Shortcut_triggered();

    // has no visual shortcut at all
    void action_Next_Document();
    void action_Prev_Document();
    void action_Last_Document();

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
    void action_Include_File(const QString &filename);
	void action_Goto_Line_Number(const QString &s);

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
	void deleteTabButtonClicked();
    void raiseCB();
    void setLangModeCB(QAction *action);
    void openPrevCB(QAction *action);
    void unloadTipsFileCB(QAction *action);
    void unloadTagsFileCB(QAction *action);
    void focusChanged(QWidget *from, QWidget *to);

public:
    QList<QAction *>   previousOpenFilesList_;
	QPointer<QDialog>  dialogFind_;
	QPointer<QDialog>  dialogReplace_;
	QPointer<TextArea> lastFocus_;    
    bool               showLineNumbers_; // is the line number display shown
	bool               showISearchLine_; // is incr. search line to be shown
    int fHistIndex_;                   // history placeholders for
    int rHistIndex_;
    bool iSearchLastLiteralCase_;      // idem, for literal mode
    bool iSearchLastRegexCase_;        // idem, for regex mode in incremental search bar
    int iSearchHistIndex_;             //   find and replace dialogs
    int iSearchLastBeginPos_;          // beg. pos. last match of current i.s.
    int iSearchStartPos_;              // start pos. of current incr. search
    QVector<DocumentWidget *> writableWindows_;      // temporary list of writable documents, used during multi-file replacements
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
