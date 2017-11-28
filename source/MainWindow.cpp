
#include "MainWindow.h"
#include "CommandRecorder.h"
#include "DialogAbout.h"
#include "DialogColors.h"
#include "DialogExecuteCommand.h"
#include "DialogFilter.h"
#include "DialogFind.h"
#include "DialogFonts.h"
#include "DialogLanguageModes.h"
#include "DialogMacros.h"
#include "DialogRepeat.h"
#include "DialogReplace.h"
#include "DialogShellMenu.h"
#include "DialogSmartIndent.h"
#include "DialogSyntaxPatterns.h"
#include "DialogTabs.h"
#include "DialogWindowBackgroundMenu.h"
#include "DialogWindowSize.h"
#include "DialogWindowTitle.h"
#include "DialogWrapMargin.h"
#include "DocumentWidget.h"
#include "Help.h"
#include "highlightData.h"
#include "LanguageMode.h"
#include "nedit.h"
#include "PatternSet.h"
#include "preferences.h"
#include "search.h"
#include "Settings.h"
#include "shift.h"
#include "SignalBlocker.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "util/ClearCase.h"
#include "util/fileUtils.h"
#include "util/utils.h"
#include "WindowMenuEvent.h"

#include <QClipboard>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>

#include <cmath>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#if !defined(DONT_HAVE_GLOB)
#include <glob.h>
#endif

namespace {

// Min. # of columns in line number display
constexpr int MIN_LINE_NUM_COLS = 4;

bool currentlyBusy = false;
qint64 busyStartTime = 0;
bool modeMessageSet = false;

QPointer<DialogWindowBackgroundMenu> WindowBackgroundMenu;
QPointer<DialogMacros>               WindowMacros;
QPointer<DialogSyntaxPatterns>       SyntaxPatterns;
QPointer<DocumentWidget>             lastFocusDocument;

auto neditDBBadFilenameChars = QLatin1String("\n");

QVector<QString> PrevOpen;

/*
** Extract the line and column number from the text string.
** Set the line and/or column number to -1 if not specified, and return -1 if
** both line and column numbers are not specified.
*/
int StringToLineAndCol(const QString &text, int *lineNum, int *column) {

    static const QRegularExpression re(QLatin1String(
                                           "^"
                                           "\\s*"
                                           "(?<row>[-+]?[1-9]\\d*)?"
                                           "\\s*"
                                           "([:,]"
                                           "\\s*"
                                           "(?<col>[-+]?[1-9]\\d*))?"
                                           "\\s*"
                                           "$"
                                           ));

    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        QString row = match.captured(QLatin1String("row"));
        QString col = match.captured(QLatin1String("col"));

        bool row_ok;
        int r = row.toInt(&row_ok);
        if(!row_ok) {
            r = -1;
        } else {
            r = qBound(0, r, INT_MAX);
        }

        bool col_ok;
        int c = col.toInt(&col_ok);
        if(!col_ok) {
            c = -1;
        } else {
            c = qBound(0, c, INT_MAX);
        }

        *lineNum = r;
        *column  = c;

        return (r == -1 && c == -1) ? -1 : 0;
    }

    return -1;
}


}

/**
 * @brief MainWindow::MainWindow
 * @param parent
 * @param flags
 */
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags) {
	ui.setupUi(this);

    connect(qApp, &QApplication::focusChanged, this, &MainWindow::focusChanged);
	
	setupMenuGroups();
	setupTabBar();
	setupMenuStrings();
	setupMenuAlternativeMenus();
    CreateLanguageModeSubMenu();
    setupMenuDefaults();

    setupPrevOpenMenuActions();
    updatePrevOpenMenu();

    showISearchLine_       = GetPrefISearchLine();
    iSearchHistIndex_      = 0;
    iSearchStartPos_       = -1;
    iSearchLastRegexCase_  = true;
    iSearchLastLiteralCase_= false;
    wasSelected_           = false;
    showLineNumbers_       = GetPrefLineNums();
	
	// default to hiding the optional panels
    ui.incrementalSearchFrame->setVisible(showISearchLine_);

    ui.action_Statistics_Line->setChecked(GetPrefStatsLine());

    CheckCloseDimEx();
}

/**
 * @brief MainWindow::parseGeometry
 * @param geometry
 */
void MainWindow::parseGeometry(QString geometry) {
    int rows = -1;
    int cols = -1;

    /* If window geometry was specified, split it apart into a window position
       component and a window size component.  Create a new geometry string
       containing the position component only.  Rows and cols are stripped off
       because we can't easily calculate the size in pixels from them until the
       whole window is put together.  Note that the preference resource is only
       for clueless users who decide to specify the standard X geometry
       application resource, which is pretty useless because width and height
       are the same as the rows and cols preferences, and specifying a window
       location will force all the windows to pile on top of one another */
    if (geometry.isEmpty()) {
        geometry = GetPrefGeometry();
    }

    if (geometry.isEmpty()) {
        rows = GetPrefRows();
        cols = GetPrefCols();
    } else {

        QRegExp regex(QLatin1String("(?:([0-9]+)(?:[xX]([0-9]+)(?:([\\+-][0-9]+)(?:([\\+-][0-9]+))?)?)?)?"));
        if(regex.exactMatch(geometry)) {
            cols = regex.cap(1).toInt();
            rows = regex.cap(2).toInt();
        }
    }

    if(DocumentWidget *document = currentDocument()) {
        QFontMetrics fm(document->fontStruct_);
		constexpr int margin = 5;
		int w = (fm.maxWidth() * cols) + (margin * 2);
		int h = (fm.ascent() + fm.descent()) * rows + (margin * 2);

		// NOTE(eteran): why 17? your guess is as good as mine
		// but getting the width/height from the actual scrollbars
		// yielded nonsense on my system (like 100px). Maybe it's a bug
		// involving HiDPI screens? I dunno
		w += 17; // area->verticalScrollBar()->width();
		h += 17; // area->horizontalScrollBar()->height();

		document->setMinimumSize(w, h);
		adjustSize();

        // NOTE(eteran): this processEvents() is to force the resize
		// to happen now, so that we can set the widget to be fully
		// resizable again right after we make everything adjust
		QApplication::processEvents();
		document->setMinimumSize(0, 0);
    }
}

/**
 * @brief MainWindow::setupTabBar
 */
void MainWindow::setupTabBar() {
	// create and hook up the tab close button
	auto deleteTabButton = new QToolButton(ui.tabWidget);
	deleteTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	deleteTabButton->setIcon(QIcon::fromTheme(tr("tab-close")));
	deleteTabButton->setAutoRaise(true);
    deleteTabButton->setFocusPolicy(Qt::NoFocus);
    deleteTabButton->setObjectName(tr("tab-close"));
	ui.tabWidget->setCornerWidget(deleteTabButton);
	
    connect(deleteTabButton, &QToolButton::clicked, this, &MainWindow::on_action_Close_triggered);

    ui.tabWidget->tabBar()->installEventFilter(this);
}

/**
 * @brief MainWindow::setupGlobalPrefenceDefaults
 */
void MainWindow::setupGlobalPrefenceDefaults() {

    // Default Indent
    switch(GetPrefAutoIndent(PLAIN_LANGUAGE_MODE)) {
    case IndentStyle::None:
        no_signals(ui.action_Default_Indent_Off)->setChecked(true);
        break;
    case IndentStyle::Auto:
        no_signals(ui.action_Default_Indent_On)->setChecked(true);
        break;
    case IndentStyle::Smart:
        no_signals(ui.action_Default_Indent_Smart)->setChecked(true);
        break;
    default:
        break;
    }

    // Default Wrap
    switch(GetPrefWrap(PLAIN_LANGUAGE_MODE)) {
    case WrapStyle::None:
        no_signals(ui.action_Default_Wrap_None)->setChecked(true);
        break;
    case WrapStyle::Newline:
        no_signals(ui.action_Default_Wrap_Auto_Newline)->setChecked(true);
        break;
    case WrapStyle::Continuous:
        no_signals(ui.action_Default_Wrap_Continuous)->setChecked(true);
        break;
    default:
        break;
    }

    // Default Search Settings
    no_signals(ui.action_Default_Search_Verbose)->setChecked(GetPrefSearchDlogs());
    no_signals(ui.action_Default_Search_Wrap_Around)->setChecked(GetPrefSearchWraps() == WrapMode::Wrap);
    no_signals(ui.action_Default_Search_Beep_On_Search_Wrap)->setChecked(GetPrefBeepOnSearchWrap());
    no_signals(ui.action_Default_Search_Keep_Dialogs_Up)->setChecked(GetPrefKeepSearchDlogs());

    switch(GetPrefSearch()) {
    case SearchType::Literal:
        no_signals(ui.action_Default_Search_Literal)->setChecked(true);
        break;
    case SearchType::CaseSense:
        no_signals(ui.action_Default_Search_Literal_Case_Sensitive)->setChecked(true);
        break;
    case SearchType::LiteralWord:
        no_signals(ui.action_Default_Search_Literal_Whole_Word)->setChecked(true);
        break;
    case SearchType::CaseSenseWord:
        no_signals(ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word)->setChecked(true);
        break;
    case SearchType::Regex:
        no_signals(ui.action_Default_Search_Regular_Expression)->setChecked(true);
        break;
    case SearchType::RegexNoCase:
        no_signals(ui.action_Default_Search_Regular_Expresison_Case_Insensitive)->setChecked(true);
        break;
    }

    // Default syntax
    if(GetPrefHighlightSyntax()) {
        no_signals(ui.action_Default_Syntax_On)->setChecked(true);
    } else {
        no_signals(ui.action_Default_Syntax_Off)->setChecked(true);
    }

    no_signals(ui.action_Default_Apply_Backlighting)->setChecked(GetPrefBacklightChars());

    // Default tab settings
    no_signals(ui.action_Default_Tab_Open_File_In_New_Tab)->setChecked(GetPrefOpenInTab());
    no_signals(ui.action_Default_Tab_Show_Tab_Bar)->setChecked(GetPrefTabBar());
    no_signals(ui.action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open)->setChecked(GetPrefTabBarHideOne());
    no_signals(ui.action_Default_Tab_Next_Prev_Tabs_Across_Windows)->setChecked(GetPrefGlobalTabNavigate());
    no_signals(ui.action_Default_Tab_Sort_Tabs_Alphabetically)->setChecked(GetPrefSortTabs());

    no_signals(ui.tabWidget)->tabBar()->setVisible(GetPrefTabBar());
    no_signals(ui.tabWidget)->setTabBarAutoHide(GetPrefTabBarHideOne());

    no_signals(ui.action_Default_Show_Tooltips)->setChecked(GetPrefToolTips());
    no_signals(ui.action_Default_Statistics_Line)->setChecked(GetPrefStatsLine());
    no_signals(ui.action_Default_Incremental_Search_Line)->setChecked(GetPrefISearchLine());
    no_signals(ui.action_Default_Show_Line_Numbers)->setChecked(GetPrefLineNums());
    no_signals(ui.action_Default_Make_Backup_Copy)->setChecked(GetPrefSaveOldVersion());
    no_signals(ui.action_Default_Incremental_Backup)->setChecked(GetPrefAutoSave());

    switch(GetPrefShowMatching()) {
    case ShowMatchingStyle::None:
        no_signals(ui.action_Default_Matching_Off)->setChecked(true);
        break;
    case ShowMatchingStyle::Delimeter:
        no_signals(ui.action_Default_Matching_Delimiter)->setChecked(true);
        break;
    case ShowMatchingStyle::Range:
        no_signals(ui.action_Default_Matching_Range)->setChecked(true);
        break;
    }

    no_signals(ui.action_Default_Matching_Syntax_Based)->setChecked(GetPrefMatchSyntaxBased());
    no_signals(ui.action_Default_Terminate_with_Line_Break_on_Save)->setChecked(GetPrefAppendLF());
    no_signals(ui.action_Default_Popups_Under_Pointer)->setChecked(GetPrefRepositionDialogs());
    no_signals(ui.action_Default_Auto_Scroll_Near_Window_Top_Bottom)->setChecked(GetPrefAutoScroll());
    no_signals(ui.action_Default_Warnings_Files_Modified_Externally)->setChecked(GetPrefWarnFileMods());
    no_signals(ui.action_Default_Warnings_Check_Modified_File_Contents)->setChecked(GetPrefWarnRealFileMods());
    no_signals(ui.action_Default_Warnings_On_Exit)->setChecked(GetPrefWarnExit());
    no_signals(ui.action_Default_Warnings_Check_Modified_File_Contents)->setEnabled(GetPrefWarnFileMods());

    no_signals(ui.action_Default_Sort_Open_Prev_Menu)->setChecked(GetPrefSortOpenPrevMenu());
    no_signals(ui.action_Default_Show_Path_In_Windows_Menu)->setChecked(GetPrefShowPathInWindowsMenu());
}

/**
 * @brief MainWindow::setupDocumentPrefernceDefaults
 */
void MainWindow::setupDocumentPrefernceDefaults() {

    // based on document, which defaults to this
    switch(GetPrefAutoIndent(PLAIN_LANGUAGE_MODE)) {
    case IndentStyle::None:
        no_signals(ui.action_Indent_Off)->setChecked(true);
        break;
    case IndentStyle::Auto:
        no_signals(ui.action_Indent_On)->setChecked(true);
        break;
    case IndentStyle::Smart:
        no_signals(ui.action_Indent_Smart)->setChecked(true);
        break;
    default:
        break;
    }

    // based on document, which defaults to this
    switch(GetPrefWrap(PLAIN_LANGUAGE_MODE)) {
    case WrapStyle::None:
        no_signals(ui.action_Wrap_None)->setChecked(true);
        break;
    case WrapStyle::Newline:
        no_signals(ui.action_Wrap_Auto_Newline)->setChecked(true);
        break;
    case WrapStyle::Continuous:
        no_signals(ui.action_Wrap_Continuous)->setChecked(true);
        break;
    default:
        break;
    }

    switch(GetPrefShowMatching()) {
    case ShowMatchingStyle::None:
        no_signals(ui.action_Matching_Off)->setChecked(true);
        break;
    case ShowMatchingStyle::Delimeter:
        no_signals(ui.action_Matching_Delimiter)->setChecked(true);
        break;
    case ShowMatchingStyle::Range:
        no_signals(ui.action_Matching_Range)->setChecked(true);
        break;
    }

    if(GetPrefSmartTags()) {
        no_signals(ui.action_Default_Tag_Smart)->setChecked(true);
    } else {
        no_signals(ui.action_Default_Tag_Show_All)->setChecked(true);
    }
}

/**
 * @brief MainWindow::setupMenuDefaults
 */
void MainWindow::setupMenuDefaults() {

    // active settings
    no_signals(ui.action_Statistics_Line)->setChecked(GetPrefStatsLine());
    no_signals(ui.action_Incremental_Search_Line)->setChecked(GetPrefISearchLine());
    no_signals(ui.action_Show_Line_Numbers)->setChecked(GetPrefLineNums());
    no_signals(ui.action_Highlight_Syntax)->setChecked(GetPrefHighlightSyntax());
    no_signals(ui.action_Apply_Backlighting)->setChecked(GetPrefBacklightChars());
    no_signals(ui.action_Make_Backup_Copy)->setChecked(GetPrefAutoSave());
    no_signals(ui.action_Incremental_Backup)->setChecked(GetPrefSaveOldVersion());
    no_signals(ui.action_Matching_Syntax)->setChecked(GetPrefMatchSyntaxBased());

    setupGlobalPrefenceDefaults();
    setupDocumentPrefernceDefaults();

    updateWindowSizeMenu();
    updateTipsFileMenuEx();
    updateTagsFileMenuEx();
    MainWindow::updateMenuItems();
}

/**
 * nedit has some menu shortcuts which are different from conventional
 * shortcuts. Fortunately, Qt has a means to do this stuff manually.
 *
 * @brief MainWindow::setupMenuStrings
 */
void MainWindow::setupMenuStrings() {

	ui.action_Shift_Left         ->setText(tr("Shift &Left\t[Shift] Ctrl+9"));
	ui.action_Shift_Right        ->setText(tr("Shift Ri&ght\t[Shift] Ctrl+0"));
	ui.action_Find               ->setText(tr("&Find...\t[Shift] Ctrl+F"));
	ui.action_Find_Again         ->setText(tr("F&ind Again\t[Shift] Ctrl+G"));
	ui.action_Find_Selection     ->setText(tr("Find &Selection\t[Shift] Ctrl+H"));
	ui.action_Find_Incremental   ->setText(tr("Fi&nd Incremental\t[Shift] Ctrl+I"));
	ui.action_Replace            ->setText(tr("&Replace...\t[Shift] Ctrl+R"));
	ui.action_Replace_Find_Again ->setText(tr("Replace Find &Again\t[Shift] Ctrl+T"));
	ui.action_Replace_Again      ->setText(tr("Re&place Again\t[Shift] Alt+T"));
	ui.action_Mark               ->setText(tr("Mar&k\tAlt+M a-z"));
	ui.action_Goto_Mark          ->setText(tr("G&oto Mark\t[Shift] Alt+G a-z"));
	ui.action_Goto_Matching      ->setText(tr("Goto &Matching (..)\t[Shift] Ctrl+M"));

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_9), this, SLOT(action_Shift_Left_Tabs()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_0), this, SLOT(action_Shift_Right_Tabs()));

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F), this, SLOT(action_Shift_Find()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_G), this, SLOT(action_Shift_Find_Again()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_H), this, SLOT(action_Shift_Find_Selection()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I), this, SLOT(action_Shift_Find_Incremental()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_R), this, SLOT(action_Shift_Replace()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M), this, SLOT(action_Shift_Goto_Matching()));

    // This is an annoying solution... we can probably do better...
    for(int key = Qt::Key_A; key <= Qt::Key_Z; ++key) {
        new QShortcut(QKeySequence(Qt::ALT + Qt::Key_M, key),             this, SLOT(action_Mark_Shortcut()));
        new QShortcut(QKeySequence(Qt::ALT + Qt::Key_G, key),             this, SLOT(action_Goto_Mark_Shortcut()));
        new QShortcut(QKeySequence(Qt::SHIFT + Qt::ALT + Qt::Key_G, key), this, SLOT(action_Shift_Goto_Mark_Shortcut()));
    }

    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown), this, SLOT(action_Next_Document()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp), this, SLOT(action_Prev_Document()));
    new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Home), this, SLOT(action_Last_Document()));
}

/**
 * @brief MainWindow::setupPrevOpenMenuActions
 */
void MainWindow::setupPrevOpenMenuActions() {
    const int maxPrevOpenFiles = GetPrefMaxPrevOpenFiles();

    if (maxPrevOpenFiles <= 0) {
        ui.action_Open_Previous->setEnabled(false);
    } else {

        // create all of the actions
        for(int i = 0; i < maxPrevOpenFiles; ++i) {
            auto action = new QAction(this);
            action->setVisible(true);
            previousOpenFilesList_.push_back(action);
        }

        auto prevMenu = new QMenu(this);

        prevMenu->addActions(previousOpenFilesList_);

        connect(prevMenu, &QMenu::triggered, this, [this](QAction *action) {

            auto filename = action->data().toString();

            if(DocumentWidget *document = currentDocument()) {
                if (filename.isNull()) {
                    return;
                }

                document->open(filename);
            }

            CheckCloseDimEx();
        });


        ui.action_Open_Previous->setMenu(prevMenu);
    }
}

/**
 * under some configurations, menu items have different text/functionality
 *
 * @brief MainWindow::setupMenuAlternativeMenus
 */
void MainWindow::setupMenuAlternativeMenus() {
	if(!GetPrefOpenInTab()) {
		ui.action_New_Window->setText(tr("New &Tab"));
    } else {
        ui.action_New_Window->setText(tr("New &Window"));
    }

#if defined(REPLACE_SCOPE)
    auto replaceScopeGroup = new QActionGroup(this);
    auto replaceScopeMenu = new QMenu(tr("Default &Replace Scope"), this);

    replaceScopeInWindow_    = replaceScopeMenu->addAction(tr("In &Window"));
    replaceScopeInSelection_ = replaceScopeMenu->addAction(tr("In &Selection"));
    replaceScopeSmart_       = replaceScopeMenu->addAction(tr("Smart"));

    replaceScopeInWindow_->setCheckable(true);
    replaceScopeInSelection_->setCheckable(true);
    replaceScopeSmart_->setCheckable(true);

    switch(GetPrefReplaceDefScope()) {
    case REPL_DEF_SCOPE_WINDOW:
        replaceScopeInWindow_->setChecked(true);
        break;
    case REPL_DEF_SCOPE_SELECTION:
        replaceScopeInSelection_->setChecked(true);
        break;
    case REPL_DEF_SCOPE_SMART:
        replaceScopeSmart_->setChecked(true);
        break;
    }

    ui.menu_Default_Searching->addMenu(replaceScopeMenu);

    replaceScopeGroup->addAction(replaceScopeInWindow_);
    replaceScopeGroup->addAction(replaceScopeInSelection_);
    replaceScopeGroup->addAction(replaceScopeSmart_);

    connect(replaceScopeGroup, &QActionGroup::triggered, this, &MainWindow::defaultReplaceScopeGroupTriggered);
#endif
}

/**
 * @brief MainWindow::setupMenuGroups
 */
