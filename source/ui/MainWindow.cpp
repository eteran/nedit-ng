
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "file.h"
#include <QToolButton>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags) {
	ui.setupUi(this);
	
	// default to hiding the optional panels
	ui.statusFrame->setVisible(false);
	ui.incrementalSearchFrame->setVisible(false);
	
	// create and hook up the tab close button
	auto deleteTabButton = new QToolButton(ui.tabWidget);
	deleteTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	deleteTabButton->setIcon(QIcon::fromTheme(QLatin1String("tab-close")));
	deleteTabButton->setAutoRaise(true);
	ui.tabWidget->setCornerWidget(deleteTabButton);
	
	connect(deleteTabButton, SIGNAL(clicked()), this, SLOT(deleteTabButtonClicked()));
	
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
// Name: 
//------------------------------------------------------------------------------
MainWindow::~MainWindow() {

}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void MainWindow::deleteTabButtonClicked() {
	ui.tabWidget->removeTab(ui.tabWidget->currentIndex());
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void MainWindow::on_action_New_triggered() {

	QString name = UniqueUntitledName();

	auto newTab = new DocumentWidget(name, this);
	ui.tabWidget->addTab(newTab, name);
}
