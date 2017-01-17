
#include <QtDebug>
#include <QToolButton>
#include <QShortcut>
#include <QFileDialog>
#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include "MainWindow.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "DialogAbout.h"
#include "DocumentWidget.h"
#include "DialogWindowTitle.h"
#include "DialogReplace.h"
#include "DialogFind.h"
#include "clearcase.h"
#include "file.h"
#include "preferences.h"
#include "shift.h"
#include "utils.h"
#include "LanguageMode.h"
#include "nedit.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <cmath>
#include <glob.h>

namespace {

const char neditDBBadFilenameChars[] = "\n";

QList<QString> PrevOpen;

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

	showStats_            = GetPrefStatsLine();
	showISearchLine_      = GetPrefISearchLine();
	showLineNumbers_      = GetPrefLineNums();
	modeMessageDisplayed_ = false;
	
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
        QString filename = PromptForExistingFileEx(doc->path_, tr("Open File"));
        if (filename.isNull()) {
            return;
        }

        doc->open(filename.toLatin1().data());
    }

    CheckCloseDim();
}

void MainWindow::on_action_Close_triggered() {
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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


    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {

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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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
		if(dialog->keepDialog()) {
			title = QString(tr("Find (in %1)")).arg(doc->filename_);
			dialog->setWindowTitle(title);
		}
	}

	if(auto dialog = getDialogReplace()) {
		if(dialog->keepDialog()) {
			title = QString(tr("Replace (in %1)")).arg(doc->filename_);
			dialog->setWindowTitle(title);
		}
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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
        if (doc->CheckReadOnly()) {
            return;
        }
        doc->Undo();
    }
}

void MainWindow::on_action_Redo_triggered() {
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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

        if(DocumentWidget *doc = DocumentWidget::documentFrom(lastFocus_)) {
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
        // Get the selected text, if there's no selection, do nothing
        std::string text = doc->buffer_->BufGetSelectionTextEx();
        fileCB(doc, text);
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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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
    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {
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

    if(auto doc = DocumentWidget::documentFrom(lastFocus_)) {

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
