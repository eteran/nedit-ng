
#include <QtDebug>
#include <QToolButton>
#include "MainWindow.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "DialogAbout.h"
#include "DocumentWidget.h"
#include "file.h"
#include "preferences.h"


//------------------------------------------------------------------------------
// Name: MainWindow
//------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags) {
	ui.setupUi(this);
	
	setupMenuGroups();
	setupTabBar();
	setupMenuStrings();
	
	showStats_            = GetPrefStatsLine();
	showISearchLine_      = GetPrefISearchLine();
	showLineNumbers_      = GetPrefLineNums();
	modeMessageDisplayed_ = false;
	
	// default to hiding the optional panels
	ui.statusFrame->setVisible(showStats_);
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

	QString name = UniqueUntitledName();
	auto newTab = new DocumentWidget(name, this);
	ui.tabWidget->addTab(newTab, name);
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

/*
** Update the optional statistics line.
*/
void MainWindow::UpdateStatsLine(DocumentWidget *doc) {
#if 0
	if (!IsTopDocument()) {
		return;
	}
#endif

	/* This routine is called for each character typed, so its performance
	   affects overall editor perfomance.  Only update if the line is on. */
	if (!showStats_) {
		return;
	}

	auto textD = lastFocus_;


	// Compose the string to display. If line # isn't available, leave it off
	int pos = textD->TextGetCursorPos();

	QString format;
	switch(doc->fileFormat_) {
	case DOS_FILE_FORMAT:
		format = tr(" DOS");
		break;
	case MAC_FILE_FORMAT:
		format = tr(" Mac");
		break;
	default:
		format = tr("");
		break;
	}

	QString string;
	QString slinecol;
	int line;
	int colNum;
	if (!textD->TextDPosToLineAndCol(pos, &line, &colNum)) {
		string   = tr("%1%2%3 %4 bytes").arg(doc->path_, doc->filename_, format).arg(doc->buffer_->BufGetLength());
		slinecol = tr("L: ---  C: ---");
	} else {
		slinecol = tr("L: %1  C: %2").arg(line).arg(colNum);
		if (showLineNumbers_) {
			string = tr("%1%2%3 byte %4 of %5").arg(doc->path_, doc->filename_, format).arg(pos).arg(doc->buffer_->BufGetLength());
		} else {
			string = tr("%1%2%3 %4 bytes").arg(doc->path_, doc->filename_, format).arg(doc->buffer_->BufGetLength());
		}
	}

	// Update the line/column number
	ui.labelStats->setText(slinecol);

	// Don't clobber the line if there's a special message being displayed
	if(!modeMessageDisplayed_) {
		ui.labelFileAndSize->setText(string);
	}
}


