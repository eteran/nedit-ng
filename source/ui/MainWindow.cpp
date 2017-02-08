
#include <QtDebug>
#include <QToolButton>
#include <QShortcut>
#include <QFileDialog>
#include <QFile>
#include <QInputDialog>
#include <QClipboard>
#include <QMessageBox>
#include <QMimeData>
#include "MainWindow.h"
#include "Settings.h"
#include "DialogExecuteCommand.h"
#include "DialogSmartIndent.h"
#include "DialogWrapMargin.h"
#include "DialogMacros.h"
#include "DialogRepeat.h"
#include "DialogWindowBackgroundMenu.h"
#include "DialogShellMenu.h"
#include "DialogFilter.h"
#include "DialogWindowSize.h"
#include "SignalBlocker.h"
#include "DialogLanguageModes.h"
#include "DialogTabs.h"
#include "DialogFonts.h"
#include "DialogColors.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "DialogAbout.h"
#include "DocumentWidget.h"
#include "DialogWindowTitle.h"
#include "DialogReplace.h"
#include "DialogFind.h"
#include "selection.h"
#include "clearcase.h"
#include "MenuItem.h"
#include "file.h"
#include "preferences.h"
#include "shift.h"
#include "utils.h"
#include "memory.h"
#include "regularExp.h"
#include "tags.h"
#include "LanguageMode.h"
#include "nedit.h"
#include "selection.h"
#include "search.h"
#include "macro.h"
#include "highlight.h"
#include "highlightData.h"
#include "util/fileUtils.h"
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include <cmath>
#include <glob.h>

namespace {

bool currentlyBusy = false;
long busyStartTime = 0;
bool modeMessageSet = false;

DialogShellMenu            *WindowShellMenu = nullptr;
DialogWindowBackgroundMenu *WindowBackgroundMenu = nullptr;
DialogMacros               *WindowMacros = nullptr;
DialogSmartIndent          *SmartIndentDlg = nullptr;

const char neditDBBadFilenameChars[] = "\n";

QList<QString> PrevOpen;

/*
 * Auxiliary function for measuring elapsed time during busy waits.
 */
long getRelTimeInTenthsOfSeconds() {
    struct timeval current;
    gettimeofday(&current, nullptr);
    return (current.tv_sec * 10 + current.tv_usec / 100000) & 0xFFFFFFFL;
}

}

//------------------------------------------------------------------------------
// Name: MainWindow
//------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags) {
	ui.setupUi(this);

    connect(qApp, SIGNAL(focusChanged(QWidget *, QWidget *)), this, SLOT(focusChanged(QWidget *, QWidget *)));
	
	setupMenuGroups();
	setupTabBar();
	setupMenuStrings();
	setupMenuAlternativeMenus();
    CreateLanguageModeSubMenu();
    setupMenuDefaults();

    // NOTE(eteran): in the original nedit, the previous menu was populated
    // when the user actually tried to look at the menu. It is simpler (but
    // perhaps marginally less efficient) to just populate it when we construct
    // the window
    updatePrevOpenMenu();

    showISearchLine_       = GetPrefISearchLine();
    iSearchHistIndex_      = 0;
    iSearchStartPos_       = -1;
    iSearchLastRegexCase_  = true;
    iSearchLastLiteralCase_= false;
    wasSelected_           = false;
	
	// default to hiding the optional panels
    ui.incrementalSearchFrame->setVisible(showISearchLine_);

    ui.action_Statistics_Line->setChecked(GetPrefStatsLine());

    CheckCloseDimEx();
}

//------------------------------------------------------------------------------
// Name: ~MainWindow
//------------------------------------------------------------------------------
MainWindow::~MainWindow() {

}

//------------------------------------------------------------------------------
// Name: setDimmensions
//------------------------------------------------------------------------------
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
            cols = regex.capturedTexts()[1].toInt();
            rows = regex.capturedTexts()[2].toInt();
        }
    }

    // TODO(eteran): get this to work!... somehow
    TextArea *area = currentDocument()->firstPane();
    area->setSize(cols, rows);
}

//------------------------------------------------------------------------------
// Name: setupTabBar
//------------------------------------------------------------------------------
void MainWindow::setupTabBar() {
	// create and hook up the tab close button
	auto deleteTabButton = new QToolButton(ui.tabWidget);
	deleteTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	deleteTabButton->setIcon(QIcon::fromTheme(tr("tab-close")));
	deleteTabButton->setAutoRaise(true);
    deleteTabButton->setFocusPolicy(Qt::NoFocus);
    deleteTabButton->setObjectName(tr("tab-close"));
	ui.tabWidget->setCornerWidget(deleteTabButton);
	
	connect(deleteTabButton, SIGNAL(clicked()), this, SLOT(deleteTabButtonClicked()));
}

//------------------------------------------------------------------------------
// Name: setupMenuDefaults
//------------------------------------------------------------------------------
void MainWindow::setupMenuDefaults() {
    // TODO(eteran): make sure that the various menus default to the current
    //               settings based on the preferences!

    // active settings
    ui.action_Statistics_Line->setChecked(GetPrefStatsLine());
    ui.action_Incremental_Search_Line->setChecked(GetPrefISearchLine());
    ui.action_Show_Line_Numbers->setChecked(GetPrefLineNums());
    ui.action_Highlight_Syntax->setChecked(GetPrefHighlightSyntax());
    ui.action_Apply_Backlighting->setChecked(false); // TODO(eteran)
    ui.action_Make_Backup_Copy->setChecked(GetPrefAutoSave());
    ui.action_Incremental_Backup->setChecked(GetPrefSaveOldVersion());
    ui.action_Matching_Syntax->setChecked(GetPrefMatchSyntaxBased());


    // based on document, which defaults to this
    switch(GetPrefAutoIndent(PLAIN_LANGUAGE_MODE)) {
    case NO_AUTO_INDENT:
        ui.action_Indent_Off->setChecked(true);
        break;
    case AUTO_INDENT:
        ui.action_Indent_On->setChecked(true);
        break;
    case SMART_INDENT:
        ui.action_Indent_Smart->setChecked(true);
        break;
    default:
        break;
    }

    // based on document, which defaults to this
    switch(GetPrefWrap(PLAIN_LANGUAGE_MODE)) {
    case NO_WRAP:
        ui.action_Wrap_None->setChecked(true);
        break;
    case NEWLINE_WRAP:
        ui.action_Wrap_Auto_Newline->setChecked(true);
        break;
    case CONTINUOUS_WRAP:
        ui.action_Wrap_Continuous->setChecked(true);
        break;
    default:
        break;
    }

    switch(static_cast<ShowMatchingStyle>(GetPrefShowMatching())) {
    case NO_FLASH:
        ui.action_Matching_Off->setChecked(true);
        break;
    case FLASH_DELIMIT:
        ui.action_Matching_Delimiter->setChecked(true);
        break;
    case FLASH_RANGE:
        ui.action_Matching_Range->setChecked(true);
        break;
    default:
        break;
    }

    // Default Indent
    switch(GetPrefAutoIndent(PLAIN_LANGUAGE_MODE)) {
    case NO_AUTO_INDENT:
        ui.action_Default_Indent_Off->setChecked(true);
        break;
    case AUTO_INDENT:
        ui.action_Default_Indent_On->setChecked(true);
        break;
    case SMART_INDENT:
        ui.action_Default_Indent_Smart->setChecked(true);
        break;
    default:
        break;
    }

    // Default Wrap
    switch(GetPrefWrap(PLAIN_LANGUAGE_MODE)) {
    case NO_WRAP:
        ui.action_Default_Wrap_None->setChecked(true);
        break;
    case NEWLINE_WRAP:
        ui.action_Default_Wrap_Auto_Newline->setChecked(true);
        break;
    case CONTINUOUS_WRAP:
        ui.action_Default_Wrap_Continuous->setChecked(true);
        break;
    default:
        break;
    }

    if(GetPrefSmartTags()) {
        ui.action_Default_Tag_Smart->setChecked(true);
    } else {
        ui.action_Default_Tag_Show_All->setChecked(true);
    }

    // Default Search Settings
    ui.action_Default_Search_Verbose->setChecked(GetPrefSearchDlogs());
    ui.action_Default_Search_Wrap_Around->setChecked(GetPrefSearchWraps());
    ui.action_Default_Search_Beep_On_Search_Wrap->setChecked(GetPrefBeepOnSearchWrap());
    ui.action_Default_Search_Keep_Dialogs_Up->setChecked(GetPrefKeepSearchDlogs());

    switch(GetPrefSearch()) {
    case SEARCH_LITERAL:
        ui.action_Default_Search_Literal->setChecked(true);
        break;
    case SEARCH_CASE_SENSE:
        ui.action_Default_Search_Literal_Case_Sensitive->setChecked(true);
        break;
    case SEARCH_LITERAL_WORD:
        ui.action_Default_Search_Literal_Whole_Word->setChecked(true);
        break;
    case SEARCH_CASE_SENSE_WORD:
        ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word->setChecked(true);
        break;
    case SEARCH_REGEX:
        ui.action_Default_Search_Regular_Expression->setChecked(true);
        break;
    case SEARCH_REGEX_NOCASE:
        ui.action_Default_Search_Regular_Expresison_Case_Insensitive->setChecked(true);
        break;
    default:
        break;
    }

    // Default syntax
    if(GetPrefHighlightSyntax()) {
        ui.action_Default_Syntax_On->setChecked(true);
    } else {
        ui.action_Default_Syntax_Off->setChecked(true);
    }

    ui.action_Default_Apply_Backlighting->setChecked(GetPrefBacklightChars());

    // Default tab settings
    ui.action_Default_Tab_Open_File_In_New_Tab->setChecked(GetPrefOpenInTab());
    ui.action_Default_Tab_Show_Tab_Bar->setChecked(GetPrefTabBar());
    ui.action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open->setChecked(GetPrefTabBarHideOne());
    ui.action_Default_Tab_Next_Prev_Tabs_Across_Windows->setChecked(GetPrefGlobalTabNavigate());
    ui.action_Default_Tab_Sort_Tabs_Alphabetically->setChecked(GetPrefSortTabs());

    ui.tabWidget->tabBar()->setVisible(GetPrefTabBar());
    ui.tabWidget->setTabBarAutoHide(GetPrefTabBarHideOne());

    ui.action_Default_Show_Tooltips->setChecked(GetPrefToolTips());
    ui.action_Default_Statistics_Line->setChecked(GetPrefStatsLine());
    ui.action_Default_Incremental_Search_Line->setChecked(GetPrefISearchLine());
    ui.action_Default_Show_Line_Numbers->setChecked(GetPrefLineNums());
    ui.action_Default_Make_Backup_Copy->setChecked(GetPrefSaveOldVersion());
    ui.action_Default_Incremental_Backup->setChecked(GetPrefAutoSave());

    switch(GetPrefShowMatching()) {
    case NO_FLASH:
        ui.action_Default_Matching_Off->setChecked(true);
        break;
    case FLASH_DELIMIT:
        ui.action_Default_Matching_Delimiter->setChecked(true);
        break;
    case FLASH_RANGE:
        ui.action_Default_Matching_Range->setChecked(true);
        break;
    }

    ui.action_Default_Matching_Syntax_Based->setChecked(GetPrefMatchSyntaxBased());
    ui.action_Default_Terminate_with_Line_Break_on_Save->setChecked(GetPrefAppendLF());
    ui.action_Default_Popups_Under_Pointer->setChecked(GetPrefRepositionDialogs());
    ui.action_Default_Auto_Scroll_Near_Window_Top_Bottom->setChecked(GetPrefAutoScroll());
    ui.action_Default_Warnings_Files_Modified_Externally->setChecked(GetPrefWarnFileMods());
    ui.action_Default_Warnings_Check_Modified_File_Contents->setChecked(GetPrefWarnRealFileMods());
    ui.action_Default_Warnings_On_Exit->setChecked(GetPrefWarnExit());
    ui.action_Default_Warnings_Check_Modified_File_Contents->setEnabled(GetPrefWarnFileMods());

    updateWindowSizeMenu();
}