void MainWindow::setupMenuGroups() {
	auto indentGroup = new QActionGroup(this);
	indentGroup->addAction(ui.action_Indent_Off);
	indentGroup->addAction(ui.action_Indent_On);
	indentGroup->addAction(ui.action_Indent_Smart);
	
    auto wrapGroup = new QActionGroup(this);
    wrapGroup->addAction(ui.action_Wrap_None);
    wrapGroup->addAction(ui.action_Wrap_Auto_Newline);
    wrapGroup->addAction(ui.action_Wrap_Continuous);
	
	auto matchingGroup = new QActionGroup(this);
	matchingGroup->addAction(ui.action_Matching_Off);
	matchingGroup->addAction(ui.action_Matching_Range);
	matchingGroup->addAction(ui.action_Matching_Delimiter);
	
	auto defaultIndentGroup = new QActionGroup(this);
	defaultIndentGroup->addAction(ui.action_Default_Indent_Off);
	defaultIndentGroup->addAction(ui.action_Default_Indent_On);
	defaultIndentGroup->addAction(ui.action_Default_Indent_Smart);
	
	auto defaultWrapGroup = new QActionGroup(this);
	defaultWrapGroup->addAction(ui.action_Default_Wrap_None);
	defaultWrapGroup->addAction(ui.action_Default_Wrap_Auto_Newline);
	defaultWrapGroup->addAction(ui.action_Default_Wrap_Continuous);
	
	auto defaultTagCollisionsGroup = new QActionGroup(this);
	defaultTagCollisionsGroup->addAction(ui.action_Default_Tag_Show_All);
	defaultTagCollisionsGroup->addAction(ui.action_Default_Tag_Smart);
	
	auto defaultSearchGroup = new QActionGroup(this);
	defaultSearchGroup->addAction(ui.action_Default_Search_Literal);
	defaultSearchGroup->addAction(ui.action_Default_Search_Literal_Case_Sensitive);
	defaultSearchGroup->addAction(ui.action_Default_Search_Literal_Whole_Word);
	defaultSearchGroup->addAction(ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word);
	defaultSearchGroup->addAction(ui.action_Default_Search_Regular_Expression);
	defaultSearchGroup->addAction(ui.action_Default_Search_Regular_Expresison_Case_Insensitive);
	
	auto defaultSyntaxGroup = new QActionGroup(this);
	defaultSyntaxGroup->addAction(ui.action_Default_Syntax_Off);
	defaultSyntaxGroup->addAction(ui.action_Default_Syntax_On);	
	
	auto defaultMatchingGroup = new QActionGroup(this);
	defaultMatchingGroup->addAction(ui.action_Default_Matching_Off);
	defaultMatchingGroup->addAction(ui.action_Default_Matching_Delimiter);
	defaultMatchingGroup->addAction(ui.action_Default_Matching_Range);

	auto defaultSizeGroup = new QActionGroup(this);
    defaultSizeGroup->addAction(ui.action_Default_Size_24_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_40_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_60_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_80_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_Custom);

    connect(indentGroup,               &QActionGroup::triggered, this, &MainWindow::indentGroupTriggered);
    connect(wrapGroup,                 &QActionGroup::triggered, this, &MainWindow::wrapGroupTriggered);
    connect(matchingGroup,             &QActionGroup::triggered, this, &MainWindow::matchingGroupTriggered);
    connect(defaultIndentGroup,        &QActionGroup::triggered, this, &MainWindow::defaultIndentGroupTriggered);
    connect(defaultWrapGroup,          &QActionGroup::triggered, this, &MainWindow::defaultWrapGroupTriggered);
    connect(defaultTagCollisionsGroup, &QActionGroup::triggered, this, &MainWindow::defaultTagCollisionsGroupTriggered);
    connect(defaultSearchGroup,        &QActionGroup::triggered, this, &MainWindow::defaultSearchGroupTriggered);
    connect(defaultSyntaxGroup,        &QActionGroup::triggered, this, &MainWindow::defaultSyntaxGroupTriggered);
    connect(defaultMatchingGroup,      &QActionGroup::triggered, this, &MainWindow::defaultMatchingGroupTriggered);
    connect(defaultSizeGroup,          &QActionGroup::triggered, this, &MainWindow::defaultSizeGroupTriggered);

}

/**
 * @brief MainWindow::on_action_New_triggered
 */
void MainWindow::on_action_New_triggered() {    
    if(DocumentWidget *document = currentDocument()) {
        action_New(document, NewMode::Prefs);
    }
}

/**
 * @brief MainWindow::action_New
 * @param document
 * @param mode
 */
void MainWindow::action_New(DocumentWidget *document, NewMode mode) {

    emit_event("new", to_string(mode));

    Q_ASSERT(document);

    bool openInTab = true;

    switch(mode) {
    case NewMode::Prefs:
        openInTab = GetPrefOpenInTab();
        break;
    case NewMode::Tab:
        openInTab = true;
        break;
    case NewMode::Window:
        openInTab = false;
        break;
    case NewMode::Opposite:
        openInTab = !GetPrefOpenInTab();
        break;
    }

    QString path = document->path_;

    MainWindow::EditNewFileEx(openInTab ? this : nullptr, QString(), false, QString(), path);
    MainWindow::CheckCloseDimEx();
}

/**
 * @brief MainWindow::PromptForExistingFileEx
 * @param path
 * @param prompt
 * @return
 */
QString MainWindow::PromptForExistingFileEx(const QString &path, const QString &prompt) {
    QFileDialog dialog(this, prompt);
    dialog.setOptions(QFileDialog::DontUseNativeDialog);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if(!path.isEmpty()) {
        dialog.setDirectory(path);
    }

    if(dialog.exec()) {
        const QStringList files = dialog.selectedFiles();
        return files[0];
    }

    return QString();
}

/**
 * @brief MainWindow::action_Open
 * @param document
 * @param filename
 */
void MainWindow::action_Open(DocumentWidget *document, const QString &filename) {

    emit_event("open", filename);
    document->open(filename);
}

void MainWindow::action_Open(DocumentWidget *document) {
    QString filename = PromptForExistingFileEx(document->path_, tr("Open File"));
    if (filename.isNull()) {
        return;
    }

    action_Open(document, filename);
    CheckCloseDimEx();
}

/**
 * @brief MainWindow::on_action_Open_triggered
 */
void MainWindow::on_action_Open_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Open(document);
    }
}

/**
 * @brief MainWindow::action_Close
 * @param document
 * @param mode
 */
void MainWindow::action_Close(DocumentWidget *document, CloseMode mode) {

    emit_event("close", to_string(mode));
    document->actionClose(mode);
}

/**
 * @brief MainWindow::on_action_Close_triggered
 */
void MainWindow::on_action_Close_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Close(document, CloseMode::Prompt);
    }
}

/**
 * @brief MainWindow::on_action_About_triggered
 */
void MainWindow::on_action_About_triggered() {
    static auto dialog = new DialogAbout(this);
	dialog->exec();
}

void MainWindow::action_Select_All(DocumentWidget *document) {

    emit_event("select_all");

    Q_UNUSED(document);
    if(QPointer<TextArea> area = lastFocus_) {
        area->selectAllAP();
    }
}

/**
 * @brief MainWindow::on_action_Select_All_triggered
 */
void MainWindow::on_action_Select_All_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Select_All(document);
    }
}

/**
 * @brief MainWindow::action_Include_File
 * @param document
 * @param filename
 */
void MainWindow::action_Include_File(DocumentWidget *document, const QString &filename) {

    emit_event("include_file", filename);

    if (document->CheckReadOnly()) {
        return;
    }

    if (filename.isNull()) {
        return;
    }

    document->includeFile(filename);
}

/**
 * @brief MainWindow::action_Include_File
 * @param document
 */
void MainWindow::action_Include_File(DocumentWidget *document) {
    if (document->CheckReadOnly()) {
        return;
    }

    QString filename = PromptForExistingFileEx(document->path_, tr("Include File"));

    if (filename.isNull()) {
        return;
    }

    document->includeFile(filename);
}

/**
 * @brief MainWindow::on_action_Include_File_triggered
 */
void MainWindow::on_action_Include_File_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Include_File(document);
    }
}

/**
 * @brief MainWindow::on_action_Cut_triggered
 */
void MainWindow::on_action_Cut_triggered() {
    if(QPointer<TextArea> area = lastFocus_) {
        area->cutClipboardAP();
	}
}

/**
 * @brief MainWindow::on_action_Copy_triggered
 */
void MainWindow::on_action_Copy_triggered() {
    if(QPointer<TextArea> area = lastFocus_) {
        area->copyClipboardAP();
	}
}

/**
 * @brief MainWindow::on_action_Paste_triggered
 */
void MainWindow::on_action_Paste_triggered() {
    if(QPointer<TextArea> area = lastFocus_) {
        area->pasteClipboardAP();
	}
}

/**
 * @brief MainWindow::on_action_Paste_Column_triggered
 */
void MainWindow::on_action_Paste_Column_triggered() {
    if(QPointer<TextArea> area = lastFocus_) {
        area->pasteClipboardAP(TextArea::RectFlag);
    }
}

void MainWindow::action_Delete(DocumentWidget *document) {

    emit_event("delete");

    if (document->CheckReadOnly()) {
        return;
    }

    document->buffer_->BufRemoveSelected();
}

/**
 * @brief MainWindow::on_action_Delete_triggered
 */
void MainWindow::on_action_Delete_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Delete(document);
    }
}

/**
 * @brief MainWindow::CreateDocument
 * @param name
 * @return
 */
DocumentWidget *MainWindow::CreateDocument(QString name) {
	auto window = new DocumentWidget(name, this);
	int i = ui.tabWidget->addTab(window, name);
	ui.tabWidget->setCurrentIndex(i);
	return window;
}

/**
 * @brief MainWindow::UpdateWindowTitle
 * @param doc
 */
void MainWindow::UpdateWindowTitle(DocumentWidget *doc) {

    QString clearCaseTag = ClearCase::GetViewTag();

	QString title = DialogWindowTitle::FormatWindowTitle(
		doc->filename_,
		doc->path_,
		clearCaseTag,
        GetPrefServerName(),
		IsServer,
		doc->filenameSet_,
		doc->lockReasons_,
		doc->fileChanged_,
        GetPrefTitleFormat());

	setWindowTitle(title);


	QString iconTitle = doc->filename_;

    if (doc->fileChanged_) {
		iconTitle.append(tr("*"));
	}

    setWindowIconText(iconTitle);

	/* If there's a find or replace dialog up in "Keep Up" mode, with a
	   file name in the title, update it too */
	if (auto dialog = qobject_cast<DialogFind *>(dialogFind_)) {
        dialog->setDocument(doc);
	}

	if(auto dialog = getDialogReplace()) {
        dialog->setDocument(doc);
	}

	// Update the Windows menus with the new name
	InvalidateWindowMenus();
}

/**
 * @brief MainWindow::getDialogReplace
 * @return
 */
DialogReplace *MainWindow::getDialogReplace() const {
	return qobject_cast<DialogReplace *>(dialogReplace_);
}

/*
** Invalidate the Window menus of all NEdit windows to but don't change
** the menus until they're needed (Originally, this was "UpdateWindowMenus",
** but creating and destroying manu items for every window every time a
** new window was created or something changed, made things move very
** slowly with more than 10 or so windows).
*/
void MainWindow::InvalidateWindowMenus() {

    for(MainWindow *window : MainWindow::allWindows()) {
        window->updateWindowMenu();
    }
}

/*
** Update the Window menu of a single window to reflect the current state of
** all NEdit windows as determined by the global window list.
*/
void MainWindow::updateWindowMenu() {

    // Make a sorted list of windows
    std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

    // NOTE(eteran): on my system, I have observed a **consistent** crash
    // when documents.size() == 17 while using std::sort. This makes zero sense at all...
    qSort(documents.begin(), documents.end(), [](const DocumentWidget *a, const DocumentWidget *b) {

        // Untitled first

        int rc = (a->filenameSet_ == b->filenameSet_) ? 0 : a->filenameSet_ && !b->filenameSet_ ? 1 : -1;
        if (rc != 0) {
            return rc < 0;
        }

        if(a->filename_ < b->filename_) {
            return true;
        }

        return a->path_ < b->path_;
    });

    ui.menu_Windows->clear();
    ui.menu_Windows->addAction(ui.action_Split_Pane);
    ui.menu_Windows->addAction(ui.action_Close_Pane);
    ui.menu_Windows->addSeparator();
    ui.menu_Windows->addAction(ui.action_Detach_Tab);
    ui.menu_Windows->addAction(ui.action_Move_Tab_To);
    ui.menu_Windows->addSeparator();

    for(DocumentWidget *document : documents) {
        QString title = document->getWindowsMenuEntry();

        ui.menu_Windows->addAction(title, this, [document]() {
            document->RaiseFocusDocumentWindow(true);
        });
    }
}

/*
** Update the read-only state of the text area(s) in the window, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void MainWindow::UpdateWindowReadOnly(DocumentWidget *doc) {

	bool state = doc->lockReasons_.isAnyLocked();

    for(TextArea *area : doc->textPanes()) {
		area->setReadOnly(state);
	}

	ui.action_Read_Only->setChecked(state);
	ui.action_Read_Only->setEnabled(!doc->lockReasons_.isAnyLockedIgnoringUser());
}

/*
** check if tab bar is to be shown on this window
*/
size_t MainWindow::TabCount() const {
	return ui.tabWidget->count();
}

/*
** Sort tabs in the tab bar alphabetically, if demanded so.
*/
void MainWindow::SortTabBar() {

    if (!GetPrefSortTabs()) {
		return;
    }

	// need more than one tab to sort
    const size_t nDoc = TabCount();
	if (nDoc < 2) {
		return;
	}

	// get a list of all the documents
    std::vector<DocumentWidget *> windows;
	windows.reserve(nDoc);

    for(size_t i = 0; i < nDoc; ++i) {
        if(auto document = documentAt(i)) {
            windows.push_back(document);
		}
	}

	// sort them first by filename, then by path
    qSort(windows.begin(), windows.end(), [](const DocumentWidget *a, const DocumentWidget *b) {
		if(a->filename_ < b->filename_) {
			return true;
		}
		return a->path_ < b->path_;
	});

	// shuffle around the tabs to their new indexes
    for(size_t i = 0; i < windows.size(); ++i) {
		int from = ui.tabWidget->indexOf(windows[i]);
		int to   = i;
        ui.tabWidget->tabBar()->moveTab(from, to);
	}
}

std::vector<MainWindow *> MainWindow::allWindows() {
    std::vector<MainWindow *> windows;

    Q_FOREACH(QWidget *widget, QApplication::topLevelWidgets()) {
        if(auto window = qobject_cast<MainWindow *>(widget)) {

            // we only include visible main windows because apparently
            // closed ones which haven't been deleted will end up in the list
            //if(window->isVisible()) {
                windows.push_back(window);
            //}
        }
    }
    return windows;
}

MainWindow *MainWindow::firstWindow() {
    Q_FOREACH(QWidget *widget, QApplication::topLevelWidgets()) {
        if(auto window = qobject_cast<MainWindow *>(widget)) {
           return window;
        }
    }
    return nullptr;
}

std::vector<DocumentWidget *> MainWindow::openDocuments() const {
    std::vector<DocumentWidget *> documents;
    documents.reserve(ui.tabWidget->count());

    for(int i = 0; i < ui.tabWidget->count(); ++i) {
        if(auto document = documentAt(i)) {
            documents.push_back(document);
        }
    }
    return documents;
}

/*
** Make sure the close menu item is dimmed appropriately for the current
** set of windows.  It should be dim only for the last Untitled, unmodified,
** editor window, and sensitive otherwise.
*/
void MainWindow::CheckCloseDimEx() {

    const std::vector<MainWindow *> windows = MainWindow::allWindows();

    if(windows.empty()) {
        return;
    }

    if(windows.size() == 1) {
        // if there is only one window, then *this* must be it... right?
        MainWindow *window = windows[0];

        std::vector<DocumentWidget *> documents = window->openDocuments();

        if(window->TabCount() == 1 && !documents.front()->filenameSet_ && !documents.front()->fileChanged_) {
            window->ui.action_Close->setEnabled(false);
        } else {
            window->ui.action_Close->setEnabled(true);
        }
    } else {
        // if there is more than one window, then by definition, more than one
        // document is open
        for(MainWindow *window : windows) {
            window->ui.action_Close->setEnabled(true);
        }
    }
}

/*
** Create either the variable Shell menu, Macro menu or Background menu
** items of "window" (driven by value of "menuType")
*/
QMenu *MainWindow::createUserMenu(DocumentWidget *document, const gsl::span<MenuData> &data) {

    auto rootMenu = new QMenu(this);
    for(int i = 0; i < data.size(); ++i) {
        const MenuData &menuData = data[i];

        bool found = menuData.info->umiLanguageModes.empty();

        for(size_t language = 0; language < menuData.info->umiLanguageModes.size(); ++language) {
            if(menuData.info->umiLanguageModes[language] == document->languageMode_) {
                found = true;
            }
        }

        if(!found) {
            continue;
        }

        QMenu *parentMenu = rootMenu;
        QString name = menuData.info->umiName;

        int index = 0;
        for (;;) {
            int subSep = name.indexOf(QLatin1Char('>'), index);
            if(subSep == -1) {
                name = name.mid(index);

                // create the actual action or, if it represents one of our
                // *very* common entries make it equivalent to the global
                // QAction representing that task
                if(menuData.item->cmd.trimmed() == QLatin1String("cut_clipboard()")) {
                    parentMenu->addAction(ui.action_Cut);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("copy_clipboard()")) {
                    parentMenu->addAction(ui.action_Copy);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("paste_clipboard()")) {
                    parentMenu->addAction(ui.action_Paste);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("undo()")) {
                    parentMenu->addAction(ui.action_Undo);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("redo()")) {
                    parentMenu->addAction(ui.action_Redo);
                } else {
                    QAction *action = parentMenu->addAction(name);
                    action->setData(i);

                    if(!menuData.item->shortcut.isEmpty()) {
                        action->setShortcut(menuData.item->shortcut);
                    }
                }

                break;
            }

            QString parentName = name.mid(index, subSep);
            int subSubSep = parentName.indexOf(QLatin1Char('>'));
            if(subSubSep != -1) {
                parentName = parentName.mid(0, subSubSep);
            }

            QList<QAction*> actions = parentMenu->actions();
            QAction *parentAction = nullptr;
            for(QAction *action : actions) {
                if(action->text() == parentName) {
                    parentAction = action;
                    break;
                }
            }

            if(!parentAction) {
                auto newMenu = new QMenu(parentName, this);
                parentMenu->addMenu(newMenu);
                parentMenu = newMenu;
            } else {
                parentMenu = parentAction->menu();
            }

            index = subSep + 1;
        }
    }
    return rootMenu;
}

void MainWindow::addToGroup(QActionGroup *group, QMenu *menu) {
    Q_FOREACH(QAction *action, menu->actions()) {
        if(QMenu *subMenu = action->menu()) {
            addToGroup(group, subMenu);
        }
        group->addAction(action);
    }
}

/*
** Update the Shell, Macro, and Window Background menus of window
** "window" from the currently loaded command descriptions.
*/
void MainWindow::UpdateUserMenus(DocumentWidget *document) {

    // NOTE(eteran): the old code used to only do this if the language mode changed
    //               we should probably restore that behavior

    /* update user menus, which are shared over all documents, only
       if language mode was changed */
    auto shellMenu = createUserMenu(document, ShellMenuData);
    ui.menu_Shell->clear();
    ui.menu_Shell->addAction(ui.action_Execute_Command);
    ui.menu_Shell->addAction(ui.action_Execute_Command_Line);
    ui.menu_Shell->addAction(ui.action_Filter_Selection);
    ui.menu_Shell->addAction(ui.action_Cancel_Shell_Command);
    ui.menu_Shell->addSeparator();
    ui.menu_Shell->addActions(shellMenu->actions());
    auto shellGroup = new QActionGroup(this);
    shellGroup->setExclusive(false);
    addToGroup(shellGroup, shellMenu);
    connect(shellGroup, &QActionGroup::triggered, this, &MainWindow::shellTriggered);

    auto macroMenu = createUserMenu(document, MacroMenuData);
    ui.menu_Macro->clear();
    ui.menu_Macro->addAction(ui.action_Learn_Keystrokes);
    ui.menu_Macro->addAction(ui.action_Finish_Learn);
    ui.menu_Macro->addAction(ui.action_Cancel_Learn);
    ui.menu_Macro->addAction(ui.action_Replay_Keystrokes);
    ui.menu_Macro->addAction(ui.action_Repeat);
    ui.menu_Macro->addSeparator();
    ui.menu_Macro->addActions(macroMenu->actions());
    auto macroGroup = new QActionGroup(this);
    macroGroup->setExclusive(false);
    addToGroup(macroGroup, macroMenu);
    connect(macroGroup, &QActionGroup::triggered, this, &MainWindow::macroTriggered);

    /* update background menu, which is owned by a single document, only
       if language mode was changed */
    document->contextMenu_ = createUserMenu(document, BGMenuData);
}

void MainWindow::UpdateUserMenus() {
    if(DocumentWidget *document = currentDocument()) {
        UpdateUserMenus(document);
    }
}




/*
** Re-build the language mode sub-menu using the current data stored
** in the master list: LanguageModes.
*/
void MainWindow::updateLanguageModeSubmenu() {

    auto languageGroup = new QActionGroup(this);
    auto languageMenu  = new QMenu(this);
    QAction *action = languageMenu->addAction(tr("Plain"));
    action->setData(static_cast<qulonglong>(PLAIN_LANGUAGE_MODE));
    action->setCheckable(true);
    action->setChecked(true);
    languageGroup->addAction(action);


    for (size_t i = 0; i < LanguageModes.size(); i++) {
        QAction *action = languageMenu->addAction(LanguageModes[i].name);
        action->setData(static_cast<qulonglong>(i));
        action->setCheckable(true);
        languageGroup->addAction(action);
    }

    ui.action_Language_Mode->setMenu(languageMenu);

    connect(languageMenu, &QMenu::triggered, this, [this](QAction *action) {

        if(auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget())) {
            const auto mode = action->data().value<qulonglong>();

            // If the mode didn't change, do nothing
            if (document->languageMode_ == mode) {
                return;
            }

            if(mode == PLAIN_LANGUAGE_MODE) {
                document->action_Set_Language_Mode(QString());
            } else {
                document->action_Set_Language_Mode(LanguageModes[mode].name);
            }
        }
    });
}

/*
** Create a submenu for chosing language mode for the current window.
*/
void MainWindow::CreateLanguageModeSubMenu() {
    updateLanguageModeSubmenu();
}

/*
**  If necessary, enlarges the window and line number display area to make
**  room for numbers.
*/
int MainWindow::updateLineNumDisp() {

    if (!showLineNumbers_) {
        return 0;
    }

    /* Decide how wide the line number field has to be to display all
       possible line numbers */
    return updateGutterWidth();
}

/*
**  Set the new gutter width in the window. Sadly, the only way to do this is
**  to set it on every single document, so we have to iterate over them.
*/
int MainWindow::updateGutterWidth() {

    int reqCols = MIN_LINE_NUM_COLS;
    int maxCols = 0;

    std::vector<DocumentWidget *> documents  = openDocuments();
    for(DocumentWidget *document : documents) {
        //  We found ourselves a document from this window. Pick a TextArea
        // (doesn't matter which), so we can probe some details...
        if(TextArea *area = document->firstPane()) {
            const int lineNumCols = area->getLineNumCols();

            /* Is the width of the line number area sufficient to display all the
               line numbers in the file?  If not, expand line number field, and the
               this width. */
            maxCols = std::max(maxCols, lineNumCols);

            const int tmpReqCols = area->getBufferLinesCount() < 1 ? 1 : static_cast<int>(log10(static_cast<double>(area->getBufferLinesCount()) + 1)) + 1;

            reqCols = std::max(reqCols, tmpReqCols);
        }
    }

#if 0
    int newColsDiff = 0;
    if (reqCols != maxCols) {
        Dimension windowWidth;
        short fontWidth;

        int newColsDiff = reqCols - maxCols;

        fs = textD_of(textArea_)->getFont();
        fontWidth = fs->max_bounds.width;

        XtVaGetValues(shell_, XmNwidth, &windowWidth, nullptr);
        XtVaSetValues(shell_, XmNwidth, (Dimension)windowWidth + (newColsDiff * fontWidth), nullptr);
    }
#endif

    for(DocumentWidget *document : documents) {
        if(TextArea *area = document->firstPane()) {
            int lineNumCols = area->getLineNumCols();
            if (lineNumCols == reqCols) {
                continue;
            }

            //  Update all panes of this document.
            for(TextArea *pane : document->textPanes()) {
                pane->setLineNumCols(reqCols);
            }
        }
    }

    return reqCols;
}

