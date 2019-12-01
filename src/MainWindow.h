
#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include "CloseMode.h"
#include "CommandSource.h"
#include "Direction.h"
#include "IndentStyle.h"
#include "NewMode.h"
#include "Search.h"
#include "SearchType.h"
#include "TextCursor.h"
#include "Util/FileFormats.h"
#include "WrapMode.h"
#include "WrapStyle.h"
#include "userCmds.h"

#include <gsl/span>
#include <vector>

#include <QFileDialog>
#include <QFileInfoList>
#include <QMainWindow>
#include <QPointer>

#include "ui_MainWindow.h"

class TextArea;
class PathInfo;
class DocumentWidget;
class DialogReplace;
class DialogFind;
class DialogShellMenu;
class DialogMacros;
class DialogWindowBackgroundMenu;
struct MenuData;
struct TextRange;

class MainWindow final : public QMainWindow {
	Q_OBJECT
	friend class DocumentWidget;
	friend class DialogReplace;

public:
	explicit MainWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
	~MainWindow() override;

private:
	void setupMenuGroups();
	void setupMenuStrings();
	void setupTabBar();
	void setupMenuAlternativeMenus();
	void createLanguageModeSubMenu();
	void setupMenuDefaults();
	void setupGlobalPrefenceDefaults();
	void setupDocumentPreferenceDefaults();
	void setupPrevOpenMenuActions();
	QMenu *createUserMenu(DocumentWidget *document, const gsl::span<MenuData> &data, CommandTypes type);
	QTabWidget *tabWidget() const;

private:
	void keyPressEvent(QKeyEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;

public:
	bool checkPrefsChangesSaved();
	bool closeAllDocumentsInWindow();
	bool execNamedBGMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, CommandSource source);
	bool execNamedMacroMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, CommandSource source);
	bool execNamedShellMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, CommandSource source);
	bool getIncrementalSearchLine() const;
	bool getShowLineNumbers() const;
	bool prefOrUserCancelsSubst(DocumentWidget *document);
	bool replaceAll(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType);
	bool replaceAndSearch(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWrap);
	bool replaceSame(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
	bool searchAndReplace(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWrap);
	bool searchAndSelect(DocumentWidget *document, TextArea *area, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap);
	bool searchAndSelectIncremental(DocumentWidget *document, TextArea *area, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap, bool continued);
	bool searchAndSelectSame(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap);
	bool searchMatchesSelection(DocumentWidget *document, const QString &searchString, SearchType searchType, TextRange *textRange, TextCursor *extentBW, TextCursor *extentFW);
	bool searchWindow(DocumentWidget *document, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap, int64_t beginPos, Search::Result *searchResult);
	DocumentWidget *createDocument(const QString &name);
	DocumentWidget *currentDocument() const;
	DocumentWidget *documentAt(int index) const;
	int updateGutterWidth();
	int updateLineNumDisp();
	QStringList promptForExistingFiles(const QString &path, const QString &prompt, QFileDialog::FileMode mode);
	size_t tabCount() const;
	std::vector<DocumentWidget *> openDocuments() const;
	void beginISearch(Direction direction);
	void editHighlightPatterns();
	void editHighlightStyles(const QString &initialStyle);
	void endISearch();
	void forceShowLineNumbers();
	void initToggleButtonsiSearch(SearchType searchType);
	void iSearchRecordLastBeginPos(Direction direction, TextCursor initPos);
	void iSearchTryBeepOnWrap(Direction direction, TextCursor beginPos, TextCursor startPos);
	void openFile(DocumentWidget *document, const QString &text);
	void parseGeometry(QString geometry);
	void replaceInSelection(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType);
	void searchForSelected(DocumentWidget *document, TextArea *area, Direction direction, SearchType searchType, WrapMode searchWrap);
	void setIncrementalSearchLine(bool value);
	void setShowLineNumbers(bool show);
	void setWindowSizeDefault(int rows, int cols);
	void showLineNumbers(bool state);
	void sortTabBar();
	void tempShowISearch(bool state);
	void updateLanguageModeSubmenu();
	void updatePrevOpenMenu();
	void updateTagsFileMenu();
	void updateTipsFileMenu();
	void updateUserMenus();
	void updateUserMenus(DocumentWidget *document);
	void updateWindowMenu();
	void updateWindowSizeMenu();
	void updateWindowSizeMenus();
	QPointer<TextArea> lastFocus();

private:
	QFileInfoList openFileHelper(DocumentWidget *document, const QString &text, QString *searchPath, QString *searchName) const;
	QFileInfoList openFileHelperSystem(DocumentWidget *document, const QRegularExpressionMatch &match, QString *searchPath, QString *searchName) const;
	QFileInfoList openFileHelperLocal(DocumentWidget *document, const QRegularExpressionMatch &match, QString *searchPath, QString *searchName) const;
	QFileInfoList openFileHelperString(DocumentWidget *document, const QString &text, QString *searchPath, QString *searchName) const;

public:
	static bool closeAllFilesAndWindows();
	static DocumentWidget *editNewFile(MainWindow *window, const QString &geometry, bool iconic, const QString &languageMode);
	static DocumentWidget *editNewFile(MainWindow *window, const QString &geometry, bool iconic, const QString &languageMode, const QDir &defaultPath);
	static DocumentWidget *findWindowWithFile(const QString &filename, const QString &path);
	static DocumentWidget *findWindowWithFile(const PathInfo &path);
	static QString promptForNewFile(DocumentWidget *document, FileFormats *format, bool *addWrap);
	static MainWindow *firstWindow();
	static MainWindow *fromDocument(const DocumentWidget *document);
	static QString uniqueUntitledName();
	static std::vector<MainWindow *> allWindows(bool includeInvisible = false);
	static void addToPrevOpenMenu(const QString &filename);
	static void allDocumentsBusy(const QString &message);
	static void allDocumentsUnbusy();
	static void checkCloseEnableState();
	static void checkCloseEnableState(const std::vector<MainWindow *> &windows);
	static void invalidatePrevOpenMenus();
	static void readNEditDB();
	static void renameHighlightPattern(const QString &oldName, const QString &newName);
	static void updateMenuItems();
	static void updateWindowMenus();
	static void writeNEditDB();
	static void removeFromPrevOpenMenu(const QString &filename);

private:
	void editIFind_textChanged(const QString &text);
	void buttonIFind_clicked();
	void checkIFindCase_toggled(bool searchCaseSense);
	void checkIFindRegex_toggled(bool searchRegex);
	void checkIFindReverse_toggled(bool value);
	void editIFind_returnPressed();
	void connectSlots();

public Q_SLOTS:
	// internal variants of signals
	void action_New_Window(DocumentWidget *document);
	void action_Set_Auto_Indent(DocumentWidget *document, IndentStyle state);
	void action_Set_Auto_Wrap(DocumentWidget *document, WrapStyle state);
	void action_Close(DocumentWidget *document, CloseMode mode = CloseMode::Prompt);
	void action_Close_Pane(DocumentWidget *document);
	void action_Delete(DocumentWidget *document);
	void action_Detach_Document_Dialog(DocumentWidget *document);
	void action_Detach_Document(DocumentWidget *document);
	void action_Execute_Command(DocumentWidget *document);
	void action_Execute_Command(DocumentWidget *document, const QString &command);
	void action_Execute_Command_Line(DocumentWidget *document);
	void action_Exit(DocumentWidget *document);
	void action_Fill_Paragraph(DocumentWidget *document);
	void action_Filter_Selection(DocumentWidget *document, CommandSource source);
	void action_Filter_Selection(DocumentWidget *document, const QString &filter, CommandSource source);
	void action_Find_Again(DocumentWidget *document, Direction direction, WrapMode wrap);
	void action_Replace_Find_Again(DocumentWidget *document, Direction direction, WrapMode wrap);
	void action_Find_Definition(DocumentWidget *document);
	void action_Find_Definition(DocumentWidget *document, const QString &argument);
	void action_Find_Dialog(DocumentWidget *document, Direction direction, SearchType type, bool keepDialog);
	void action_Find(DocumentWidget *document, const QString &string, Direction direction, SearchType type, WrapMode searchWrap);
	void action_Find_Incremental(DocumentWidget *document, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWraps, bool isContinue);
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
	void action_Replace(DocumentWidget *document, const QString &searchString, const QString &replaceString, Direction direction, SearchType type, WrapMode wrap);
	void action_Replace_Find(DocumentWidget *document, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWraps);
	void action_Replace_In_Selection(DocumentWidget *document, const QString &searchString, const QString &replaceString, SearchType type);
	void action_Revert_to_Saved(DocumentWidget *document);
	void action_Save_As(DocumentWidget *document);
	void action_Save_As(DocumentWidget *document, const QString &filename, bool wrapped);
	void action_Save(DocumentWidget *document);
	void action_Select_All(DocumentWidget *document);
	void action_Shell_Menu_Command(DocumentWidget *document, const QString &name);
	void action_Shift_Find_Again(DocumentWidget *document);
	void action_Shift_Find(DocumentWidget *document);
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

	// has no visual shortcut at all
	void action_Next_Document();
	void action_Prev_Document();
	void action_Last_Document();

	// These are a bit weird as they are multi-key shortcuts
	// and act a bit differently from the menu
	void action_Mark_Shortcut();
	void action_Shift_Goto_Mark_Shortcut();
	void action_Goto_Mark_Shortcut();
	void action_Goto_Mark_Shortcut_Helper(bool shifted);

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

public:
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
#ifdef PER_TAB_CLOSE
	void on_tabWidget_tabCloseRequested(int index);
#endif
	void on_tabWidget_tabCountChanged(int count);
	void on_tabWidget_currentChanged(int index);
	void on_tabWidget_customContextMenuRequested(const QPoint &pos);

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

public Q_SLOTS:
	void selectionChanged(bool selected);
	void undoAvailable(bool available);
	void redoAvailable(bool available);
	void updateStatus(DocumentWidget *document, TextArea *area);
	void updateWindowReadOnly(DocumentWidget *document);
	void updateWindowTitle(DocumentWidget *document);

public:
	int fHistIndex_;
	int rHistIndex_;
	bool showISearchLine_;
	bool showLineNumbers_;

private:
	QList<QAction *> previousOpenFilesList_;
	QPointer<DialogFind> dialogFind_;
	QPointer<DialogReplace> dialogReplace_;
	QPointer<DialogShellMenu> dialogShellMenu_;
	QPointer<DialogMacros> dialogMacros_;
	QPointer<DialogWindowBackgroundMenu> dialogWindowBackgroundMenu_;
	QPointer<TextArea> lastFocus_;

private:
	bool iSearchLastLiteralCase_    = false;          // idem, for literal mode
	bool iSearchLastRegexCase_      = true;           // idem, for regex mode in incremental search bar
	int iSearchHistIndex_           = 0;              // find and replace dialogs
	TextCursor iSearchLastBeginPos_ = {};             // beg. pos. last match of current i.s.
	TextCursor iSearchStartPos_     = TextCursor(-1); // start pos. of current incr. search

public:
	Ui::MainWindow ui;
};

#endif