//------------------------------------------------------------------------------
// Name: setupMenuStrings
// Desc: nedit has some menu shortcuts which are different from conventional
//       shortcuts. Fortunately, Qt has a means to do this stuff manually.
//------------------------------------------------------------------------------
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
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_H), this, SLOT(action_Shift_Find_Selection_triggered()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I), this, SLOT(action_Shift_Find_Incremental_triggered()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_R), this, SLOT(action_Shift_Replace_triggered()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M), this, SLOT(action_Shift_Goto_Matching_triggered()));

    // This is an annoying solution... we can probably do better...
    // NOTE(eteran): this assumes that the Qt::Key constants are
    // in numerical order!
    for(int i = Qt::Key_A; i <= Qt::Key_Z; ++i) {
        new QShortcut(QKeySequence(Qt::ALT + Qt::Key_M, i),             this, SLOT(action_Mark_Shortcut_triggered()));
        new QShortcut(QKeySequence(Qt::ALT + Qt::Key_G, i),             this, SLOT(action_Goto_Mark_Shortcut_triggered()));
        new QShortcut(QKeySequence(Qt::SHIFT + Qt::ALT + Qt::Key_G, i), this, SLOT(action_Shift_Goto_Mark_Shortcut_triggered()));
    }

    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown), this, SLOT(action_Next_Document()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp), this, SLOT(action_Prev_Document()));
    new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Home), this, SLOT(action_Last_Document()));
}

//------------------------------------------------------------------------------
// Name: setupMenuAlternativeMenus
// Desc: under some configurations, menu items have different text/functionality
//------------------------------------------------------------------------------
void MainWindow::setupMenuAlternativeMenus() {
	if(!GetPrefOpenInTab()) {
		ui.action_New_Window->setText(tr("New &Tab"));
    } else {
        ui.action_New_Window->setText(tr("New &Window"));
    }

    if (GetPrefMaxPrevOpenFiles() <= 0) {
        ui.action_Open_Previous->setEnabled(false);
    }
}

//------------------------------------------------------------------------------
// Name: setupMenuGroups
//------------------------------------------------------------------------------
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

    connect(indentGroup,               SIGNAL(triggered(QAction *)), this, SLOT(indentGroupTriggered(QAction *)));
    connect(wrapGroup,                 SIGNAL(triggered(QAction *)), this, SLOT(wrapGroupTriggered(QAction *)));
    connect(matchingGroup,             SIGNAL(triggered(QAction *)), this, SLOT(matchingGroupTriggered(QAction *)));
    connect(defaultIndentGroup,        SIGNAL(triggered(QAction *)), this, SLOT(defaultIndentGroupTriggered(QAction *)));
    connect(defaultWrapGroup,          SIGNAL(triggered(QAction *)), this, SLOT(defaultWrapGroupTriggered(QAction *)));
    connect(defaultTagCollisionsGroup, SIGNAL(triggered(QAction *)), this, SLOT(defaultTagCollisionsGroupTriggered(QAction *)));
    connect(defaultSearchGroup,        SIGNAL(triggered(QAction *)), this, SLOT(defaultSearchGroupTriggered(QAction *)));
    connect(defaultSyntaxGroup,        SIGNAL(triggered(QAction *)), this, SLOT(defaultSyntaxGroupTriggered(QAction *)));
    connect(defaultMatchingGroup,      SIGNAL(triggered(QAction *)), this, SLOT(defaultMatchingGroupTriggered(QAction *)));
    connect(defaultSizeGroup,          SIGNAL(triggered(QAction *)), this, SLOT(defaultSizeGroupTriggered(QAction *)));

}

//------------------------------------------------------------------------------
// Name: deleteTabButtonClicked
//------------------------------------------------------------------------------
void MainWindow::deleteTabButtonClicked() {
    Q_EMIT on_action_Close_triggered();
}

//------------------------------------------------------------------------------
// Name: on_action_New_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_New_triggered() {
    action_New(QLatin1String("prefs"));
}

//------------------------------------------------------------------------------
// Name: on_action_New_triggered
//------------------------------------------------------------------------------
void MainWindow::action_New(const QString &mode) {

    bool openInTab = GetPrefOpenInTab();

	if (!mode.isEmpty()) {
		if (mode == QLatin1String("prefs")) {
			/* accept default */;
		} else if (mode == QLatin1String("tab")) {
			openInTab = true;
		} else if (mode == QLatin1String("window")) {
			openInTab = true;
		} else if (mode == QLatin1String("opposite")) {
			openInTab = !openInTab;
		} else {
			fprintf(stderr, "nedit: Unknown argument to action procedure \"new\": %s\n", qPrintable(mode));
		}
	}

	QString path;

    if(auto doc = currentDocument()) {
        path = doc->path_;
    }


    EditNewFileEx(openInTab ? this : nullptr, QString(), false, QString(), path);
    CheckCloseDimEx();
}

QString MainWindow::PromptForExistingFileEx(const QString &path, const QString &prompt) {
    QFileDialog dialog(this, prompt);
    dialog.setOptions(QFileDialog::DontUseNativeDialog);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if(!path.isEmpty()) {
        dialog.setDirectory(path);
    }

    if(dialog.exec()) {
        return dialog.selectedFiles()[0];
    }

    return QString();
}

//------------------------------------------------------------------------------
// Name: on_action_Open_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Open_triggered() {

    if(auto doc = currentDocument()) {
        QString filename = PromptForExistingFileEx(doc->path_, tr("Open File"));
        if (filename.isNull()) {
            return;
        }

        doc->open(filename.toLatin1().data());
    }

    CheckCloseDimEx();
}

//------------------------------------------------------------------------------
// Name: on_action_Close_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Close_triggered() {
    if(auto doc = currentDocument()) {
        doc->actionClose(QString());
    }
}

//------------------------------------------------------------------------------
// Name: on_action_About_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_About_triggered() {
    static auto dialog = new DialogAbout(this);
	dialog->exec();
}