/*
** Temporarily show and hide the incremental search line if the line is not
** already up.
*/
void MainWindow::TempShowISearch(bool state) {
    if (showISearchLine_) {
        return;
    }

    if(ui.incrementalSearchFrame->isVisible() != state) {
        ui.incrementalSearchFrame->setVisible(state);
    }
}

/*
** Find a name for an untitled file, unique in the name space of in the opened
** files in this session, i.e. Untitled or Untitled_nn, and write it into
** the string "name".
*/
QString MainWindow::UniqueUntitledNameEx() {

    for (int i = 0; i < INT_MAX; i++) {
        QString name;

        if (i == 0) {
            name = tr("Untitled");
        } else {
            name = tr("Untitled_%1").arg(i);
        }

        std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

        auto it = std::find_if(documents.begin(), documents.end(), [name](DocumentWidget *document) {
            return document->filename_ == name;
        });

        if(it == documents.end()) {
            return name;
        }
    }

    return QString();
}

void MainWindow::action_Undo(DocumentWidget *document) {

    emit_event("undo");

    if (document->CheckReadOnly()) {
        return;
    }

    document->Undo();
}

void MainWindow::on_action_Undo_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Undo(document);
    }
}

void MainWindow::action_Redo(DocumentWidget *document) {

    emit_event("redo");

    if (document->CheckReadOnly()) {
        return;
    }
    document->Redo();
}

void MainWindow::on_action_Redo_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Redo(document);
    }
}

/*
** Check if there is already a window open for a given file
*/
DocumentWidget *MainWindow::FindWindowWithFile(const QString &name, const QString &path) {

    const std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

    if (!GetPrefHonorSymlinks()) {

        QString fullname = tr("%1%2").arg(path, name);

        struct stat attribute;
        if (::stat(fullname.toLatin1().data(), &attribute) == 0) {

            auto it = std::find_if(documents.begin(), documents.end(), [attribute](DocumentWidget *document){
                return (attribute.st_dev == document->dev_) && (attribute.st_ino == document->ino_);
            });

            if(it != documents.end()) {
                return *it;
            }
        } /*  else:  Not an error condition, just a new file. Continue to check
              whether the filename is already in use for an unsaved document.  */

    }

    auto it = std::find_if(documents.begin(), documents.end(), [name, path](DocumentWidget *document) {
        return (document->filename_ == name) && (document->path_ == path);
    });

    if(it != documents.end()) {
        return *it;
    }

    return nullptr;
}

/*
** Force ShowLineNumbers() to re-evaluate line counts for the window if line
** counts are required.
*/
void MainWindow::forceShowLineNumbers() {
    const bool showLineNum = showLineNumbers_;
    if (showLineNum) {
        showLineNumbers_ = false;
        ShowLineNumbers(showLineNum);
    }
}

/*
** Turn on and off the display of line numbers
*/
void MainWindow::ShowLineNumbers(bool state) {

    if (showLineNumbers_ == state) {
        return;
    }

    showLineNumbers_ = state;

    /* Just setting showLineNumbers_ is sufficient to tell
       updateLineNumDisp() to expand the line number areas and the this
       size for the number of lines required.  To hide the line number
       display, set the width to zero, and contract the this width. */
    int reqCols = 0;
    if (state) {
        reqCols = updateLineNumDisp();
    }

    /* line numbers panel is shell-level, hence other
       tabbed documents in the this should synch */
    for(DocumentWidget *doc : openDocuments()) {

        showLineNumbers_ = state;

        for(TextArea *area : doc->textPanes()) {
            //  reqCols should really be cast here, but into what? XmRInt?
            area->setLineNumCols(reqCols);
        }
    }
}

/*
** Add a file to the list of previously opened files for display in the
** File menu.
*/
void MainWindow::AddToPrevOpenMenu(const QString &filename) {

    // If the Open Previous command is disabled, just return
    if (GetPrefMaxPrevOpenFiles() < 1) {
        return;
    }

    /*  Refresh list of previously opened files to avoid Big Race Condition,
        where two sessions overwrite each other's changes in NEdit's
        history file.
        Of course there is still Little Race Condition, which occurs if a
        Session A reads the list, then Session B reads the list and writes
        it before Session A gets a chance to write.  */
    ReadNEditDB();

    // If the name is already in the list, move it to the start
    int index = PrevOpen.indexOf(filename);
    if(index != -1) {
        PrevOpen.move(index, 0);
        invalidatePrevOpenMenus();
        WriteNEditDB();
        return;
    }

    // If the list is already full, make room
    if (PrevOpen.size() >= GetPrefMaxPrevOpenFiles()) {
        //  This is only safe if GetPrefMaxPrevOpenFiles() > 0.
        PrevOpen.pop_back();
    }

    PrevOpen.push_front(filename);

    // Mark the Previously Opened Files menu as invalid in all windows
    invalidatePrevOpenMenus();

    // Undim the menu in all windows if it was previously empty
    if (PrevOpen.size() > 0) {
        std::vector<MainWindow *> windows = allWindows();
        for(MainWindow *window : windows) {
            window->ui.action_Open_Previous->setEnabled(true);
        }
    }

    WriteNEditDB();
}

/*
**  Read database of file names for 'Open Previous' submenu.
**
**  Eventually, this may hold window positions, and possibly file marks (in
**  which case it should be moved to a different module) but for now it's
**  just a list of previously opened files.
**
**  This list is read once at startup and potentially refreshed before a
**  new entry is about to be written to the file or before the menu is
**  displayed. If the file is modified since the last read (or not read
**  before), it is read in, otherwise nothing is done.
*/
void MainWindow::ReadNEditDB() {

	static QDateTime lastNeditdbModTime;

    /*  If the Open Previous command is disabled or the user set the
        resource to an (invalid) negative value, just return.  */
    if (GetPrefMaxPrevOpenFiles() < 1) {
        return;
    }

    /* Don't move this check ahead of the previous statements. PrevOpen
       must be initialized at all times. */
    QString fullName = Settings::historyFile();
    if(fullName.isNull()) {
        return;
    }

	QFileInfo info(fullName);
	QDateTime mtime = info.lastModified();

	/*  Stat history file to see whether someone touched it after this
		session last changed it.  */
	if(lastNeditdbModTime >= mtime) {
		//  Do nothing, history file is unchanged.
		return;
	} else {
		//  Memorize modtime to compare to next time.
		lastNeditdbModTime = mtime;
	}

    // open the file
    QFile file(fullName);
    if(!file.open(QIODevice::ReadOnly)) {
		return;
	}

    //  Clear previous list.
    PrevOpen.clear();

    /* read lines of the file, lines beginning with # are considered to be
       comments and are thrown away.  Lines are subject to cursory checking,
       then just copied to the Open Previous file menu list */
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        if(line.isEmpty()) {
			// blank line
			continue;
		}

        if (line.startsWith(QLatin1Char('#'))) {
            // comment
            continue;
        }

		// NOTE(eteran): right now, neditDBBadFilenameChars only consists of a
		// newline character, I don't see how it is even possible for that to
		// happen as we are reading lines which are obviously delimited by
		// '\n', but the check remains as we could hypothetically ban other
		// characters in the future.
        if(line.contains(neditDBBadFilenameChars)) {
            qWarning("NEdit: History file may be corrupted");
            continue;
        }

        PrevOpen.push_back(line);

        if (PrevOpen.size() >= GetPrefMaxPrevOpenFiles()) {
            // too many entries
            return;
        }
    }
}

/*
** Mark the Previously Opened Files menus of all NEdit windows as invalid.
** Since actually changing the menus is slow, they're just marked and updated
** when the user pulls one down.
*/
void MainWindow::invalidatePrevOpenMenus() {

    /* Mark the menus invalid (to be updated when the user pulls one
       down), unless the menu is torn off, meaning it is visible to the user
       and should be updated immediately */
    for(MainWindow *window : allWindows()) {
        window->updatePrevOpenMenu();
    }
}

/*
** Write dynamic database of file names for "Open Previous".  Eventually,
** this may hold window positions, and possibly file marks, in which case,
** it should be moved to a different module, but for now it's just a list
** of previously opened files.
*/
void MainWindow::WriteNEditDB() {


    QString fullName = Settings::historyFile();
    if(fullName.isNull()) {
        return;
    }

    // If the Open Previous command is disabled, just return
    if (GetPrefMaxPrevOpenFiles() < 1) {
        return;
    }

    QFile file(fullName);
    if(file.open(QIODevice::WriteOnly)) {
        QTextStream ts(&file);

        static const QString fileHeader = tr("# File name database for NEdit Open Previous command");
        ts << fileHeader << tr("\n");

        // Write the list of file names
        Q_FOREACH(const QString &line, PrevOpen) {
            if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')) && !line.contains(neditDBBadFilenameChars)) {
                ts << line << tr("\n");
            }
        }
    }
}

/*
** Update the Previously Opened Files menu of a single window to reflect the
** current state of the list as retrieved from FIXME.
** Thanks to Markus Schwarzenberg for the sorting part.
*/
void MainWindow::updatePrevOpenMenu() {

    if (GetPrefMaxPrevOpenFiles() == 0) {
        ui.action_Open_Previous->setEnabled(false);
        return;
    }

    //  Read history file to get entries written by other sessions.
    ReadNEditDB();

    // Sort the previously opened file list if requested
    QVector<QString> prevOpenSorted = PrevOpen;
    if (GetPrefSortOpenPrevMenu()) {
        qSort(prevOpenSorted);
    }

    for(int i = 0; i < prevOpenSorted.size(); ++i) {
        QString filename = prevOpenSorted[i];
        previousOpenFilesList_[i]->setText(filename);
        previousOpenFilesList_[i]->setData(filename);
        previousOpenFilesList_[i]->setVisible(true);
    }

    for(int i = prevOpenSorted.size(); i < previousOpenFilesList_.size(); ++i) {
        previousOpenFilesList_[i]->setVisible(false);
    }
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if(auto document = documentAt(index)) {
        document->documentRaised();
    }
}

void MainWindow::on_tabWidget_customContextMenuRequested(const QPoint &pos) {
    const int index = ui.tabWidget->tabBar()->tabAt(pos);
    if(index != -1) {
        auto *menu = new QMenu(this);
        QAction *const newTab    = menu->addAction(tr("New Tab"));
        QAction *const closeTab  = menu->addAction(tr("Close Tab"));
        menu->addSeparator();
        QAction *const detachTab = menu->addAction(tr("Detach Tab"));
        QAction *const moveTab   = menu->addAction(tr("Move Tab To..."));

        // make sure that these are always in sync with the primary UI
        detachTab->setEnabled(ui.action_Detach_Tab->isEnabled());
        moveTab->setEnabled(ui.action_Move_Tab_To->isEnabled());

        // make the icons the same too :-P
        newTab->setIcon(ui.action_New->icon());
        closeTab->setIcon(ui.action_Close->icon());
        detachTab->setIcon(ui.action_Detach_Tab->icon());
        moveTab->setIcon(ui.action_Move_Tab_To->icon());

        if(QAction *const selected = menu->exec(ui.tabWidget->tabBar()->mapToGlobal(pos))) {

            if(DocumentWidget *document = documentAt(index)) {

                if(selected == newTab) {
                    MainWindow::EditNewFileEx(this, QString(), false, QString(), document->path_);
                } else if(selected == closeTab) {
                    document->actionClose(CloseMode::Prompt);
                } else if(selected == detachTab) {
                    if(TabCount() > 1) {
                        auto new_window = new MainWindow(nullptr);
                        new_window->ui.tabWidget->addTab(document, document->filename_);
                        new_window->show();
                    }
                } else if(selected == moveTab) {
                    document->moveDocument(this);
                }
            }
        }
    }
}

/**
 * @brief MainWindow::action_Open_Selected
 * @param document
 */
void MainWindow::action_Open_Selected(DocumentWidget *document) {

    emit_event("open_selected");

    // Get the selected text, if there's no selection, do nothing
    const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
    if(mimeData->hasText()) {
        openFile(document, mimeData->text());
    } else {
        QApplication::beep();
    }

    CheckCloseDimEx();
}

/**
 * @brief MainWindow::on_action_Open_Selected_triggered
 */
void MainWindow::on_action_Open_Selected_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Open_Selected(document);
    }
}

/**
 * @brief MainWindow::openFile
 * @param document
 * @param text
 */
void MainWindow::openFile(DocumentWidget *document, const QString &text) {

	// TODO(eteran): 2.0, let the user specify a list of potential paths!
    //       can we ask the system simply or similar?
    //       `gcc -print-prog-name=cc1plus` -v
    //       `gcc -print-prog-name=cc1` -v
    //       etc...
    static auto includeDir = QLatin1String("/usr/include/");

    /* get the string, or skip if we can't get the selection data, or it's
       obviously not a file name */
    if (text.size() > MAXPATHLEN || text.isEmpty()) {
        QApplication::beep();
        return;
    }

    static QRegularExpression reSystem(QLatin1String("#include\\s*<([^>]+)>"));
    static QRegularExpression reLocal(QLatin1String("#include\\s*\"([^\"]+)\""));

    QString nameText = text;

    // extract name from #include syntax
	// TODO(eteran): 2.0, support import/include syntax from multiple languages
    QRegularExpressionMatch match = reLocal.match(nameText);
    if(match.hasMatch()) {
        nameText = match.captured(1);
    } else {
        match = reSystem.match(nameText);
        if(match.hasMatch()) {
            nameText = QString(QLatin1String("%1%2")).arg(includeDir, match.captured(1));
        }
    }

    // strip whitespace from name
    nameText.remove(QLatin1Char(' '));
    nameText.remove(QLatin1Char('\t'));
    nameText.remove(QLatin1Char('\n'));

    // Process ~ characters in name
    nameText = ExpandTildeEx(nameText);

    // If path name is relative, make it refer to current window's directory
    if (!QFileInfo(nameText).isAbsolute()) {
        nameText = QString(QLatin1String("%1%2")).arg(document->path_, nameText);
    }

#if !defined(DONT_HAVE_GLOB)
    // Expand wildcards in file name.
    {
        glob_t globbuf;
        glob(nameText.toLatin1().data(), GLOB_NOCHECK, nullptr, &globbuf);

        for (size_t i = 0; i < globbuf.gl_pathc; i++) {
            QString pathname;
            QString filename;
            if (ParseFilenameEx(QString::fromLatin1(globbuf.gl_pathv[i]), &filename, &pathname) != 0) {
                QApplication::beep();
            } else {
                DocumentWidget::EditExistingFileEx(
                            GetPrefOpenInTab() ? document : nullptr,
                            filename,
                            pathname,
                            0,
                            QString(),
                            false,
                            QString(),
                            GetPrefOpenInTab(),
                            false);
            }
        }
        globfree(&globbuf);
    }
#else
	QString pathname;
	QString filename;
	if (ParseFilenameEx(nameText, &filename, &pathname) != 0) {
		QApplication::beep();
	} else {
		DocumentWidget::EditExistingFileEx(
                    GetPrefOpenInTab() ? document : nullptr,
		            filename,
		            pathname,
		            0,
		            QString(),
		            false,
		            QString(),
		            GetPrefOpenInTab(),
		            false);
	}
#endif

    CheckCloseDimEx();
}

/**
 * @brief MainWindow::action_Shift_Left
 * @param document
 */
void MainWindow::action_Shift_Left(DocumentWidget *document) {

    emit_event("shift_left");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ShiftSelectionEx(document, area, SHIFT_LEFT, false);
    }
}

/**
 * @brief MainWindow::on_action_Shift_Left_triggered
 */
void MainWindow::on_action_Shift_Left_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Left(document);
    }
}

void MainWindow::action_Shift_Right(DocumentWidget *document) {

    emit_event("shift_right");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ShiftSelectionEx(document, area, SHIFT_RIGHT, false);
    }
}

/**
 * @brief MainWindow::on_action_Shift_Right_triggered
 */
void MainWindow::on_action_Shift_Right_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Right(document);
    }
}

/**
 * @brief MainWindow::action_Shift_Left_Tabs
 * @param document
 */
void MainWindow::action_Shift_Left_Tabs(DocumentWidget *document) {

    emit_event("shift_left_by_tab");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ShiftSelectionEx(document, area, SHIFT_LEFT, true);
    }
}

void MainWindow::action_Shift_Left_Tabs() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Left_Tabs(document);
    }
}

/**
 * @brief MainWindow::action_Shift_Right_Tabs
 * @param document
 */
void MainWindow::action_Shift_Right_Tabs(DocumentWidget *document) {

    emit_event("shift_right_by_tab");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ShiftSelectionEx(document, area, SHIFT_RIGHT, true);
    }
}

/**
 * @brief MainWindow::action_Shift_Right_Tabs
 */
void MainWindow::action_Shift_Right_Tabs() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Right_Tabs(document);
    }
}

/**
 * @brief MainWindow::action_Lower_case
 * @param document
 */
void MainWindow::action_Lower_case(DocumentWidget *document) {

    emit_event("lowercase");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        DowncaseSelectionEx(document, area);
    }
}

/**
 * @brief MainWindow::on_action_Lower_case_triggered
 */
void MainWindow::on_action_Lower_case_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Lower_case(document);
    }
}

/**
 * @brief MainWindow::action_Upper_case
 * @param document
 */
void MainWindow::action_Upper_case(DocumentWidget *document) {

    emit_event("uppercase");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        UpcaseSelectionEx(document, area);
    }
}

/**
 * @brief MainWindow::on_action_Upper_case_triggered
 */
void MainWindow::on_action_Upper_case_triggered() {    

    if(DocumentWidget *document = currentDocument()) {
        action_Upper_case(document);
    }
}

void MainWindow::action_Fill_Paragraph(DocumentWidget *document) {

    emit_event("fill_paragraph");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        FillSelectionEx(document, area);
    }
}

/**
 * @brief MainWindow::on_action_Fill_Paragraph_triggered
 */
void MainWindow::on_action_Fill_Paragraph_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Fill_Paragraph(document);
    }
}

/**
 * @brief MainWindow::on_action_Insert_Form_Feed_triggered
 */
void MainWindow::on_action_Insert_Form_Feed_triggered() {

    if(QPointer<TextArea> area = lastFocus_) {
        area->insertStringAP(QLatin1String("\f"));
    }
}

/**
 * @brief MainWindow::action_Insert_Ctrl_Code
 * @param document
 * @param str
 */
void MainWindow::action_Insert_Ctrl_Code(DocumentWidget *document, const QString &str) {

    emit_event("insert_string", str);

    if (document->CheckReadOnly()) {
        return;
    }

    auto s = str.toStdString();

    if(QPointer<TextArea> area = lastFocus_) {
        area->insertStringAP(QString::fromStdString(s));
    }
}

/**
 * @brief MainWindow::action_Insert_Ctrl_Code
 * @param document
 */
void MainWindow::action_Insert_Ctrl_Code(DocumentWidget *document) {
    if (document->CheckReadOnly()) {
        return;
    }

    bool ok;
    int n = QInputDialog::getInt(this, tr("Insert Ctrl Code"), tr("ASCII Character Code:"), 0, 0, 255, 1, &ok);
    if(ok) {
        QString str(QChar::fromLatin1(static_cast<char>(n)));
        action_Insert_Ctrl_Code(document, str);
    }
}

/**
 * @brief MainWindow::on_action_Insert_Ctrl_Code_triggered
 */
void MainWindow::on_action_Insert_Ctrl_Code_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Insert_Ctrl_Code(document);
    }
}

/**
 * @brief MainWindow::action_Goto_Line_Number
 * @param s
 */
void MainWindow::action_Goto_Line_Number(DocumentWidget *document, const QString &s) {

    emit_event("goto_line_number", s);

    int lineNum;
    int column;

    /* Accept various formats:
          [line]:[column]   (menu action)
          line              (macro call)
          line, column      (macro call) */
    if (StringToLineAndCol(s, &lineNum, &column) == -1) {
        QApplication::beep();
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        document->gotoAP(area, lineNum, column);
    }
}

/**
 * @brief MainWindow::action_Goto_Line_Number
 * @param document
 */
void MainWindow::action_Goto_Line_Number(DocumentWidget *document) {
    bool ok;
    QString text = QInputDialog::getText(
                       this,
                       tr("Goto Line Number"),
                       tr("Goto Line (and/or Column)  Number:"),
                       QLineEdit::Normal,
                       QString(),
                       &ok);

    if (!ok) {
        return;
    }

    action_Goto_Line_Number(document, text);
}

/**
 * @brief MainWindow::on_action_Goto_Line_Number_triggered
 */
void MainWindow::on_action_Goto_Line_Number_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Goto_Line_Number(document);
    }

}

/**
 * @brief MainWindow::action_Goto_Selected
 * @param document
 */
void MainWindow::action_Goto_Selected(DocumentWidget *document) {

    emit_event("goto_selected");

    const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
    if(!mimeData->hasText()) {
        QApplication::beep();
        return;
    }

    action_Goto_Line_Number(document, mimeData->text());
}

/**
 * @brief MainWindow::on_action_Goto_Selected_triggered
 */
void MainWindow::on_action_Goto_Selected_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Goto_Selected(document);
    }
}

/**
 * @brief MainWindow::action_Find
 * @param string
 * @param direction
 * @param keepDialogs
 * @param type
 */
void MainWindow::action_Find_Dialog(DocumentWidget *document, Direction direction, SearchType type, bool keepDialog) {
    DoFindDlogEx(
        document,
        direction,
        keepDialog,
        type);
}

void MainWindow::action_Shift_Find(DocumentWidget *document) {
    action_Find_Dialog(
        document,
        Direction::Backward,
        GetPrefSearch(),
        GetPrefKeepSearchDlogs());
}

/**
 * @brief MainWindow::action_Shift_Find
 * @param document
 */
void MainWindow::action_Shift_Find() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Find(document);
    }
}

/**
 * @brief MainWindow::action_FindAgain
 * @param direction
 * @param wrap
 */
void MainWindow::action_Find_Again(DocumentWidget *document, Direction direction, WrapMode wrap) {

    emit_event("find_again", to_string(direction), to_string(wrap));

    if(QPointer<TextArea> area = lastFocus_) {
        SearchAndSelectSameEx(
            document,
            area,
            direction,
            wrap);
    }
}

void MainWindow::action_Shift_Find_Again(DocumentWidget *document) {
    action_Find_Again(
        document,
        Direction::Backward,
        GetPrefSearchWraps());
}

/**
 * @brief MainWindow::action_Shift_Find_Again
 * @param document
 */
void MainWindow::action_Shift_Find_Again() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Find_Again(document);
    }
}

/**
 * @brief MainWindow::action_Find_Selection
 */
void MainWindow::action_Find_Selection(DocumentWidget *document, Direction direction, SearchType type, WrapMode wrap) {

    emit_event("find_selection", to_string(direction), to_string(type), to_string(wrap));

    if(QPointer<TextArea> area = lastFocus_) {
        SearchForSelectedEx(
            document,
            area,
            direction,
            type,
            wrap);
    }
}

