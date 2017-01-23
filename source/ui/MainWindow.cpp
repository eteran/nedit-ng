
#include <QtDebug>
#include <QToolButton>
#include <QShortcut>
#include <QFileDialog>
#include <QFile>
#include <QInputDialog>
#include <QClipboard>
#include <QMessageBox>
#include "MainWindow.h"
#include "DialogExecuteCommand.h"
#include "SignalBlocker.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "DialogAbout.h"
#include "DocumentWidget.h"
#include "DialogWindowTitle.h"
#include "DialogReplace.h"
#include "DialogFind.h"
#include "selection.h"
#include "clearcase.h"
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <cmath>
#include <glob.h>

namespace {

const char neditDBBadFilenameChars[] = "\n";

QList<QString> PrevOpen;

bool StringToNum(const char *string, int *number) {
    const char *c = string;

    while (*c == ' ' || *c == '\t') {
        ++c;
    }
    if (*c == '+' || *c == '-') {
        ++c;
    }
    while (isdigit((uint8_t)*c)) {
        ++c;
    }
    while (*c == ' ' || *c == '\t') {
        ++c;
    }
    if (*c != '\0') {
        // if everything went as expected, we should be at end, but we're not
        return false;
    }
    if (number) {
        if (sscanf(string, "%d", number) != 1) {
            // This case is here to support old behavior
            *number = 0;
        }
    }
    return true;
}

void gotoAP(DocumentWidget *document, TextArea *w, QStringList args) {

    int lineNum;
    int column;
    int position;
    int curCol;

    /* Accept various formats:
          [line]:[column]   (menu action)
          line              (macro call)
          line, column      (macro call) */
    if (args.size() == 0 ||
            args.size() > 2 ||
            (args.size() == 1 && StringToLineAndCol(args[0].toLatin1().data(), &lineNum, &column) == -1) ||
            (args.size() == 2 && (!StringToNum(args[0].toLatin1().data(), &lineNum) || !StringToNum(args[1].toLatin1().data(), &column)))) {
        fprintf(stderr, "nedit: goto_line_number action requires line and/or column number\n");
        return;
    }

    auto textD = w;

    // User specified column, but not line number
    if (lineNum == -1) {
        position = textD->TextGetCursorPos();
        if (textD->TextDPosToLineAndCol(position, &lineNum, &curCol) == False) {
            return;
        }
    } else if (column == -1) {
        // User didn't specify a column
        SelectNumberedLineEx(document, w, lineNum);
        return;
    }

    position = textD->TextDLineAndColToPos(lineNum, column);
    if (position == -1) {
        return;
    }

    textD->TextSetCursorPos(position);
    return;
}



DocumentWidget *EditNewFile(MainWindow *inWindow, char *geometry, bool iconic, const char *languageMode, const QString &defaultPath) {

	Q_UNUSED(geometry);
	Q_UNUSED(languageMode);

	DocumentWidget *window;

	/*... test for creatability? */

	// Find a (relatively) unique name for the new file
    QString name = MainWindow::UniqueUntitledNameEx();

	// create new window/document
	if (inWindow) {
		window = inWindow->CreateDocument(name);
	} else {
		// TODO(eteran): implement geometry stuff
		auto win = new MainWindow();
		window = win->CreateDocument(name);

		//win->resize(...);

		if(iconic) {
			win->showMinimized();
		} else {
            win->show();
		}

		inWindow = win;
	}


	window->filename_ = name;
	window->path_     = !defaultPath.isEmpty() ? defaultPath : GetCurrentDirEx();

	// do we have a "/" at the end? if not, add one
    if (!window->path_.isEmpty() && !window->path_.endsWith(QLatin1Char('/'))) {
        window->path_.append(QLatin1Char('/'));
	}

	window->SetWindowModified(false);
	window->lockReasons_.clear();
	inWindow->UpdateWindowReadOnly(window);
	window->UpdateStatsLine(nullptr);
	inWindow->UpdateWindowTitle(window);
	window->RefreshTabState();


	if(!languageMode) {
		window->DetermineLanguageMode(true);
	} else {
		window->SetLanguageMode(FindLanguageMode(languageMode), true);
	}

	inWindow->ShowTabBar(inWindow->GetShowTabBar());

    if (iconic && inWindow->isMinimized()) {
        window->RaiseDocument();
    } else {
        window->RaiseDocumentWindow();
    }

	inWindow->SortTabBar();
	return window;
}

}