//------------------------------------------------------------------------------
// Name: on_action_Select_All_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Select_All_triggered() {
	if(TextArea *w = lastFocus_) {
		w->selectAllAP();
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Include_File_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Include_File_triggered() {


    if(auto doc = currentDocument()) {

        if (doc->CheckReadOnly()) {
            return;
        }

        QString filename = PromptForExistingFileEx(doc->path_, tr("Include File"));

        if (filename.isNull()) {
            return;
        }

        doc->includeFile(filename);
    }
}

//------------------------------------------------------------------------------
// Name: on_action_Cut_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Cut_triggered() {
	if(TextArea *w = lastFocus_) {
		w->cutClipboardAP();
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Copy_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Copy_triggered() {
	if(TextArea *w = lastFocus_) {
		w->copyClipboardAP();
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Paste_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Paste_triggered() {
	if(TextArea *w = lastFocus_) {
		w->pasteClipboardAP();
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Paste_Column_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Paste_Column_triggered() {
    if(TextArea *w = lastFocus_) {
        w->pasteClipboardAP(TextArea::RectFlag);
    }
}

//------------------------------------------------------------------------------
// Name: on_action_Delete_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Delete_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        doc->buffer_->BufRemoveSelected();
    }
}

DocumentWidget *MainWindow::CreateDocument(QString name) {
	auto window = new DocumentWidget(name, this);
	int i = ui.tabWidget->addTab(window, name);
	ui.tabWidget->setCurrentIndex(i);
	return window;
}

void MainWindow::UpdateWindowTitle(DocumentWidget *doc) {

	QString clearCaseTag = GetClearCaseViewTag();

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

    // TODO(eteran): For version 2.0, consider using a disk icon instead of this "*"
    //               as this would be more conventional in modern program
    if (doc->fileChanged_) {
		iconTitle.append(tr("*"));
	}

    // NOTE(eteran): is the functional equivalent to "XmNiconName"?
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

    // TODO(eteran): if we are clever, we can probably make all windows
    // literally share the same QMenu object for this...

    // Make a sorted list of windows
    QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

    std::sort(documents.begin(), documents.end(), [](const DocumentWidget *a, const DocumentWidget *b) {

        // Untitled first
        int rc = a->filenameSet_ == b->filenameSet_ ? 0 : a->filenameSet_ && !b->filenameSet_ ? 1 : -1;
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
        QAction *action = ui.menu_Windows->addAction(title, this, SLOT(raiseCB()));
        action->setData(reinterpret_cast<qulonglong>(document));
    }
}

/*
** Update the read-only state of the text area(s) in the window, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void MainWindow::UpdateWindowReadOnly(DocumentWidget *doc) {

	bool state = doc->lockReasons_.isAnyLocked();

    QList<TextArea *> textAreas = doc->textPanes();
	for(TextArea *area : textAreas) {
		area->setReadOnly(state);
	}

	ui.action_Read_Only->setChecked(state);
	ui.action_Read_Only->setEnabled(!doc->lockReasons_.isAnyLockedIgnoringUser());
}



/*
** check if tab bar is to be shown on this window
*/
int MainWindow::TabCount() {
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
	const int nDoc = TabCount();
	if (nDoc < 2) {
		return;
	}

	// get a list of all the documents
	QVector<DocumentWidget *> windows;
	windows.reserve(nDoc);

	for(int i = 0; i < nDoc; ++i) {
		if(auto doc = qobject_cast<DocumentWidget *>(ui.tabWidget->widget(i))) {
			windows.push_back(doc);
		}
	}

	// sort them first by filename, then by path
	std::sort(windows.begin(), windows.end(), [](const DocumentWidget *a, const DocumentWidget *b) {
		if(a->filename_ < b->filename_) {
			return true;
		}
		return a->path_ < b->path_;
	});

	// shuffle around the tabs to their new indexes
	for(int i = 0; i < windows.size(); ++i) {
		int from = ui.tabWidget->indexOf(windows[i]);
		int to   = i;
        ui.tabWidget->tabBar()->moveTab(from, to);
	}
}

QList<MainWindow *> MainWindow::allWindows() {
    QList<MainWindow *> windows;

    for (QWidget *widget : QApplication::topLevelWidgets()) {
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
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if(auto window = qobject_cast<MainWindow *>(widget)) {
           return window;
        }
    }
    return nullptr;
}

QList<DocumentWidget *> MainWindow::openDocuments() const {
    QList<DocumentWidget *> list;
    for(int i = 0; i < ui.tabWidget->count(); ++i) {
        if(auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->widget(i))) {
            list.push_back(document);
        }
    }
    return list;
}



/*
** Make sure the close menu item is dimmed appropriately for the current
** set of windows.  It should be dim only for the last Untitled, unmodified,
** editor window, and sensitive otherwise.
*/
void MainWindow::CheckCloseDimEx() {

    QList<MainWindow *> windows = MainWindow::allWindows();

    if(windows.empty()) {
        // should never happen...
        return;
    }

    if(windows.size() == 1) {
        // NOTE(eteran): if there is only one window, then *this* must be it...
        // right?
        MainWindow *window = windows[0];

        QList<DocumentWidget *> documents = window->openDocuments();

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

void MainWindow::raiseCB() {
    if(const auto action = qobject_cast<QAction *>(sender())) {
        if(const auto ptr = reinterpret_cast<DocumentWidget *>(action->data().value<qulonglong>())) {
            ptr->RaiseFocusDocumentWindow(true);
        }
    }
}

/*
** Create either the variable Shell menu, Macro menu or Background menu
** items of "window" (driven by value of "menuType")
*/
QMenu *MainWindow::createUserMenu(DocumentWidget *document, const QVector<MenuData> &data) {

    auto rootMenu = new QMenu(this);
    for(int i = 0; i < data.size(); ++i) {
        const MenuData &menuData = data[i];

        bool found = menuData.info->umiNbrOfLanguageModes == 0;
        for(int language = 0; language < menuData.info->umiNbrOfLanguageModes; ++language) {
            if(menuData.info->umiLanguageMode[language] == document->languageMode_) {
                found = true;
            }
        }

        if(!found) {
            continue;
        }

        QMenu *parentMenu = rootMenu;
        QString name = QString::fromLatin1(menuData.info->umiName);

        int index = 0;
        for (;;) {
            int subSep = name.indexOf(QLatin1Char('>'), index);
            if(subSep == -1) {
                name = name.mid(index);

                // add the mnemonic to the string in the appropriate place
                int pos = name.indexOf(QLatin1Char(menuData.item->mnemonic));
                if(pos != -1) {
                    name.insert(pos, QLatin1String("&"));
                }

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
    for(QAction *action : menu->actions()) {
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

    // TODO(eteran): the old code used to only do this if the language mode changed
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
    connect(shellGroup, SIGNAL(triggered(QAction*)), this, SLOT(shellTriggered(QAction *)));

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
    connect(macroGroup, SIGNAL(triggered(QAction*)), this, SLOT(macroTriggered(QAction *)));

    /* update background menu, which is owned by a single document, only
       if language mode was changed */
    document->contextMenu_ = createUserMenu(document, BGMenuData);
}

/*
** Re-build the language mode sub-menu using the current data stored
** in the master list: LanguageModes.
*/
void MainWindow::updateLanguageModeSubmenu() {

    auto languageGroup = new QActionGroup(this);
    auto languageMenu = new QMenu(this);
    QAction *action = languageMenu->addAction(tr("Plain"));
    action->setData(PLAIN_LANGUAGE_MODE);
    action->setCheckable(true);
    action->setChecked(true);
    languageGroup->addAction(action);


    for (int i = 0; i < NLanguageModes; i++) {
        QAction *action = languageMenu->addAction(LanguageModes[i]->name);
        action->setData(i);
        action->setCheckable(true);
        languageGroup->addAction(action);
    }

    ui.action_Language_Mode->setMenu(languageMenu);
    connect(languageMenu, SIGNAL(triggered(QAction *)), this, SLOT(setLangModeCB(QAction *)));
}

/*
** Create a submenu for chosing language mode for the current window.
*/
void MainWindow::CreateLanguageModeSubMenu() {
    updateLanguageModeSubmenu();
}

void MainWindow::setLangModeCB(QAction *action) {

    if(auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget())) {
        const int mode = action->data().value<int>();

        // If the mode didn't change, do nothing
        if (document->languageMode_ == mode) {
            return;
        }

        if(mode == PLAIN_LANGUAGE_MODE) {
            document->setLanguageMode(QString());
        } else {
            document->setLanguageMode(LanguageModes[mode]->name);
        }
    }
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
**
**  (Iteration taken from TabCount(); is there a better way to do it?)
*/
int MainWindow::updateGutterWidth() {

    int reqCols = MIN_LINE_NUM_COLS;
    int maxCols = 0;

    QList<DocumentWidget *> documents  = openDocuments();
    for(DocumentWidget *document : documents) {
        //  We found ourselves a document from this window. Pick a TextArea
        // (doesn't matter which), so we can probe some details...
        if(TextArea *area = document->firstPane()) {
            const int lineNumCols = area->getLineNumCols();

            /* Is the width of the line number area sufficient to display all the
               line numbers in the file?  If not, expand line number field, and the
               this width. */
            if (lineNumCols > maxCols) {
                maxCols = lineNumCols;
            }

            int tmpReqCols = area->getBufferLinesCount() < 1 ? 1 : static_cast<int>(log10(static_cast<double>(area->getBufferLinesCount()) + 1)) + 1;

            if (tmpReqCols > reqCols) {
                reqCols = tmpReqCols;
            }
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

        UpdateWMSizeHints();
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

        QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

        auto it = std::find_if(documents.begin(), documents.end(), [name](DocumentWidget *window) {
            return window->filename_ == name;
        });

        if(it == documents.end()) {
            return name;
        }
    }

    return QString();
}

void MainWindow::on_action_Undo_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        doc->Undo();
    }
}

void MainWindow::on_action_Redo_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        doc->Redo();
    }
}

/*
** Check if there is already a window open for a given file
*/
DocumentWidget *MainWindow::FindWindowWithFile(const QString &name, const QString &path) {

    QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

    /* I don't think this algorithm will work on vms so I am disabling it for now */
    if (!GetPrefHonorSymlinks()) {

        QString fullname = QString(QLatin1String("%1%2")).arg(path, name);

        struct stat attribute;
        if (::stat(fullname.toLatin1().data(), &attribute) == 0) {
            auto it = std::find_if(documents.begin(), documents.end(), [attribute](DocumentWidget *document){
                return attribute.st_dev == document->device_ && attribute.st_ino == document->inode_;
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
    bool showLineNum = showLineNumbers_;
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
    } else {
        // NOTE(eteran): here reqCols is 0, so the code at the bottom of this
        // branch seems redundant to the code at the very bottom of this branch
#if 0
        Dimension windowWidth;
        XtVaGetValues(shell_, XmNwidth, &windowWidth, nullptr);
        int marginWidth = textD_of(textArea_)->getMarginWidth();
        XtVaSetValues(shell_, XmNwidth, windowWidth - textD->getRect().left + marginWidth, nullptr);
#endif

        if(DocumentWidget *doc = currentDocument()) {
            QList<TextArea *> areas = doc->textPanes();
            for(TextArea *area : areas) {
                area->setLineNumCols(0);
            }
        }
    }

    /* line numbers panel is shell-level, hence other
       tabbed documents in the this should synch */
    QList<DocumentWidget *> docs = openDocuments();
    for(DocumentWidget *doc : docs) {

        // TODO(eteran): seems redundant now...
        showLineNumbers_ = state;

        QList<TextArea *> areas = doc->textPanes();
        for(TextArea *area : areas) {
            //  reqCols should really be cast here, but into what? XmRInt?
            area->setLineNumCols(reqCols);
        }

    }
#if 0
    // Tell WM that the non-expandable part of the this has changed size
    UpdateWMSizeHints();
#endif
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

    // Add it to the list
    PrevOpen.push_front(filename);

    // Mark the Previously Opened Files menu as invalid in all windows
    invalidatePrevOpenMenus();

    // Undim the menu in all windows if it was previously empty
    if (PrevOpen.size() > 0) {
        QList<MainWindow *> windows = allWindows();
        for(MainWindow *window : windows) {
            window->ui.action_Open_Previous->setEnabled(true);
        }
    }

    // Write the menu contents to disk to restore in later sessions
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

    char line[MAXPATHLEN + 2];
    struct stat attribute;
    FILE *fp;
    size_t lineLen;
    static time_t lastNeditdbModTime = 0;

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

    /*  Stat history file to see whether someone touched it after this
        session last changed it.  */
    if (stat(fullName.toLatin1().data(), &attribute) == 0) {
        if (lastNeditdbModTime >= attribute.st_mtime) {
            //  Do nothing, history file is unchanged.
            return;
        } else {
            //  Memorize modtime to compare to next time.
            lastNeditdbModTime = attribute.st_mtime;
        }
    } else {
        //  stat() failed, probably for non-exiting history database.
        if (ENOENT != errno) {
            perror("nedit: Error reading history database");
        }
        return;
    }

    // open the file
    if ((fp = fopen(fullName.toLatin1().data(), "r")) == nullptr) {
        return;
    }

    //  Clear previous list.
    PrevOpen.clear();

    /* read lines of the file, lines beginning with # are considered to be
       comments and are thrown away.  Lines are subject to cursory checking,
       then just copied to the Open Previous file menu list */
    while (true) {
        if (fgets(line, sizeof(line), fp) == nullptr) {
            // end of file
            fclose(fp);
            return;
        }
        if (line[0] == '#') {
            // comment
            continue;
        }
        lineLen = strlen(line);
        if (lineLen == 0) {
            // blank line
            continue;
        }
        if (line[lineLen - 1] != '\n') {
            // no newline, probably truncated
            fprintf(stderr, "nedit: Line too long in history file\n");
            while (fgets(line, sizeof(line), fp)) {
                lineLen = strlen(line);
                if (lineLen > 0 && line[lineLen - 1] == '\n') {
                    break;
                }
            }
            continue;
        }
        line[--lineLen] = '\0';
        if (strcspn(line, neditDBBadFilenameChars) != lineLen) {
            // non-filename characters
            fprintf(stderr, "nedit: History file may be corrupted\n");
            continue;
        }

        auto nameCopy = QString::fromLatin1(line);
        PrevOpen.push_back(nameCopy);

        if (PrevOpen.size() >= GetPrefMaxPrevOpenFiles()) {
            // too many entries
            fclose(fp);
            return;
        }
    }

    // NOTE(eteran): fixes resource leak
    fclose(fp);
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
    QList<MainWindow *> windows = allWindows();
    for(MainWindow *window : windows) {
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
        for(const QString &line : PrevOpen) {
            const size_t lineLen = line.size();

            if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')) && strcspn(line.toLatin1().data(), neditDBBadFilenameChars) == lineLen) {
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
        delete ui.action_Open_Previous;
        return;
    }

    //  Read history file to get entries written by other sessions.
    ReadNEditDB();

    // Sort the previously opened file list if requested
    QList<QString> prevOpenSorted = PrevOpen;
    if (GetPrefSortOpenPrevMenu()) {
        qSort(prevOpenSorted);
    }

    delete ui.action_Open_Previous->menu();

    auto prevOpenMenu = new QMenu(this);

    for(const QString &item :prevOpenSorted) {
        QAction *action = prevOpenMenu->addAction(item);
        action->setData(item);
    }

    connect(prevOpenMenu, SIGNAL(triggered(QAction *)), this, SLOT(openPrevCB(QAction *)));

    ui.action_Open_Previous->setMenu(prevOpenMenu);
}

void MainWindow::openPrevCB(QAction *action) {

    QString filename = action->data().toString();

    if(auto doc = currentDocument()) {
        if (filename.isNull()) {
            return;
        }

        doc->open(filename.toLatin1().data());
    }

    CheckCloseDimEx();
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if(auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->widget(index))) {
        document->documentRaised();
    }
}

void MainWindow::on_tabWidget_customContextMenuRequested(int index, const QPoint &pos) {

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

    if(QAction *const selected = menu->exec(mapToGlobal(pos))) {

        if(DocumentWidget *document = documentAt(index)) {

            if(selected == newTab) {
                MainWindow::EditNewFileEx(this, QString(), false, QString(), document->path_);
            } else if(selected == closeTab) {
                document->actionClose(QString());
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

void MainWindow::on_action_Open_Selected_triggered() {

    if(auto doc = currentDocument()) {

        // Get the selected text, if there's no selection, do nothing
        const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
        if(mimeData->hasText()) {
            fileCB(doc, mimeData->text().toStdString());
        } else {
            QApplication::beep();
        }
    }

    CheckCloseDimEx();
}

//------------------------------------------------------------------------------
// Name: fileCB
// Desc: opens a "selected file"
//------------------------------------------------------------------------------
void MainWindow::fileCB(DocumentWidget *window, const std::string &text) {

    char nameText[MAXPATHLEN];
    char includeName[MAXPATHLEN];
    char filename[MAXPATHLEN];
    char pathname[MAXPATHLEN];
    char *inPtr;
    char *outPtr;

    static const char includeDir[] = "/usr/include/";

    /* get the string, or skip if we can't get the selection data, or it's
       obviously not a file name */
    if (text.size() > MAXPATHLEN || text.empty()) {
        QApplication::beep();
        return;
    }

    snprintf(nameText, sizeof(nameText), "%s", text.c_str());

    // extract name from #include syntax
    if (sscanf(nameText, "#include \"%[^\"]\"", includeName) == 1) {
        strcpy(nameText, includeName);
    } else if (sscanf(nameText, "#include <%[^<>]>", includeName) == 1) {
        sprintf(nameText, "%s%s", includeDir, includeName);
    }

    // strip whitespace from name
    for (inPtr = nameText, outPtr = nameText; *inPtr != '\0'; inPtr++) {
        if (*inPtr != ' ' && *inPtr != '\t' && *inPtr != '\n') {
            *outPtr++ = *inPtr;
        }
    }
    *outPtr = '\0';

    // Process ~ characters in name
    ExpandTilde(nameText);

    // If path name is relative, make it refer to current window's directory
    if (nameText[0] != '/') {
        snprintf(filename, sizeof(filename), "%s%s", window->path_.toLatin1().data(), nameText);
        strcpy(nameText, filename);
    }

    // Expand wildcards in file name.
    {
        glob_t globbuf;
        glob(nameText, GLOB_NOCHECK, nullptr, &globbuf);
        for (size_t i = 0; i < globbuf.gl_pathc; i++) {
            if (ParseFilename(globbuf.gl_pathv[i], filename, pathname) != 0) {
                QApplication::beep();
            } else {
                DocumentWidget::EditExistingFileEx(
                            GetPrefOpenInTab() ? window : nullptr,
                            QString::fromLatin1(filename),
                            QString::fromLatin1(pathname),
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

    CheckCloseDimEx();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Shift_Left_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        ShiftSelectionEx(doc, lastFocus_, SHIFT_LEFT, false);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Shift_Right_triggered() {

    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        ShiftSelectionEx(doc, lastFocus_, SHIFT_RIGHT, false);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Left_Tabs() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        ShiftSelectionEx(doc, lastFocus_, SHIFT_LEFT, true);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Right_Tabs() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        ShiftSelectionEx(doc, lastFocus_, SHIFT_RIGHT, true);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Lower_case_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        DowncaseSelectionEx(doc, lastFocus_);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Upper_case_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        UpcaseSelectionEx(doc, lastFocus_);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Fill_Paragraph_triggered() {

    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }
        FillSelectionEx(doc, lastFocus_);
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Insert_Form_Feed_triggered() {

    if(TextArea *w = lastFocus_) {
        w->insertStringAP(QLatin1String("\f"));
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Insert_Ctrl_Code_triggered() {

    if(auto doc = currentDocument()) {

        if (doc->CheckReadOnly()) {
            return;
        }

        bool ok;
        int n = QInputDialog::getInt(this, tr("Insert Ctrl Code"), tr("ASCII Character Code:"), 0, 0, 255, 1, &ok);
        if(ok) {
            char charCodeString[2];
            charCodeString[0] = static_cast<uint8_t>(n);
            charCodeString[1] = '\0';

            if (!doc->buffer_->BufSubstituteNullChars(charCodeString, 1)) {
                QMessageBox::critical(this, tr("Error"), tr("Too much binary data"));
                return;
            }

            if(TextArea *w = lastFocus_) {
                w->insertStringAP(QString::fromLatin1(charCodeString));
            }
        }
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Goto_Line_Number_triggered() {
    if(auto doc = currentDocument()) {

        int lineNum;
        int column;

        bool ok;
        QString text = QInputDialog::getText(this, tr("Goto Line Number"), tr("Goto Line (and/or Column)  Number:"), QLineEdit::Normal, QString(), &ok);

        if (!ok) {
            return;
        }

        if (StringToLineAndCol(text.toLatin1().data(), &lineNum, &column) == -1) {
            QApplication::beep();
            return;
        }

        if(TextArea *w = lastFocus_) {
            doc->gotoAP(w, QStringList() << text);
        }
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Goto_Selected_triggered() {
    if(auto doc = currentDocument()) {

        const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
        if(mimeData->hasText()) {
            if(TextArea *w = lastFocus_) {
                doc->gotoAP(w, QStringList() << mimeData->text());
            }
        } else {
            QApplication::beep();
        }
    }
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Find_triggered() {
    DoFindDlogEx(
        this,
        currentDocument(),
        SEARCH_FORWARD,
        GetPrefKeepSearchDlogs(),
        GetPrefSearch());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Find() {
    DoFindDlogEx(
        this,
        currentDocument(),
        SEARCH_BACKWARD,
        GetPrefKeepSearchDlogs(),
        GetPrefSearch());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Find_Again_triggered() {

    SearchAndSelectSameEx(
        this,
        currentDocument(),
        lastFocus_,
        SEARCH_FORWARD,
        GetPrefSearchWraps());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Find_Again() {
    SearchAndSelectSameEx(
        this,
        currentDocument(),
        lastFocus_,
        SEARCH_BACKWARD,
        GetPrefSearchWraps());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Find_Selection_triggered() {

    SearchForSelectedEx(
        this,
        currentDocument(),
        lastFocus_,
        SEARCH_FORWARD,
        GetPrefSearch(),
        GetPrefSearchWraps());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Find_Selection_triggered() {
    SearchForSelectedEx(
        this,
        currentDocument(),
        lastFocus_,
        SEARCH_BACKWARD,
        GetPrefSearch(),
        GetPrefSearchWraps());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Find_Incremental_triggered() {
    BeginISearchEx(SEARCH_FORWARD);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Find_Incremental_triggered() {
    BeginISearchEx(SEARCH_BACKWARD);
}

//------------------------------------------------------------------------------
// Name: on_editIFind_textChanged
//------------------------------------------------------------------------------
/*
** Called when user types in the incremental search line.  Redoes the
** search for the new search string.
*/
void MainWindow::on_editIFind_textChanged(const QString &searchString) {

    SearchType searchType;

    if(ui.checkIFindCase->isChecked()) {
        if (ui.checkIFindRegex->isChecked()) {
            searchType = SEARCH_REGEX;
        } else {
            searchType = SEARCH_CASE_SENSE;
        }
    } else {
        if (ui.checkIFindRegex->isChecked()) {
            searchType = SEARCH_REGEX_NOCASE;
        } else {
            searchType = SEARCH_LITERAL;
        }
    }

    const SearchDirection direction = ui.checkIFindReverse->isChecked() ? SEARCH_BACKWARD : SEARCH_FORWARD;

    /* If the search type is a regular expression, test compile it.  If it
       fails, silently skip it.  (This allows users to compose the expression
       in peace when they have unfinished syntax, but still get beeps when
       correct syntax doesn't match) */
    if (isRegexType(searchType)) {
        try {
            auto compiledRE = std::make_unique<regexp>(searchString.toStdString(), defaultRegexFlags(searchType));
        } catch(const regex_error &) {
            return;
        }
    }

    /* Call the incremental search action proc to do the searching and
       selecting (this allows it to be recorded for learn/replay).  If
       there's an incremental search already in progress, mark the operation
       as "continued" so the search routine knows to re-start the search
       from the original starting position */
    if(auto doc = currentDocument()) {
        doc->findIncrAP(searchString, direction, searchType, GetPrefSearchWraps(), iSearchStartPos_ != -1);
    }
}

//------------------------------------------------------------------------------
// Name: on_buttonIFind_clicked
//------------------------------------------------------------------------------
void MainWindow::on_buttonIFind_clicked() {
    // NOTE(eteran): same as pressing return
    Q_EMIT on_editIFind_returnPressed();
}

//------------------------------------------------------------------------------
// Name: on_editIFind_returnPressed
// Desc: User pressed return in the incremental search bar.  Do a new search with
//       the search string displayed.  The direction of the search is toggled if
//       the Ctrl key or the Shift key is pressed when the text field is activated.
//------------------------------------------------------------------------------
void MainWindow::on_editIFind_returnPressed() {

    SearchType searchType;

    /* Fetch the string, search type and direction from the incremental
       search bar widgets at the top of the window */
    QString searchString = ui.editIFind->text();

    if (ui.checkIFindCase->isChecked()) {
        if (ui.checkIFindRegex->isChecked())
            searchType = SEARCH_REGEX;
        else
            searchType = SEARCH_CASE_SENSE;
    } else {
        if (ui.checkIFindRegex->isChecked())
            searchType = SEARCH_REGEX_NOCASE;
        else
            searchType = SEARCH_LITERAL;
    }

    SearchDirection direction = ui.checkIFindReverse->isChecked() ? SEARCH_BACKWARD : SEARCH_FORWARD;

    // Reverse the search direction if the Ctrl or Shift key was pressed
    if(QApplication::keyboardModifiers() & (Qt::CTRL | Qt::SHIFT)) {
        direction = (direction == SEARCH_FORWARD) ? SEARCH_BACKWARD : SEARCH_FORWARD;
    }

    // find the text and mark it
    if(auto doc = currentDocument()) {
        doc->findAP(searchString, direction, searchType, GetPrefSearchWraps());
    }
}

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
        searchStr  = SearchHistory[historyIndex(index)];
        searchType = SearchTypeHistory[historyIndex(index)];
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
    Q_EMIT on_editIFind_returnPressed();
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
    // TODO(eteran): original nedit seems to have code to do this @search.c:3071
    //               but in practice, toggling the case/regex often doesn't seem
    //               to change the search
    Q_EMIT on_editIFind_returnPressed();
}

void MainWindow::on_checkIFindReverse_toggled(bool value) {

    Q_UNUSED(value);

    // When search parameters (direction or search type), redo the search
    Q_EMIT on_editIFind_returnPressed();
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
    case SEARCH_LITERAL:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(false);
        break;
    case SEARCH_CASE_SENSE:
        iSearchLastLiteralCase_ = true;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(true);
        break;
    case SEARCH_LITERAL_WORD:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(false);
        break;
    case SEARCH_CASE_SENSE_WORD:
        iSearchLastLiteralCase_ = true;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(false);
        ui.checkIFindCase->setChecked(true);
        break;
    case SEARCH_REGEX:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = true;
        ui.checkIFindRegex->setChecked(true);
        ui.checkIFindCase->setChecked(true);
        break;
    case SEARCH_REGEX_NOCASE:
        iSearchLastLiteralCase_ = false;
        iSearchLastRegexCase_   = false;
        ui.checkIFindRegex->setChecked(true);
        ui.checkIFindCase->setChecked(false);
        break;
    default:
        Q_ASSERT(0);
    }
}

/*
** Pop up and clear the incremental search line and prepare to search.
*/
void MainWindow::BeginISearchEx(SearchDirection direction) {

    iSearchStartPos_ = -1;
    ui.editIFind->setText(QLatin1String(""));
    no_signals(ui.checkIFindReverse)->setChecked(direction == SEARCH_BACKWARD);


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
    saveSearchHistory(QLatin1String(""), QString(), SEARCH_LITERAL, false);

    // Pop down the search line (if it's not pegged up in Preferences)
    TempShowISearch(false);
}

void MainWindow::on_action_Replace_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        DoFindReplaceDlogEx(
                    this,
                    currentDocument(),
                    lastFocus_,
                    SEARCH_FORWARD,
                    GetPrefKeepSearchDlogs(),
                    GetPrefSearch());
    }
}

void MainWindow::action_Shift_Replace_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        DoFindReplaceDlogEx(
                    this,
                    currentDocument(),
                    lastFocus_,
                    SEARCH_BACKWARD,
                    GetPrefKeepSearchDlogs(),
                    GetPrefSearch());
    }
}

void MainWindow::on_action_Replace_Find_Again_triggered() {

    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        ReplaceFindSameEx(
                    this,
                    currentDocument(),
                    lastFocus_,
                    SEARCH_FORWARD,
                    GetPrefSearchWraps());
    }
}

void MainWindow::action_Shift_Replace_Find_Again_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        ReplaceFindSameEx(
                    this,
                    currentDocument(),
                    lastFocus_,
                    SEARCH_BACKWARD,
                    GetPrefSearchWraps());
    }
}

void MainWindow::on_action_Replace_Again_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        ReplaceSameEx(
                    this,
                    currentDocument(),
                    lastFocus_,
                    SEARCH_FORWARD,
                    GetPrefSearchWraps());
    }
}

void MainWindow::action_Shift_Replace_Again_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        ReplaceSameEx(
                    this,
                    currentDocument(),
                    lastFocus_,
                    SEARCH_BACKWARD,
                    GetPrefSearchWraps());
    }
}

void MainWindow::on_action_Mark_triggered() {

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

    if (result.size() != 1 || !isalpha(static_cast<uint8_t>(result[0].toLatin1()))) {
        QApplication::beep();
        return;
    }

    if(auto doc = currentDocument()) {
        doc->markAP(result[0]);
    }
}

void MainWindow::action_Mark_Shortcut_triggered() {

    if(auto shortcut = qobject_cast<QShortcut *>(sender())) {
        QKeySequence sequence = shortcut->key();

        if(auto doc = currentDocument()) {
            switch(sequence[1]) {
            case Qt::Key_A: doc->markAP(QLatin1Char('A')); break;
            case Qt::Key_B: doc->markAP(QLatin1Char('B')); break;
            case Qt::Key_C: doc->markAP(QLatin1Char('C')); break;
            case Qt::Key_D: doc->markAP(QLatin1Char('D')); break;
            case Qt::Key_E: doc->markAP(QLatin1Char('E')); break;
            case Qt::Key_F: doc->markAP(QLatin1Char('F')); break;
            case Qt::Key_G: doc->markAP(QLatin1Char('G')); break;
            case Qt::Key_H: doc->markAP(QLatin1Char('H')); break;
            case Qt::Key_I: doc->markAP(QLatin1Char('I')); break;
            case Qt::Key_J: doc->markAP(QLatin1Char('J')); break;
            case Qt::Key_K: doc->markAP(QLatin1Char('K')); break;
            case Qt::Key_L: doc->markAP(QLatin1Char('L')); break;
            case Qt::Key_M: doc->markAP(QLatin1Char('M')); break;
            case Qt::Key_N: doc->markAP(QLatin1Char('N')); break;
            case Qt::Key_O: doc->markAP(QLatin1Char('O')); break;
            case Qt::Key_P: doc->markAP(QLatin1Char('P')); break;
            case Qt::Key_Q: doc->markAP(QLatin1Char('Q')); break;
            case Qt::Key_R: doc->markAP(QLatin1Char('R')); break;
            case Qt::Key_S: doc->markAP(QLatin1Char('S')); break;
            case Qt::Key_T: doc->markAP(QLatin1Char('T')); break;
            case Qt::Key_U: doc->markAP(QLatin1Char('U')); break;
            case Qt::Key_V: doc->markAP(QLatin1Char('V')); break;
            case Qt::Key_W: doc->markAP(QLatin1Char('W')); break;
            case Qt::Key_X: doc->markAP(QLatin1Char('X')); break;
            case Qt::Key_Y: doc->markAP(QLatin1Char('Y')); break;
            case Qt::Key_Z: doc->markAP(QLatin1Char('Z')); break;
            default:
                QApplication::beep();
                break;
            }
        }
    }
}

void MainWindow::on_action_Goto_Mark_triggered() {

    bool extend = (QApplication::keyboardModifiers() & Qt::SHIFT);

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

    if (result.size() != 1 || !isalpha(static_cast<uint8_t>(result[0].toLatin1()))) {
        QApplication::beep();
        return;
    }

    if(auto doc = currentDocument()) {
        doc->gotoMarkAP(result[0], extend);
    }
}


void MainWindow::action_Shift_Goto_Mark_Shortcut_triggered() {
    if(auto shortcut = qobject_cast<QShortcut *>(sender())) {
        QKeySequence sequence = shortcut->key();

        if(auto doc = currentDocument()) {
            switch(sequence[1]) {
            case Qt::Key_A: doc->gotoMarkAP(QLatin1Char('A'), true); break;
            case Qt::Key_B: doc->gotoMarkAP(QLatin1Char('B'), true); break;
            case Qt::Key_C: doc->gotoMarkAP(QLatin1Char('C'), true); break;
            case Qt::Key_D: doc->gotoMarkAP(QLatin1Char('D'), true); break;
            case Qt::Key_E: doc->gotoMarkAP(QLatin1Char('E'), true); break;
            case Qt::Key_F: doc->gotoMarkAP(QLatin1Char('F'), true); break;
            case Qt::Key_G: doc->gotoMarkAP(QLatin1Char('G'), true); break;
            case Qt::Key_H: doc->gotoMarkAP(QLatin1Char('H'), true); break;
            case Qt::Key_I: doc->gotoMarkAP(QLatin1Char('I'), true); break;
            case Qt::Key_J: doc->gotoMarkAP(QLatin1Char('J'), true); break;
            case Qt::Key_K: doc->gotoMarkAP(QLatin1Char('K'), true); break;
            case Qt::Key_L: doc->gotoMarkAP(QLatin1Char('L'), true); break;
            case Qt::Key_M: doc->gotoMarkAP(QLatin1Char('M'), true); break;
            case Qt::Key_N: doc->gotoMarkAP(QLatin1Char('N'), true); break;
            case Qt::Key_O: doc->gotoMarkAP(QLatin1Char('O'), true); break;
            case Qt::Key_P: doc->gotoMarkAP(QLatin1Char('P'), true); break;
            case Qt::Key_Q: doc->gotoMarkAP(QLatin1Char('Q'), true); break;
            case Qt::Key_R: doc->gotoMarkAP(QLatin1Char('R'), true); break;
            case Qt::Key_S: doc->gotoMarkAP(QLatin1Char('S'), true); break;
            case Qt::Key_T: doc->gotoMarkAP(QLatin1Char('T'), true); break;
            case Qt::Key_U: doc->gotoMarkAP(QLatin1Char('U'), true); break;
            case Qt::Key_V: doc->gotoMarkAP(QLatin1Char('V'), true); break;
            case Qt::Key_W: doc->gotoMarkAP(QLatin1Char('W'), true); break;
            case Qt::Key_X: doc->gotoMarkAP(QLatin1Char('X'), true); break;
            case Qt::Key_Y: doc->gotoMarkAP(QLatin1Char('Y'), true); break;
            case Qt::Key_Z: doc->gotoMarkAP(QLatin1Char('Z'), true); break;
            default:
                QApplication::beep();
                break;
            }
        }
    }
}

void MainWindow::action_Goto_Mark_Shortcut_triggered() {
    if(auto shortcut = qobject_cast<QShortcut *>(sender())) {
        QKeySequence sequence = shortcut->key();

        if(auto doc = currentDocument()) {
            switch(sequence[1]) {
            case Qt::Key_A: doc->gotoMarkAP(QLatin1Char('A'), false); break;
            case Qt::Key_B: doc->gotoMarkAP(QLatin1Char('B'), false); break;
            case Qt::Key_C: doc->gotoMarkAP(QLatin1Char('C'), false); break;
            case Qt::Key_D: doc->gotoMarkAP(QLatin1Char('D'), false); break;
            case Qt::Key_E: doc->gotoMarkAP(QLatin1Char('E'), false); break;
            case Qt::Key_F: doc->gotoMarkAP(QLatin1Char('F'), false); break;
            case Qt::Key_G: doc->gotoMarkAP(QLatin1Char('G'), false); break;
            case Qt::Key_H: doc->gotoMarkAP(QLatin1Char('H'), false); break;
            case Qt::Key_I: doc->gotoMarkAP(QLatin1Char('I'), false); break;
            case Qt::Key_J: doc->gotoMarkAP(QLatin1Char('J'), false); break;
            case Qt::Key_K: doc->gotoMarkAP(QLatin1Char('K'), false); break;
            case Qt::Key_L: doc->gotoMarkAP(QLatin1Char('L'), false); break;
            case Qt::Key_M: doc->gotoMarkAP(QLatin1Char('M'), false); break;
            case Qt::Key_N: doc->gotoMarkAP(QLatin1Char('N'), false); break;
            case Qt::Key_O: doc->gotoMarkAP(QLatin1Char('O'), false); break;
            case Qt::Key_P: doc->gotoMarkAP(QLatin1Char('P'), false); break;
            case Qt::Key_Q: doc->gotoMarkAP(QLatin1Char('Q'), false); break;
            case Qt::Key_R: doc->gotoMarkAP(QLatin1Char('R'), false); break;
            case Qt::Key_S: doc->gotoMarkAP(QLatin1Char('S'), false); break;
            case Qt::Key_T: doc->gotoMarkAP(QLatin1Char('T'), false); break;
            case Qt::Key_U: doc->gotoMarkAP(QLatin1Char('U'), false); break;
            case Qt::Key_V: doc->gotoMarkAP(QLatin1Char('V'), false); break;
            case Qt::Key_W: doc->gotoMarkAP(QLatin1Char('W'), false); break;
            case Qt::Key_X: doc->gotoMarkAP(QLatin1Char('X'), false); break;
            case Qt::Key_Y: doc->gotoMarkAP(QLatin1Char('Y'), false); break;
            case Qt::Key_Z: doc->gotoMarkAP(QLatin1Char('Z'), false); break;
            default:
                QApplication::beep();
                break;
            }
        }
    }
}

void MainWindow::on_action_Goto_Matching_triggered() {
    if(auto doc = currentDocument()) {
        doc->GotoMatchingCharacter(lastFocus_);
    }
}

void MainWindow::action_Shift_Goto_Matching_triggered() {
    if(auto doc = currentDocument()) {
        doc->SelectToMatchingCharacter(lastFocus_);
    }
}

void MainWindow::updateTipsFileMenuEx() {

    auto tipsMenu = new QMenu(this);

    const tagFile *tf = TipsFileList;
    while(tf) {

        auto filename = QString::fromStdString(tf->filename);
        QAction *action = tipsMenu->addAction(filename);
        action->setData(filename);

        tf = tf->next;
    }

    ui.action_Unload_Calltips_File->setMenu(tipsMenu);
    connect(tipsMenu, SIGNAL(triggered(QAction *)), this, SLOT(unloadTipsFileCB(QAction *)));
}

void MainWindow::updateTagsFileMenuEx() {
    auto tagsMenu = new QMenu(this);

    const tagFile *tf = TagsFileList;
    while(tf) {

        auto filename = QString::fromStdString(tf->filename);
        QAction *action = tagsMenu->addAction(filename);
        action->setData(filename);

        tf = tf->next;
    }

    ui.action_Unload_Tags_File->setMenu(tagsMenu);
    connect(tagsMenu, SIGNAL(triggered(QAction *)), this, SLOT(unloadTagsFileCB(QAction *)));
}

void MainWindow::unloadTipsFileCB(QAction *action) {
    const QString filename = action->data().value<QString>();
    if(!filename.isEmpty()) {
        unloadTipsAP(filename);
    }
}

void MainWindow::unloadTagsFileCB(QAction *action) {
    const QString filename = action->data().value<QString>();
    if(!filename.isEmpty()) {
        unloadTagsAP(filename);
    }
}

void MainWindow::unloadTipsAP(const QString &filename) {

    if (DeleteTagsFile(filename.toLatin1().data(), TIP, true)) {

        for(MainWindow *window : MainWindow::allWindows()) {
            window->updateTipsFileMenuEx();
        }
    }
}

void MainWindow::unloadTagsAP(const QString &filename) {

    if (DeleteTagsFile(filename.toLatin1().data(), TAG, true)) {

        for(MainWindow *window : MainWindow::allWindows()) {
             window->updateTagsFileMenuEx();
        }
    }
}


void MainWindow::loadTipsAP(const QString &filename) {

    if (!AddTagsFileEx(filename, TIP)) {
        QMessageBox::warning(this, tr("Error Reading File"), tr("Error reading tips file:\n'%1'\ntips not loaded").arg(filename));
    }
}

void MainWindow::loadTagsAP(const QString &filename) {

    if (!AddTagsFileEx(filename, TAG)) {
        QMessageBox::warning(this, tr("Error Reading File"), tr("Error reading ctags file:\n'%1'\ntags not loaded").arg(filename));
    }
}

void MainWindow::on_action_Load_Calltips_File_triggered() {

    if(auto doc = currentDocument()) {
        QString filename = PromptForExistingFileEx(doc->path_, tr("Load Calltips File"));
        if (filename.isNull()) {
            return;
        }

        loadTipsAP(filename);
        updateTipsFileMenuEx();
    }
}

void MainWindow::on_action_Load_Tags_File_triggered() {

    if(auto doc = currentDocument()) {
        QString filename = PromptForExistingFileEx(doc->path_, tr("Load Tags File"));
        if (filename.isNull()) {
            return;
        }

        loadTagsAP(filename);
        updateTagsFileMenuEx();
    }
}

void MainWindow::on_action_Load_Macro_File_triggered() {
    if(auto doc = currentDocument()) {
        QString filename = PromptForExistingFileEx(doc->path_, tr("Load Macro File"));
        if (filename.isNull()) {
            return;
        }

        loadMacroAP(filename);
    }
}

void MainWindow::loadMacroAP(const QString &filename) {
    if(auto doc = currentDocument()) {
        ReadMacroFileEx(doc, filename.toStdString(), true);
    }
}

void MainWindow::on_action_Show_Calltip_triggered() {
    if(auto doc = currentDocument()) {
        doc->FindDefCalltip(lastFocus_, nullptr); // TODO(eteran): there was an optional arg?
    }
}

void MainWindow::on_action_Find_Definition_triggered() {
    if(auto doc = currentDocument()) {
        doc->FindDefinition(lastFocus_, nullptr); // TODO(eteran): there was an optional arg?
    }
}

void MainWindow::on_action_Execute_Command_triggered() {
    if(auto doc = currentDocument()) {
        static DialogExecuteCommand *dialog = nullptr;

        if (doc->CheckReadOnly())
            return;

        if(!dialog) {
            dialog = new DialogExecuteCommand(this);
        }

        int r = dialog->exec();
        if(!r) {
            return;
        }

        QString commandText = dialog->ui.textCommand->text();
        if(!commandText.isEmpty()) {
            doc->execAP(lastFocus_, commandText);
        }
    }
}

void MainWindow::on_action_Detach_Tab_triggered() {
    if(TabCount() > 1) {
        if(auto doc = currentDocument()) {
            auto new_window = new MainWindow(nullptr);

            new_window->ui.tabWidget->addTab(doc, doc->filename_);
            new_window->show();
        }
    }
}

void MainWindow::on_action_Print_triggered() {
    if(auto doc = currentDocument()) {
        doc->PrintWindow(lastFocus_, false);
    }
}

void MainWindow::on_action_Print_Selection_triggered() {
    if(auto doc = currentDocument()) {
        doc->PrintWindow(lastFocus_, true);
    }
}

void MainWindow::on_action_Split_Pane_triggered() {
    if(auto doc = currentDocument()) {
        doc->splitPane();
        ui.action_Close_Pane->setEnabled(doc->textPanesCount() > 1);
    }
}

void MainWindow::on_action_Close_Pane_triggered() {
    if(auto doc = currentDocument()) {
        doc->closePane();
        ui.action_Close_Pane->setEnabled(doc->textPanesCount() > 1);
    }
}

void MainWindow::on_action_Move_Tab_To_triggered() {
    if(auto doc = currentDocument()) {
        doc->moveDocument(this);
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
    QList<DocumentWidget *> documents = openDocuments();
    for(DocumentWidget *document : documents) {
        document->ShowStatsLine(state);
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

void MainWindow::indentGroupTriggered(QAction *action) {

    if(auto document = currentDocument()) {
        if(action == ui.action_Indent_Off) {
            document->SetAutoIndent(NO_AUTO_INDENT);
        } else if(action == ui.action_Indent_Off) {
            document->SetAutoIndent(AUTO_INDENT);
        } else if(action == ui.action_Indent_Smart) {
            document->SetAutoIndent(SMART_INDENT);
        } else {
            qDebug("nedit: set_auto_indent invalid argument");
        }
    }
}

void MainWindow::wrapGroupTriggered(QAction *action) {
    if(auto document = currentDocument()) {
        if(action == ui.action_Wrap_None) {
            document->SetAutoWrap(NO_WRAP);
        } else if(action == ui.action_Wrap_Auto_Newline) {
            document->SetAutoWrap(NEWLINE_WRAP);
        } else if(action == ui.action_Wrap_Continuous) {
            document->SetAutoWrap(CONTINUOUS_WRAP);
        } else {
            qDebug("nedit: set_wrap_text invalid argument");
        }
    }
}

void MainWindow::on_action_Wrap_Margin_triggered() {
    if(auto document = currentDocument()) {

        auto dialog = new DialogWrapMargin(document, this);

        // Set default value
        int margin = document->firstPane()->getWrapMargin();

        dialog->ui.checkWrapAndFill->setChecked(margin == 0);
        dialog->ui.spinWrapAndFill->setValue(margin);

        dialog->exec();
        delete dialog;
    }
}

void MainWindow::on_action_Tab_Stops_triggered() {
    if(auto document = currentDocument()) {
        auto dialog = new DialogTabs(document, this);
        dialog->exec();
        delete dialog;
    }
}

void MainWindow::on_action_Text_Fonts_triggered() {
    if(auto document = currentDocument()) {

        document->dialogFonts_ = new DialogFonts(document, true, this);
        document->dialogFonts_->exec();

        delete document->dialogFonts_;
    }
}

void MainWindow::on_action_Highlight_Syntax_toggled(bool state) {
    if(auto document = currentDocument()) {

        document->highlightSyntax_ = state;

        if (document->highlightSyntax_) {
            StartHighlightingEx(document, true);
        } else {
            document->StopHighlightingEx();
        }
    }
}

void MainWindow::on_action_Apply_Backlighting_toggled(bool state) {
    if(auto document = currentDocument()) {
        document->SetBacklightChars(state ? GetPrefBacklightCharTypes() : QString());
    }
}

void MainWindow::on_action_Make_Backup_Copy_toggled(bool state) {
    if(auto document = currentDocument()) {
        document->saveOldVersion_ = state;
    }
}

void MainWindow::on_action_Incremental_Backup_toggled(bool state) {
    if(auto document = currentDocument()) {
        document->autoSave_ = state;
    }
}

void MainWindow::matchingGroupTriggered(QAction *action) {

    // TODO(eteran): implement the backwards compatibility at the macro level
    /* For backward compatibility with pre-5.2 versions, we also
       accept 0 and 1 as aliases for NO_FLASH and FLASH_DELIMIT.
       It is quite unlikely, though, that anyone ever used this
       action procedure via the macro language or a key binding,
       so this can probably be left out safely. */

    if(auto document = currentDocument()) {
        if(action == ui.action_Matching_Off) {
            document->SetShowMatching(NO_FLASH);
        } else if(action == ui.action_Matching_Delimiter) {
            document->SetShowMatching(FLASH_DELIMIT);
        } else if(action == ui.action_Matching_Range) {
            document->SetShowMatching(FLASH_RANGE);
        } else {
            qDebug("nedit: Invalid argument for set_show_matching");
        }
    }
}

void MainWindow::on_action_Matching_Syntax_toggled(bool state) {
    if(auto document = currentDocument()) {
        document->matchSyntaxBased_ = state;
    }
}

void MainWindow::on_action_Overtype_toggled(bool state) {
    if(auto document = currentDocument()) {
        document->SetOverstrike(state);
    }
}

void MainWindow::on_action_Read_Only_toggled(bool state) {
    if(auto document = currentDocument()) {
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
        SetPrefAutoIndent(NO_AUTO_INDENT);
    } else if(action == ui.action_Default_Indent_On) {
        SetPrefAutoIndent(AUTO_INDENT);
    } else if(action == ui.action_Default_Indent_Smart) {
        SetPrefAutoIndent(SMART_INDENT);
    } else {
        qDebug("nedit: invalid default indent");
    }
}

void MainWindow::on_action_Default_Program_Smart_Indent_triggered() {


    if (!SmartIndentDlg) {
        // TODO(eteran): do we really want this to be associated with THIS document?
        if(auto document = currentDocument()) {
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
        SetPrefWrap(NO_WRAP);
    } else if(action == ui.action_Default_Wrap_Auto_Newline) {
        SetPrefWrap(NEWLINE_WRAP);
    } else if(action == ui.action_Default_Wrap_Continuous) {
        SetPrefWrap(CONTINUOUS_WRAP);
    } else {
        qDebug("nedit: invalid default wrap");
    }
}

void MainWindow::on_action_Default_Wrap_Margin_triggered() {

    auto dialog = new DialogWrapMargin(nullptr, this);

    // Set default value
    int margin = GetPrefWrapMargin();

    dialog->ui.checkWrapAndFill->setChecked(margin == 0);
    dialog->ui.spinWrapAndFill->setValue(margin);

    dialog->exec();
    delete dialog;
}

void MainWindow::defaultTagCollisionsGroupTriggered(QAction *action) {
    if(action == ui.action_Default_Tag_Show_All) {
        SetPrefSmartTags(false);
    } else if(action == ui.action_Default_Tag_Smart) {
        SetPrefSmartTags(true);
    } else {
        qDebug("nedit: invalid default collisions");
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

        struct stat attribute;
        if (stat(shell.toLatin1().data(), &attribute) == -1) {
            int resp = QMessageBox::warning(this, tr("Command Shell"), tr("The selected shell is not available.\nDo you want to use it anyway?"), QMessageBox::Ok | QMessageBox::Cancel);
            if(resp == QMessageBox::Cancel) {
                return;
            }
        }

        SetPrefShell(shell);
    }
}

void MainWindow::on_action_Default_Tab_Stops_triggered() {
    auto dialog = new DialogTabs(nullptr, this);
    dialog->exec();
    delete dialog;
}

void MainWindow::on_action_Default_Text_Fonts_triggered() {
    if(auto document = currentDocument()) {
        document->dialogFonts_ = new DialogFonts(nullptr, false, this);
        document->dialogFonts_->exec();
        delete document->dialogFonts_;
    }
}

void MainWindow::on_action_Default_Colors_triggered() {
    if(auto document = currentDocument()) {
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
#if 0
    InvalidateWindowMenus();
#endif
}

void MainWindow::on_action_Default_Customize_Window_Title_triggered() {
    if(auto document = currentDocument()) {
        auto dialog = new DialogWindowTitle(document, this);
        dialog->exec();
        delete dialog;
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

void MainWindow::defaultSearchGroupTriggered(QAction *action) {

    if(action == ui.action_Default_Search_Literal) {
        SetPrefSearch(SEARCH_LITERAL);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Literal_Case_Sensitive) {
        SetPrefSearch(SEARCH_CASE_SENSE);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal_Case_Sensitive)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Literal_Whole_Word) {
        SetPrefSearch(SEARCH_LITERAL_WORD);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal_Whole_Word)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word) {
        SetPrefSearch(SEARCH_CASE_SENSE_WORD);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Regular_Expression) {
        SetPrefSearch(SEARCH_REGEX);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Search_Regular_Expression)->setChecked(true);
        }
    } else if(action == ui.action_Default_Search_Regular_Expresison_Case_Insensitive) {
        SetPrefSearch(SEARCH_REGEX_NOCASE);
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
    // TODO(eteran): eventually move this logic to be local
    EditHighlightPatterns(this);
}

void MainWindow::on_action_Default_Syntax_Text_Drawing_Styles_triggered() {
    // TODO(eteran): eventually move this logic to be local
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
        // TODO(eteran): actually disable tooltips (on the tabbar?)
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
        SetPrefShowMatching(NO_FLASH);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Matching_Off)->setChecked(true);
        }
    } else if(action == ui.action_Default_Matching_Delimiter) {
        SetPrefShowMatching(FLASH_DELIMIT);
        for(MainWindow *window : allWindows()) {
            no_signals(window->ui.action_Default_Matching_Delimiter)->setChecked(true);
        }
    } else if(action == ui.action_Default_Matching_Range) {
        SetPrefShowMatching(FLASH_RANGE);
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
        auto dialog = new DialogWindowSize(this);
        dialog->exec();
        delete dialog;
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

    bool crossWindows = GetPrefGlobalTabNavigate();
    int currentIndex  = ui.tabWidget->currentIndex();
    int nextIndex     = currentIndex + 1;
    int tabCount      = ui.tabWidget->count();

    // TODO(eteran): implement crossing windows supports
    if(!crossWindows) {
        if(nextIndex == tabCount) {
            ui.tabWidget->setCurrentIndex(0);
        } else {
            ui.tabWidget->setCurrentIndex(nextIndex);
        }
    }
}

void MainWindow::action_Prev_Document() {
    bool crossWindows = GetPrefGlobalTabNavigate();
    int currentIndex  = ui.tabWidget->currentIndex();
    int prevIndex     = currentIndex - 1;
    int tabCount      = ui.tabWidget->count();

    // TODO(eteran): implement crossing windows supports
    if(!crossWindows) {
        if(currentIndex == 0) {
            ui.tabWidget->setCurrentIndex(tabCount - 1);
        } else {
            ui.tabWidget->setCurrentIndex(prevIndex);
        }
    }
}

void MainWindow::action_Last_Document() {
    // this will put the focus on whatever document last had the focus
    // TODO(eteran): implement!
}

DocumentWidget *MainWindow::EditNewFileEx(MainWindow *inWindow, QString geometry, bool iconic, const QString &languageMode, const QString &defaultPath) {

    DocumentWidget *document;

    /*... test for creatability? */

    // Find a (relatively) unique name for the new file
    QString name = MainWindow::UniqueUntitledNameEx();

    // create new window/document
    if (inWindow) {
        document = inWindow->CreateDocument(name);
    } else {
        auto win = new MainWindow();
        document = win->CreateDocument(name);

        win->parseGeometry(geometry);

        if(iconic) {
            win->showMinimized();
        } else {
            win->show();
        }

        inWindow = win;
    }


    document->filename_ = name;
    document->path_     = !defaultPath.isEmpty() ? defaultPath : GetCurrentDirEx();

    // do we have a "/" at the end? if not, add one
    if (!document->path_.isEmpty() && !document->path_.endsWith(QLatin1Char('/'))) {
        document->path_.append(QLatin1Char('/'));
    }

    document->SetWindowModified(false);
    document->lockReasons_.clear();
    inWindow->UpdateWindowReadOnly(document);
    document->UpdateStatsLine(nullptr);
    inWindow->UpdateWindowTitle(document);
    document->RefreshTabState();


    if(languageMode.isNull()) {
        document->DetermineLanguageMode(true);
    } else {
        document->SetLanguageMode(FindLanguageMode(languageMode), true);
    }

    if (iconic && inWindow->isMinimized()) {
        document->RaiseDocument();
    } else {
        document->RaiseDocumentWindow();
    }

    inWindow->SortTabBar();
    return document;
}

void MainWindow::AllWindowsBusyEx(const QString &message) {

    if (!currentlyBusy) {
        busyStartTime = getRelTimeInTenthsOfSeconds();
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

    } else if (!modeMessageSet && !message.isNull() && getRelTimeInTenthsOfSeconds() - busyStartTime > 10) {

        // Show the mode message when we've been busy for more than a second
        for(DocumentWidget *document : DocumentWidget::allDocuments()) {
            document->SetModeMessageEx(message);
        }
        modeMessageSet = true;
    }

    // NOTE(eteran): this used to be a BusyWait(window->shell)
    BusyWaitEx();
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

void MainWindow::BusyWaitEx() {
    static const int timeout = 100000; /* 1/10 sec = 100 ms = 100,000 us */
    static struct timeval last = {0, 0};
    struct timeval current;
    gettimeofday(&current, nullptr);

    if ((current.tv_sec != last.tv_sec) || (current.tv_usec - last.tv_usec > timeout)) {
        QApplication::processEvents();
        last = current;
    }
}

void MainWindow::on_action_Save_triggered() {
    if(auto document = currentDocument()) {
        if (document->CheckReadOnly()) {
            return;
        }

        document->SaveWindow();
    }
}

/*
** Wrapper for HandleCustomNewFileSB which uses the current window's path
** (if set) as the default directory, and asks about embedding newlines
** to make wrapping permanent.
*/
bool MainWindow::PromptForNewFileEx(DocumentWidget *document, const QString prompt, char *fullname, FileFormats *fileFormat, bool *addWrap) {

    *fileFormat = document->fileFormat_;

    bool retVal = false;

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
            case DOS_FILE_FORMAT:
                dosCheck->setChecked(true);
                break;
            case MAC_FILE_FORMAT:
                macCheck->setChecked(true);
                break;
            case UNIX_FILE_FORMAT:
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
#if 0
            // TODO(eteran): implement this once this is hoisted into a QObject
            //               since Qt4 doesn't support lambda based connections
            QObject::connect(wrapCheck, &QCheckBox::toggled, [&](bool checked) {
                if(checked) {
                    int ret = QMessageBox::information(nullptr, QLatin1String("Add Wrap"),
                        QLatin1String("This operation adds permanent line breaks to\n"
                        "match the automatic wrapping done by the\n"
                        "Continuous Wrap mode Preferences Option.\n\n"
                        "*** This Option is Irreversable ***\n\n"
                        "Once newlines are inserted, continuous wrapping\n"
                        "will no longer work automatically on these lines"),
                        QMessageBox::Ok, QMessageBox::Cancel);

                    if(ret != QMessageBox::Ok) {
                        wrapCheck->setChecked(false);
                    }
                }
            });
#endif

            if (document->wrapMode_ == CONTINUOUS_WRAP) {
                layout->addWidget(wrapCheck, row, 1, 1, 1);
            }

            if(dialog.exec()) {
                if(dosCheck->isChecked()) {
                    document->fileFormat_ = DOS_FILE_FORMAT;
                } else if(macCheck->isChecked()) {
                    document->fileFormat_ = MAC_FILE_FORMAT;
                } else if(unixCheck->isChecked()) {
                    document->fileFormat_ = UNIX_FILE_FORMAT;
                }

                *addWrap = wrapCheck->isChecked();
                strcpy(fullname, dialog.selectedFiles()[0].toLocal8Bit().data());
                retVal = true;
            }

        }
    }

    return retVal;
}

void MainWindow::on_action_Save_As_triggered() {
    if(auto document = currentDocument()) {

        bool addWrap;
        FileFormats fileFormat;
        char fullname[MAXPATHLEN];

        bool response = PromptForNewFileEx(document, tr("Save File As"), fullname, &fileFormat, &addWrap);
        if (!response) {
            return;
        }

        document->fileFormat_ = fileFormat;
        document->SaveWindowAs(fullname, addWrap);
    }
}

void MainWindow::on_action_Revert_to_Saved_triggered() {

    if(auto document = currentDocument()) {
        // re-reading file is irreversible, prompt the user first
        if (document->fileChanged_) {

            QMessageBox messageBox(this);
            messageBox.setWindowTitle(tr("Discard Changes"));
            messageBox.setIcon(QMessageBox::Question);
            messageBox.setText(tr("Discard changes to\n%1%2?").arg(document->path_).arg(document->filename_));
            QPushButton *buttonOk     = messageBox.addButton(QMessageBox::Ok);
            QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonOk);

            messageBox.exec();
            if(messageBox.clickedButton() == buttonCancel) {
                return;
            }

        } else {

            QMessageBox messageBox(nullptr /*window->shell_*/);
            messageBox.setWindowTitle(tr("Reload File"));
            messageBox.setIcon(QMessageBox::Question);
            messageBox.setText(tr("Re-load file\n%1%2?").arg(document->path_).arg(document->filename_));
            QPushButton *buttonOk   = messageBox.addButton(tr("Re-read"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonOk);

            messageBox.exec();
            if(messageBox.clickedButton() == buttonCancel) {
                return;
            }
        }

        document->RevertToSaved();
    }
}

//------------------------------------------------------------------------------
// Name: on_action_New_Window_triggered
// Desc: whatever the setting is for what to do with "New",
//       this does the opposite
//------------------------------------------------------------------------------
void MainWindow::on_action_New_Window_triggered() {
    if(auto document = currentDocument()) {
        MainWindow::EditNewFileEx(GetPrefOpenInTab() ? nullptr : this, QString(), false, QString(), document->path_);
        CheckCloseDimEx();
    }
}

//------------------------------------------------------------------------------
// Name: on_action_Exit_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Exit_triggered() {

    const int DF_MAX_MSG_LENGTH = 2048;

    QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

    if (!CheckPrefsChangesSavedEx()) {
        return;
    }

    /* If this is not the last window (more than one window is open),
       confirm with the user before exiting. */
    // NOTE(eteran): test if the current window is NOT the only window
    if (GetPrefWarnExit() && !(documents.size() < 2)) {

        QString exitMsg(tr("Editing: "));

        /* List the windows being edited and make sure the
           user really wants to exit */
        // This code assembles a list of document names being edited and elides as necessary
        for(int i = 0; i < documents.size(); ++i) {
            DocumentWidget *const document  = documents[i];

            QString filename = tr("%1%2").arg(document->filename_).arg(document->fileChanged_ ? tr("*") : tr(""));

            if (exitMsg.size() + filename.size() + 30 >= DF_MAX_MSG_LENGTH) {
                exitMsg.append(tr("..."));
                break;
            }

            // NOTE(eteran): test if this is the last window
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

/*
** Check if preferences have changed, and if so, ask the user if he wants
** to re-save.  Returns False if user requests cancelation of Exit (or whatever
** operation triggered this call to be made).
*/
bool MainWindow::CheckPrefsChangesSavedEx() {

    if (!PrefsHaveChanged) {
        return true;
    }

    QMessageBox messageBox(this);
    messageBox.setWindowTitle(tr("Default Preferences"));
    messageBox.setIcon(QMessageBox::Question);

    messageBox.setText((ImportedFile.isNull())
        ? tr("Default Preferences have changed.\nSave changes to NEdit preference file?")
        : tr("Default Preferences have changed.  SAVING \nCHANGES WILL INCORPORATE ADDITIONAL\nSETTINGS FROM FILE: %s").arg(ImportedFile));

    QPushButton *buttonSave     = messageBox.addButton(tr("Save"), QMessageBox::AcceptRole);
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
            return document->CloseFileAndWindow(PROMPT_SBC_DIALOG_RESPONSE);
        }
    } else {

        // close all _modified_ documents belong to this window
        for(DocumentWidget *document : openDocuments()) {
            if (document->fileChanged_) {
                if (!document->CloseFileAndWindow(PROMPT_SBC_DIALOG_RESPONSE)) {
                    return false;
                }
            }
        }

        // if there's still documents left in the window...
        for(DocumentWidget *document : openDocuments()) {
            if (!document->CloseFileAndWindow(PROMPT_SBC_DIALOG_RESPONSE)) {
                return false;
            }
        }
    }

    return true;
}

void MainWindow::closeEvent(QCloseEvent *event) {


    QList<MainWindow *> windows = MainWindow::allWindows();
    if(windows.size() == 1) {
        // this is only window, then this is the same as exit
        Q_EMIT on_action_Exit_triggered();
    } else {

        if (TabCount() == 1) {
            if(DocumentWidget *document = currentDocument()) {
                document->CloseFileAndWindow(PROMPT_SBC_DIALOG_RESPONSE);
            }
        } else {
            int resp = QMessageBox::Cancel;
            if (GetPrefWarnExit()) {
                // TODO(eteran): this is probably better off with "Ok" "Cancel", but we are being consistant with the original UI for now
                resp = QMessageBox::question(this, tr("Close Window"), tr("Close ALL documents in this window?"), QMessageBox::Cancel, QMessageBox::Close);
            }

            if (resp == QMessageBox::Close) {
                CloseAllDocumentInWindow();
                delete this;
                event->accept();
            } else {
                event->ignore();
            }
        }
    }
}

void MainWindow::on_action_Execute_Command_Line_triggered() {
    if(auto doc = currentDocument()) {
        if (doc->CheckReadOnly()) {
            return;
        }

        doc->ExecCursorLineEx(lastFocus_, false);
    }
}

void MainWindow::on_action_Filter_Selection_triggered() {
    if(auto doc = currentDocument()) {
        static DialogFilter *dialog = nullptr;

        if (doc->CheckReadOnly()) {
            return;
        }

        if (!doc->buffer_->primary_.selected) {
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
        if(!filterText.isEmpty()) {
            doc->filterSelection(filterText);
        }
    }
}

void MainWindow::on_action_Cancel_Shell_Command_triggered() {
    if(auto doc = currentDocument()) {
        doc->AbortShellCommandEx();
    }
}


void MainWindow::shellTriggered(QAction *action) {
    if(auto doc = currentDocument()) {
        const int index = action->data().toInt();
        const QString name = ShellMenuData[index].item->name;
        doc->DoNamedShellMenuCmd(lastFocus_, name, false);
    }
}

void MainWindow::on_action_Learn_Keystrokes_triggered() {
    if(auto doc = currentDocument()) {
        BeginLearnEx(doc);
    }
}

void MainWindow::on_action_Finish_Learn_triggered() {
    if(auto doc = currentDocument()) {
        Q_UNUSED(doc);
        FinishLearnEx();
    }
}

void MainWindow::on_action_Cancel_Learn_triggered() {
    if(auto doc = currentDocument()) {
        CancelMacroOrLearnEx(doc);
    }
}

void MainWindow::macroTriggered(QAction *action) {

    // TODO(eteran): implement what this comment says!
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

    if(auto doc = currentDocument()) {
        const int index = action->data().toInt();
        const QString name = MacroMenuData[index].item->name;
        doc->DoNamedMacroMenuCmd(lastFocus_, name, false);

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

void MainWindow::on_action_Repeat_triggered() {
    if(LastCommand.isNull()) {
        QMessageBox::warning(this, tr("Repeat Macro"), tr("No previous commands or learn/replay sequences to repeat"));
        return;
    }

    // TODO(eteran): redundant to work done in DialogRepeat::setCommand function
    int index = LastCommand.indexOf(QLatin1Char('('));
    if(index == -1) {
        return;
    }

    auto dialog = new DialogRepeat(currentDocument(), this);
    dialog->setCommand(LastCommand);
    dialog->show();
}


void MainWindow::focusChanged(QWidget *from, QWidget *to) {

    Q_UNUSED(from);

    if(auto area = qobject_cast<TextArea *>(to)) {
        if(auto document = DocumentWidget::documentFrom(area)) {
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
    // TODO(eteran): implement
    qDebug("TODO(eteran): implement this");
}