/**
 * @brief MainWindow::action_Shift_Find_Selection
 * @param document
 */
void MainWindow::action_Shift_Find_Selection(DocumentWidget *document) {
    action_Find_Selection(
        document,
        Direction::Backward,
        GetPrefSearch(),
        GetPrefSearchWraps());
}

/**
 * @brief MainWindow::action_Shift_Find_Selection
 */
void MainWindow::action_Shift_Find_Selection() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Find_Selection(document);
    }
}

/*
** Called when user types in the incremental search line.  Redoes the
** search for the new search string.
*/
void MainWindow::on_editIFind_textChanged(const QString &text) {

    SearchType searchType;

    if(ui.checkIFindCase->isChecked()) {
        if (ui.checkIFindRegex->isChecked()) {
            searchType = SearchType::Regex;
        } else {
            searchType = SearchType::CaseSense;
        }
    } else {
        if (ui.checkIFindRegex->isChecked()) {
            searchType = SearchType::RegexNoCase;
        } else {
            searchType = SearchType::Literal;
        }
    }

    const Direction direction = ui.checkIFindReverse->isChecked() ?
                Direction::Backward :
                Direction::Forward;

    /* If the search type is a regular expression, test compile it.  If it
       fails, silently skip it.  (This allows users to compose the expression
       in peace when they have unfinished syntax, but still get beeps when
       correct syntax doesn't match) */
    if (isRegexType(searchType)) {
        try {
            auto compiledRE = make_regex(text, defaultRegexFlags(searchType));
        } catch(const regex_error &) {
            return;
        }
    }

    /* Call the incremental search action proc to do the searching and
       selecting (this allows it to be recorded for learn/replay).  If
       there's an incremental search already in progress, mark the operation
       as "continued" so the search routine knows to re-start the search
       from the original starting position */
    if(DocumentWidget *document = currentDocument()) {
        action_Find_Incremental(document, text, direction, searchType, GetPrefSearchWraps(), iSearchStartPos_ != -1);
    }
}

/**
 * @brief MainWindow::on_action_Find_Incremental_triggered
 */
void MainWindow::on_action_Find_Incremental_triggered() {
    BeginISearchEx(Direction::Forward);
}

/**
 * @brief MainWindow::action_Shift_Find_Incremental
 * @param document
 */
void MainWindow::action_Shift_Find_Incremental(DocumentWidget *document) {
    Q_UNUSED(document);

    BeginISearchEx(Direction::Backward);
}

/**
 * @brief MainWindow::action_Shift_Find_Incremental
 */
void MainWindow::action_Shift_Find_Incremental() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Find_Incremental(document);
    }
}

/**
 * @brief MainWindow::action_Find_Incremental
 * @param document
 * @param searchString
 * @param direction
 * @param searchType
 * @param searchWraps
 * @param isContinue
 */
void MainWindow::action_Find_Incremental(DocumentWidget *document, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWraps, bool isContinue) {

    if(QPointer<TextArea> area = lastFocus_) {
        SearchAndSelectIncrementalEx(
                document,
                area,
                searchString,
                direction,
                searchType,
                searchWraps,
                isContinue);
    }
}

/**
 * @brief MainWindow::on_buttonIFind_clicked
 */
void MainWindow::on_buttonIFind_clicked() {
    // same as pressing return
    on_editIFind_returnPressed();
}

/**
 * User pressed return in the incremental search bar. Do a new search with the
 * search string displayed.  The direction of the search is toggled if the Ctrl
 * key or the Shift key is pressed when the text field is activated.
 *
 * @brief MainWindow::on_editIFind_returnPressed
 */
void MainWindow::on_editIFind_returnPressed() {

    SearchType searchType;

    /* Fetch the string, search type and direction from the incremental
       search bar widgets at the top of the window */
    QString searchString = ui.editIFind->text();

    if (ui.checkIFindCase->isChecked()) {
        if (ui.checkIFindRegex->isChecked()) {
            searchType = SearchType::Regex;
        } else {
            searchType = SearchType::CaseSense;
        }
    } else {
        if (ui.checkIFindRegex->isChecked()) {
            searchType = SearchType::RegexNoCase;
        } else {
            searchType = SearchType::Literal;
        }
    }

    Direction direction = ui.checkIFindReverse->isChecked() ? Direction::Backward : Direction::Forward;

    // Reverse the search direction if the Ctrl or Shift key was pressed
    if(QApplication::keyboardModifiers() & (Qt::CTRL | Qt::SHIFT)) {
        direction = (direction == Direction::Forward) ? Direction::Backward : Direction::Forward;
    }

    // find the text and mark it
    if(DocumentWidget *document = currentDocument()) {
        action_Find(document, searchString, direction, searchType, GetPrefSearchWraps());
    }
}

/**
 * @brief MainWindow::keyPressEvent
 * @param event
 */
void MainWindow::keyPressEvent(QKeyEvent *event) {

	int index;

	// only process up and down arrow keys
    if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down && event->key() != Qt::Key_Escape) {
        QMainWindow::keyPressEvent(event);
        return;
    }

	index = iSearchHistIndex_;

    // allow escape key to cancel search
    if (event->key() == Qt::Key_Escape) {
        EndISearchEx();
        return;
    }

    // increment or decrement the index depending on which arrow was pressed
    index += (event->key() == Qt::Key_Up) ? 1 : -1;

    // if the index is out of range, beep and return
    if (index != 0 && historyIndex(index) == -1) {
        QApplication::beep();
        return;
    }

    QString searchStr;
    SearchType searchType;

    // determine the strings and button settings to use
    if (index == 0) {
		searchStr  = QLatin1String("");
        searchType = GetPrefSearch();
    } else {
        searchStr  = SearchReplaceHistory[historyIndex(index)].search;
        searchType = SearchReplaceHistory[historyIndex(index)].type;
    }

    /* Set the info used in the value changed callback before calling XmTextSetStringEx(). */
    iSearchHistIndex_ = index;
    initToggleButtonsiSearch(searchType);

    // Beware the value changed callback is processed as part of this call
    int previousPosition = ui.editIFind->cursorPosition();
    ui.editIFind->setText(searchStr);
    ui.editIFind->setCursorPosition(previousPosition);
}

/*
** The next few callbacks handle the states of find/replace toggle
** buttons, which depend on the state of the "Regex" button, and the
** sensitivity of the Whole Word buttons.
** Callbacks are necessary for both "Regex" and "Case Sensitive"
** buttons to make sure the states are saved even after a cancel operation.
**
** If sticky case sensitivity is requested, the behaviour is as follows:
**   The first time "Regular expression" is checked, "Match case" gets
**   checked too. Thereafter, checking or unchecking "Regular expression"
**   restores the "Match case" button to the setting it had the last
**   time when literals or REs where used.
** Without sticky behaviour, the state of the Regex button doesn't influence
** the state of the Case Sensitive button.
**
** Independently, the state of the buttons is always restored to the
** default state when a dialog is popped up, and when the user returns
** from stepping through the search history.
*/
void MainWindow::on_checkIFindCase_toggled(bool searchCaseSense) {

    /* Save the state of the Case Sensitive button
       depending on the state of the Regex button*/
    if (ui.checkIFindRegex->isChecked()) {
        iSearchLastRegexCase_ = searchCaseSense;
    } else {
        iSearchLastLiteralCase_ = searchCaseSense;
    }

    // When search parameters (direction or search type), redo the search
    on_editIFind_textChanged(ui.editIFind->text());
}

void MainWindow::on_checkIFindRegex_toggled(bool searchRegex) {

    bool searchCaseSense = ui.checkIFindCase->isChecked();

    // In sticky mode, restore the state of the Case Sensitive button
    if (GetPrefStickyCaseSenseBtn()) {
        if (searchRegex) {
            iSearchLastLiteralCase_ = searchCaseSense;
            no_signals(ui.checkIFindCase)->setChecked(iSearchLastRegexCase_);
        } else {
            iSearchLastRegexCase_ = searchCaseSense;
            no_signals(ui.checkIFindCase)->setChecked(iSearchLastLiteralCase_);
        }
    }
    // The iSearch bar has no Whole Word button to enable/disable.

    // When search parameters (direction or search type), redo the search
    on_editIFind_textChanged(ui.editIFind->text());
}

void MainWindow::on_checkIFindReverse_toggled(bool value) {

	Q_UNUSED(value);

    // When search parameters (direction or search type), redo the search
    on_editIFind_textChanged(ui.editIFind->text());
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
void MainWindow::initToggleButtonsiSearch(SearchType searchType) {
    /* Set the initial search type and remember the corresponding case
       sensitivity states in case sticky case sensitivity is required. */

    switch (searchType) {
    case SearchType::Literal:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(false);
        break;
    case SearchType::CaseSense:
        iSearchLastLiteralCase_ = true;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(true);
        break;
    case SearchType::LiteralWord:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(false);
        break;
    case SearchType::CaseSenseWord:
        iSearchLastLiteralCase_ = true;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(true);
        break;
    case SearchType::Regex:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(true);
        ui.checkIFindCase->setChecked(true);
        break;
    case SearchType::RegexNoCase:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = false;
        ui.checkIFindRegex->setChecked(true);
        ui.checkIFindCase->setChecked(false);
        break;
    }
}

/*
** Pop up and clear the incremental search line and prepare to search.
*/
void MainWindow::BeginISearchEx(Direction direction) {

    iSearchStartPos_ = -1;
	ui.editIFind->setText(QLatin1String(""));
    no_signals(ui.checkIFindReverse)->setChecked(direction == Direction::Backward);


    /* Note: in contrast to the replace and find dialogs, the regex and
       case toggles are not reset to their default state when the incremental
       search bar is redisplayed. I'm not sure whether this is the best
       choice. If not, an initToggleButtons() call should be inserted
       here. But in that case, it might be appropriate to have different
       default search modes for i-search and replace/find. */

    TempShowISearch(true);
    ui.editIFind->setFocus();
}

/*
** Incremental searching is anchored at the position where the cursor
** was when the user began typing the search string.  Call this routine
** to forget about this original anchor, and if the search bar is not
** permanently up, pop it down.
*/
void MainWindow::EndISearchEx() {

    /* Note: Please maintain this such that it can be freely peppered in
       mainline code, without callers having to worry about performance
       or visual glitches.  */

    // Forget the starting position used for the current run of searches
    iSearchStartPos_ = -1;

    // Mark the end of incremental search history overwriting
    saveSearchHistory(QLatin1String(""), QString(), SearchType::Literal, false);

    // Pop down the search line (if it's not pegged up in Preferences)
    TempShowISearch(false);
}

/**
 * @brief MainWindow::action_Shift_Replace
 * @param document
 */
void MainWindow::action_Shift_Replace(DocumentWidget *document) {
    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        DoFindReplaceDlogEx(
                    document,
                    area,
                    Direction::Backward,
                    GetPrefKeepSearchDlogs(),
                    GetPrefSearch());
    }
}

/**
 * @brief MainWindow::action_Shift_Replace
 */
void MainWindow::action_Shift_Replace() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Replace(document);
    }
}

/**
 * @brief MainWindow::action_Replace_Find_Again
 * @param document
 * @param direction
 */
void MainWindow::action_Replace_Find_Again(DocumentWidget *document, Direction direction, WrapMode wrap) {
    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ReplaceFindSameEx(
                    document,
                    area,
                    direction,
                    wrap);
    }
}

/**
 * @brief MainWindow::on_action_Replace_Find_Again_triggered
 */
void MainWindow::on_action_Replace_Find_Again_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Replace_Find_Again(document, Direction::Forward, GetPrefSearchWraps());
    }
}

/**
 * @brief MainWindow::action_Shift_Replace_Find_Again
 * @param document
 */
void MainWindow::action_Shift_Replace_Find_Again(DocumentWidget *document) {
    action_Replace_Find_Again(document, Direction::Backward, GetPrefSearchWraps());
}

void MainWindow::action_Replace_Again(DocumentWidget *document, Direction direction, WrapMode wrap) {

    emit_event("replace_again", to_string(direction), to_string(wrap));

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ReplaceSameEx(
                    document,
                    area,
                    direction,
                    wrap);
    }
}

/**
 * @brief MainWindow::on_action_Replace_Again_triggered
 */
void MainWindow::on_action_Replace_Again_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Replace_Again(
                    document,
                    Direction::Forward,
                    GetPrefSearchWraps());
    }
}

/**
 * @brief MainWindow::action_Shift_Replace_Again_triggered
 */
void MainWindow::action_Shift_Replace_Again() {

    if(DocumentWidget *document = currentDocument()) {
        action_Replace_Again(
                    document,
                    Direction::Backward,
                    GetPrefSearchWraps());
    }
}

/**
 * @brief MainWindow::action_Mark
 * @param mark
 */
void MainWindow::action_Mark(DocumentWidget *document, const QString &mark) {

    if (mark.size() != 1 || !mark[0].isLetter()) {
        qWarning("NEdit: action requires a single-letter label");
        QApplication::beep();
        return;
    }

    emit_event("mark", mark);

    QChar ch = mark[0];
    if(QPointer<TextArea> area = lastFocus_) {
        document->AddMarkEx(area, ch);
    }
}

void MainWindow::action_Mark(DocumentWidget *document) {
    bool ok;
    QString result = QInputDialog::getText(
        this,
        tr("Mark"),
        tr("Enter a single letter label to use for recalling\n"
           "the current selection and cursor position.\n\n"
           "(To skip this dialog, use the accelerator key,\n"
           "followed immediately by a letter key (a-z))"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if(!ok) {
        return;
    }

    action_Mark(document, result);
}

/**
 * @brief MainWindow::on_action_Mark_triggered
 */
void MainWindow::on_action_Mark_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Mark(document);
    }
}

/**
 * @brief MainWindow::action_Mark_Shortcut_triggered
 */
void MainWindow::action_Mark_Shortcut() {

    if(auto shortcut = qobject_cast<QShortcut *>(sender())) {
        QKeySequence sequence = shortcut->key();

        DocumentWidget *document = currentDocument();

        switch(sequence[1]) {
        case Qt::Key_A: action_Mark(document, QLatin1String("A")); break;
        case Qt::Key_B: action_Mark(document, QLatin1String("B")); break;
        case Qt::Key_C: action_Mark(document, QLatin1String("C")); break;
        case Qt::Key_D: action_Mark(document, QLatin1String("D")); break;
        case Qt::Key_E: action_Mark(document, QLatin1String("E")); break;
        case Qt::Key_F: action_Mark(document, QLatin1String("F")); break;
        case Qt::Key_G: action_Mark(document, QLatin1String("G")); break;
        case Qt::Key_H: action_Mark(document, QLatin1String("H")); break;
        case Qt::Key_I: action_Mark(document, QLatin1String("I")); break;
        case Qt::Key_J: action_Mark(document, QLatin1String("J")); break;
        case Qt::Key_K: action_Mark(document, QLatin1String("K")); break;
        case Qt::Key_L: action_Mark(document, QLatin1String("L")); break;
        case Qt::Key_M: action_Mark(document, QLatin1String("M")); break;
        case Qt::Key_N: action_Mark(document, QLatin1String("N")); break;
        case Qt::Key_O: action_Mark(document, QLatin1String("O")); break;
        case Qt::Key_P: action_Mark(document, QLatin1String("P")); break;
        case Qt::Key_Q: action_Mark(document, QLatin1String("Q")); break;
        case Qt::Key_R: action_Mark(document, QLatin1String("R")); break;
        case Qt::Key_S: action_Mark(document, QLatin1String("S")); break;
        case Qt::Key_T: action_Mark(document, QLatin1String("T")); break;
        case Qt::Key_U: action_Mark(document, QLatin1String("U")); break;
        case Qt::Key_V: action_Mark(document, QLatin1String("V")); break;
        case Qt::Key_W: action_Mark(document, QLatin1String("W")); break;
        case Qt::Key_X: action_Mark(document, QLatin1String("X")); break;
        case Qt::Key_Y: action_Mark(document, QLatin1String("Y")); break;
        case Qt::Key_Z: action_Mark(document, QLatin1String("Z")); break;
        default:
            QApplication::beep();
            break;
        }
    }
}


void MainWindow::action_Goto_Mark(DocumentWidget *document, const QString &mark, bool extend) {

    emit_event("goto_mark", mark);

    if (mark.size() != 1 || !mark[0].isLetter()) {
        qWarning("NEdit: action requires a single-letter label");
        QApplication::beep();
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        document->gotoMark(area, mark[0], extend);
    }
}

/**
 * @brief MainWindow::action_Goto_Mark_Dialog
 * @param extend
 */
void MainWindow::action_Goto_Mark_Dialog(DocumentWidget *document, bool extend) {
    bool ok;
    QString result = QInputDialog::getText(
        this,
        tr("Goto Mark"),
        tr("Enter the single letter label used to mark\n"
                      "the selection and/or cursor position.\n\n"
                      "(To skip this dialog, use the accelerator\n"
                      "key, followed immediately by the letter)"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if(!ok) {
        return;
    }

    action_Goto_Mark(document, result, extend);
}

/**
 * @brief MainWindow::on_action_Goto_Mark_triggered
 */
void MainWindow::on_action_Goto_Mark_triggered() {

    bool extend = (QApplication::keyboardModifiers() & Qt::SHIFT);

    if(DocumentWidget *document = currentDocument()) {
        action_Goto_Mark_Dialog(document, extend);
    }
}


void MainWindow::action_Shift_Goto_Mark_Shortcut() {

    if(auto shortcut = qobject_cast<QShortcut *>(sender())) {
        QKeySequence sequence = shortcut->key();

        DocumentWidget *document = currentDocument();

        switch(sequence[1]) {
        case Qt::Key_A: action_Goto_Mark(document, QLatin1String("A"), true); break;
        case Qt::Key_B: action_Goto_Mark(document, QLatin1String("B"), true); break;
        case Qt::Key_C: action_Goto_Mark(document, QLatin1String("C"), true); break;
        case Qt::Key_D: action_Goto_Mark(document, QLatin1String("D"), true); break;
        case Qt::Key_E: action_Goto_Mark(document, QLatin1String("E"), true); break;
        case Qt::Key_F: action_Goto_Mark(document, QLatin1String("F"), true); break;
        case Qt::Key_G: action_Goto_Mark(document, QLatin1String("G"), true); break;
        case Qt::Key_H: action_Goto_Mark(document, QLatin1String("H"), true); break;
        case Qt::Key_I: action_Goto_Mark(document, QLatin1String("I"), true); break;
        case Qt::Key_J: action_Goto_Mark(document, QLatin1String("J"), true); break;
        case Qt::Key_K: action_Goto_Mark(document, QLatin1String("K"), true); break;
        case Qt::Key_L: action_Goto_Mark(document, QLatin1String("L"), true); break;
        case Qt::Key_M: action_Goto_Mark(document, QLatin1String("M"), true); break;
        case Qt::Key_N: action_Goto_Mark(document, QLatin1String("N"), true); break;
        case Qt::Key_O: action_Goto_Mark(document, QLatin1String("O"), true); break;
        case Qt::Key_P: action_Goto_Mark(document, QLatin1String("P"), true); break;
        case Qt::Key_Q: action_Goto_Mark(document, QLatin1String("Q"), true); break;
        case Qt::Key_R: action_Goto_Mark(document, QLatin1String("R"), true); break;
        case Qt::Key_S: action_Goto_Mark(document, QLatin1String("S"), true); break;
        case Qt::Key_T: action_Goto_Mark(document, QLatin1String("T"), true); break;
        case Qt::Key_U: action_Goto_Mark(document, QLatin1String("U"), true); break;
        case Qt::Key_V: action_Goto_Mark(document, QLatin1String("V"), true); break;
        case Qt::Key_W: action_Goto_Mark(document, QLatin1String("W"), true); break;
        case Qt::Key_X: action_Goto_Mark(document, QLatin1String("X"), true); break;
        case Qt::Key_Y: action_Goto_Mark(document, QLatin1String("Y"), true); break;
        case Qt::Key_Z: action_Goto_Mark(document, QLatin1String("Z"), true); break;
        default:
            QApplication::beep();
            break;
        }
    }
}

void MainWindow::action_Goto_Mark_Shortcut() {
    if(auto shortcut = qobject_cast<QShortcut *>(sender())) {
        QKeySequence sequence = shortcut->key();

        DocumentWidget *document = currentDocument();

        switch(sequence[1]) {
        case Qt::Key_A: action_Goto_Mark(document, QLatin1String("A"), false); break;
        case Qt::Key_B: action_Goto_Mark(document, QLatin1String("B"), false); break;
        case Qt::Key_C: action_Goto_Mark(document, QLatin1String("C"), false); break;
        case Qt::Key_D: action_Goto_Mark(document, QLatin1String("D"), false); break;
        case Qt::Key_E: action_Goto_Mark(document, QLatin1String("E"), false); break;
        case Qt::Key_F: action_Goto_Mark(document, QLatin1String("F"), false); break;
        case Qt::Key_G: action_Goto_Mark(document, QLatin1String("G"), false); break;
        case Qt::Key_H: action_Goto_Mark(document, QLatin1String("H"), false); break;
        case Qt::Key_I: action_Goto_Mark(document, QLatin1String("I"), false); break;
        case Qt::Key_J: action_Goto_Mark(document, QLatin1String("J"), false); break;
        case Qt::Key_K: action_Goto_Mark(document, QLatin1String("K"), false); break;
        case Qt::Key_L: action_Goto_Mark(document, QLatin1String("L"), false); break;
        case Qt::Key_M: action_Goto_Mark(document, QLatin1String("M"), false); break;
        case Qt::Key_N: action_Goto_Mark(document, QLatin1String("N"), false); break;
        case Qt::Key_O: action_Goto_Mark(document, QLatin1String("O"), false); break;
        case Qt::Key_P: action_Goto_Mark(document, QLatin1String("P"), false); break;
        case Qt::Key_Q: action_Goto_Mark(document, QLatin1String("Q"), false); break;
        case Qt::Key_R: action_Goto_Mark(document, QLatin1String("R"), false); break;
        case Qt::Key_S: action_Goto_Mark(document, QLatin1String("S"), false); break;
        case Qt::Key_T: action_Goto_Mark(document, QLatin1String("T"), false); break;
        case Qt::Key_U: action_Goto_Mark(document, QLatin1String("U"), false); break;
        case Qt::Key_V: action_Goto_Mark(document, QLatin1String("V"), false); break;
        case Qt::Key_W: action_Goto_Mark(document, QLatin1String("W"), false); break;
        case Qt::Key_X: action_Goto_Mark(document, QLatin1String("X"), false); break;
        case Qt::Key_Y: action_Goto_Mark(document, QLatin1String("Y"), false); break;
        case Qt::Key_Z: action_Goto_Mark(document, QLatin1String("Z"), false); break;
        default:
            QApplication::beep();
            break;
        }
    }
}

void MainWindow::action_Goto_Matching(DocumentWidget *document) {

    emit_event("goto_matching");
    if(QPointer<TextArea> area = lastFocus_) {
        document->GotoMatchingCharacter(area);
    }
}

void MainWindow::on_action_Goto_Matching_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Goto_Matching(document);
    }
}