//------------------------------------------------------------------------------
// Name: MainWindow
//------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags) {
	ui.setupUi(this);
	
	setupMenuGroups();
	setupTabBar();
	setupMenuStrings();
	setupMenuAlternativeMenus();
    CreateLanguageModeSubMenu();

    // NOTE(eteran): in the original nedit, the previous menu was populated
    // when the user actually tried to look at the menu. It is simpler (but
    // perhaps marginally less efficient) to just populate it when we construct
    // the window
    updatePrevOpenMenu();

    showStats_             = GetPrefStatsLine();
    showISearchLine_       = GetPrefISearchLine();
    showLineNumbers_       = GetPrefLineNums();
    modeMessageDisplayed_  = false;
    iSearchHistIndex_      = 0;
    iSearchStartPos_       = -1;
    iSearchLastRegexCase_  = true;
    iSearchLastLiteralCase_= false;
    wasSelected_           = false;
	
	// default to hiding the optional panels
    ui.incrementalSearchFrame->setVisible(showISearchLine_);

    // these default to disabled.
    // TODO(eteran): we can probably (if we haven't already) enforce this
    // in the UIC file directly instead of in code
    ui.action_Delete->setEnabled(false);
    ui.action_Undo->setEnabled(false);
    ui.action_Redo->setEnabled(false);
}

//------------------------------------------------------------------------------
// Name: ~MainWindow
//------------------------------------------------------------------------------
MainWindow::~MainWindow() {

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
	ui.tabWidget->setCornerWidget(deleteTabButton);
	
	connect(deleteTabButton, SIGNAL(clicked()), this, SLOT(deleteTabButtonClicked()));
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
}

