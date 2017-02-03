
#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include "smartIndentCBStruct.h"
#include "SearchDirection.h"
#include "SearchType.h"
#include "FileFormats.h"
#include <QMainWindow>
#include <QPointer>

class TextArea;
class DocumentWidget;
class DialogReplace;
class MenuData;

#include "ui_MainWindow.h"

class MainWindow : public QMainWindow {
	Q_OBJECT
	friend class DocumentWidget;
    friend class DialogReplace;

public:
    MainWindow (QWidget *parent = 0, Qt::WindowFlags flags = 0);
	virtual ~MainWindow();
	
private:
	void setupMenuGroups();
	void setupMenuStrings();
	void setupTabBar();
	void setupMenuAlternativeMenus();
    void CreateLanguageModeSubMenu();
    void setupMenuDefaults();
    QMenu *createUserMenu(DocumentWidget *document, const QVector<MenuData> &data);

public:
    void updateLanguageModeSubmenu();
    void UpdateUserMenus(DocumentWidget *document);

private:
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;

public:
    void parseGeometry(const char *geometry);
	void UpdateWindowTitle(DocumentWidget *doc);
	DialogReplace *getDialogReplace() const;
	void InvalidateWindowMenus();
	void UpdateWindowReadOnly(DocumentWidget *doc);
	void SortTabBar();
	int TabCount();    
    QList<DocumentWidget *> openDocuments() const;
    int updateLineNumDisp();
    int updateGutterWidth();
    void TempShowISearch(bool state);
    QString PromptForExistingFileEx(const QString &path, const QString &prompt);
    void forceShowLineNumbers();
    void ShowLineNumbers(bool state);
    void AddToPrevOpenMenu(const QString &filename);
    static void ReadNEditDB();
    static void WriteNEditDB();
    void invalidatePrevOpenMenus();
    void updatePrevOpenMenu();
    void fileCB(DocumentWidget *window, const std::string &value);
    void initToggleButtonsiSearch(SearchType searchType);
    void EndISearchEx();
    void BeginISearchEx(SearchDirection direction);
    QString PromptForExistingFileEx(const QString &prompt);
    void updateTipsFileMenuEx();
    void updateTagsFileMenuEx();
    DocumentWidget *currentDocument() const;
    DocumentWidget *documentAt(int index) const;
    void setWindowSizeDefault(int rows, int cols);
    void updateWindowSizeMenus();
    void updateWindowSizeMenu();
    bool PromptForNewFileEx(DocumentWidget *document, const QString prompt, char *fullname, FileFormats *fileFormat, bool *addWrap);
    bool CheckPrefsChangesSavedEx();
    bool CloseAllDocumentInWindow();
    void addToGroup(QActionGroup *group, QMenu *menu);


public:
    static QList<MainWindow *> allWindows();
    static QList<DocumentWidget *> allDocuments();
    static QString UniqueUntitledNameEx();
    static DocumentWidget *FindWindowWithFile(const QString &name, const QString &path);
    static DocumentWidget *EditNewFileEx(MainWindow *inWindow, char *geometry, bool iconic, const char *languageMode, const QString &defaultPath);
    static void AllWindowsBusyEx(const QString &message);
    static void AllWindowsUnbusyEx();
    static void BusyWaitEx();
    static void CheckCloseDimEx();
    static bool CloseAllFilesAndWindowsEx();

public:
	DocumentWidget *CreateDocument(QString name);

public Q_SLOTS:
	// internal variants
    void action_New(const QString &mode = QString());
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

public Q_SLOTS:
    void on_tabWidget_currentChanged(int index);
    void on_tabWidget_customContextMenuRequested(int index, const QPoint &pos);
    void on_editIFind_textChanged(const QString &text);
    void on_editIFind_returnPressed();
    void on_buttonIFind_clicked();
    void on_checkIFindCase_toggled(bool searchCaseSense);
    void on_checkIFindRegex_toggled(bool value);
    void on_checkIFindReverse_toggled(bool value);

public Q_SLOTS:
    // File Menu
	void on_action_New_triggered();
	void on_action_New_Window_triggered();
	void on_action_Open_triggered();
	void on_action_About_triggered();
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
#if 0

    void on_action_Replay_Keystrokes_triggered();

#endif
    void on_action_About_Qt_triggered();

public Q_SLOTS:
    void unloadTipsAP(const QString &filename);
    void unloadTagsAP(const QString &filename);
    void loadTipsAP(const QString &filename);
    void loadTagsAP(const QString &filename);
    void loadMacroAP(const QString &filename);

private Q_SLOTS:
	void deleteTabButtonClicked();
    void raiseCB();
    void setLangModeCB(QAction *action);
    void openPrevCB(QAction *action);
    void unloadTipsFileCB(QAction *action);
    void unloadTagsFileCB(QAction *action);
    void focusChanged(QWidget *from, QWidget *to);

public:
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

public:
    Ui::MainWindow ui;
};

#endif