void MainWindow::action_Shift_Goto_Matching(DocumentWidget *document) {

    emit_event("select_to_matching");
    if(QPointer<TextArea> area = lastFocus_) {
        document->SelectToMatchingCharacter(area);
    }
}

void MainWindow::action_Shift_Goto_Matching() {
    if(DocumentWidget *document = currentDocument()) {
        action_Shift_Goto_Matching(document);
    }
}

void MainWindow::updateTipsFileMenuEx() {

    auto tipsMenu = new QMenu(this);

    for(TagFile &tf : TipsFileList) {
        auto filename = tf.filename;
        QAction *action = tipsMenu->addAction(filename);
        action->setData(filename);
    }

    ui.action_Unload_Calltips_File->setMenu(tipsMenu);

    connect(tipsMenu, &QMenu::triggered, this, [this](QAction *action) {
        auto filename = action->data().toString();
        if(!filename.isEmpty()) {
            if(DocumentWidget *document = currentDocument()) {
                action_Unload_Tips_File(document, filename);
            }
        }
    });
}

void MainWindow::updateTagsFileMenuEx() {
    auto tagsMenu = new QMenu(this);

    for(TagFile &tf : TagsFileList) {
        auto filename = tf.filename;
        QAction *action = tagsMenu->addAction(filename);
        action->setData(filename);
    }

    ui.action_Unload_Tags_File->setMenu(tagsMenu);
    connect(tagsMenu, &QMenu::triggered, this, [this](QAction *action) {
        auto filename = action->data().toString();
        if(!filename.isEmpty()) {
            if(DocumentWidget *document = currentDocument()) {
                action_Unload_Tags_File(document, filename);
            }
        }
    });
}


void MainWindow::action_Unload_Tips_File(DocumentWidget *document, const QString &filename) {

    Q_UNUSED(document);
    emit_event("unload_tips_file", filename);

    if (DeleteTagsFileEx(filename, TagSearchMode::TIP, true)) {
        for(MainWindow *window : MainWindow::allWindows()) {
            window->updateTipsFileMenuEx();
        }
    }
}

void MainWindow::action_Unload_Tags_File(DocumentWidget *document, const QString &filename) {

    Q_UNUSED(document);
    emit_event("unload_tags_file", filename);

    if (DeleteTagsFileEx(filename, TagSearchMode::TAG, true)) {
        for(MainWindow *window : MainWindow::allWindows()) {
             window->updateTagsFileMenuEx();
        }
    }
}


void MainWindow::action_Load_Tips_File(DocumentWidget *document, const QString &filename) {

    Q_UNUSED(document);
    emit_event("load_tips_file", filename);

    if (!AddTagsFileEx(filename, TagSearchMode::TIP)) {
        QMessageBox::warning(this, tr("Error Reading File"), tr("Error reading tips file:\n'%1'\ntips not loaded").arg(filename));
    }
}

void MainWindow::action_Load_Calltips_File(DocumentWidget *document) {
    QString filename = PromptForExistingFileEx(document->path_, tr("Load Calltips File"));
    if (filename.isNull()) {
        return;
    }

    action_Load_Tips_File(document, filename);
    updateTipsFileMenuEx();
}

void MainWindow::on_action_Load_Calltips_File_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Load_Calltips_File(document);
    }
}

void MainWindow::action_Load_Tags_File(DocumentWidget *document, const QString &filename) {

    emit_event("load_tags_file", filename);

    if (!AddTagsFileEx(filename, TagSearchMode::TAG)) {
        QMessageBox::warning(
                    document,
                    tr("Error Reading File"),
                    tr("Error reading ctags file:\n'%1'\ntags not loaded").arg(filename));
    }
}

void MainWindow::action_Load_Tags_File(DocumentWidget *document) {
    QString filename = PromptForExistingFileEx(document->path_, tr("Load Tags File"));
    if (filename.isNull()) {
        return;
    }

    action_Load_Tags_File(document, filename);
    updateTagsFileMenuEx();
}

void MainWindow::on_action_Load_Tags_File_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Load_Tags_File(document);
    }
}

void MainWindow::action_Load_Macro_File(DocumentWidget *document, const QString &filename) {

    emit_event("load_macro_file", filename);
    document->ReadMacroFileEx(filename, true);
}

void MainWindow::action_Load_Macro_File(DocumentWidget *document) {
    QString filename = PromptForExistingFileEx(document->path_, tr("Load Macro File"));
    if (filename.isNull()) {
        return;
    }

    action_Load_Macro_File(document, filename);
}

void MainWindow::on_action_Load_Macro_File_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Load_Macro_File(document);
    }
}

void MainWindow::action_Print(DocumentWidget *document) {

    emit_event("print");
    if(QPointer<TextArea> area = lastFocus_) {
        document->PrintWindow(area, false);
    }
}

void MainWindow::on_action_Print_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Print(document);
    }
}

void MainWindow::action_Print_Selection(DocumentWidget *document) {

    emit_event("print_selection");
    if(QPointer<TextArea> area = lastFocus_) {
        document->PrintWindow(area, true);
    }
}

void MainWindow::on_action_Print_Selection_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Print_Selection(document);
    }
}

void MainWindow::action_Split_Pane(DocumentWidget *document) {

    emit_event("split_pane");
    document->splitPane();
    ui.action_Close_Pane->setEnabled(document->textPanesCount() > 1);
}

void MainWindow::on_action_Split_Pane_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Split_Pane(document);
    }
}

void MainWindow::action_Close_Pane(DocumentWidget *document) {

    emit_event("close_pane");
    document->closePane();
    ui.action_Close_Pane->setEnabled(document->textPanesCount() > 1);
}

void MainWindow::on_action_Close_Pane_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Close_Pane(document);
    }
}

void MainWindow::action_Move_Tab_To(DocumentWidget *document) {

    emit_event("move_document_dialog");
    document->moveDocument(this);
}

void MainWindow::on_action_Move_Tab_To_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Move_Tab_To(document);
    }
}

void MainWindow::on_action_About_Qt_triggered() {
    QMessageBox::aboutQt(this);
}

DocumentWidget *MainWindow::currentDocument() const {
    return qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget());
}

DocumentWidget *MainWindow::documentAt(int index) const {
    return qobject_cast<DocumentWidget *>(ui.tabWidget->widget(index));
}

void MainWindow::on_action_Statistics_Line_toggled(bool state) {
    std::vector<DocumentWidget *> documents = openDocuments();
    for(DocumentWidget *document : documents) {
        document->ShowStatsLine(state);

        if(document->IsTopDocument()) {
            document->UpdateStatsLine(nullptr);
        }
    }
}

/*
** Turn on and off the continuing display of the incremental search line
** (when off, it is popped up and down as needed via TempShowISearch)
*/
void MainWindow::on_action_Incremental_Search_Line_toggled(bool state) {
    if (showISearchLine_ == state) {
        return;
    }

    showISearchLine_ = state;

    ui.incrementalSearchFrame->setVisible(state);
}

void MainWindow::on_action_Show_Line_Numbers_toggled(bool state) {
   ShowLineNumbers(state);
}

void MainWindow::action_Set_Auto_Indent(DocumentWidget *document, IndentStyle state) {
    emit_event("set_auto_indent", to_string(state));
    document->SetAutoIndent(state);
}

void MainWindow::setAutoIndent(IndentStyle state) {

    if(DocumentWidget *document = currentDocument()) {
        action_Set_Auto_Indent(document, state);
    }
}

void MainWindow::indentGroupTriggered(QAction *action) {
    if(action == ui.action_Indent_Off) {
        setAutoIndent(IndentStyle::None);
    } else if(action == ui.action_Indent_On) {
        setAutoIndent(IndentStyle::Auto);
    } else if(action == ui.action_Indent_Smart) {
        setAutoIndent(IndentStyle::Smart);
    } else {
        qWarning("NEdit: set_auto_indent invalid argument");
    }
}

void MainWindow::wrapGroupTriggered(QAction *action) {
    if(DocumentWidget *document = currentDocument()) {
        if(action == ui.action_Wrap_None) {
            document->SetAutoWrap(WrapStyle::None);
        } else if(action == ui.action_Wrap_Auto_Newline) {
            document->SetAutoWrap(WrapStyle::Newline);
        } else if(action == ui.action_Wrap_Continuous) {
            document->SetAutoWrap(WrapStyle::Continuous);
        } else {
            qWarning("NEdit: set_wrap_text invalid argument");
        }
    }
}

void MainWindow::on_action_Wrap_Margin_triggered() {
    if(DocumentWidget *document = currentDocument()) {

		auto dialog = std::make_unique<DialogWrapMargin>(document, this);

        // Set default value
        int margin = document->firstPane()->getWrapMargin();

        dialog->ui.checkWrapAndFill->setChecked(margin == 0);
        dialog->ui.spinWrapAndFill->setValue(margin);

        dialog->exec();
    }
}

void MainWindow::on_action_Tab_Stops_triggered() {
    if(DocumentWidget *document = currentDocument()) {
		auto dialog = std::make_unique<DialogTabs>(document, this);
        dialog->exec();
    }
}

void MainWindow::on_action_Text_Fonts_triggered() {
    if(DocumentWidget *document = currentDocument()) {

        document->dialogFonts_ = new DialogFonts(document, this);
        document->dialogFonts_->exec();
        delete document->dialogFonts_;
    }
}

void MainWindow::on_action_Highlight_Syntax_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {

        document->highlightSyntax_ = state;

        if (document->highlightSyntax_) {
            document->StartHighlightingEx(true);
        } else {
            document->StopHighlightingEx();
        }
    }
}

void MainWindow::on_action_Apply_Backlighting_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {
        document->SetBacklightChars(state ? GetPrefBacklightCharTypes() : QString());
    }
}

void MainWindow::on_action_Make_Backup_Copy_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {
        document->saveOldVersion_ = state;
    }
}

void MainWindow::on_action_Incremental_Backup_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {
        document->autoSave_ = state;
    }
}

void MainWindow::matchingGroupTriggered(QAction *action) {

    if(DocumentWidget *document = currentDocument()) {
        if(action == ui.action_Matching_Off) {
            document->SetShowMatching(ShowMatchingStyle::None);
        } else if(action == ui.action_Matching_Delimiter) {
            document->SetShowMatching(ShowMatchingStyle::Delimeter);
        } else if(action == ui.action_Matching_Range) {
            document->SetShowMatching(ShowMatchingStyle::Range);
        } else {
            qWarning("NEdit: Invalid argument for set_show_matching");
        }
    }
}

void MainWindow::on_action_Matching_Syntax_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {
        document->matchSyntaxBased_ = state;
    }
}

void MainWindow::on_action_Overtype_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {
        document->SetOverstrike(state);
    }
}

void MainWindow::on_action_Read_Only_toggled(bool state) {
    if(DocumentWidget *document = currentDocument()) {
        document->lockReasons_.setUserLocked(state);
        UpdateWindowTitle(document);
        UpdateWindowReadOnly(document);
    }
}

void MainWindow::on_action_Save_Defaults_triggered() {
    SaveNEditPrefsEx(this, false);
}

void MainWindow::on_action_Default_Language_Modes_triggered() {
    auto dialog = new DialogLanguageModes(this);
    dialog->show();
}

void MainWindow::defaultIndentGroupTriggered(QAction *action) {

    if(action == ui.action_Default_Indent_Off) {
        SetPrefAutoIndent(IndentStyle::None);
    } else if(action == ui.action_Default_Indent_On) {
        SetPrefAutoIndent(IndentStyle::Auto);
    } else if(action == ui.action_Default_Indent_Smart) {
        SetPrefAutoIndent(IndentStyle::Smart);
    } else {
        qWarning("NEdit: invalid default indent");
    }
}

void MainWindow::on_action_Default_Program_Smart_Indent_triggered() {

    if (!SmartIndentDlg) {
        // We pass this document so that the dialog can show the information for the currently
        // active language mode
        if(DocumentWidget *document = currentDocument()) {
            SmartIndentDlg = new DialogSmartIndent(document, this);
        }
    }

    if (LanguageModeName(0).isNull()) {
        QMessageBox::warning(this, tr("Language Mode"), tr("No Language Modes defined"));
        return;
    }

    SmartIndentDlg->show();
    SmartIndentDlg->raise();
}

void MainWindow::defaultWrapGroupTriggered(QAction *action) {

    if(action == ui.action_Default_Wrap_None) {
        SetPrefWrap(WrapStyle::None);
    } else if(action == ui.action_Default_Wrap_Auto_Newline) {
        SetPrefWrap(WrapStyle::Newline);
    } else if(action == ui.action_Default_Wrap_Continuous) {
        SetPrefWrap(WrapStyle::Continuous);
    } else {
        qWarning("NEdit: invalid default wrap");
    }
}

void MainWindow::on_action_Default_Wrap_Margin_triggered() {

	auto dialog = std::make_unique<DialogWrapMargin>(nullptr, this);

    // Set default value
    int margin = GetPrefWrapMargin();

    dialog->ui.checkWrapAndFill->setChecked(margin == 0);
    dialog->ui.spinWrapAndFill->setValue(margin);

    dialog->exec();
}

void MainWindow::defaultTagCollisionsGroupTriggered(QAction *action) {
    if(action == ui.action_Default_Tag_Show_All) {
        SetPrefSmartTags(false);
    } else if(action == ui.action_Default_Tag_Smart) {
        SetPrefSmartTags(true);
    } else {
        qWarning("NEdit: invalid default collisions");
    }
}

void MainWindow::on_action_Default_Command_Shell_triggered() {
    bool ok;
    QString shell = QInputDialog::getText(this,
        tr("Command Shell"),
        tr("Enter shell path:"),
        QLineEdit::Normal,
        GetPrefShell(),
        &ok);

    if (ok && !shell.isEmpty()) {

		if(!QFile::exists(shell)) {
            int resp = QMessageBox::warning(
                        this,
                        tr("Command Shell"),
                        tr("The selected shell is not available.\nDo you want to use it anyway?"),
                        QMessageBox::Ok | QMessageBox::Cancel);
            if(resp == QMessageBox::Cancel) {
                return;
            }
        }

        SetPrefShell(shell);
    }
}

void MainWindow::on_action_Default_Tab_Stops_triggered() {
	auto dialog = std::make_unique<DialogTabs>(nullptr, this);
    dialog->exec();
}

void MainWindow::on_action_Default_Text_Fonts_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        document->dialogFonts_ = new DialogFonts(nullptr, this);
        document->dialogFonts_->exec();
        delete document->dialogFonts_;
    }
}

void MainWindow::on_action_Default_Colors_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        if(!document->dialogColors_) {
            document->dialogColors_ = new DialogColors(this);
        }

        document->dialogColors_->show();
        document->dialogColors_->raise();
    }
}

/*
** Present a dialog for editing the user specified commands in the shell menu
*/
void MainWindow::on_action_Default_Shell_Menu_triggered() {

    static QPointer<DialogShellMenu> WindowShellMenu;

    if(!WindowShellMenu) {
        WindowShellMenu = new DialogShellMenu(this);
    }

    WindowShellMenu->show();
    WindowShellMenu->raise();
}

/*
** Present a dialogs for editing the user specified commands in the Macro
** and background menus
*/
void MainWindow::on_action_Default_Macro_Menu_triggered() {

    if(!WindowMacros) {
        WindowMacros = new DialogMacros(this);
    }

    WindowMacros->show();
    WindowMacros->raise();
}

void MainWindow::on_action_Default_Window_Background_Menu_triggered() {

    if(!WindowBackgroundMenu) {
        WindowBackgroundMenu = new DialogWindowBackgroundMenu(this);
    }

    WindowBackgroundMenu->show();
    WindowBackgroundMenu->raise();
}

void MainWindow::on_action_Default_Sort_Open_Prev_Menu_toggled(bool state) {
    /* Set the preference, make the other windows' menus agree,
       and invalidate their Open Previous menus */
    SetPrefSortOpenPrevMenu(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Sort_Open_Prev_Menu)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Show_Path_In_Windows_Menu_toggled(bool state) {

    // Set the preference and make the other windows' menus agree
    SetPrefShowPathInWindowsMenu(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Show_Path_In_Windows_Menu)->setChecked(state);
    }

    InvalidateWindowMenus();
}

void MainWindow::on_action_Default_Customize_Window_Title_triggered() {
    if(DocumentWidget *document = currentDocument()) {
		auto dialog = std::make_unique<DialogWindowTitle>(document, this);
        dialog->exec();
    }
}

void MainWindow::on_action_Default_Search_Verbose_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefSearchDlogs(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Search_Verbose)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Search_Wrap_Around_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefSearchWraps(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Search_Wrap_Around)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Search_Beep_On_Search_Wrap_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefBeepOnSearchWrap(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Search_Beep_On_Search_Wrap)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Search_Keep_Dialogs_Up_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefKeepSearchDlogs(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Search_Keep_Dialogs_Up)->setChecked(state);
    }
}

#if defined(REPLACE_SCOPE)
void MainWindow::defaultReplaceScopeGroupTriggered(QAction *action) {

    if(action == replaceScopeInWindow_) {
        SetPrefReplaceDefScope(REPL_DEF_SCOPE_WINDOW);
        for(MainWindow *window : allWindows()) {
            no_signals(window->replaceScopeInWindow_)->setChecked(true);
        }
    } else if(action == replaceScopeInSelection_) {
        SetPrefReplaceDefScope(REPL_DEF_SCOPE_SELECTION);
        for(MainWindow *window : allWindows()) {
            no_signals(window->replaceScopeInSelection_)->setChecked(true);
        }
    } else if(action == replaceScopeSmart_) {
        SetPrefReplaceDefScope(REPL_DEF_SCOPE_SMART);
        for(MainWindow *window : allWindows()) {
            no_signals(window->replaceScopeSmart_)->setChecked(true);
        }
    }
}
#endif

void MainWindow::defaultSearchGroupTriggered(QAction *action) {

    if(action == ui.action_Default_Search_Literal) {
        SetPrefSearch(SearchType::Literal);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Literal_Case_Sensitive) {
        SetPrefSearch(SearchType::CaseSense);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal_Case_Sensitive)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Literal_Whole_Word) {
        SetPrefSearch(SearchType::LiteralWord);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal_Whole_Word)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word) {
        SetPrefSearch(SearchType::CaseSenseWord);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Regular_Expression) {
        SetPrefSearch(SearchType::Regex);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Regular_Expression)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Regular_Expresison_Case_Insensitive) {
        SetPrefSearch(SearchType::RegexNoCase);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Regular_Expresison_Case_Insensitive)->setChecked(true);
        }
    }
}

void MainWindow::defaultSyntaxGroupTriggered(QAction *action) {
    if(action == ui.action_Default_Syntax_Off) {
        SetPrefHighlightSyntax(false);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Syntax_Off)->setChecked(true);
        }
    } else if(action == ui.action_Default_Syntax_On) {
        SetPrefHighlightSyntax(true);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Syntax_On)->setChecked(true);
        }
    }
}

void MainWindow::on_action_Default_Syntax_Recognition_Patterns_triggered() {
    EditHighlightPatterns();
}

void MainWindow::on_action_Default_Syntax_Text_Drawing_Styles_triggered() {
    // TODO(eteran): 2.0, eventually move this logic to be local
    EditHighlightStyles(this, QString());
}

void MainWindow::on_action_Default_Apply_Backlighting_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefBacklightChars(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Apply_Backlighting)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Tab_Open_File_In_New_Tab_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefOpenInTab(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Tab_Open_File_In_New_Tab)->setChecked(state);

        if(!GetPrefOpenInTab()) {
            window->ui.action_New_Window->setText(tr("New &Tab"));
        } else {
            window->ui.action_New_Window->setText(tr("New &Window"));
        }
    }
}

void MainWindow::on_action_Default_Tab_Show_Tab_Bar_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefTabBar(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Tab_Show_Tab_Bar)->setChecked(state);
        window->ui.tabWidget->tabBar()->setVisible(state);
    }
}

void MainWindow::on_action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefTabBarHideOne(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open)->setChecked(state);
        window->ui.tabWidget->setTabBarAutoHide(state);
    }
}

void MainWindow::on_action_Default_Tab_Next_Prev_Tabs_Across_Windows_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefGlobalTabNavigate(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Tab_Next_Prev_Tabs_Across_Windows)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Tab_Sort_Tabs_Alphabetically_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefSortTabs(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Tab_Sort_Tabs_Alphabetically)->setChecked(state);
    }

    /* If we just enabled sorting, sort all tabs.  Note that this reorders
       the next pointers underneath us, which is scary, but SortTabBar never
       touches windows that are earlier in the window list so it's ok. */
    if (state) {
        for(MainWindow *window : allWindows()) {
            window->SortTabBar();
        }
    }
}

void MainWindow::on_action_Default_Show_Tooltips_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefToolTips(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Show_Tooltips)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Statistics_Line_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefStatsLine(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Statistics_Line)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Incremental_Search_Line_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefISearchLine(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Incremental_Search_Line)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Show_Line_Numbers_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefLineNums(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Show_Line_Numbers)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Make_Backup_Copy_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefSaveOldVersion(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Make_Backup_Copy)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Incremental_Backup_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefAutoSave(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Incremental_Backup)->setChecked(state);
    }
}

void MainWindow::defaultMatchingGroupTriggered(QAction *action) {
    if(action == ui.action_Default_Matching_Off) {
        SetPrefShowMatching(ShowMatchingStyle::None);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Matching_Off)->setChecked(true);
        }
    } else if(action == ui.action_Default_Matching_Delimiter) {
        SetPrefShowMatching(ShowMatchingStyle::Delimeter);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Matching_Delimiter)->setChecked(true);
        }
    } else if(action == ui.action_Default_Matching_Range) {
        SetPrefShowMatching(ShowMatchingStyle::Range);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Matching_Range)->setChecked(true);
        }
    }
}

void MainWindow::on_action_Default_Matching_Syntax_Based_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefMatchSyntaxBased(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Matching_Syntax_Based)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Terminate_with_Line_Break_on_Save_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefAppendLF(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Terminate_with_Line_Break_on_Save)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Popups_Under_Pointer_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefRepositionDialogs(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Popups_Under_Pointer)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Auto_Scroll_Near_Window_Top_Bottom_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefAutoScroll(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Auto_Scroll_Near_Window_Top_Bottom)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Warnings_Files_Modified_Externally_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefWarnFileMods(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Warnings_Files_Modified_Externally)->setChecked(state);
        no_signals(window->ui.action_Default_Warnings_Check_Modified_File_Contents)->setEnabled(GetPrefWarnFileMods());
    }
}

void MainWindow::on_action_Default_Warnings_Check_Modified_File_Contents_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefWarnRealFileMods(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Warnings_Check_Modified_File_Contents)->setChecked(state);
    }
}

