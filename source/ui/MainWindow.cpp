
#include <QtDebug>
#include <QToolButton>
#include <QShortcut>
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
#include "utils.h"
#include "LanguageMode.h"
#include "nedit.h"

namespace {

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

	showStats_            = GetPrefStatsLine();
	showISearchLine_      = GetPrefISearchLine();
	showLineNumbers_      = GetPrefLineNums();
	modeMessageDisplayed_ = false;
	
	// default to hiding the optional panels
	ui.incrementalSearchFrame->setVisible(showISearchLine_);	
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
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Left_Tabs() {
    qDebug("[action_Shift_Left_Tabs]");
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void MainWindow::action_Shift_Right_Tabs() {
    qDebug("[action_Shift_Right_Tabs]");
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
	if(TextArea *w = lastFocus_) {
		if(auto doc = qobject_cast<DocumentWidget *>(w->parent()->parent())) {
			path = doc->path_;
		}
	}

	EditNewFile(openInTab ? this : nullptr, nullptr, false, nullptr, path);
	CheckCloseDim();
}

//------------------------------------------------------------------------------
// Name: on_action_Open_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_Open_triggered() {

}

//------------------------------------------------------------------------------
// Name: on_action_About_triggered
//------------------------------------------------------------------------------
void MainWindow::on_action_About_triggered() {
	auto dialog = new DialogAbout(this);
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
    QAction *action = languageMenu->addAction(tr("Plain"), this, SLOT(setLangModeCB()));
    action->setData(PLAIN_LANGUAGE_MODE);
    action->setCheckable(true);
    action->setChecked(true);
    languageGroup->addAction(action);


    for (int i = 0; i < NLanguageModes; i++) {
        QAction *action = languageMenu->addAction(LanguageModes[i]->name, this, SLOT(setLangModeCB()));
        action->setData(i);
        action->setCheckable(true);
        languageGroup->addAction(action);
    }

    ui.action_Language_Mode->setMenu(languageMenu);
}

/*
** Create a submenu for chosing language mode for the current window.
*/
void MainWindow::CreateLanguageModeSubMenu() {
    updateLanguageModeSubmenu();
}

void MainWindow::setLangModeCB() {

    if(auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget())) {

        if(const auto action = qobject_cast<QAction *>(sender())) {
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
    if(TextArea *w = lastFocus_) {
        if(auto doc = qobject_cast<DocumentWidget *>(w->parent()->parent())) {
            if (doc->CheckReadOnly()) {
                return;
            }
            doc->Undo();
        }
    }
}

void MainWindow::on_action_Redo_triggered() {

    if(TextArea *w = lastFocus_) {
        if(auto doc = qobject_cast<DocumentWidget *>(w->parent()->parent())) {

            if (doc->CheckReadOnly()) {
                return;
            }
            doc->Redo();
        }
    }
}