//------------------------------------------------------------------------------
// Name: setupMenuAlternativeMenus
// Desc: under some configurations, menu items have different text/functionality
//------------------------------------------------------------------------------
void MainWindow::setupMenuAlternativeMenus() {
	if(!GetPrefOpenInTab()) {
		ui.action_New_Window->setText(tr("New &Tab"));
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
	
	auto indentWrap = new QActionGroup(this);
	indentWrap->addAction(ui.action_Wrap_None);
	indentWrap->addAction(ui.action_Wrap_Auto_Newline);
	indentWrap->addAction(ui.action_Wrap_Continuous);
	
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
	defaultSizeGroup->addAction(ui.action_Default_Size_24_x_24);
	defaultSizeGroup->addAction(ui.action_Default_Size_40_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_60_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_80_x_80);
	defaultSizeGroup->addAction(ui.action_Default_Size_Custom);
}

//------------------------------------------------------------------------------
// Name: deleteTabButtonClicked
//------------------------------------------------------------------------------
void MainWindow::deleteTabButtonClicked() {
	ui.tabWidget->removeTab(ui.tabWidget->currentIndex());
}

//------------------------------------------------------------------------------
// Name: on_action_New_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_New_triggered() {
    action_New(QLatin1String("prefs"));
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_New_Window_triggered() {
	qDebug("[on_action_New_Window_triggered]");
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::on_action_Execute_Command_Line_triggered() {
	qDebug("[on_action_Execute_Command_Line_triggered]");
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


	EditNewFile(openInTab ? this : nullptr, nullptr, false, nullptr, path);
	CheckCloseDim();
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

    CheckCloseDim();
}

void MainWindow::on_action_Close_triggered() {
    if(auto doc = currentDocument()) {
        doc->actionClose(QLatin1String(""));
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
		QLatin1String(GetPrefServerName()),
		IsServer,
		doc->filenameSet_,
		doc->lockReasons_,
		doc->fileChanged_,
		QLatin1String(GetPrefTitleFormat()));

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

    QList<DocumentWidget *> documents = MainWindow::allDocuments();
    for(DocumentWidget *doc : documents) {
        if(doc == ui.tabWidget->currentWidget()) {
            doc->updateWindowMenu();
        } else {
            doc->windowMenuValid_ = false;
        }
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

void MainWindow::ShowTabBar(bool state) {
	ui.tabWidget->setHideTabBar(!state);
}

/*
** check if tab bar is to be shown on this window
*/
bool MainWindow::GetShowTabBar() {

	if (!GetPrefTabBar()) {
		return false;
	} else if (ui.tabWidget->count()  == 1) {
		return !GetPrefTabBarHideOne();
	} else {
		return true;
	}
}

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
		ui.tabWidget->getTabBar()->moveTab(from, to);
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

QList<DocumentWidget *> MainWindow::openDocuments() const {
    QList<DocumentWidget *> list;
    for(int i = 0; i < ui.tabWidget->count(); ++i) {
        if(auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->widget(i))) {
            list.push_back(document);
        }
    }
    return list;
}

QList<DocumentWidget *> MainWindow::allDocuments() {
    QList<MainWindow *> windows = MainWindow::allWindows();
    QList<DocumentWidget *> documents;

    for(MainWindow *window : windows) {
        documents.append(window->openDocuments());
    }
    return documents;

}

/*
** Make sure the close menu item is dimmed appropriately for the current
** set of windows.  It should be dim only for the last Untitled, unmodified,
** editor window, and sensitive otherwise.
*/
void MainWindow::CheckCloseDim() {

    QList<MainWindow *> windows = MainWindow::allWindows();

    if(windows.empty()) {
        // should never happen...
        return;
    }

    if(windows.size() == 1) {
        // NOTE(eteran): if there is only one window, then *this* must be it...
        // right?

        QList<DocumentWidget *> documents = openDocuments();

        if(TabCount() == 1 && !documents.front()->filenameSet_ && !documents.front()->fileChanged_) {
            ui.action_Close->setEnabled(false);
        } else {
            ui.action_Close->setEnabled(true);
        }
    } else {
        // if there is more than one windows, then by definition, more than one
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
        XFontStruct *fs;
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

        QList<DocumentWidget *> documents = allDocuments();

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

    QList<DocumentWidget *> documents = allDocuments();

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
    QString fullName = GetRCFileNameEx(NEDIT_HISTORY);
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


    QString fullName = GetRCFileNameEx(NEDIT_HISTORY);
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

    CheckCloseDim();
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

    if(QAction *const selected = menu->exec(mapToGlobal(pos))) {

        // TODO(eteran): implement this logic
        if(selected == newTab) {
        } else if(selected == closeTab) {
            Q_UNUSED(index);
        } else if(selected == detachTab) {
            Q_UNUSED(index);
        } else if(selected == moveTab) {
            Q_UNUSED(index);
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

    CheckCloseDim();
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
                DocumentWidget::EditExistingFileEx(GetPrefOpenInTab() ? window : nullptr, QLatin1String(filename), QLatin1String(pathname), 0, nullptr, false, nullptr, GetPrefOpenInTab(), false);
            }
        }
        globfree(&globbuf);
    }

    CheckCloseDim();
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
            gotoAP(doc, w, QStringList() << text);
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
                gotoAP(doc, w, QStringList() << mimeData->text());
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
            auto compiledRE = mem::make_unique<regexp>(searchString.toStdString(), defaultRegexFlags(searchType));
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

    SearchDirection direction = ui.checkIFindReverse ? SEARCH_BACKWARD : SEARCH_FORWARD;

    // Reverse the search direction if the Ctrl or Shift key was pressed
    if(QApplication::keyboardModifiers() & (Qt::CTRL | Qt::SHIFT)) {
        direction = direction == SEARCH_FORWARD ? SEARCH_BACKWARD : SEARCH_FORWARD;
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

    const char *searchStr;
    SearchType searchType;

    // determine the strings and button settings to use
    if (index == 0) {
        searchStr = "";
        searchType = GetPrefSearch();
    } else {
        searchStr = SearchHistory[historyIndex(index)];
        searchType = SearchTypeHistory[historyIndex(index)];
    }

    /* Set the info used in the value changed callback before calling XmTextSetStringEx(). */
    iSearchHistIndex_ = index;
    initToggleButtonsiSearch(searchType);

    // Beware the value changed callback is processed as part of this call
    int previousPosition = ui.editIFind->cursorPosition();
    ui.editIFind->setText(QLatin1String(searchStr));
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
    saveSearchHistory("", nullptr, SEARCH_LITERAL, false);

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
            auto new_window = new MainWindow();

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

DocumentWidget *MainWindow::currentDocument() {
    return qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget());
}