void MainWindow::on_action_Default_Warnings_On_Exit_toggled(bool state) {
    // Set the preference and make the other windows' menus agree
    SetPrefWarnExit(state);
    for(MainWindow *window : allWindows()) {
        no_signals(window->ui.action_Default_Warnings_On_Exit)->setChecked(state);
    }
}

void MainWindow::defaultSizeGroupTriggered(QAction *action) {
    if(action == ui.action_Default_Size_24_x_80) {
        setWindowSizeDefault(24, 80);
    } else if(action == ui.action_Default_Size_40_x_80) {
        setWindowSizeDefault(40, 80);
    } else if(action == ui.action_Default_Size_60_x_80) {
        setWindowSizeDefault(60, 80);
    } else if(action == ui.action_Default_Size_80_x_80) {
        setWindowSizeDefault(80, 80);
    } else if(action == ui.action_Default_Size_Custom) {
		auto dialog = std::make_unique<DialogWindowSize>(this);
        dialog->exec();
        updateWindowSizeMenus();
    }
}

void MainWindow::setWindowSizeDefault(int rows, int cols) {
    SetPrefRows(rows);
    SetPrefCols(cols);
    updateWindowSizeMenus();
}

void MainWindow::updateWindowSizeMenus() {
    for(MainWindow *window : allWindows()) {
        window->updateWindowSizeMenu();
    }
}

void MainWindow::updateWindowSizeMenu() {
    const int rows = GetPrefRows();
    const int cols = GetPrefCols();

    no_signals(ui.action_Default_Size_24_x_80)->setChecked(rows == 24 && cols == 80);
    no_signals(ui.action_Default_Size_40_x_80)->setChecked(rows == 40 && cols == 80);
    no_signals(ui.action_Default_Size_60_x_80)->setChecked(rows == 60 && cols == 80);
    no_signals(ui.action_Default_Size_80_x_80)->setChecked(rows == 80 && cols == 80);

    if ((rows != 24 && rows != 40 && rows != 60 && rows != 80) || cols != 80) {
        no_signals(ui.action_Default_Size_Custom)->setChecked(true);
        ui.action_Default_Size_Custom->setText(tr("Custom... (%1 x %2)").arg(rows).arg(cols));
    } else {
        ui.action_Default_Size_Custom->setText(tr("Custom..."));
    }
}

void MainWindow::action_Next_Document() {

    emit_event("next_document");

    bool crossWindows = GetPrefGlobalTabNavigate();
    int currentIndex  = ui.tabWidget->currentIndex();
    int nextIndex     = currentIndex + 1;
    int tabCount      = ui.tabWidget->count();

    if(!crossWindows) {
        if(nextIndex == tabCount) {
            ui.tabWidget->setCurrentIndex(0);
        } else {
            ui.tabWidget->setCurrentIndex(nextIndex);
        }
    } else {
        if(nextIndex == tabCount) {

            std::vector<MainWindow *> windows = MainWindow::allWindows();
            auto thisIndex = std::find(windows.begin(), windows.end(), this);
            auto nextIndex = std::next(thisIndex);
			
            if(nextIndex == windows.end()) {
                nextIndex = windows.begin();
			}

            // raise the window set the focus to the first document in it
            MainWindow *nextWindow = *nextIndex;
            DocumentWidget *firstWidget = nextWindow->documentAt(0);

            Q_ASSERT(firstWidget);

            if(auto document = firstWidget) {
                document->RaiseFocusDocumentWindow(true);
                nextWindow->ui.tabWidget->setCurrentWidget(document);
            }
			
        } else {
            ui.tabWidget->setCurrentIndex(nextIndex);
        }	
	}
}

void MainWindow::action_Prev_Document() {

    emit_event("previous_document");

    bool crossWindows = GetPrefGlobalTabNavigate();
    int currentIndex  = ui.tabWidget->currentIndex();
    int prevIndex     = currentIndex - 1;
    int tabCount      = ui.tabWidget->count();

    if(!crossWindows) {
        if(currentIndex == 0) {
            ui.tabWidget->setCurrentIndex(tabCount - 1);
        } else {
            ui.tabWidget->setCurrentIndex(prevIndex);
        }
    } else {
        if(currentIndex == 0) {

            std::vector<MainWindow *> windows = MainWindow::allWindows();
            auto thisIndex = std::find(windows.begin(), windows.end(), this);
            decltype(thisIndex) nextIndex;

            if(thisIndex == windows.begin()) {
                nextIndex = std::prev(windows.end());
            } else {
                nextIndex = std::prev(thisIndex);
            }

            // raise the window set the focus to the first document in it
            MainWindow *nextWindow = *nextIndex;
            QWidget *lastWidget = nextWindow->documentAt(nextWindow->ui.tabWidget->count() - 1);

            Q_ASSERT(qobject_cast<DocumentWidget *>(lastWidget));

            if(auto document = qobject_cast<DocumentWidget *>(lastWidget)) {
                document->RaiseFocusDocumentWindow(true);
                nextWindow->ui.tabWidget->setCurrentWidget(document);
            }

        } else {
            ui.tabWidget->setCurrentIndex(prevIndex);
        }
    }
}

void MainWindow::action_Last_Document() {

    emit_event("last_document");

    if(lastFocusDocument) {
        lastFocusDocument->RaiseFocusDocumentWindow(true);
    }
}

DocumentWidget *MainWindow::EditNewFileEx(MainWindow *window, QString geometry, bool iconic, const QString &languageMode, const QString &defaultPath) {

    DocumentWidget *document;

    // Find a (relatively) unique name for the new file
    QString name = MainWindow::UniqueUntitledNameEx();

    // create new window/document
    if (window) {
        document = window->CreateDocument(name);
    } else {
        window   = new MainWindow();
        document = window->CreateDocument(name);

        window->parseGeometry(geometry);

        if(iconic) {
            window->showMinimized();
        } else {
            window->show();
        }
    }

    document->filename_ = name;
    document->path_     = !defaultPath.isEmpty() ? defaultPath : GetCurrentDirEx();

    // do we have a "/" at the end? if not, add one
    if (!document->path_.isEmpty() && !document->path_.endsWith(QLatin1Char('/'))) {
        document->path_.append(QLatin1Char('/'));
    }

    document->SetWindowModified(false);
    document->lockReasons_.clear();
    window->UpdateWindowReadOnly(document);
    document->UpdateStatsLine(nullptr);
    window->UpdateWindowTitle(document);
    document->RefreshTabState();


    if(languageMode.isNull()) {
        document->DetermineLanguageMode(true);
    } else {
        document->action_Set_Language_Mode(languageMode, true);
    }

    if (iconic && window->isMinimized()) {
        document->RaiseDocument();
    } else {
        document->RaiseDocumentWindow();
    }

    window->SortTabBar();
    return document;
}

void MainWindow::AllWindowsBusyEx(const QString &message) {

    if (!currentlyBusy) {
        busyStartTime = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        modeMessageSet = false;

        for(DocumentWidget *document : DocumentWidget::allDocuments()) {
            /* We don't the display message here yet, but defer it for
               a while. If the wait is short, we don't want
               to have it flash on and off the screen.  However,
               we can't use a time since in generally we are in
               a tight loop and only processing exposure events, so it's
               up to the caller to make sure that this routine is called
               at regular intervals.
            */
            document->setCursor(Qt::WaitCursor);
        }

    } else if (!modeMessageSet && !message.isNull() && (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - busyStartTime) > 1000) {

        // Show the mode message when we've been busy for more than a second
        for(DocumentWidget *document : DocumentWidget::allDocuments()) {
            document->SetModeMessageEx(message);
        }
        modeMessageSet = true;
    }

    currentlyBusy = true;
}


void MainWindow::AllWindowsUnbusyEx() {

    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        document->ClearModeMessageEx();
        document->setCursor(Qt::ArrowCursor);
    }

    currentlyBusy  = false;
    modeMessageSet = false;
    busyStartTime  = 0;
}

void MainWindow::action_Save(DocumentWidget *document) {

    emit_event("save");

    if (document->CheckReadOnly()) {
        return;
    }

    document->SaveWindow();
}

void MainWindow::on_action_Save_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Save(document);
    }
}

/*
** Wrapper for HandleCustomNewFileSB which uses the current window's path
** (if set) as the default directory, and asks about embedding newlines
** to make wrapping permanent.
*/
QString MainWindow::PromptForNewFileEx(DocumentWidget *document, const QString &prompt, FileFormats *fileFormat, bool *addWrap) {

    *fileFormat = document->fileFormat_;

    QString filename;

    QFileDialog dialog(this, prompt);

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(document->path_);
    dialog.setOptions(QFileDialog::DontUseNativeDialog);

    if(QGridLayout* const layout = qobject_cast<QGridLayout*>(dialog.layout())) {
        if(layout->rowCount() == 4 && layout->columnCount() == 3) {
            auto boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);

            auto unixCheck = new QRadioButton(tr("&Unix"));
            auto dosCheck  = new QRadioButton(tr("D&OS"));
            auto macCheck  = new QRadioButton(tr("&Macintosh"));

            switch(document->fileFormat_) {
            case FileFormats::Dos:
                dosCheck->setChecked(true);
                break;
            case FileFormats::Mac:
                macCheck->setChecked(true);
                break;
            case FileFormats::Unix:
                unixCheck->setChecked(true);
                break;
            }

            auto group = new QButtonGroup();
            group->addButton(unixCheck);
            group->addButton(dosCheck);
            group->addButton(macCheck);

            boxLayout->addWidget(unixCheck);
            boxLayout->addWidget(dosCheck);
            boxLayout->addWidget(macCheck);

            int row = layout->rowCount();

            layout->addWidget(new QLabel(tr("Format: ")), row, 0, 1, 1);
            layout->addLayout(boxLayout, row, 1, 1, 1, Qt::AlignLeft);

            ++row;

            auto wrapCheck = new QCheckBox(tr("&Add line breaks where wrapped"));
            if(*addWrap) {
                wrapCheck->setChecked(true);
            }

            QObject::connect(wrapCheck, &QCheckBox::toggled, this, [wrapCheck, this](bool checked) {
                if(checked) {
                    int ret = QMessageBox::information(this, tr("Add Wrap"),
                        tr("This operation adds permanent line breaks to match the automatic wrapping done by the Continuous Wrap mode Preferences Option.\n\n"
                           "*** This Option is Irreversable ***\n\n"
                           "Once newlines are inserted, continuous wrapping will no longer work automatically on these lines"),
                        QMessageBox::Ok | QMessageBox::Cancel);

                    if(ret != QMessageBox::Ok) {
                        wrapCheck->setChecked(false);
                    }
                }
            });


            if (document->wrapMode_ == WrapStyle::Continuous) {
                layout->addWidget(wrapCheck, row, 1, 1, 1);
            }

            if(dialog.exec()) {
                if(dosCheck->isChecked()) {
                    document->fileFormat_ = FileFormats::Dos;
                } else if(macCheck->isChecked()) {
                    document->fileFormat_ = FileFormats::Mac;
                } else if(unixCheck->isChecked()) {
                    document->fileFormat_ = FileFormats::Unix;
                }

                *addWrap = wrapCheck->isChecked();
                const QStringList files = dialog.selectedFiles();
                filename = files[0];
            }

        }
    }

    return filename;
}

void MainWindow::action_Save_As(DocumentWidget *document, const QString &filename, bool wrapped) {

    if(wrapped) {
        emit_event("save_as", filename, QLatin1String("wrapped"));
    } else {
        emit_event("save_as", filename);
    }

    document->SaveWindowAs(filename, wrapped);
}

void MainWindow::action_Save_As(DocumentWidget *document) {
    bool addWrap = false;
    FileFormats fileFormat;

    QString fullname = PromptForNewFileEx(document, tr("Save File As"), &fileFormat, &addWrap);
    if (fullname.isNull()) {
        return;
    }

    document->fileFormat_ = fileFormat;
    action_Save_As(document, fullname, addWrap);
}

/**
 * @brief MainWindow::on_action_Save_As_triggered
 */
void MainWindow::on_action_Save_As_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Save_As(document);
    }
}

/**
 * @brief MainWindow::action_Revert_to_Saved
 */
void MainWindow::action_Revert_to_Saved(DocumentWidget *document) {
    emit_event("revert_to_saved");
    document->RevertToSaved();
}

/**
 * @brief MainWindow::on_action_Revert_to_Saved_triggered
 */
void MainWindow::on_action_Revert_to_Saved_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        // re-reading file is irreversible, prompt the user first
        if (document->fileChanged_) {

			int result = QMessageBox::question(
			            this,
			            tr("Discard Changes"),
			            tr("Discard changes to\n%1%2?").arg(document->path_, document->filename_),
			            QMessageBox::Ok | QMessageBox::Cancel
			            );
			if(result == QMessageBox::Cancel) {
				return;
			}
        } else {

			QMessageBox messageBox(this);
            messageBox.setWindowTitle(tr("Reload File"));
            messageBox.setIcon(QMessageBox::Question);
            messageBox.setText(tr("Re-load file\n%1%2?").arg(document->path_, document->filename_));
            QPushButton *buttonOk   = messageBox.addButton(tr("Re-read"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonOk);

            messageBox.exec();
            if(messageBox.clickedButton() == buttonCancel) {
                return;
            }
        }

        action_Revert_to_Saved(document);
    }
}

/**
 * @brief MainWindow::action_New_Window
 * @param document
 */
void MainWindow::action_New_Window(DocumentWidget *document) {
    emit_event("new_window");
    MainWindow::EditNewFileEx(GetPrefOpenInTab() ? nullptr : this, QString(), false, QString(), document->path_);
    CheckCloseDimEx();
}

/**
 * whatever the setting is for what to do with "New", this does the opposite
 *
 * @brief MainWindow::on_action_New_Window_triggered
 */
void MainWindow::on_action_New_Window_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_New_Window(document);
    }
}

void MainWindow::action_Exit(DocumentWidget *document) {

    emit_event("exit");

    Q_UNUSED(document);

    std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

    if (!CheckPrefsChangesSavedEx()) {
        return;
    }

    /* If this is not the last window (more than one window is open),
       confirm with the user before exiting. */

    // NOTE(eteran): test if the current window is NOT the only window
    if (GetPrefWarnExit() && !(documents.size() < 2)) {

        auto exitMsg = tr("Editing: ");

        /* List the windows being edited and make sure the
           user really wants to exit */
        // This code assembles a list of document names being edited and elides as necessary
        for(size_t i = 0; i < documents.size(); ++i) {
            DocumentWidget *const document  = documents[i];

            auto filename = tr("%1%2").arg(document->filename_, document->fileChanged_ ? tr("*") : QString());

            constexpr int DF_MAX_MSG_LENGTH = 2048;

            if (exitMsg.size() + filename.size() + 30 >= DF_MAX_MSG_LENGTH) {
                exitMsg.append(tr("..."));
                break;
            }

            // if this is the last window, use proper grammar :-)
            if (i == (documents.size() - 1)) {
                exitMsg.append(tr("and %1.").arg(filename));
            } else {
                exitMsg.append(tr("%1, ").arg(filename));
            }
        }

        exitMsg.append(tr("\n\nExit NEdit?"));

        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Exit"));
        messageBox.setIcon(QMessageBox::Question);
        messageBox.setText(exitMsg);
        QPushButton *buttonExit   = messageBox.addButton(tr("Exit"), QMessageBox::AcceptRole);
        QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
        Q_UNUSED(buttonExit);

        messageBox.exec();
        if(messageBox.clickedButton() == buttonCancel) {
            return;
        }
    }

    // Close all files and exit when the last one is closed
    if (CloseAllFilesAndWindowsEx()) {
        QApplication::quit();
    }
}

/**
 * @brief MainWindow::on_action_Exit_triggered
 */
void MainWindow::on_action_Exit_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Exit(document);
    }
}

/*
** Check if preferences have changed, and if so, ask the user if he wants
** to re-save.  Returns False if user requests cancelation of Exit (or whatever
** operation triggered this call to be made).
*/
bool MainWindow::CheckPrefsChangesSavedEx() {

    if (!PreferencesChanged()) {
        return true;
    }

    QString importedFile = ImportedSettingsFile();

    QMessageBox messageBox(this);
    messageBox.setWindowTitle(tr("Default Preferences"));
    messageBox.setIcon(QMessageBox::Question);

    messageBox.setText((importedFile.isNull())
        ? tr("Default Preferences have changed.\nSave changes to NEdit preference file?")
        : tr("Default Preferences have changed.\nSAVING CHANGES WILL INCORPORATE ADDITIONAL SETTINGS FROM FILE: %s").arg(importedFile));

    QPushButton *buttonSave     = messageBox.addButton(QMessageBox::Save);
    QPushButton *buttonDontSave = messageBox.addButton(tr("Don't Save"), QMessageBox::AcceptRole);
    QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
    Q_UNUSED(buttonCancel);

    messageBox.exec();
    if(messageBox.clickedButton() == buttonSave) {
        SaveNEditPrefsEx(this, true);
        return true;
    } else if(messageBox.clickedButton() == buttonDontSave) {
        return true;
    } else {
        return false;
    }

}

/*
** close all the documents in a window
*/
bool MainWindow::CloseAllDocumentInWindow() {

    if (TabCount() == 1) {
        // only one document in the window
        if(DocumentWidget *document = currentDocument()) {
            return document->CloseFileAndWindow(CloseMode::Prompt);
        }
    } else {

        // close all _modified_ documents belong to this window
        for(DocumentWidget *document : openDocuments()) {
            if (document->fileChanged_) {
                if (!document->CloseFileAndWindow(CloseMode::Prompt)) {
                    return false;
                }
            }
        }

        // if there's still documents left in the window...
        for(DocumentWidget *document : openDocuments()) {
            if (!document->CloseFileAndWindow(CloseMode::Prompt)) {
                return false;
            }
        }
    }

    return true;
}

void MainWindow::closeEvent(QCloseEvent *event) {

    std::vector<MainWindow *> windows = MainWindow::allWindows();
    if(windows.size() == 1) {
        // this is only window, then this is the same as exit
        event->ignore();
        on_action_Exit_triggered();

    } else {

        if (TabCount() == 1) {
            if(DocumentWidget *document = currentDocument()) {
                document->CloseFileAndWindow(CloseMode::Prompt);
            }
        } else {
            int resp = QMessageBox::Cancel;
            if (GetPrefWarnExit()) {
                // TODO(eteran): 2.0 this is probably better off with "Ok" "Cancel", but we are being consistant with the original UI for now
                resp = QMessageBox::question(this,
                                             tr("Close Window"),
                                             tr("Close ALL documents in this window?"),
                                             QMessageBox::Cancel,
                                             QMessageBox::Close);
            }

            if (resp == QMessageBox::Close) {
                CloseAllDocumentInWindow();
                event->accept();
            } else {
                event->ignore();
            }
        }
    }
}

void MainWindow::action_Execute_Command_Line(DocumentWidget *document) {

    emit_event("execute_command_line");

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        document->ExecCursorLineEx(area, false);
    }
}

void MainWindow::on_action_Execute_Command_Line_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        action_Execute_Command_Line(document);
    }
}

void MainWindow::on_action_Cancel_Shell_Command_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        document->AbortShellCommandEx();
    }
}


void MainWindow::on_action_Learn_Keystrokes_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        document->BeginLearnEx();
    }
}

void MainWindow::on_action_Finish_Learn_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        document->FinishLearnEx();
    }
}

void MainWindow::on_action_Replay_Keystrokes_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        document->ReplayEx();
    }
}

void MainWindow::on_action_Cancel_Learn_triggered() {
    if(DocumentWidget *document = currentDocument()) {
        document->CancelMacroOrLearnEx();
    }
}

/*
** Close all files and windows, leaving one untitled window
*/
bool MainWindow::CloseAllFilesAndWindowsEx() {

    for(MainWindow *window : MainWindow::allWindows()) {

        // NOTE(eteran): the original code took measures to ensure that
        // if this was being called from a macro, that the window which
        // the macro was running in would be closed last to avoid an infinite
        // loop due to closing modifying the list of windows while it was being
        // iterated. Since this code operates on a COPY of the list of windows
        // I don't think such measures are necessary anymore. But I will
        // include the original comment just in case..
        //

        /*
         * When we're exiting through a macro, the document running the
         * macro does not disappear from the list, so we could get stuck
         * in an endless loop if we try to close it. Therefore, we close
         * other documents first. (Note that the document running the macro
         * may get closed because it is in the same window as another
         * document that gets closed, but it won't disappear; it becomes
         * Untitled.)
         */

        if (!window->CloseAllDocumentInWindow()) {
            return false;
        }

        delete window;
    }

    return true;
}

void MainWindow::action_Repeat(DocumentWidget *document) {
    QString LastCommand = CommandRecorder::getInstance()->lastCommand;

    if(LastCommand.isNull()) {
        QMessageBox::warning(
                    this,
                    tr("Repeat Macro"),
                    tr("No previous commands or learn/replay sequences to repeat"));
        return;
    }

    int index = LastCommand.indexOf(QLatin1Char('('));
    if(index == -1) {
        return;
    }

    auto dialog = new DialogRepeat(document, this);
    dialog->setCommand(LastCommand);
    dialog->show();
}

void MainWindow::on_action_Repeat_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Repeat(document);
    }
}


void MainWindow::focusChanged(QWidget *from, QWidget *to) {

    if(auto area = qobject_cast<TextArea *>(from)) {
        if(auto document = DocumentWidget::fromArea(area)) {
            lastFocusDocument = document;
        }
    }

    if(auto area = qobject_cast<TextArea *>(to)) {
        if(auto document = DocumentWidget::fromArea(area)) {
            // record which window pane last had the keyboard focus
            lastFocus_ = area;

            // update line number statistic to reflect current focus pane
            document->UpdateStatsLine(area);

            // finish off the current incremental search
            EndISearchEx();

            // Check for changes to read-only status and/or file modifications
            document->CheckForChangesToFileEx();
        }
    }
}

void MainWindow::on_action_Help_triggered() {
    Help::displayTopic(Help::Topic::HELP_START);
}

bool MainWindow::eventFilter(QObject *object, QEvent *event) {

    if(qobject_cast<QTabBar*>(object)) {
        if(event->type() == QEvent::ToolTip) {
            if(!GetPrefToolTips()) {
                return true;
            }
        }
    }

    return false;
}

/*
** Dim/undim buttons for pasting replay macros into macro and bg menu dialogs
*/
void MainWindow::DimPasteReplayBtns(bool enabled) {

    if(WindowMacros) {
        WindowMacros->setPasteReplayEnabled(enabled);
    }

    if(WindowBackgroundMenu) {
        WindowBackgroundMenu->setPasteReplayEnabled(enabled);
    }
}

/**
 * @brief MainWindow::action_Find
 * @param string
 * @param direction
 * @param keepDialogs
 * @param type
 */
void MainWindow::action_Find(DocumentWidget *document, const QString &string, Direction direction, SearchType type, WrapMode searchWrap) {

    emit_event("find", string, to_string(direction), to_string(type), to_string(searchWrap));

    if(QPointer<TextArea> area = lastFocus_) {
        SearchAndSelectEx(
                    document,
                    area,
                    string,
                    direction,
                    type,
                    searchWrap);
    }
}

/**
 * @brief MainWindow::on_action_Find_triggered
 */
void MainWindow::on_action_Find_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Find_Dialog(
                    document,
                    Direction::Forward,
                    GetPrefSearch(),
                    GetPrefKeepSearchDlogs());
    }
}

/**
 * @brief MainWindow::on_action_Find_Again_triggered
 */
void MainWindow::on_action_Find_Again_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Find_Again(
                    document,
                    Direction::Forward,
                    GetPrefSearchWraps());
    }
}

/**
 * @brief MainWindow::on_action_Find_Selection_triggered
 */
void MainWindow::on_action_Find_Selection_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Find_Selection(
                    document,
                    Direction::Forward,
                    GetPrefSearch(),
                    GetPrefSearchWraps());
    }
}

/**
 * @brief MainWindow::action_Replace
 * @param direction
 * @param searchString
 * @param replaceString
 * @param type
 * @param wrap
 */
void MainWindow::action_Replace(DocumentWidget *document, const QString &searchString, const QString &replaceString, Direction direction, SearchType type, WrapMode wrap) {

    emit_event("replace", searchString, replaceString, to_string(direction), to_string(type), to_string(wrap));

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        SearchAndReplaceEx(
                    document,
                    area,
                    searchString,
                    replaceString,
                    direction,
                    type,
                    wrap);
    }
}

/**
 * @brief action_Replace_Dialog
 * @param direction
 * @param type
 * @param keepDialog
 */
void MainWindow::action_Replace_Dialog(DocumentWidget *document, Direction direction, SearchType type, bool keepDialog) {

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        DoFindReplaceDlogEx(document,
                            area,
                            direction,
                            keepDialog,
                            type);
    }
}

/**
 * @brief MainWindow::on_action_Replace_triggered
 */
void MainWindow::on_action_Replace_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Replace_Dialog(document,
                              Direction::Forward,
                              GetPrefSearch(),
                              GetPrefKeepSearchDlogs());
    }
}

/**
 * @brief MainWindow::action_Replace_All
 * @param searchString
 * @param replaceString
 * @param type
 */
void MainWindow::action_Replace_All(DocumentWidget *document, const QString &searchString, const QString &replaceString, SearchType type) {

    emit_event("replace_all", searchString, replaceString, to_string(type));

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ReplaceAllEx(document,
                     area,
                     searchString,
                     replaceString,
                     type);
    }
}

/**
 * @brief MainWindow::action_Show_Tip
 * @param document
 * @param argument
 */
void MainWindow::action_Show_Tip(DocumentWidget *document, const QString &argument) {
    if(!argument.isEmpty()) {
        emit_event("show_tip", argument);
    } else {
        emit_event("show_tip");
    }

    if(QPointer<TextArea> area = lastFocus_) {
        document->FindDefCalltip(area, argument);
    }
}

/**
 * @brief MainWindow::action_Find_Definition
 * @param argument
 */
void MainWindow::action_Find_Definition(DocumentWidget *document, const QString &argument) {
    if(!argument.isEmpty()) {
        emit_event("find_definition", argument);
    } else {
        emit_event("find_definition");
    }

    if(QPointer<TextArea> area = lastFocus_) {
        document->FindDefinition(area, argument);
    }
}

void MainWindow::action_Find_Definition(DocumentWidget *document) {
    action_Find_Definition(document, QString());
}

/**
 * @brief MainWindow::on_action_Find_Definition_triggered
 */
void MainWindow::on_action_Find_Definition_triggered() {
    action_Find_Definition(currentDocument());
}

void MainWindow::action_Show_Calltip(DocumentWidget *document) {
    action_Show_Tip(document, QString());
}

/**
 * @brief MainWindow::on_action_Show_Calltip_triggered
 */
void MainWindow::on_action_Show_Calltip_triggered() {
    action_Show_Calltip(currentDocument());
}

/**
 * @brief MainWindow::action_Filter_Selection
 * @param document
 */
void MainWindow::action_Filter_Selection(DocumentWidget *document) {

    static QPointer<DialogFilter> dialog;

    if (document->CheckReadOnly()) {
        return;
    }

    if (!document->buffer_->BufGetPrimary().selected) {
        QApplication::beep();
        return;
    }

    if(!dialog) {
        dialog = new DialogFilter(this);
    }

    int r = dialog->exec();
    if(!r) {
        return;
    }

    QString filterText = dialog->ui.textFilter->text();
    action_Filter_Selection(document, filterText);
}

/**
 * @brief MainWindow::on_action_Filter_Selection_triggered
 */
void MainWindow::on_action_Filter_Selection_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Filter_Selection(document);
    }
}

/**
 * @brief action_Filter_Selection
 * @param filter
 */
void MainWindow::action_Filter_Selection(DocumentWidget *document, const QString &filter) {

    emit_event("filter_selection", filter);

    if (document->CheckReadOnly()) {
        return;
    }

    if (!document->buffer_->BufGetPrimary().selected) {
        QApplication::beep();
        return;
    }

    if(!filter.isEmpty()) {
        document->filterSelection(filter);
    }
}

/**
 * @brief MainWindow::action_Execute_Command
 * @param command
 */
void MainWindow::action_Execute_Command(DocumentWidget *document, const QString &command) {

    emit_event("execute_command", command);

    if (document->CheckReadOnly())
        return;

    if(!command.isEmpty()) {
        if(QPointer<TextArea> area = lastFocus_) {
            document->execAP(area, command);
        }
    }
}

void MainWindow::action_Execute_Command(DocumentWidget *document) {

    static QPointer<DialogExecuteCommand> dialog;

    if (document->CheckReadOnly())
        return;

    if(!dialog) {
        dialog = new DialogExecuteCommand(this);
    }

    int r = dialog->exec();
    if(!r) {
        return;
    }

    QString commandText = dialog->ui.textCommand->text();
    action_Execute_Command(document, commandText);
}

/**
 * @brief MainWindow::on_action_Execute_Command_triggered
 */
void MainWindow::on_action_Execute_Command_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Execute_Command(document);
    }
}

/**
 * @brief MainWindow::shellTriggered
 * @param action
 */
void MainWindow::shellTriggered(QAction *action) {
    const int index = action->data().toInt();
    const QString name = ShellMenuData[index].item->name;

    if(DocumentWidget *document = currentDocument()) {
        action_Shell_Menu_Command(document, name);
    }
}

/**
 * @brief MainWindow::action_Shell_Menu_Command
 * @param command
 */
void MainWindow::action_Shell_Menu_Command(DocumentWidget *document, const QString &name) {
    emit_event("shell_menu_command", name);
    if(QPointer<TextArea> area = lastFocus_) {
        DoNamedShellMenuCmd(document, area, name, false);
    }
}

/**
 * @brief MainWindow::macroTriggered
 * @param action
 */
void MainWindow::macroTriggered(QAction *action) {

    /* Don't allow users to execute a macro command from the menu (or accel)
       if there's already a macro command executing, UNLESS the macro is
       directly called from another one.  NEdit can't handle
       running multiple, independent uncoordinated, macros in the same
       window.  Macros may invoke macro menu commands recursively via the
       macro_menu_command action proc, which is important for being able to
       repeat any operation, and to embed macros within eachother at any
       level, however, a call here with a macro running means that THE USER
       is explicitly invoking another macro via the menu or an accelerator,
       UNLESS the macro event marker is set */
    if(DocumentWidget *document = currentDocument()) {
        if(document->macroCmdData_) {
            QApplication::beep();
            return;
        }

        const int index = action->data().toInt();
        const QString name = MacroMenuData[index].item->name;
        action_Macro_Menu_Command(document, name);
    }
}

/**
 * @brief MainWindow::action_Macro_Menu_Command
 * @param name
 */
void MainWindow::action_Macro_Menu_Command(DocumentWidget *document, const QString &name) {
    emit_event("macro_menu_command", name);
    if(QPointer<TextArea> area = lastFocus_) {
        DoNamedMacroMenuCmd(document, area, name, false);
    }
}

/**
 * @brief MainWindow::action_Repeat_Macro
 * @param macro
 * @param how
 */
void MainWindow::action_Repeat_Macro(DocumentWidget *document, const QString &macro, int how) {

    // NOTE(eteran): we don't record this event
    document->repeatMacro(macro, how);
}

void MainWindow::on_action_Detach_Tab_triggered() {

    if(DocumentWidget *document = currentDocument()) {
        action_Detach_Document(document);
    }
}

void MainWindow::action_Detach_Document(DocumentWidget *document) {

    emit_event("detach_document");
    if(TabCount() > 1) {
        auto new_window = new MainWindow(nullptr);
        new_window->ui.tabWidget->addTab(document, document->filename_);
        new_window->show();
    }
}

void MainWindow::action_Detach_Document_Dialog(DocumentWidget *document) {

    if(TabCount() > 1) {

        int result = QMessageBox::question(
                    nullptr,
                    tr("Detach Tab"),
                    tr("Detach %1?").arg(document->filename_),
                    QMessageBox::Yes | QMessageBox::No);

        if(result == QMessageBox::Yes) {
            action_Detach_Document(document);
        }
    }
}

MainWindow *MainWindow::fromDocument(const DocumentWidget *document) {
    return qobject_cast<MainWindow *>(document->window());
}

/*
** Present a dialog for editing highlight pattern information
*/
void MainWindow::EditHighlightPatterns() {

    if(SyntaxPatterns) {
        SyntaxPatterns->show();
        SyntaxPatterns->raise();
        return;
    }

    if (LanguageModeName(0).isNull()) {

        QMessageBox::warning(this,
                             tr("No Language Modes"),
                             tr("No Language Modes available for syntax highlighting\n"
                                "Add language modes under Preferenses->Language Modes"));
        return;
    }

    DocumentWidget *document = currentDocument();

    QString languageName = LanguageModeName(document->languageMode_ == PLAIN_LANGUAGE_MODE ? 0 : document->languageMode_);
    SyntaxPatterns = new DialogSyntaxPatterns(this);
    SyntaxPatterns->setLanguageName(languageName);
    SyntaxPatterns->show();
    SyntaxPatterns->raise();
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void MainWindow::UpdateLanguageModeMenu() {
    if(!SyntaxPatterns) {
        return;
    }

    SyntaxPatterns->UpdateLanguageModeMenu();
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing highlight styles updated (via a call to createHighlightStylesMenu)
*/
void MainWindow::updateHighlightStyleMenu() {
    if(!SyntaxPatterns) {
        return;
    }

    SyntaxPatterns->updateHighlightStyleMenu();
}

/*
** Change the language mode name of pattern sets for language "oldName" to
** "newName" in both the stored patterns, and the pattern set currently being
** edited in the dialog.
*/
void MainWindow::RenameHighlightPattern(const QString &oldName, const QString &newName) {

    for(PatternSet &patternSet : PatternSets) {
        if (patternSet.languageMode == oldName) {
            patternSet.languageMode = newName;
        }
    }

    if(SyntaxPatterns) {
        SyntaxPatterns->RenameHighlightPattern(oldName, newName);
    }
}

/*
** Returns True if there are highlight patterns, or potential patterns
** not yet committed in the syntax highlighting dialog for a language mode,
*/
bool MainWindow::LMHasHighlightPatterns(const QString &languageMode) {
    if (FindPatternSet(languageMode) != nullptr) {
        return true;
    }

    return SyntaxPatterns && SyntaxPatterns->LMHasHighlightPatterns(languageMode);
}

bool MainWindow::GetIncrementalSearchLineMS() const {
    return showISearchLine_;
}

void MainWindow::SetIncrementalSearchLineMS(bool value) {

    emit_event("set_incremental_search_line", QString::number(value));

    showISearchLine_ = value;
    no_signals(ui.action_Incremental_Search_Line)->setChecked(value);
}

/*
** Search the text in "document", attempting to match "searchString"
*/
bool MainWindow::SearchWindowEx(DocumentWidget *document, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW) {
    bool found;
    int fileEnd = document->buffer_->BufGetLength() - 1;
    bool outsideBounds;

    // reject empty string
    if (searchString.isEmpty()) {
        return false;
    }

    // get the entire text buffer from the text area widget
    view::string_view fileString = document->buffer_->BufAsStringEx();

    /* If we're already outside the boundaries, we must consider wrapping
       immediately (Note: fileEnd+1 is a valid starting position. Consider
       searching for $ at the end of a file ending with \n.) */
    if ((direction == Direction::Forward && beginPos > fileEnd + 1) || (direction == Direction::Backward && beginPos < 0)) {
        outsideBounds = true;
    } else {
        outsideBounds = false;
    }

    /* search the string copied from the text area widget, and present
       dialogs, or just beep.  iSearchStartPos is not a perfect indicator that
       an incremental search is in progress.  A parameter would be better. */
    if (iSearchStartPos_ == -1) { // normal search

        found = !outsideBounds && SearchString(
                    fileString,
                    searchString,
                    direction,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    startPos,
                    endPos,
                    extentBW,
                    extentFW,
                    document->GetWindowDelimitersEx());


        if (dialogFind_) {
            if(!dialogFind_->keepDialog()) {
                dialogFind_->hide();
            }
        }

        if(auto dialog = getDialogReplace()) {
            if (!dialog->keepDialog()) {
                dialog->hide();
            }
        }

        if (!found) {
            if (searchWrap == WrapMode::Wrap) {
                if (direction == Direction::Forward && beginPos != 0) {
                    if (GetPrefBeepOnSearchWrap()) {
                        QApplication::beep();
                    } else if (GetPrefSearchDlogs()) {

                        QMessageBox messageBox(document);
                        messageBox.setWindowTitle(tr("Wrap Search"));
                        messageBox.setIcon(QMessageBox::Question);
                        messageBox.setText(tr("Continue search from beginning of file?"));
                        QPushButton *buttonContinue = messageBox.addButton(tr("Continue"), QMessageBox::AcceptRole);
                        QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
                        Q_UNUSED(buttonContinue);

                        messageBox.exec();
                        if(messageBox.clickedButton() == buttonCancel) {
                            return false;
                        }
                    }

                    found = SearchString(
                                fileString,
                                searchString,
                                direction,
                                searchType,
                                WrapMode::NoWrap,
                                0,
                                startPos,
                                endPos,
                                extentBW,
                                extentFW,
                                document->GetWindowDelimitersEx());

                } else if (direction == Direction::Backward && beginPos != fileEnd) {
                    if (GetPrefBeepOnSearchWrap()) {
                        QApplication::beep();
                    } else if (GetPrefSearchDlogs()) {

                        QMessageBox messageBox(document);
                        messageBox.setWindowTitle(tr("Wrap Search"));
                        messageBox.setIcon(QMessageBox::Question);
                        messageBox.setText(tr("Continue search from end of file?"));
                        QPushButton *buttonContinue = messageBox.addButton(tr("Continue"), QMessageBox::AcceptRole);
                        QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
                        Q_UNUSED(buttonContinue);

                        messageBox.exec();
                        if(messageBox.clickedButton() == buttonCancel) {
                            return false;
                        }
                    }
                    found = SearchString(
                                fileString,
                                searchString,
                                direction,
                                searchType,
                                WrapMode::NoWrap,
                                fileEnd + 1,
                                startPos,
                                endPos,
                                extentBW,
                                extentFW,
                                document->GetWindowDelimitersEx());
                }
            }
            if (!found) {
                if (GetPrefSearchDlogs()) {
                    QMessageBox::information(document, tr("String not found"), tr("String was not found"));
                } else {
                    QApplication::beep();
                }
            }
        }
    } else { // incremental search
        if (outsideBounds && searchWrap == WrapMode::Wrap) {
            if (direction == Direction::Forward) {
                beginPos = 0;
            } else {
                beginPos = fileEnd + 1;
            }
            outsideBounds = false;
        }

        found = !outsideBounds && SearchString(
                    fileString,
                    searchString,
                    direction,
                    searchType,
                    searchWrap,
                    beginPos,
                    startPos,
                    endPos,
                    extentBW,
                    extentFW,
                    document->GetWindowDelimitersEx());

        if (found) {
            iSearchTryBeepOnWrapEx(direction, beginPos, *startPos);
        } else {
            QApplication::beep();
        }
    }

    return found;
}

/*
** Search for "searchString" in "document", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  Also
** adds the search string to the global search history.
*/
bool MainWindow::SearchAndSelectEx(DocumentWidget *document, TextArea *area, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap) {
    int startPos;
    int endPos;
    int beginPos;
    int selStart;
    int selEnd;
    int movedFwd = 0;

    // Save a copy of searchString in the search history
    saveSearchHistory(searchString, QString(), searchType, false);

    /* set the position to start the search so we don't find the same
       string that was found on the last search	*/
    if (searchMatchesSelectionEx(document, searchString, searchType, &selStart, &selEnd, nullptr, nullptr)) {
        // selection matches search string, start before or after sel.
        if (direction == Direction::Backward) {
            beginPos = selStart - 1;
        } else {
            beginPos = selStart + 1;
            movedFwd = 1;
        }
    } else {
        selStart = -1;
        selEnd = -1;
        // no selection, or no match, search relative cursor

        int cursorPos = area->TextGetCursorPos();
        if (direction == Direction::Backward) {
            // use the insert position - 1 for backward searches
            beginPos = cursorPos - 1;
        } else {
            // use the insert position for forward searches
            beginPos = cursorPos;
        }
    }

    /* when the i-search bar is active and search is repeated there
       (Return), the action "find" is called (not: "find_incremental").
       "find" calls this function SearchAndSelect.
       To keep track of the iSearchLastBeginPos correctly in the
       repeated i-search case it is necessary to call the following
       function here, otherwise there are no beeps on the repeated
       incremental search wraps.  */
    iSearchRecordLastBeginPosEx(direction, beginPos);

    // do the search.  SearchWindow does appropriate dialogs and beeps
    if (!SearchWindowEx(document, searchString, direction, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr)) {
        return false;
    }

    /* if the search matched an empty string (possible with regular exps)
       beginning at the start of the search, go to the next occurrence,
       otherwise repeated finds will get "stuck" at zero-length matches */
    if (direction == Direction::Forward && beginPos == startPos && beginPos == endPos) {
        if (!movedFwd && !SearchWindowEx(document, searchString, direction, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr)) {
            return false;
        }
    }

    // if matched text is already selected, just beep
    if (selStart == startPos && selEnd == endPos) {
        QApplication::beep();
        return false;
    }

    // select the text found string
    document->buffer_->BufSelect(startPos, endPos);
    document->MakeSelectionVisible(area);

    area->TextSetCursorPos(endPos);
    area->syncronizeSelection();

    return true;
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  If
** "continued" is TRUE and a prior incremental search starting position is
** recorded, search from that original position, otherwise, search from the
** current cursor position.
*/
bool MainWindow::SearchAndSelectIncrementalEx(DocumentWidget *document, TextArea *area, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap, bool continued) {
    int beginPos;
    int startPos;
    int endPos;

    /* If there's a search in progress, start the search from the original
       starting position, otherwise search from the cursor position. */
    if (!continued || iSearchStartPos_ == -1) {
        iSearchStartPos_ = area->TextGetCursorPos();
        iSearchRecordLastBeginPosEx(direction, iSearchStartPos_);
    }

    beginPos = iSearchStartPos_;

    /* If the search string is empty, beep eventually if text wrapped
       back to the initial position, re-init iSearchLastBeginPos,
       clear the selection, set the cursor back to what would be the
       beginning of the search, and return. */
    if (searchString.isEmpty()) {
        int beepBeginPos = (direction == Direction::Backward) ? beginPos - 1 : beginPos;
        iSearchTryBeepOnWrapEx(direction, beepBeginPos, beepBeginPos);
        iSearchRecordLastBeginPosEx(direction, iSearchStartPos_);
        document->buffer_->BufUnselect();

        area->TextSetCursorPos(beginPos);
        return true;
    }

    /* Save the string in the search history, unless we're cycling thru
       the search history itself, which can be detected by matching the
       search string with the search string of the current history index. */
    if (!(iSearchHistIndex_ > 1 && (searchString == SearchReplaceHistory[historyIndex(iSearchHistIndex_)].search))) {
        saveSearchHistory(searchString, QString(), searchType, true);
        // Reset the incremental search history pointer to the beginning
        iSearchHistIndex_ = 1;
    }

    // begin at insert position - 1 for backward searches
    if (direction == Direction::Backward)
        beginPos--;

    // do the search.  SearchWindow does appropriate dialogs and beeps
    if (!SearchWindowEx(document, searchString, direction, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr))
        return false;

    iSearchLastBeginPos_ = startPos;

    /* if the search matched an empty string (possible with regular exps)
       beginning at the start of the search, go to the next occurrence,
       otherwise repeated finds will get "stuck" at zero-length matches */
    if (direction == Direction::Forward && beginPos == startPos && beginPos == endPos)
        if (!SearchWindowEx(document, searchString, direction, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr))
            return false;

    iSearchLastBeginPos_ = startPos;

    // select the text found string
    document->buffer_->BufSelect(startPos, endPos);
    document->MakeSelectionVisible(area);

    area->TextSetCursorPos(endPos);
    area->syncronizeSelection();

    return true;
}

/*
** Replace selection with "replaceString" and search for string "searchString" in window "window",
** using algorithm "searchType" and direction "direction"
*/
bool MainWindow::ReplaceAndSearchEx(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWrap) {
    int startPos = 0;
    int endPos = 0;
    int searchExtentBW;
    int searchExtentFW;

    // Save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    bool replaced = false;

    // Replace the selected text only if it matches the search string
    if (searchMatchesSelectionEx(document, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {

        int replaceLen = 0;

        // replace the text
        if (isRegexType(searchType)) {
            std::string replaceResult;
            const std::string foundString = document->buffer_->BufGetRangeEx(searchExtentBW, searchExtentFW + 1);

            replaceUsingREEx(
                searchString,
                replaceString,
                foundString,
                startPos - searchExtentBW,
                replaceResult,
                startPos == 0 ? '\0' : document->buffer_->BufGetCharacter(startPos - 1),
                document->GetWindowDelimitersEx(),
                defaultRegexFlags(searchType));

            document->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
            replaceLen = gsl::narrow<int>(replaceResult.size());
        } else {
            document->buffer_->BufReplaceEx(startPos, endPos, replaceString.toLatin1().data());
            replaceLen = replaceString.size();
        }

        // Position the cursor so the next search will work correctly based
        // on the direction of the search
        area->TextSetCursorPos(startPos + ((direction == Direction::Forward) ? replaceLen : 0));
        replaced = true;
    }

    // do the search; beeps/dialogs are taken care of
    SearchAndSelectEx(document, area, searchString, direction, searchType, searchWrap);
    return replaced;
}

void MainWindow::DoFindDlogEx(DocumentWidget *document, Direction direction, bool keepDialogs, SearchType searchType) {

    if(!dialogFind_) {
        dialogFind_ = new DialogFind(this, document);
    }

    dialogFind_->setTextField(document);

    if(dialogFind_->isVisible()) {
        dialogFind_->raise();
        dialogFind_->activateWindow();
        return;
    }

    // Set the initial search type
    dialogFind_->initToggleButtons(searchType);

    // Set the initial direction based on the direction argument
    dialogFind_->ui.checkBackward->setChecked(direction == Direction::Forward ? false : true);

    // Set the state of the Keep Dialog Up button
    dialogFind_->ui.checkKeep->setChecked(keepDialogs);

    // Set the state of the Find button
    dialogFind_->fUpdateActionButtons();

    // start the search history mechanism at the current history item
    fHistIndex_ = 0;

    dialogFind_->show();
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** return search type in "searchType", and return TRUE as the function value.
** Otherwise, return FALSE.
*/
bool MainWindow::SearchAndSelectSameEx(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {
    if (NHist < 1) {
        QApplication::beep();
        return false;
    }

    return SearchAndSelectEx(
                document,
                area,                
                SearchReplaceHistory[historyIndex(1)].search,
                direction,
                SearchReplaceHistory[historyIndex(1)].type,
                searchWrap);
}

/*
** Search for string "searchString" in window "window", using algorithm
** "searchType" and direction "direction", and replace it with "replaceString"
** Also adds the search and replace strings to the global search history.
*/
bool MainWindow::SearchAndReplaceEx(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWrap) {
    int startPos;
    int endPos;
    int replaceLen;
    int searchExtentBW;
    int searchExtentFW;

    // Save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    // If the text selected in the window matches the search string,
    // the user is probably using search then replace method, so
    // replace the selected text regardless of where the cursor is.
    // Otherwise, search for the string.
    if (!searchMatchesSelectionEx(document, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {
        // get the position to start the search

        int beginPos;
        int cursorPos = area->TextGetCursorPos();
        if (direction == Direction::Backward) {
            // use the insert position - 1 for backward searches
            beginPos = cursorPos - 1;
        } else {
            // use the insert position for forward searches
            beginPos = cursorPos;
        }

        // do the search
        bool found = SearchWindowEx(document, searchString, direction, searchType, searchWrap, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW);
        if (!found)
            return false;
    }

    // replace the text
    if (isRegexType(searchType)) {
        std::string replaceResult;
        const std::string foundString = document->buffer_->BufGetRangeEx(searchExtentBW, searchExtentFW + 1);

        QString delimieters = document->GetWindowDelimitersEx();

        replaceUsingREEx(
            searchString,
            replaceString,
            foundString,
            startPos - searchExtentBW,
            replaceResult,
            startPos == 0 ? '\0' : document->buffer_->BufGetCharacter(startPos - 1),
            delimieters,
            defaultRegexFlags(searchType));

        document->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
        replaceLen = gsl::narrow<int>(replaceResult.size());
    } else {
        document->buffer_->BufReplaceEx(startPos, endPos, replaceString.toLatin1().data());
        replaceLen = replaceString.size();
    }

    /* after successfully completing a replace, selected text attracts
       attention away from the area of the replacement, particularly
       when the selection represents a previous search. so deselect */
    document->buffer_->BufUnselect();

    /* temporarily shut off autoShowInsertPos before setting the cursor
       position so MakeSelectionVisible gets a chance to place the replaced
       string at a pleasing position on the screen (otherwise, the cursor would
       be automatically scrolled on screen and MakeSelectionVisible would do
       nothing) */
    area->setAutoShowInsertPos(false);

    area->TextSetCursorPos(startPos + ((direction == Direction::Forward) ? replaceLen : 0));
    document->MakeSelectionVisible(area);
    area->setAutoShowInsertPos(true);

    return true;
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool MainWindow::ReplaceSameEx(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {
    if (NHist < 1) {
        QApplication::beep();
        return false;
    }

    return SearchAndReplaceEx(
                document,
                area,                
                SearchReplaceHistory[historyIndex(1)].search,
                SearchReplaceHistory[historyIndex(1)].replace,
                direction,
                SearchReplaceHistory[historyIndex(1)].type,
                searchWrap);
}

void MainWindow::action_Replace_Find(DocumentWidget *document, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWraps) {

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ReplaceAndSearchEx(
                    document,
                    area,
                    searchString,
                    replaceString,
                    direction,
                    searchType,
                    searchWraps);
    }
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool MainWindow::ReplaceFindSameEx(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {
    if (NHist < 1) {
        QApplication::beep();
        return false;
    }

    return ReplaceAndSearchEx(
                document,
                area,                
                SearchReplaceHistory[historyIndex(1)].search,
                SearchReplaceHistory[historyIndex(1)].replace,
                direction,
                SearchReplaceHistory[historyIndex(1)].type,
                searchWrap);
}

void MainWindow::SearchForSelectedEx(DocumentWidget *document, TextArea *area, Direction direction, SearchType searchType, WrapMode searchWrap) {

    // skip if we can't get the selection data or it's too long
    // should be of type text???
    const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
    if(!mimeData->hasText()) {
        if (GetPrefSearchDlogs()) {
            QMessageBox::warning(document, tr("Wrong Selection"), tr("Selection not appropriate for searching"));
        } else {
            QApplication::beep();
        }
        return;
    }

    // make the selection the current search string
    QString searchString = mimeData->text();

    if (searchString.isEmpty()) {
        QApplication::beep();
        return;
    }

    /* Use the passed method for searching, unless it is regex, since this
       kind of search is by definition a literal search */
    if (searchType == SearchType::Regex) {
        searchType = SearchType::CaseSense;
    } else if (searchType == SearchType::RegexNoCase) {
        searchType = SearchType::Literal;
    }

    // search for it in the window
    SearchAndSelectEx(
                document,
                area,                
                searchString,
                direction,
                searchType,
                searchWrap);
}

void MainWindow::action_Replace_In_Selection(DocumentWidget *document, const QString &searchString, const QString &replaceString, SearchType type) {

    emit_event("replace_in_selection", searchString, replaceString, to_string(type));

    if (document->CheckReadOnly()) {
        return;
    }

    if(QPointer<TextArea> area = lastFocus_) {
        ReplaceInSelectionEx(
                    document,
                    area,
                    searchString,
                    replaceString,
                    type);
    }
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString"
** within the current primary selection in "window". Also adds the search and
** replace strings to the global search history.
*/
void MainWindow::ReplaceInSelectionEx(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType) {

    int selStart;
    int selEnd;
    int beginPos;
    int startPos;
    int endPos;
    int realOffset;
    int replaceLen;
    bool found;
    bool isRect;
    int rectStart;
    int rectEnd;
    int lineStart;
    int cursorPos;
    int extentBW;
    int extentFW;
    std::string fileString;
    bool substSuccess = false;
    bool anyFound     = false;
    bool cancelSubst  = true;

    // save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    // find out where the selection is
    if (!document->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
        return;
    }

    // get the selected text
    if (isRect) {
        selStart   = document->buffer_->BufStartOfLine(selStart);
        selEnd     = document->buffer_->BufEndOfLine( selEnd);
        fileString = document->buffer_->BufGetRangeEx(selStart, selEnd);
    } else {
        fileString = document->buffer_->BufGetSelectionTextEx();
    }

    /* create a temporary buffer in which to do the replacements to hide the
       intermediate steps from the display routines, and so everything can
       be undone in a single operation */
    TextBuffer tempBuf;
    tempBuf.BufSetAllEx(fileString);

    // search the string and do the replacements in the temporary buffer

    replaceLen = replaceString.size();
    found      = true;
    beginPos   = 0;
    cursorPos  = 0;
    realOffset = 0;

    while (found) {
        found = SearchString(
                    fileString,
                    searchString,
                    Direction::Forward,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    &startPos,
                    &endPos,
                    &extentBW,
                    &extentFW,
                    document->GetWindowDelimitersEx());

        if (!found) {
            break;
        }

        anyFound = true;
        /* if the selection is rectangular, verify that the found
           string is in the rectangle */
        if (isRect) {
            lineStart = document->buffer_->BufStartOfLine(selStart + startPos);
            if (document->buffer_->BufCountDispChars(lineStart, selStart + startPos) < rectStart || document->buffer_->BufCountDispChars(lineStart, selStart + endPos) > rectEnd) {

                if(static_cast<size_t>(endPos) == fileString.size()) {
                    break;
                }

                /* If the match starts before the left boundary of the
                   selection, and extends past it, we should not continue
                   search after the end of the (false) match, because we
                   could miss a valid match starting between the left boundary
                   and the end of the false match. */
                if (document->buffer_->BufCountDispChars(lineStart, selStart + startPos) < rectStart && document->buffer_->BufCountDispChars(lineStart, selStart + endPos) > rectStart) {
                    beginPos += 1;
                } else {
                    beginPos = (startPos == endPos) ? endPos + 1 : endPos;
                }
                continue;
            }
        }

        /* Make sure the match did not start past the end (regular expressions
           can consider the artificial end of the range as the end of a line,
           and match a fictional whole line beginning there) */
        if (startPos == (selEnd - selStart)) {
            found = false;
            break;
        }

        // replace the string and compensate for length change
        if (isRegexType(searchType)) {
            std::string replaceResult;
            const std::string foundString = tempBuf.BufGetRangeEx(extentBW + realOffset, extentFW + realOffset + 1);

            substSuccess = replaceUsingREEx(
                            searchString,
                            replaceString,
                            foundString,
                            startPos - extentBW,
                            replaceResult,
                            (startPos + realOffset) == 0 ? '\0' : tempBuf.BufGetCharacter(startPos + realOffset - 1),
                            document->GetWindowDelimitersEx(),
                            defaultRegexFlags(searchType));

            if (!substSuccess) {
                cancelSubst = prefOrUserCancelsSubstEx(document);

                if (cancelSubst) {
                    //  No point in trying other substitutions.
                    break;
                }
            }

            tempBuf.BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceResult);
            replaceLen = gsl::narrow<int>(replaceResult.size());
        } else {
            // at this point plain substitutions (should) always work
            tempBuf.BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceString.toLatin1().data());
            substSuccess = true;
        }

        realOffset += replaceLen - (endPos - startPos);
        // start again after match unless match was empty, then endPos+1
        beginPos = (startPos == endPos) ? endPos + 1 : endPos;
        cursorPos = endPos;

        if (static_cast<size_t>(endPos) == fileString.size()) {
            break;
        }
    }

    if (anyFound) {
        if (substSuccess || !cancelSubst) {
            /*  Either the substitution was successful (the common case) or the
                user does not care and wants to have a faulty replacement.  */

            // replace the selected range in the real buffer
            document->buffer_->BufReplaceEx(selStart, selEnd, tempBuf.BufAsStringEx());

            // set the insert point at the end of the last replacement
            area->TextSetCursorPos(selStart + cursorPos + realOffset);

            /* leave non-rectangular selections selected (rect. ones after replacement
               are less useful since left/right positions are randomly adjusted) */
            if (!isRect) {
                document->buffer_->BufSelect(selStart, selEnd + realOffset);
                area->syncronizeSelection();
            }
        }
    } else {
        //  Nothing found, tell the user about it
        if (GetPrefSearchDlogs()) {

            if (auto dialog = qobject_cast<DialogFind *>(dialogFind_)) {
                if(!dialog->keepDialog()) {
                    dialog->hide();
                }
            }

            if(auto dialog = getDialogReplace()) {
                if (!dialog->keepDialog()) {
                    dialog->hide();
                }
            }

            QMessageBox::information(document, tr("String not found"), tr("String was not found"));
        } else {
            QApplication::beep();
        }
    }
}

/*
**  Uses the resource nedit.truncSubstitution to determine how to deal with
**  regex failures. This function only knows about the resource (via the usual
**  setting getter) and asks or warns the user depending on that.
**
**  One could argue that the dialoging should be determined by the setting
**  'searchDlogs'. However, the incomplete substitution is not just a question
**  of verbosity, but of data loss. The search is successful, only the
**  replacement fails due to an internal limitation of NEdit.
**
**  The parameters 'parent' and 'display' are only used to put dialogs and
**  beeps at the right place.
**
**  The result is either predetermined by the resource or the user's choice.
*/
bool MainWindow::prefOrUserCancelsSubstEx(DocumentWidget *document) {

    bool cancel = true;

    switch (GetPrefTruncSubstitution()) {
    case TruncSubstitution::Silent:
        //  silently fail the operation
        cancel = true;
        break;

    case TruncSubstitution::Fail:
        //  fail the operation and pop up a dialog informing the user
        QApplication::beep();

        QMessageBox::information(document, tr("Substitution Failed"), tr("The result length of the substitution exceeded an internal limit.\n"
                                                                         "The substitution is canceled."));

        cancel = true;
        break;

    case TruncSubstitution::Warn:
        //  pop up dialog and ask for confirmation
        QApplication::beep();

        {
            QMessageBox messageBox(document);
            messageBox.setWindowTitle(tr("Substitution Failed"));
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setText(tr("The result length of the substitution exceeded an internal limit.\n"
                                  "Executing the substitution will result in loss of data."));

            QPushButton *buttonLose   = messageBox.addButton(QLatin1String("Lose Data"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonLose);

            messageBox.exec();
            cancel = (messageBox.clickedButton() == buttonCancel);
        }
        break;

    case TruncSubstitution::Ignore:
        //  silently conclude the operation; THIS WILL DESTROY DATA.
        cancel = false;
        break;
    }

    return cancel;
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString".
** Also adds the search and replace strings to the global search history.
*/
bool MainWindow::ReplaceAllEx(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType) {

    int copyStart;
    int copyEnd;

    // reject empty string
    if (searchString.isEmpty()) {
        return false;
    }

    // save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    // view the entire text buffer from the text area widget as a string
    view::string_view fileString = document->buffer_->BufAsStringEx();

    QString delimieters = document->GetWindowDelimitersEx();

    bool ok;
    std::string newFileString = ReplaceAllInStringEx(
                fileString,
                searchString,
                replaceString,
                searchType,
                &copyStart,
                &copyEnd,
                delimieters,
                &ok);

    if(!ok) {
        if (document->multiFileBusy_) {
            // only needed during multi-file replacements
            document->replaceFailed_ = true;
        } else if (GetPrefSearchDlogs()) {

            if (auto dialog = qobject_cast<DialogFind *>(dialogFind_)) {
                if(!dialog->keepDialog()) {
                    dialog->hide();
                }
            }

            if(auto dialog = getDialogReplace()) {
                if (!dialog->keepDialog()) {
                    dialog->hide();
                }
            }

            QMessageBox::information(document, tr("String not found"), tr("String was not found"));
        } else {
            QApplication::beep();
        }

        return false;
    }

    // replace the contents of the text widget with the substituted text
    document->buffer_->BufReplaceEx(copyStart, copyEnd, newFileString);

    // Move the cursor to the end of the last replacement
    area->TextSetCursorPos(copyStart + gsl::narrow<int>(newFileString.size()));

    return true;
}

void MainWindow::DoFindReplaceDlogEx(DocumentWidget *document, TextArea *area, Direction direction, bool keepDialogs, SearchType searchType) {

    Q_UNUSED(area);

    if (!dialogReplace_) {
        dialogReplace_ = new DialogReplace(this, document);
    }

    auto dialog = getDialogReplace();

    dialog->setTextField(document);

    // If the window is already up, just pop it to the top
    if(dialog->isVisible()) {
        dialog->raise();
        dialog->activateWindow();
        return;
    }

    // Blank the Replace with field
    dialog->ui.textReplace->setText(QString());

    // Set the initial search type
    dialog->initToggleButtons(searchType);

    // Set the initial direction based on the direction argument
    dialog->ui.checkBackward->setChecked(direction == Direction::Forward ? false : true);

    // Set the state of the Keep Dialog Up button
    dialog->ui.checkKeep->setChecked(keepDialogs);

#if defined(REPLACE_SCOPE)

    if (wasSelected_) {
        // If a selection exists, the default scope depends on the preference
        // of the user.
        switch (GetPrefReplaceDefScope()) {
        case REPL_DEF_SCOPE_SELECTION:
            // The user prefers selection scope, no matter what the size of
            // the selection is.
            dialog->ui.radioSelection->setChecked(true);
            break;
        case REPL_DEF_SCOPE_SMART:
            if (document->selectionSpansMultipleLines()) {
                /* If the selection spans multiple lines, the user most
                   likely wants to perform a replacement in the selection */
                dialog->ui.radioSelection->setChecked(true);
            } else {
                /* It's unlikely that the user wants a replacement in a
                   tiny selection only. */
                dialog->ui.radioWindow->setChecked(true);
            }
            break;
        default:
            // The user always wants window scope as default.
            dialog->ui.radioWindow->setChecked(true);
            break;
        }
    } else {
        // No selection -> always choose "In Window" as default.
        dialog->ui.radioWindow->setChecked(true);
    }
#endif

    dialog->UpdateReplaceActionButtons();

    // Start the search history mechanism at the current history item
    rHistIndex_ = 0;

    dialog->show();
}

/*
** Reset window->iSearchLastBeginPos_ to the resulting initial
** search begin position for incremental searches.
*/
void MainWindow::iSearchRecordLastBeginPosEx(Direction direction, int initPos) {
    iSearchLastBeginPos_ = initPos;
    if (direction == Direction::Backward) {
        iSearchLastBeginPos_--;
    }
}

/*
** If this is an incremental search and BeepOnSearchWrap is on:
** Emit a beep if the search wrapped over BOF/EOF compared to
** the last startPos of the current incremental search.
*/
void MainWindow::iSearchTryBeepOnWrapEx(Direction direction, int beginPos, int startPos) {
    if (GetPrefBeepOnSearchWrap()) {
        if (direction == Direction::Forward) {
            if ((startPos >= beginPos && iSearchLastBeginPos_ < beginPos) || (startPos < beginPos && iSearchLastBeginPos_ >= beginPos)) {
                QApplication::beep();
            }
        } else {
            if ((startPos <= beginPos && iSearchLastBeginPos_ > beginPos) || (startPos > beginPos && iSearchLastBeginPos_ <= beginPos)) {
                QApplication::beep();
            }
        }
    }
}

/*
** Return TRUE if "searchString" exactly matches the text in the window's
** current primary selection using search algorithm "searchType".  If true,
** also return the position of the selection in "left" and "right".
*/
bool MainWindow::searchMatchesSelectionEx(DocumentWidget *document, const QString &searchString, SearchType searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW) {

    int selLen;
    int selStart;
    int selEnd;
    int startPos;
    int endPos;
    int extentBW;
    int extentFW;
    int beginPos;
    int regexLookContext = isRegexType(searchType) ? 1000 : 0;
    std::string string;
    int rectStart;
    int rectEnd;
    int lineStart = 0;
    bool isRect;

    // find length of selection, give up on no selection or too long
    if (!document->buffer_->BufGetEmptySelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
        return false;
    }

    // if the selection is rectangular, don't match if it spans lines
    if (isRect) {
        lineStart = document->buffer_->BufStartOfLine(selStart);
        if (lineStart != document->buffer_->BufStartOfLine(selEnd)) {
            return false;
        }
    }

    /* get the selected text plus some additional context for regular
       expression lookahead */
    if (isRect) {
        int stringStart = lineStart + rectStart - regexLookContext;
        if (stringStart < 0) {
            stringStart = 0;
        }

        string = document->buffer_->BufGetRangeEx(stringStart, lineStart + rectEnd + regexLookContext);
        selLen = rectEnd - rectStart;
        beginPos = lineStart + rectStart - stringStart;
    } else {
        int stringStart = selStart - regexLookContext;
        if (stringStart < 0) {
            stringStart = 0;
        }

        string = document->buffer_->BufGetRangeEx(stringStart, selEnd + regexLookContext);
        selLen = selEnd - selStart;
        beginPos = selStart - stringStart;
    }
    if (string.empty()) {
        return false;
    }

    // search for the string in the selection (we are only interested
    // in an exact match, but the procedure SearchString does important
    // stuff like applying the correct matching algorithm)
    bool found = SearchString(
                string,
                searchString,
                Direction::Forward,
                searchType,
                WrapMode::NoWrap,
                beginPos,
                &startPos,
                &endPos,
                &extentBW,
                &extentFW,
                document->GetWindowDelimitersEx());

    // decide if it is an exact match
    if (!found) {
        return false;
    }

    if (startPos != beginPos || endPos - beginPos != selLen) {
        return false;
    }

    // return the start and end of the selection
    if (isRect) {
        document->buffer_->GetSimpleSelection(left, right);
    } else {
        *left  = selStart;
        *right = selEnd;
    }

    if(searchExtentBW) {
        *searchExtentBW = *left - (startPos - extentBW);
    }

    if(searchExtentFW) {
        *searchExtentFW = *right + extentFW - endPos;
    }

    return true;
}

/*
** Update the "Find Definition", "Unload Tags File", "Show Calltip",
** and "Unload Calltips File" menu items in the existing windows.
*/
void MainWindow::updateMenuItems() {
    const bool tipStat = !TipsFileList.empty();
    const bool tagStat = !TagsFileList.empty();

    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Unload_Calltips_File->setEnabled(tipStat);
        window->ui.action_Unload_Tags_File->setEnabled(tagStat);
        window->ui.action_Show_Calltip->setEnabled(tipStat || tagStat);
        window->ui.action_Find_Definition->setEnabled(tagStat);
    }
}

/*
** Search through the shell menu and execute the first command with menu item
** name "itemName".  Returns True on successs and False on failure.
*/
bool MainWindow::DoNamedShellMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, bool fromMacro) {

    if(MenuData *p = findMenuItem(name, DialogTypes::SHELL_CMDS)) {

        if (p->item->output == TO_SAME_WINDOW && document->CheckReadOnly()) {
            return false;
        }

        document->DoShellMenuCmd(
            this,
            area,
            p->item->cmd,
            p->item->input,
            p->item->output,
            p->item->repInput,
            p->item->saveFirst,
            p->item->loadAfter,
            fromMacro);

        return true;
    }

    return false;
}

/*
** Search through the Macro or background menu and execute the first command
** with menu item name "itemName".  Returns True on successs and False on
** failure.
*/
bool MainWindow::DoNamedMacroMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, bool fromMacro) {

    Q_UNUSED(fromMacro);
    Q_UNUSED(area);

    if(MenuData *p = findMenuItem(name, DialogTypes::MACRO_CMDS)) {
        document->DoMacroEx(
            p->item->cmd,
            tr("macro menu command"));

        return true;
    }

    return false;
}

bool MainWindow::DoNamedBGMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, bool fromMacro) {
    Q_UNUSED(fromMacro);
    Q_UNUSED(area);

    if(MenuData *p = findMenuItem(name, DialogTypes::BG_MENU_CMDS)) {
        document->DoMacroEx(
            p->item->cmd,
            tr("background menu macro"));

        return true;
    }

    return false;
}

void MainWindow::SetShowLineNumbers(bool show) {

    emit_event("set_show_line_numbers", QString::fromLatin1(show ? "1" : "0"));

    no_signals(ui.action_Show_Line_Numbers)->setChecked(show);
    showLineNumbers_ = show;
}

bool MainWindow::GetShowLineNumbers() const {
    return showLineNumbers_;
}
