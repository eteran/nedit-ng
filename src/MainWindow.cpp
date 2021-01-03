
#include "MainWindow.h"
#include "CommandRecorder.h"
#include "DialogAbout.h"
#include "DialogColors.h"
#include "DialogDrawingStyles.h"
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
#include "Font.h"
#include "Help.h"
#include "Highlight.h"
#include "LanguageMode.h"
#include "Location.h"
#include "PatternSet.h"
#include "Preferences.h"
#include "Regex.h"
#include "Search.h"
#include "Settings.h"
#include "SignalBlocker.h"
#include "SmartIndent.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "Util/ClearCase.h"
#include "Util/FileSystem.h"
#include "Util/algorithm.h"
#include "Util/regex.h"
#include "Util/utils.h"
#include "WindowMenuEvent.h"
#include "nedit.h"
#include "shift.h"
#include "userCmds.h"

#include <QButtonGroup>
#include <QClipboard>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QToolTip>
#include <qplatformdefs.h>

#include <cmath>

#ifdef Q_OS_LINUX
#include <QLibrary>
#endif

namespace {

bool currentlyBusy   = false;
bool modeMessageSet  = false;
qint64 busyStartTime = 0;

QPointer<DocumentWidget> lastFocusDocument;

QVector<QString> PrevOpen;

/*
** Extract the line and column number from the text string.
*/
boost::optional<Location> StringToLineAndCol(const QString &text) {

	static const QRegularExpression re(QLatin1String(
		"^"
		"\\s*"
		"(?<row>[-+]?[1-9]\\d*)?"
		"\\s*"
		"([:,]"
		"\\s*"
		"(?<col>[-+]?[1-9]\\d*)?)?"
		"\\s*"
		"$"));

	const QRegularExpressionMatch match = re.match(text);
	if (match.hasMatch()) {
		const QString row = match.captured(QLatin1String("row"));
		const QString col = match.captured(QLatin1String("col"));

		bool row_ok;
		int r = row.toInt(&row_ok);
		if (!row_ok) {
			r = -1;
		} else {
			r = qBound(0, r, INT_MAX);
		}

		bool col_ok;
		int c = col.toInt(&col_ok);
		if (!col_ok) {
			c = -1;
		} else {
			c = qBound(0, c, INT_MAX);
		}

		if (r == -1 && c == -1) {
			return boost::none;
		}

		return Location{r, c};
	}

	return boost::none;
}

/**
 * @brief addToGroup
 * @param group
 * @param menu
 */
void addToGroup(QActionGroup *group, QMenu *menu) {
	Q_FOREACH (QAction *action, menu->actions()) {
		if (QMenu *subMenu = action->menu()) {
			addToGroup(group, subMenu);
		}
		group->addAction(action);
	}
}

/*
** Capitalize or lowercase the contents of the selection (or of the character
** before the cursor if there is no selection).
*/
template <int (&F)(int) noexcept>
void changeCase(DocumentWidget *document, TextArea *area) {

	TextBuffer *buf = document->buffer();

	// Get the selection.  Use character before cursor if no selection
	if (boost::optional<SelectionPos> pos = buf->BufGetSelectionPos()) {
		bool modified = false;

		std::string text = buf->BufGetSelectionText();

		for (char &ch : text) {
			const char oldChar = ch;
			ch                 = safe_ctype<F>(ch);
			if (ch != oldChar) {
				modified = true;
			}
		}

		if (modified) {
			buf->BufReplaceSelected(text);
		}

		if (pos->isRect) {
			buf->BufRectSelect(pos->start, pos->end, pos->rectStart, pos->rectEnd);
		} else {
			buf->BufSelect(pos->start, pos->end);
		}
	} else {
		TextCursor cursorPos = area->cursorPos();
		if (cursorPos == buf->BufStartOfBuffer()) {
			QApplication::beep();
			return;
		}

		char ch = buf->BufGetCharacter(cursorPos - 1);

		ch = safe_ctype<F>(ch);
		buf->BufReplace(cursorPos - 1, cursorPos, ch);
	}
}

/**
 * @brief upcaseSelection
 * @param document
 * @param area
 */
void upcaseSelection(DocumentWidget *document, TextArea *area) {
	changeCase<::toupper>(document, area);
}

/**
 * @brief downcaseSelection
 * @param document
 * @param area
 */
void downcaseSelection(DocumentWidget *document, TextArea *area) {
	changeCase<::tolower>(document, area);
}

}

/**
 * @brief MainWindow::MainWindow
 * @param parent
 * @param flags
 */
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
	: QMainWindow(parent, flags) {

	ui.setupUi(this);
	connectSlots();

	connect(qApp, &QApplication::focusChanged, this, &MainWindow::focusChanged);

	ui.menu_Windows->setStyleSheet(QLatin1String("QMenu { menu-scrollable: 1; }"));

#ifdef Q_OS_LINUX
	using DisablerFunc = void (*)(QWidget *);

	static auto setNoAccel = reinterpret_cast<DisablerFunc>(QLibrary::resolve(QLatin1String("libKF5WidgetsAddons.so"), "_ZN19KAcceleratorManager10setNoAccelEP7QWidget"));
	if (setNoAccel) {
		setNoAccel(ui.tabWidget->tabBar());
	}
#endif

	setupTabBar();
	setupMenuStrings();
	setupMenuAlternativeMenus();
	createLanguageModeSubMenu();
	setupMenuDefaults();
	setupMenuGroups();

	setupPrevOpenMenuActions();
	updatePrevOpenMenu();

	// determine the strings and button settings to use
	initToggleButtonsiSearch(Preferences::GetPrefSearch());

	showISearchLine_ = Preferences::GetPrefISearchLine();
	showLineNumbers_ = Preferences::GetPrefLineNums();

	// make sure that the ifind button has an icon
	ui.buttonIFind->setIcon(QIcon::fromTheme(QLatin1String("edit-find")));

	// default to hiding the optional panels
	ui.incrementalSearchFrame->setVisible(showISearchLine_);

	ui.action_Statistics_Line->setChecked(Preferences::GetPrefStatsLine());

	// make sure we include this windows which is in the middle of being created
	std::vector<MainWindow *> windows = MainWindow::allWindows();
	auto it                           = std::find(windows.begin(), windows.end(), this);
	if (it == windows.end()) {
		windows.push_back(this);
	}

	MainWindow::checkCloseEnableState(windows);
	const bool enabled = windows.size() > 1;
	for (MainWindow *window : windows) {
		window->ui.action_Move_Tab_To->setEnabled(enabled);
	}

	connect(
		this, &MainWindow::checkForChangesToFile, this, [](DocumentWidget *document) {
			document->checkForChangesToFile();
		},
		Qt::QueuedConnection);
}

/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow() {
	// disconnect this signal explicitly or we set off the UBSAN during qApp
	// destruction
	disconnect(qApp, &QApplication::focusChanged, this, &MainWindow::focusChanged);
}

/**
 * @brief MainWindow::connectSlots
 */
void MainWindow::connectSlots() {
	connect(ui.buttonIFind, &QPushButton::clicked, this, &MainWindow::buttonIFind_clicked);
	connect(ui.editIFind, &QLineEdit::textChanged, this, &MainWindow::editIFind_textChanged);
	connect(ui.checkIFindCase, &QCheckBox::toggled, this, &MainWindow::checkIFindCase_toggled);
	connect(ui.checkIFindRegex, &QCheckBox::toggled, this, &MainWindow::checkIFindRegex_toggled);
	connect(ui.checkIFindReverse, &QCheckBox::toggled, this, &MainWindow::checkIFindReverse_toggled);
	connect(ui.editIFind, &QLineEdit::returnPressed, this, &MainWindow::editIFind_returnPressed);
	connect(ui.action_New, &QAction::triggered, this, &MainWindow::action_New_triggered);
	connect(ui.action_New_Window, &QAction::triggered, this, &MainWindow::action_New_Window_triggered);
	connect(ui.action_Open, &QAction::triggered, this, &MainWindow::action_Open_triggered);
	connect(ui.action_Select_All, &QAction::triggered, this, &MainWindow::action_Select_All_triggered);
	connect(ui.action_Open_Selected, &QAction::triggered, this, &MainWindow::action_Open_Selected_triggered);
	connect(ui.action_Close, &QAction::triggered, this, &MainWindow::action_Close_triggered);
	connect(ui.action_Include_File, &QAction::triggered, this, &MainWindow::action_Include_File_triggered);
	connect(ui.action_Load_Calltips_File, &QAction::triggered, this, &MainWindow::action_Load_Calltips_File_triggered);
	connect(ui.action_Load_Tags_File, &QAction::triggered, this, &MainWindow::action_Load_Tags_File_triggered);
	connect(ui.action_Load_Macro_File, &QAction::triggered, this, &MainWindow::action_Load_Macro_File_triggered);
	connect(ui.action_Print, &QAction::triggered, this, &MainWindow::action_Print_triggered);
	connect(ui.action_Print_Selection, &QAction::triggered, this, &MainWindow::action_Print_Selection_triggered);
	connect(ui.action_Save, &QAction::triggered, this, &MainWindow::action_Save_triggered);
	connect(ui.action_Save_As, &QAction::triggered, this, &MainWindow::action_Save_As_triggered);
	connect(ui.action_Revert_to_Saved, &QAction::triggered, this, &MainWindow::action_Revert_to_Saved_triggered);
	connect(ui.action_Exit, &QAction::triggered, this, &MainWindow::action_Exit_triggered);
	connect(ui.action_Undo, &QAction::triggered, this, &MainWindow::action_Undo_triggered);
	connect(ui.action_Redo, &QAction::triggered, this, &MainWindow::action_Redo_triggered);
	connect(ui.action_Cut, &QAction::triggered, this, &MainWindow::action_Cut_triggered);
	connect(ui.action_Copy, &QAction::triggered, this, &MainWindow::action_Copy_triggered);
	connect(ui.action_Paste, &QAction::triggered, this, &MainWindow::action_Paste_triggered);
	connect(ui.action_Paste_Column, &QAction::triggered, this, &MainWindow::action_Paste_Column_triggered);
	connect(ui.action_Delete, &QAction::triggered, this, &MainWindow::action_Delete_triggered);
	connect(ui.action_Shift_Left, &QAction::triggered, this, &MainWindow::action_Shift_Left_triggered);
	connect(ui.action_Shift_Right, &QAction::triggered, this, &MainWindow::action_Shift_Right_triggered);
	connect(ui.action_Lower_case, &QAction::triggered, this, &MainWindow::action_Lower_case_triggered);
	connect(ui.action_Upper_case, &QAction::triggered, this, &MainWindow::action_Upper_case_triggered);
	connect(ui.action_Fill_Paragraph, &QAction::triggered, this, &MainWindow::action_Fill_Paragraph_triggered);
	connect(ui.action_Insert_Form_Feed, &QAction::triggered, this, &MainWindow::action_Insert_Form_Feed_triggered);
	connect(ui.action_Insert_Ctrl_Code, &QAction::triggered, this, &MainWindow::action_Insert_Ctrl_Code_triggered);
	connect(ui.action_Goto_Line_Number, &QAction::triggered, this, &MainWindow::action_Goto_Line_Number_triggered);
	connect(ui.action_Goto_Selected, &QAction::triggered, this, &MainWindow::action_Goto_Selected_triggered);
	connect(ui.action_Find, &QAction::triggered, this, &MainWindow::action_Find_triggered);
	connect(ui.action_Find_Again, &QAction::triggered, this, &MainWindow::action_Find_Again_triggered);
	connect(ui.action_Find_Selection, &QAction::triggered, this, &MainWindow::action_Find_Selection_triggered);
	connect(ui.action_Find_Incremental, &QAction::triggered, this, &MainWindow::action_Find_Incremental_triggered);
	connect(ui.action_Replace, &QAction::triggered, this, &MainWindow::action_Replace_triggered);
	connect(ui.action_Replace_Find_Again, &QAction::triggered, this, &MainWindow::action_Replace_Find_Again_triggered);
	connect(ui.action_Replace_Again, &QAction::triggered, this, &MainWindow::action_Replace_Again_triggered);
	connect(ui.action_Mark, &QAction::triggered, this, &MainWindow::action_Mark_triggered);
	connect(ui.action_Goto_Mark, &QAction::triggered, this, &MainWindow::action_Goto_Mark_triggered);
	connect(ui.action_Goto_Matching, &QAction::triggered, this, &MainWindow::action_Goto_Matching_triggered);
	connect(ui.action_Show_Calltip, &QAction::triggered, this, &MainWindow::action_Show_Calltip_triggered);
	connect(ui.action_Find_Definition, &QAction::triggered, this, &MainWindow::action_Find_Definition_triggered);
	connect(ui.action_Execute_Command, &QAction::triggered, this, &MainWindow::action_Execute_Command_triggered);
	connect(ui.action_Execute_Command_Line, &QAction::triggered, this, &MainWindow::action_Execute_Command_Line_triggered);
	connect(ui.action_Filter_Selection, &QAction::triggered, this, &MainWindow::action_Filter_Selection_triggered);
	connect(ui.action_Cancel_Shell_Command, &QAction::triggered, this, &MainWindow::action_Cancel_Shell_Command_triggered);
	connect(ui.action_Detach_Tab, &QAction::triggered, this, &MainWindow::action_Detach_Tab_triggered);
	connect(ui.action_Split_Pane, &QAction::triggered, this, &MainWindow::action_Split_Pane_triggered);
	connect(ui.action_Close_Pane, &QAction::triggered, this, &MainWindow::action_Close_Pane_triggered);
	connect(ui.action_Move_Tab_To, &QAction::triggered, this, &MainWindow::action_Move_Tab_To_triggered);
	connect(ui.action_Wrap_Margin, &QAction::triggered, this, &MainWindow::action_Wrap_Margin_triggered);
	connect(ui.action_Tab_Stops, &QAction::triggered, this, &MainWindow::action_Tab_Stops_triggered);
	connect(ui.action_Text_Fonts, &QAction::triggered, this, &MainWindow::action_Text_Fonts_triggered);
	connect(ui.action_Save_Defaults, &QAction::triggered, this, &MainWindow::action_Save_Defaults_triggered);
	connect(ui.action_Default_Language_Modes, &QAction::triggered, this, &MainWindow::action_Default_Language_Modes_triggered);
	connect(ui.action_Default_Program_Smart_Indent, &QAction::triggered, this, &MainWindow::action_Default_Program_Smart_Indent_triggered);
	connect(ui.action_Default_Wrap_Margin, &QAction::triggered, this, &MainWindow::action_Default_Wrap_Margin_triggered);
	connect(ui.action_Default_Command_Shell, &QAction::triggered, this, &MainWindow::action_Default_Command_Shell_triggered);
	connect(ui.action_Default_Tab_Stops, &QAction::triggered, this, &MainWindow::action_Default_Tab_Stops_triggered);
	connect(ui.action_Default_Text_Fonts, &QAction::triggered, this, &MainWindow::action_Default_Text_Fonts_triggered);
	connect(ui.action_Default_Colors, &QAction::triggered, this, &MainWindow::action_Default_Colors_triggered);
	connect(ui.action_Default_Shell_Menu, &QAction::triggered, this, &MainWindow::action_Default_Shell_Menu_triggered);
	connect(ui.action_Default_Macro_Menu, &QAction::triggered, this, &MainWindow::action_Default_Macro_Menu_triggered);
	connect(ui.action_Default_Window_Background_Menu, &QAction::triggered, this, &MainWindow::action_Default_Window_Background_Menu_triggered);
	connect(ui.action_Default_Customize_Window_Title, &QAction::triggered, this, &MainWindow::action_Default_Customize_Window_Title_triggered);
	connect(ui.action_Default_Syntax_Recognition_Patterns, &QAction::triggered, this, &MainWindow::action_Default_Syntax_Recognition_Patterns_triggered);
	connect(ui.action_Default_Syntax_Text_Drawing_Styles, &QAction::triggered, this, &MainWindow::action_Default_Syntax_Text_Drawing_Styles_triggered);
	connect(ui.action_Learn_Keystrokes, &QAction::triggered, this, &MainWindow::action_Learn_Keystrokes_triggered);
	connect(ui.action_Finish_Learn, &QAction::triggered, this, &MainWindow::action_Finish_Learn_triggered);
	connect(ui.action_Cancel_Learn, &QAction::triggered, this, &MainWindow::action_Cancel_Learn_triggered);
	connect(ui.action_Repeat, &QAction::triggered, this, &MainWindow::action_Repeat_triggered);
	connect(ui.action_Replay_Keystrokes, &QAction::triggered, this, &MainWindow::action_Replay_Keystrokes_triggered);
	connect(ui.action_About, &QAction::triggered, this, &MainWindow::action_About_triggered);
	connect(ui.action_About_Qt, &QAction::triggered, this, &MainWindow::action_About_Qt_triggered);
	connect(ui.action_Help, &QAction::triggered, this, &MainWindow::action_Help_triggered);

	connect(ui.action_Statistics_Line, &QAction::toggled, this, &MainWindow::action_Statistics_Line_toggled);
	connect(ui.action_Incremental_Search_Line, &QAction::toggled, this, &MainWindow::action_Incremental_Search_Line_toggled);
	connect(ui.action_Show_Line_Numbers, &QAction::toggled, this, &MainWindow::action_Show_Line_Numbers_toggled);
	connect(ui.action_Highlight_Syntax, &QAction::toggled, this, &MainWindow::action_Highlight_Syntax_toggled);
	connect(ui.action_Apply_Backlighting, &QAction::toggled, this, &MainWindow::action_Apply_Backlighting_toggled);
	connect(ui.action_Make_Backup_Copy, &QAction::toggled, this, &MainWindow::action_Make_Backup_Copy_toggled);
	connect(ui.action_Incremental_Backup, &QAction::toggled, this, &MainWindow::action_Incremental_Backup_toggled);
	connect(ui.action_Matching_Syntax, &QAction::toggled, this, &MainWindow::action_Matching_Syntax_toggled);
	connect(ui.action_Overtype, &QAction::toggled, this, &MainWindow::action_Overtype_toggled);
	connect(ui.action_Read_Only, &QAction::toggled, this, &MainWindow::action_Read_Only_toggled);
	connect(ui.action_Default_Sort_Open_Prev_Menu, &QAction::toggled, this, &MainWindow::action_Default_Sort_Open_Prev_Menu_toggled);
	connect(ui.action_Default_Show_Path_In_Windows_Menu, &QAction::toggled, this, &MainWindow::action_Default_Show_Path_In_Windows_Menu_toggled);
	connect(ui.action_Default_Search_Verbose, &QAction::toggled, this, &MainWindow::action_Default_Search_Verbose_toggled);
	connect(ui.action_Default_Search_Wrap_Around, &QAction::toggled, this, &MainWindow::action_Default_Search_Wrap_Around_toggled);
	connect(ui.action_Default_Search_Beep_On_Search_Wrap, &QAction::toggled, this, &MainWindow::action_Default_Search_Beep_On_Search_Wrap_toggled);
	connect(ui.action_Default_Search_Keep_Dialogs_Up, &QAction::toggled, this, &MainWindow::action_Default_Search_Keep_Dialogs_Up_toggled);
	connect(ui.action_Default_Apply_Backlighting, &QAction::toggled, this, &MainWindow::action_Default_Apply_Backlighting_toggled);
	connect(ui.action_Default_Tab_Open_File_In_New_Tab, &QAction::toggled, this, &MainWindow::action_Default_Tab_Open_File_In_New_Tab_toggled);
	connect(ui.action_Default_Tab_Show_Tab_Bar, &QAction::toggled, this, &MainWindow::action_Default_Tab_Show_Tab_Bar_toggled);
	connect(ui.action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open, &QAction::toggled, this, &MainWindow::action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open_toggled);
	connect(ui.action_Default_Tab_Next_Prev_Tabs_Across_Windows, &QAction::toggled, this, &MainWindow::action_Default_Tab_Next_Prev_Tabs_Across_Windows_toggled);
	connect(ui.action_Default_Tab_Sort_Tabs_Alphabetically, &QAction::toggled, this, &MainWindow::action_Default_Tab_Sort_Tabs_Alphabetically_toggled);
	connect(ui.action_Default_Show_Tooltips, &QAction::toggled, this, &MainWindow::action_Default_Show_Tooltips_toggled);
	connect(ui.action_Default_Statistics_Line, &QAction::toggled, this, &MainWindow::action_Default_Statistics_Line_toggled);
	connect(ui.action_Default_Incremental_Search_Line, &QAction::toggled, this, &MainWindow::action_Default_Incremental_Search_Line_toggled);
	connect(ui.action_Default_Show_Line_Numbers, &QAction::toggled, this, &MainWindow::action_Default_Show_Line_Numbers_toggled);
	connect(ui.action_Default_Make_Backup_Copy, &QAction::toggled, this, &MainWindow::action_Default_Make_Backup_Copy_toggled);
	connect(ui.action_Default_Incremental_Backup, &QAction::toggled, this, &MainWindow::action_Default_Incremental_Backup_toggled);
	connect(ui.action_Default_Matching_Syntax_Based, &QAction::toggled, this, &MainWindow::action_Default_Matching_Syntax_Based_toggled);
	connect(ui.action_Default_Terminate_with_Line_Break_on_Save, &QAction::toggled, this, &MainWindow::action_Default_Terminate_with_Line_Break_on_Save_toggled);
	connect(ui.action_Default_Popups_Under_Pointer, &QAction::toggled, this, &MainWindow::action_Default_Popups_Under_Pointer_toggled);
	connect(ui.action_Default_Auto_Scroll_Near_Window_Top_Bottom, &QAction::toggled, this, &MainWindow::action_Default_Auto_Scroll_Near_Window_Top_Bottom_toggled);
	connect(ui.action_Default_Warnings_Files_Modified_Externally, &QAction::toggled, this, &MainWindow::action_Default_Warnings_Files_Modified_Externally_toggled);
	connect(ui.action_Default_Warnings_Check_Modified_File_Contents, &QAction::toggled, this, &MainWindow::action_Default_Warnings_Check_Modified_File_Contents_toggled);
	connect(ui.action_Default_Warnings_On_Exit, &QAction::toggled, this, &MainWindow::action_Default_Warnings_On_Exit_toggled);

#ifdef PER_TAB_CLOSE
	connect(ui.tabWidget, &TabWidget::tabCloseRequested, this, &MainWindow::tabWidget_tabCloseRequested);
#endif
	connect(ui.tabWidget, &TabWidget::tabCountChanged, this, &MainWindow::tabWidget_tabCountChanged);
	connect(ui.tabWidget, &TabWidget::currentChanged, this, &MainWindow::tabWidget_currentChanged);
	connect(ui.tabWidget, &TabWidget::customContextMenuRequested, this, &MainWindow::tabWidget_customContextMenuRequested);
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
		geometry = Preferences::GetPrefGeometry();
	}

	if (geometry.isEmpty()) {
		rows = Preferences::GetPrefRows();
		cols = Preferences::GetPrefCols();
	} else {
		static const QRegularExpression re(QLatin1String("(?:([0-9]+)(?:[xX]([0-9]+)(?:([\\+-][0-9]+)(?:([\\+-][0-9]+))?)?)?)?"));
		const QRegularExpressionMatch match = re.match(geometry);
		if (match.hasMatch()) {
			cols = match.captured(1).toInt();
			rows = match.captured(2).toInt();
		}
	}

	if (DocumentWidget *document = currentDocument()) {
		QFontMetrics fm(document->font_);

		const int extent = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);

		TextArea *area   = document->firstPane();
		const QMargins m = area->getMargins();

		const int w = extent + (Font::maxWidth(fm) * cols) + m.left() + m.right() + area->lineNumberAreaWidth();
		const int h = extent + (fm.ascent() + fm.descent()) * rows + m.top() + m.bottom();

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
#ifndef PER_TAB_CLOSE
	// create and hook up the tab close button
	auto deleteTabButton = new QToolButton(ui.tabWidget);
	deleteTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	deleteTabButton->setIcon(QIcon::fromTheme(QLatin1String("tab-close")));
	deleteTabButton->setAutoRaise(true);
	deleteTabButton->setFocusPolicy(Qt::NoFocus);
	deleteTabButton->setObjectName(tr("tab-close"));
	ui.tabWidget->setCornerWidget(deleteTabButton);

	connect(deleteTabButton, &QToolButton::clicked, this, &MainWindow::action_Close_triggered);
#else
	ui.tabWidget->setTabsClosable(true);
#endif
	ui.tabWidget->tabBar()->installEventFilter(this);
	ui.editIFind->installEventFilter(this);
}

/**
 * @brief MainWindow::setupGlobalPrefenceDefaults
 */
void MainWindow::setupGlobalPrefenceDefaults() {

	// Default Indent
	switch (Preferences::GetPrefAutoIndent(PLAIN_LANGUAGE_MODE)) {
	case IndentStyle::None:
		ui.action_Default_Indent_Off->setChecked(true);
		break;
	case IndentStyle::Auto:
		ui.action_Default_Indent_On->setChecked(true);
		break;
	case IndentStyle::Smart:
		ui.action_Default_Indent_Smart->setChecked(true);
		break;
	default:
		break;
	}

	// Default Wrap
	switch (Preferences::GetPrefWrap(PLAIN_LANGUAGE_MODE)) {
	case WrapStyle::None:
		ui.action_Default_Wrap_None->setChecked(true);
		break;
	case WrapStyle::Newline:
		ui.action_Default_Wrap_Auto_Newline->setChecked(true);
		break;
	case WrapStyle::Continuous:
		ui.action_Default_Wrap_Continuous->setChecked(true);
		break;
	default:
		break;
	}

	// Default Search Settings
	no_signals(ui.action_Default_Search_Verbose)->setChecked(Preferences::GetPrefSearchDlogs());
	no_signals(ui.action_Default_Search_Wrap_Around)->setChecked(Preferences::GetPrefSearchWraps() == WrapMode::Wrap);
	no_signals(ui.action_Default_Search_Beep_On_Search_Wrap)->setChecked(Preferences::GetPrefBeepOnSearchWrap());
	no_signals(ui.action_Default_Search_Keep_Dialogs_Up)->setChecked(Preferences::GetPrefKeepSearchDlogs());

	switch (Preferences::GetPrefSearch()) {
	case SearchType::Literal:
		ui.action_Default_Search_Literal->setChecked(true);
		break;
	case SearchType::CaseSense:
		ui.action_Default_Search_Literal_Case_Sensitive->setChecked(true);
		break;
	case SearchType::LiteralWord:
		ui.action_Default_Search_Literal_Whole_Word->setChecked(true);
		break;
	case SearchType::CaseSenseWord:
		ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word->setChecked(true);
		break;
	case SearchType::Regex:
		ui.action_Default_Search_Regular_Expression->setChecked(true);
		break;
	case SearchType::RegexNoCase:
		ui.action_Default_Search_Regular_Expresison_Case_Insensitive->setChecked(true);
		break;
	}

	// Default syntax
	if (Preferences::GetPrefHighlightSyntax()) {
		ui.action_Default_Syntax_On->setChecked(true);
	} else {
		ui.action_Default_Syntax_Off->setChecked(true);
	}

	no_signals(ui.action_Default_Apply_Backlighting)->setChecked(Preferences::GetPrefBacklightChars());

	// Default tab settings
	no_signals(ui.action_Default_Tab_Open_File_In_New_Tab)->setChecked(Preferences::GetPrefOpenInTab());
	no_signals(ui.action_Default_Tab_Show_Tab_Bar)->setChecked(Preferences::GetPrefTabBar());
	no_signals(ui.action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open)->setChecked(Preferences::GetPrefTabBarHideOne());
	no_signals(ui.action_Default_Tab_Next_Prev_Tabs_Across_Windows)->setChecked(Preferences::GetPrefGlobalTabNavigate());
	no_signals(ui.action_Default_Tab_Sort_Tabs_Alphabetically)->setChecked(Preferences::GetPrefSortTabs());

	// enable moving tabs if they aren't sortable
	tabWidget()->tabBar()->setMovable(!Preferences::GetPrefSortTabs());

	no_signals(ui.tabWidget)->tabBar()->setVisible(Preferences::GetPrefTabBar());
	ui.tabWidget->setTabBarAutoHide(Preferences::GetPrefTabBarHideOne());

	no_signals(ui.action_Default_Show_Tooltips)->setChecked(Preferences::GetPrefToolTips());
	no_signals(ui.action_Default_Statistics_Line)->setChecked(Preferences::GetPrefStatsLine());
	no_signals(ui.action_Default_Incremental_Search_Line)->setChecked(Preferences::GetPrefISearchLine());
	no_signals(ui.action_Default_Show_Line_Numbers)->setChecked(Preferences::GetPrefLineNums());
	no_signals(ui.action_Default_Make_Backup_Copy)->setChecked(Preferences::GetPrefSaveOldVersion());
	no_signals(ui.action_Default_Incremental_Backup)->setChecked(Preferences::GetPrefAutoSave());

	switch (Preferences::GetPrefShowMatching()) {
	case ShowMatchingStyle::None:
		ui.action_Default_Matching_Off->setChecked(true);
		break;
	case ShowMatchingStyle::Delimiter:
		ui.action_Default_Matching_Delimiter->setChecked(true);
		break;
	case ShowMatchingStyle::Range:
		ui.action_Default_Matching_Range->setChecked(true);
		break;
	}

	no_signals(ui.action_Default_Matching_Syntax_Based)->setChecked(Preferences::GetPrefMatchSyntaxBased());
	no_signals(ui.action_Default_Terminate_with_Line_Break_on_Save)->setChecked(Preferences::GetPrefAppendLF());
	no_signals(ui.action_Default_Popups_Under_Pointer)->setChecked(Preferences::GetPrefRepositionDialogs());
	no_signals(ui.action_Default_Auto_Scroll_Near_Window_Top_Bottom)->setChecked(Preferences::GetPrefAutoScroll());
	no_signals(ui.action_Default_Warnings_Files_Modified_Externally)->setChecked(Preferences::GetPrefWarnFileMods());
	no_signals(ui.action_Default_Warnings_Check_Modified_File_Contents)->setChecked(Preferences::GetPrefWarnRealFileMods());
	no_signals(ui.action_Default_Warnings_On_Exit)->setChecked(Preferences::GetPrefWarnExit());
	ui.action_Default_Warnings_Check_Modified_File_Contents->setEnabled(Preferences::GetPrefWarnFileMods());

	no_signals(ui.action_Default_Sort_Open_Prev_Menu)->setChecked(Preferences::GetPrefSortOpenPrevMenu());
	no_signals(ui.action_Default_Show_Path_In_Windows_Menu)->setChecked(Preferences::GetPrefShowPathInWindowsMenu());
}

/**
 * @brief MainWindow::setupDocumentPreferenceDefaults
 */
void MainWindow::setupDocumentPreferenceDefaults() {

	// based on document, which defaults to this
	switch (Preferences::GetPrefAutoIndent(PLAIN_LANGUAGE_MODE)) {
	case IndentStyle::None:
		ui.action_Indent_Off->setChecked(true);
		break;
	case IndentStyle::Auto:
		ui.action_Indent_On->setChecked(true);
		break;
	case IndentStyle::Smart:
		ui.action_Indent_Smart->setChecked(true);
		break;
	default:
		break;
	}

	// based on document, which defaults to this
	switch (Preferences::GetPrefWrap(PLAIN_LANGUAGE_MODE)) {
	case WrapStyle::None:
		ui.action_Wrap_None->setChecked(true);
		break;
	case WrapStyle::Newline:
		ui.action_Wrap_Auto_Newline->setChecked(true);
		break;
	case WrapStyle::Continuous:
		ui.action_Wrap_Continuous->setChecked(true);
		break;
	default:
		break;
	}

	switch (Preferences::GetPrefShowMatching()) {
	case ShowMatchingStyle::None:
		ui.action_Matching_Off->setChecked(true);
		break;
	case ShowMatchingStyle::Delimiter:
		ui.action_Matching_Delimiter->setChecked(true);
		break;
	case ShowMatchingStyle::Range:
		ui.action_Matching_Range->setChecked(true);
		break;
	}

	if (Preferences::GetPrefSmartTags()) {
		ui.action_Default_Tag_Smart->setChecked(true);
	} else {
		ui.action_Default_Tag_Show_All->setChecked(true);
	}
}

/**
 * @brief MainWindow::setupMenuDefaults
 */
void MainWindow::setupMenuDefaults() {

	// active settings
	no_signals(ui.action_Statistics_Line)->setChecked(Preferences::GetPrefStatsLine());
	no_signals(ui.action_Incremental_Search_Line)->setChecked(Preferences::GetPrefISearchLine());
	no_signals(ui.action_Show_Line_Numbers)->setChecked(Preferences::GetPrefLineNums());
	no_signals(ui.action_Highlight_Syntax)->setChecked(Preferences::GetPrefHighlightSyntax());
	no_signals(ui.action_Apply_Backlighting)->setChecked(Preferences::GetPrefBacklightChars());
	no_signals(ui.action_Make_Backup_Copy)->setChecked(Preferences::GetPrefAutoSave());
	no_signals(ui.action_Incremental_Backup)->setChecked(Preferences::GetPrefSaveOldVersion());
	no_signals(ui.action_Matching_Syntax)->setChecked(Preferences::GetPrefMatchSyntaxBased());

	setupGlobalPrefenceDefaults();
	setupDocumentPreferenceDefaults();

	updateWindowSizeMenu();
	updateTipsFileMenu();
	updateTagsFileMenu();
	MainWindow::updateMenuItems();
}

template <class Func>
QShortcut *create_shortcut(const QKeySequence &seq, QWidget *parent, Func func) {
	auto shortcut = new QShortcut(seq, parent);
	QObject::connect(shortcut, &QShortcut::activated, parent, func);
	return shortcut;
}

/**
 * nedit has some menu shortcuts which are different from conventional
 * shortcuts. Fortunately, Qt has a means to do this stuff manually.
 *
 * @brief MainWindow::setupMenuStrings
 */
void MainWindow::setupMenuStrings() {

	ui.action_Shift_Left->setText(tr("Shift &Left\t[Shift] Ctrl+9"));
	ui.action_Shift_Right->setText(tr("Shift Ri&ght\t[Shift] Ctrl+0"));
	ui.action_Find->setText(tr("&Find...\t[Shift] Ctrl+F"));
	ui.action_Find_Again->setText(tr("F&ind Again\t[Shift] Ctrl+G"));
	ui.action_Find_Selection->setText(tr("Find &Selection\t[Shift] Ctrl+H"));
	ui.action_Find_Incremental->setText(tr("Fi&nd Incremental\t[Shift] Ctrl+I"));
	ui.action_Replace->setText(tr("&Replace...\t[Shift] Ctrl+R"));
	ui.action_Replace_Find_Again->setText(tr("Replace Find &Again\t[Shift] Ctrl+T"));
	ui.action_Replace_Again->setText(tr("Re&place Again\t[Shift] Alt+T"));
	ui.action_Mark->setText(tr("Mar&k\tAlt+M a-z"));
	ui.action_Goto_Mark->setText(tr("G&oto Mark\t[Shift] Alt+G a-z"));
	ui.action_Goto_Matching->setText(tr("Goto &Matching (..)\t[Shift] Ctrl+M"));

	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_9), this, [this]() { action_Shift_Left_Tabs(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_0), this, [this]() { action_Shift_Right_Tabs(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F), this, [this]() { action_Shift_Find(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_G), this, [this]() { action_Shift_Find_Again(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_H), this, [this]() { action_Shift_Find_Selection(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I), this, [this]() { action_Shift_Find_Incremental(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_R), this, [this]() { action_Shift_Replace(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M), this, [this]() { action_Shift_Goto_Matching(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Y), this, [this]() { action_Shift_Open_Selected(); });

	// This is an annoying solution... we can probably do better...
	for (int key = Qt::Key_A; key <= Qt::Key_Z; ++key) {
		create_shortcut(QKeySequence(Qt::ALT + Qt::Key_M, key), this, [this]() { action_Mark_Shortcut(); });
		create_shortcut(QKeySequence(Qt::ALT + Qt::Key_G, key), this, [this]() { action_Goto_Mark_Shortcut(); });
		create_shortcut(QKeySequence(Qt::SHIFT + Qt::ALT + Qt::Key_G, key), this, [this]() { action_Shift_Goto_Mark_Shortcut(); });
	}

	create_shortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown), this, [this]() { action_Next_Document(); });
	create_shortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp), this, [this]() { action_Prev_Document(); });
	create_shortcut(QKeySequence(Qt::ALT + Qt::Key_Home), this, [this]() { action_Last_Document(); });
}

/**
 * @brief MainWindow::setupPrevOpenMenuActions
 */
void MainWindow::setupPrevOpenMenuActions() {
	const int maxPrevOpenFiles = Preferences::GetPrefMaxPrevOpenFiles();

	if (maxPrevOpenFiles <= 0) {
		ui.action_Open_Previous->setEnabled(false);
	} else {

		// create all of the actions
		for (int i = 0; i < maxPrevOpenFiles; ++i) {
			auto action = new QAction(this);
			action->setVisible(true);
			previousOpenFilesList_.push_back(action);
		}

		auto prevMenu = new QMenu(this);

		prevMenu->addActions(previousOpenFilesList_);

		connect(prevMenu, &QMenu::triggered, this, [this](QAction *action) {
			auto filename = action->data().toString();

			if (DocumentWidget *document = currentDocument()) {
				if (filename.isNull()) {
					return;
				}

				if (!document->open(filename)) {
					int r = QMessageBox::question(
						this,
						tr("Error Opening File"),
						tr("File could not be opened, would you like to remove it from the 'Open Previous' list?"),
						QMessageBox::Yes | QMessageBox::No);

					if (r == QMessageBox::Yes) {
						removeFromPrevOpenMenu(filename);
						MainWindow::invalidatePrevOpenMenus();
					}
				}
			}

			MainWindow::checkCloseEnableState();
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
	if (!Preferences::GetPrefOpenInTab()) {
		ui.action_New_Window->setText(tr("New &Tab"));
	} else {
		ui.action_New_Window->setText(tr("New &Window"));
	}
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

	connect(indentGroup, &QActionGroup::triggered, this, &MainWindow::indentGroupTriggered);
	connect(wrapGroup, &QActionGroup::triggered, this, &MainWindow::wrapGroupTriggered);
	connect(matchingGroup, &QActionGroup::triggered, this, &MainWindow::matchingGroupTriggered);
	connect(defaultIndentGroup, &QActionGroup::triggered, this, &MainWindow::defaultIndentGroupTriggered);
	connect(defaultWrapGroup, &QActionGroup::triggered, this, &MainWindow::defaultWrapGroupTriggered);
	connect(defaultTagCollisionsGroup, &QActionGroup::triggered, this, &MainWindow::defaultTagCollisionsGroupTriggered);
	connect(defaultSearchGroup, &QActionGroup::triggered, this, &MainWindow::defaultSearchGroupTriggered);
	connect(defaultSyntaxGroup, &QActionGroup::triggered, this, &MainWindow::defaultSyntaxGroupTriggered);
	connect(defaultMatchingGroup, &QActionGroup::triggered, this, &MainWindow::defaultMatchingGroupTriggered);
	connect(defaultSizeGroup, &QActionGroup::triggered, this, &MainWindow::defaultSizeGroupTriggered);
}

/**
 * @brief MainWindow::action_New_triggered
 */
void MainWindow::action_New_triggered() {
	if (DocumentWidget *document = currentDocument()) {
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

	const bool openInTab = [mode]() {
		switch (mode) {
		case NewMode::Prefs:
			return Preferences::GetPrefOpenInTab();
		case NewMode::Tab:
			return true;
		case NewMode::Window:
			return false;
		case NewMode::Opposite:
			return !Preferences::GetPrefOpenInTab();
		}
		Q_UNREACHABLE();
	}();

	MainWindow::editNewFile(openInTab ? this : nullptr, QString(), /*iconic=*/false, QString(), QDir(document->path()));
	MainWindow::checkCloseEnableState();
}

/**
 * @brief MainWindow::action_Open
 * @param document
 * @param filename
 */
void MainWindow::action_Open(DocumentWidget *document, const QString &filename) {

	emit_event("open", filename);
	document->open(filename);
	MainWindow::checkCloseEnableState();
}

/**
 * @brief MainWindow::action_Open
 * @param document
 */
void MainWindow::action_Open(DocumentWidget *document) {
	QStringList filenames = promptForExistingFiles(this, document->path(), tr("Open File"), QFileDialog::ExistingFiles);
	if (filenames.isEmpty()) {
		return;
	}

	for (const QString &filename : filenames) {
		action_Open(document, filename);
	}
}

/**
 * @brief MainWindow::action_Open_triggered
 */
void MainWindow::action_Open_triggered() {

	if (DocumentWidget *document = currentDocument()) {
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
 * @brief MainWindow::action_Close_triggered
 */
void MainWindow::action_Close_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Close(document, CloseMode::Prompt);
	}
}

/**
 * @brief MainWindow::action_About_triggered
 */
void MainWindow::action_About_triggered() {
	auto dialog = std::make_unique<DialogAbout>(this);
	dialog->exec();
}

/**
 * @brief MainWindow::action_Select_All
 * @param document
 */
void MainWindow::action_Select_All(DocumentWidget *document) {

	emit_event("select_all");
	document->firstPane()->selectAllAP();
}

/**
 * @brief MainWindow::action_Select_All_triggered
 */
void MainWindow::action_Select_All_triggered() {

	if (DocumentWidget *document = currentDocument()) {
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

	if (document->checkReadOnly()) {
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
	if (document->checkReadOnly()) {
		return;
	}

	QStringList filenames = promptForExistingFiles(this, document->path(), tr("Include File"), QFileDialog::ExistingFile);

	if (filenames.isEmpty()) {
		return;
	}

	document->includeFile(filenames[0]);
}

/**
 * @brief MainWindow::action_Include_File_triggered
 */
void MainWindow::action_Include_File_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Include_File(document);
	}
}

/**
 * @brief MainWindow::action_Cut_triggered
 */
void MainWindow::action_Cut_triggered() {
	if (QPointer<TextArea> area = lastFocus()) {
		area->cutClipboard();
	}
}

/**
 * @brief MainWindow::action_Copy_triggered
 */
void MainWindow::action_Copy_triggered() {
	if (QPointer<TextArea> area = lastFocus()) {
		area->copyClipboard();
	}
}

/**
 * @brief MainWindow::action_Paste_triggered
 */
void MainWindow::action_Paste_triggered() {
	if (QPointer<TextArea> area = lastFocus()) {
		area->pasteClipboard();
	}
}

/**
 * @brief MainWindow::action_Paste_Column_triggered
 */
void MainWindow::action_Paste_Column_triggered() {
	if (QPointer<TextArea> area = lastFocus()) {
		area->pasteClipboard(TextArea::RectFlag);
	}
}

/**
 * @brief MainWindow::action_Delete
 * @param document
 */
void MainWindow::action_Delete(DocumentWidget *document) {

	emit_event("delete");

	if (document->checkReadOnly()) {
		return;
	}

	document->buffer()->BufRemoveSelected();
}

/**
 * @brief MainWindow::action_Delete_triggered
 */
void MainWindow::action_Delete_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Delete(document);
	}
}

/**
 * @brief MainWindow::createDocument
 * @param name
 * @return
 */
DocumentWidget *MainWindow::createDocument(const QString &name) {
	auto document = new DocumentWidget(name, this);

	connect(document, &DocumentWidget::canUndoChanged, this, &MainWindow::undoAvailable);
	connect(document, &DocumentWidget::canRedoChanged, this, &MainWindow::redoAvailable);
	connect(document, &DocumentWidget::updateStatus, this, &MainWindow::updateStatus);
	connect(document, &DocumentWidget::updateWindowReadOnly, this, &MainWindow::updateWindowReadOnly);
	connect(document, &DocumentWidget::updateWindowTitle, this, &MainWindow::updateWindowTitle);
	connect(document, &DocumentWidget::fontChanged, this, &MainWindow::updateWindowHints);

	ui.tabWidget->addTab(document, name);
	return document;
}

/**
 * @brief MainWindow::undoAvailable
 * @param available
 */
void MainWindow::undoAvailable(bool available) {
	ui.action_Undo->setEnabled(available);
}

/**
 * @brief MainWindow::redoAvailable
 * @param available
 */
void MainWindow::redoAvailable(bool available) {
	ui.action_Redo->setEnabled(available);
}

/**
 * @brief MainWindow::selectionChanged
 * @param document
 * @param selected
 */
void MainWindow::selectionChanged(bool selected) {

	ui.action_Print_Selection->setEnabled(selected);
	ui.action_Cut->setEnabled(selected);
	ui.action_Copy->setEnabled(selected);
	ui.action_Delete->setEnabled(selected);

	/* Note we don't change the selection for items like "Open Selected" and
	 * "Find Selected". That's because it works on selections in external
	 * applications. Disabling it if there's no NEdit selection disables
	 * this feature. */
	ui.action_Filter_Selection->setEnabled(selected);
}

/**
 * @brief MainWindow::updateWindowMenus
 */
void MainWindow::updateWindowMenus() {

	for (MainWindow *window : MainWindow::allWindows()) {
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

	std::sort(documents.begin(), documents.end(), [](const DocumentWidget *a, const DocumentWidget *b) {
		// Untitled first, then filename and path are considered
		return std::tie(a->info_->filenameSet, a->info_->filename, a->info_->path) <
			   std::tie(b->info_->filenameSet, b->info_->filename, b->info_->path);
	});

	ui.menu_Windows->clear();
	ui.menu_Windows->addAction(ui.action_Split_Pane);
	ui.menu_Windows->addAction(ui.action_Close_Pane);
	ui.menu_Windows->addSeparator();
	ui.menu_Windows->addAction(ui.action_Detach_Tab);
	ui.menu_Windows->addAction(ui.action_Move_Tab_To);
	ui.menu_Windows->addSeparator();

	for (DocumentWidget *document : documents) {
		QString title = document->getWindowsMenuEntry();

		QAction *action = ui.menu_Windows->addAction(title);
		connect(action, &QAction::triggered, document, [document]() {
			document->raiseFocusDocumentWindow(true);
		});
	}
}

/**
 * @brief MainWindow::tabCount
 * @return
 */
size_t MainWindow::tabCount() const {
	return static_cast<size_t>(ui.tabWidget->count());
}

/*
** Sort tabs in the tab bar alphabetically, if demanded so.
*/
void MainWindow::sortTabBar() {

	if (!Preferences::GetPrefSortTabs()) {
		return;
	}

	// get a list of all the documents
	std::vector<DocumentWidget *> documents = openDocuments();

	// sort them first by filename, then by path
	std::sort(documents.begin(), documents.end(), [](const DocumentWidget *a, const DocumentWidget *b) {
		return std::tie(a->info_->filename, a->info_->path) < std::tie(b->info_->filename, b->info_->path);
	});

	// shuffle around the tabs to their new indexes
	int i = 0;
	for (DocumentWidget *document : documents) {
		int from = ui.tabWidget->indexOf(document);
		ui.tabWidget->tabBar()->moveTab(from, i);
		++i;
	}
}

/**
 * @brief MainWindow::allWindows
 * @return
 */
std::vector<MainWindow *> MainWindow::allWindows(bool includeInvisible) {

	const QWidgetList widgets = QApplication::topLevelWidgets();

	std::vector<MainWindow *> windows;
	windows.reserve(static_cast<size_t>(widgets.size()));

	for (QWidget *widget : widgets) {
		if (auto window = qobject_cast<MainWindow *>(widget)) {

			// only include visible windows, since we make windows scheduled for
			// delete inVisible
			if (window->isVisible() || includeInvisible) {
				windows.push_back(window);
			}
		}
	}

	return windows;
}

/**
 * @brief MainWindow::firstWindow
 * @return
 */
MainWindow *MainWindow::firstWindow() {

	const QWidgetList widgets = QApplication::topLevelWidgets();

	for (QWidget *widget : widgets) {
		if (auto window = qobject_cast<MainWindow *>(widget)) {
			return window;
		}
	}

	return nullptr;
}

/**
 * @brief MainWindow::openDocuments
 * @return
 */
std::vector<DocumentWidget *> MainWindow::openDocuments() const {

	const int count = ui.tabWidget->count();

	std::vector<DocumentWidget *> documents;
	documents.reserve(static_cast<size_t>(count));

	for (int i = 0; i < count; ++i) {
		if (auto document = documentAt(i)) {
			documents.push_back(document);
		}
	}
	return documents;
}

/*
** Make sure the close menu item is enabled/disabled appropriately for the
** current set of windows.  It should be disabled only for the last Untitled,
** unmodified, editor window, and enabled otherwise.
*/
void MainWindow::checkCloseEnableState(const std::vector<MainWindow *> &windows) {
	if (windows.empty()) {
		return;
	}

	if (windows.size() == 1) {
		MainWindow *window = windows[0];

		std::vector<DocumentWidget *> documents = window->openDocuments();

		if (window->tabCount() == 1 && !documents.front()->filenameSet() && !documents.front()->fileChanged()) {
			window->ui.action_Close->setEnabled(false);
		} else {
			window->ui.action_Close->setEnabled(true);
		}
	} else {
		// if there is more than one window, then by definition, more than one
		// document is open
		for (MainWindow *window : windows) {
			window->ui.action_Close->setEnabled(true);
		}
	}
}

void MainWindow::checkCloseEnableState() {
	const std::vector<MainWindow *> windows = MainWindow::allWindows();
	checkCloseEnableState(windows);
}

/*
** Create either the Shell menu, Macro menu or Background menu
*/
QMenu *MainWindow::createUserMenu(DocumentWidget *document, const gsl::span<MenuData> &data, CommandTypes type) {

	auto rootMenu = new QMenu(this);
	for (qulonglong i = 0; i < data.size(); ++i) {
		const MenuData &menuData = data[i];

		const std::vector<size_t> &modes = menuData.info->umiLanguageModes;

		bool found = modes.empty();

		if (!found) {
			found = std::any_of(modes.begin(), modes.end(), [document](size_t languageMode) {
				return languageMode == document->languageMode_;
			});
		}

		if (!found) {
			continue;
		}

		QMenu *parentMenu = rootMenu;
		QString name      = menuData.info->umiName;

		int index = 0;
		for (;;) {
			const int subSep = name.indexOf(QLatin1Char('>'), index);
			if (subSep == -1) {
				name = name.mid(index);

				if (type != CommandTypes::Shell) {
					// create the actual action or, if it represents one of our
					// *very* common entries make it equivalent to the global
					// QAction representing that task
					if (menuData.item.cmd.trimmed() == QLatin1String("cut_clipboard()")) {
						parentMenu->addAction(ui.action_Cut);
					} else if (menuData.item.cmd.trimmed() == QLatin1String("copy_clipboard()")) {
						parentMenu->addAction(ui.action_Copy);
					} else if (menuData.item.cmd.trimmed() == QLatin1String("paste_clipboard()")) {
						parentMenu->addAction(ui.action_Paste);
					} else if (menuData.item.cmd.trimmed() == QLatin1String("undo()")) {
						parentMenu->addAction(ui.action_Undo);
					} else if (menuData.item.cmd.trimmed() == QLatin1String("redo()")) {
						parentMenu->addAction(ui.action_Redo);
					} else {
						QAction *action = parentMenu->addAction(name);
						action->setData(i);

						if (!menuData.item.shortcut.isEmpty()) {
							action->setShortcut(menuData.item.shortcut);
						}

						// if this action REQUIRES a selection, default to disabled
						if (menuData.item.input == InSrcs::FROM_SELECTION) {
							action->setEnabled(false);
						}
					}
				} else {
					QAction *action = parentMenu->addAction(name);
					action->setData(i);

					if (!menuData.item.shortcut.isEmpty()) {
						action->setShortcut(menuData.item.shortcut);
					}

					// if this action REQUIRES a selection, default to disabled
					if (menuData.item.input == InSrcs::FROM_SELECTION) {
						action->setEnabled(false);
					}
				}
				break;
			}

			QString parentName  = name.mid(index, subSep);
			const int subSubSep = parentName.indexOf(QLatin1Char('>'));
			if (subSubSep != -1) {
				parentName = parentName.left(subSubSep);
			}

			QList<QAction *> actions = parentMenu->actions();
			QAction *parentAction    = nullptr;
			for (QAction *action : actions) {
				if (action->text() == parentName) {
					parentAction = action;
					break;
				}
			}

			if (!parentAction) {
				auto newMenu = new QMenu(parentName, parentMenu);
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

/*
** Update the Shell, Macro, and Window Background menus of window
** "window" from the currently loaded command descriptions.
*/
void MainWindow::updateUserMenus(DocumentWidget *document) {

	// update user menus, which are shared over all documents
	delete shellMenu_;
	shellMenu_ = createUserMenu(document, ShellMenuData, CommandTypes::Shell);
	ui.menu_Shell->clear();
	ui.menu_Shell->addAction(ui.action_Execute_Command);
	ui.menu_Shell->addAction(ui.action_Execute_Command_Line);
	ui.menu_Shell->addAction(ui.action_Filter_Selection);
	ui.menu_Shell->addAction(ui.action_Cancel_Shell_Command);
	ui.menu_Shell->addSeparator();
	ui.menu_Shell->addActions(shellMenu_->actions());

	delete shellGroup_;
	shellGroup_ = new QActionGroup(this);
	shellGroup_->setExclusive(false);
	addToGroup(shellGroup_, shellMenu_);
	connect(shellGroup_, &QActionGroup::triggered, this, &MainWindow::shellTriggered);

	delete macroMenu_;
	macroMenu_ = createUserMenu(document, MacroMenuData, CommandTypes::Macro);
	ui.menu_Macro->clear();
	ui.menu_Macro->addAction(ui.action_Learn_Keystrokes);
	ui.menu_Macro->addAction(ui.action_Finish_Learn);
	ui.menu_Macro->addAction(ui.action_Cancel_Learn);
	ui.menu_Macro->addAction(ui.action_Replay_Keystrokes);
	ui.menu_Macro->addAction(ui.action_Repeat);
	ui.menu_Macro->addSeparator();
	ui.menu_Macro->addActions(macroMenu_->actions());

	delete macroGroup_;
	macroGroup_ = new QActionGroup(this);
	macroGroup_->setExclusive(false);
	addToGroup(macroGroup_, macroMenu_);
	connect(macroGroup_, &QActionGroup::triggered, this, &MainWindow::macroTriggered);

	// update background menu, which is owned by a single document
	delete document->contextMenu_;
	document->contextMenu_ = createUserMenu(document, BGMenuData, CommandTypes::Context);

	// handler for BG menu scripts
	connect(document->contextMenu_, &QMenu::triggered, this, [this](QAction *action) {
		QVariant data = action->data();
		if (!data.isNull()) {

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
			if (DocumentWidget *document = currentDocument()) {
				if (document->macroCmdData_) {
					QApplication::beep();
					return;
				}

				const auto index   = data.toUInt();
				const QString name = BGMenuData[index].item.name;
				if (QPointer<TextArea> area = lastFocus()) {
					execNamedBGMenuCmd(document, area, name, CommandSource::User);
				}
			}
		}
	});
}

/**
 * @brief MainWindow::updateUserMenus
 */
void MainWindow::updateUserMenus() {
	if (DocumentWidget *document = currentDocument()) {
		updateUserMenus(document);
	}
}

/*
** Re-build the language mode sub-menu using the current data stored
** in the master list: LanguageModes.
*/
void MainWindow::updateLanguageModeSubmenu() {

	auto languageGroup   = new QActionGroup(this);
	auto languageMenu    = new QMenu(this);
	QAction *plainAction = languageMenu->addAction(QLatin1String("Plain"));
	plainAction->setData(static_cast<qulonglong>(PLAIN_LANGUAGE_MODE));
	plainAction->setCheckable(true);
	plainAction->setChecked(true);
	languageGroup->addAction(plainAction);

	for (size_t i = 0; i < Preferences::LanguageModes.size(); i++) {
		QAction *action = languageMenu->addAction(Preferences::LanguageModes[i].name);
		action->setData(static_cast<qulonglong>(i));
		action->setCheckable(true);
		languageGroup->addAction(action);
	}

	ui.action_Language_Mode->setMenu(languageMenu);

	connect(languageMenu, &QMenu::triggered, this, [this](QAction *action) {
		if (auto document = qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget())) {
			const auto mode = action->data().value<qulonglong>();

			// If the mode didn't change, do nothing
			if (document->languageMode_ == mode) {
				return;
			}

			if (mode == PLAIN_LANGUAGE_MODE) {
				document->action_Set_Language_Mode(QString());
			} else {
				document->action_Set_Language_Mode(Preferences::LanguageModes[mode].name);
			}
		}
	});
}

/*
** Create a submenu for chosing language mode for the current window.
*/
void MainWindow::createLanguageModeSubMenu() {
	updateLanguageModeSubmenu();
}

/**
 * @brief MainWindow::updateLineNumDisp
 * @return The number of columns required for the line number area.
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

	// Min. # of columns in line number display
	constexpr int MIN_LINE_NUM_COLS = 4;

	int reqCols = MIN_LINE_NUM_COLS;
	int maxCols = 0;

	std::vector<DocumentWidget *> documents = openDocuments();
	for (DocumentWidget *document : documents) {
		//  We found ourselves a document from this window. Pick a TextArea
		// (doesn't matter which), so we can probe some details...
		if (TextArea *area = document->firstPane()) {
			const int lineNumCols   = area->getLineNumCols();
			const int64_t lineCount = area->getBufferLinesCount();

			/* Is the width of the line number area sufficient to display all the
			   line numbers in the file?  If not, expand line number field, and the
			   this width. */
			maxCols = std::max(maxCols, lineNumCols);

			const int tmpReqCols = (lineCount < 1) ? 1 : static_cast<int>(::log10(static_cast<double>(lineCount) + 1)) + 1;

			reqCols = std::max(reqCols, tmpReqCols);
		}
	}

	for (DocumentWidget *document : documents) {
		if (TextArea *area = document->firstPane()) {
			int lineNumCols = area->getLineNumCols();
			if (lineNumCols == reqCols) {
				continue;
			}

			//  Update all panes of this document.
			for (TextArea *pane : document->textPanes()) {
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
void MainWindow::tempShowISearch(bool state) {
	if (showISearchLine_) {
		return;
	}

	ui.incrementalSearchFrame->setVisible(state);
}

/*
** Find a name for an untitled file, unique in the name space of in the opened
** files in this session, i.e. Untitled or Untitled_nn, and write it into
** the string "name".
*/
QString MainWindow::uniqueUntitledName() {

	const std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

	for (int i = 0; i < INT_MAX; i++) {

		const QString name = [i]() {
			if (i == 0) {
				return tr("Untitled");
			} else {
				return tr("Untitled_%1").arg(i);
			}
		}();

		auto it = std::find_if(documents.begin(), documents.end(), [name](DocumentWidget *document) {
			return document->filename() == name;
		});

		if (it == documents.end()) {
			return name;
		}
	}

	return QString();
}

/**
 * @brief MainWindow::action_Undo
 * @param document
 */
void MainWindow::action_Undo(DocumentWidget *document) {

	emit_event("undo");

	if (document->checkReadOnly()) {
		return;
	}

	document->undo();
}

/**
 * @brief MainWindow::action_Undo_triggered
 */
void MainWindow::action_Undo_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Undo(document);
	}
}

/**
 * @brief MainWindow::action_Redo
 * @param document
 */
void MainWindow::action_Redo(DocumentWidget *document) {

	emit_event("redo");

	if (document->checkReadOnly()) {
		return;
	}

	document->redo();
}

/**
 * @brief MainWindow::action_Redo_triggered
 */
void MainWindow::action_Redo_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Redo(document);
	}
}

DocumentWidget *MainWindow::findWindowWithFile(const PathInfo &path) {
	return findWindowWithFile(path.filename, path.pathname);
}

/*
** Check if there is already a window open for a given file
*/
DocumentWidget *MainWindow::findWindowWithFile(const QString &filename, const QString &path) {

	const std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

#ifdef Q_OS_UNIX
	if (!Preferences::GetPrefHonorSymlinks()) {

		auto fullname = QStringLiteral("%1%2").arg(path, filename);

		QT_STATBUF attribute;
		if (QT_STAT(fullname.toUtf8().data(), &attribute) == 0) {

			auto it = std::find_if(documents.begin(), documents.end(), [attribute](DocumentWidget *document) {
				return (attribute.st_dev == document->device()) && (attribute.st_ino == document->inode());
			});

			if (it != documents.end()) {
				return *it;
			}
		}
		/* else: Not an error condition, just a new file. Continue to check
		 * whether the filename is already in use for an unsaved document. */
	}
#endif

	auto it = std::find_if(documents.begin(), documents.end(), [filename, path](DocumentWidget *document) {
		return (document->filename() == filename) && (document->path() == path);
	});

	if (it != documents.end()) {
		return *it;
	}

	return nullptr;
}

/*
** Force ShowLineNumbers() to re-evaluate line counts for the window if line
** counts are required.
*/
void MainWindow::forceShowLineNumbers() {

	// we temporarily set showLineNumbers_ to false, because the ShowLineNumbers
	// function only has an effect if the state is changing
	if (std::exchange(showLineNumbers_, false)) {
		showLineNumbers(true);
	}
}

/*
** Turn on and off the display of line numbers
*/
void MainWindow::showLineNumbers(bool state) {

	showLineNumbers_ = state;

	/* Just setting showLineNumbers_ is sufficient to tell
	   updateLineNumDisp() to expand the line number areas and the this
	   size for the number of lines required.  To hide the line number
	   display, set the width to zero, and contract the this width. */
	const int reqCols = showLineNumbers_ ? updateLineNumDisp() : 0;

	/* line numbers panel is shell-level, hence other
	   tabbed documents in the this should sync */
	for (DocumentWidget *document : openDocuments()) {
		for (TextArea *area : document->textPanes()) {
			area->setLineNumCols(reqCols);
		}
	}
}

/*
** Add a file to the list of previously opened files for display in the
** File menu.
*/
void MainWindow::addToPrevOpenMenu(const QString &filename) {

	const int maxPrevOpenFiles = Preferences::GetPrefMaxPrevOpenFiles();

	// If the Open Previous command is disabled, just return
	if (maxPrevOpenFiles < 1) {
		return;
	}

	/*  Refresh list of previously opened files to avoid Big Race Condition,
		where two sessions overwrite each other's changes in NEdit's
		history file.
		Of course there is still Little Race Condition, which occurs if a
		Session A reads the list, then Session B reads the list and writes
		it before Session A gets a chance to write.  */
	MainWindow::readNEditDB();

	// If the name is already in the list, move it to the start
	const int index = PrevOpen.indexOf(filename);
	if (index != -1) {
		moveItem(PrevOpen, index, 0);
		MainWindow::invalidatePrevOpenMenus();
		MainWindow::writeNEditDB();
		return;
	}

	// If the list is already full, make room
	if (PrevOpen.size() >= maxPrevOpenFiles) {
		//  This is only safe if maxPrevOpenFiles > 0.
		PrevOpen.pop_back();
	}

	PrevOpen.push_front(filename);

	// Mark the Previously Opened Files menu as invalid in all windows
	MainWindow::invalidatePrevOpenMenus();

	// Undim the menu in all windows if it was previously empty
	if (!PrevOpen.isEmpty()) {
		for (MainWindow *window : MainWindow::allWindows()) {
			window->ui.action_Open_Previous->setEnabled(true);
		}
	}

	MainWindow::writeNEditDB();
}

/*
** Read database of file names for 'Open Previous' submenu.
**
** Eventually, this may hold window positions, and possibly file marks (in
** which case it should be moved to a different module) but for now it's
** just a list of previously opened files.
**
** This list is read once at startup and potentially refreshed before a
** new entry is about to be written to the file.
** If the file is modified since the last read (or not read before), it is
** read in, otherwise nothing is done.
*/
void MainWindow::readNEditDB() {

	static QDateTime lastNeditdbModTime;

	const int maxPrevOpenFiles = Preferences::GetPrefMaxPrevOpenFiles();

	/*  If the Open Previous command is disabled or the user set the
		resource to an (invalid) negative value, just return.  */
	if (maxPrevOpenFiles < 1) {
		return;
	}

	const QString historyFile = Settings::historyFile();
	if (historyFile.isNull()) {
		return;
	}

	const QFileInfo info(historyFile);
	const QDateTime mtime = info.lastModified();

	/*  Stat history file to see whether someone touched it after this
		session last changed it.  */
	if (lastNeditdbModTime >= mtime) {
		//  Do nothing, history file is unchanged.
		return;
	}

	//  Memorize modtime to compare to next time.
	lastNeditdbModTime = mtime;

	// open the file
	QFile file(historyFile);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}

	//  Clear previous list.
	PrevOpen.clear();

	/* read lines of the file, lines beginning with # are considered to be
	   comments and are thrown away. */
	QTextStream in(&file);
	while (!in.atEnd()) {
		QString line = in.readLine();

		if (line.isEmpty()) {
			// blank line
			continue;
		}

		if (line.startsWith(QLatin1Char('#'))) {
			// comment
			continue;
		}

		PrevOpen.push_back(line);

		if (PrevOpen.size() >= maxPrevOpenFiles) {
			// too many entries
			return;
		}
	}
}

/**
 * @brief MainWindow::invalidatePrevOpenMenus
 */
void MainWindow::invalidatePrevOpenMenus() {

	for (MainWindow *window : MainWindow::allWindows()) {
		window->updatePrevOpenMenu();
	}
}

/*
** Write dynamic database of file names for "Open Previous".  Eventually,
** this may hold window positions, and possibly file marks, in which case,
** it should be moved to a different module, but for now it's just a list
** of previously opened files.
*/
void MainWindow::writeNEditDB() {

	QString historyFile = Settings::historyFile();
	if (historyFile.isNull()) {
		return;
	}

	// If the Open Previous command is disabled, just return
	if (Preferences::GetPrefMaxPrevOpenFiles() < 1) {
		return;
	}

	QFile file(historyFile);
	if (file.open(QIODevice::WriteOnly)) {
		QTextStream ts(&file);

		static const QString fileHeader = tr("# File name database for NEdit Open Previous command");
		ts << fileHeader << '\n';

		// Write the list of file names
		Q_FOREACH (const QString &line, PrevOpen) {
			if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
				ts << line << '\n';
			}
		}
	}
}

/**
 * @brief MainWindow::removeFromPrevOpenMenu
 * @param filename
 */
void MainWindow::removeFromPrevOpenMenu(const QString &filename) {

	MainWindow::readNEditDB();
	PrevOpen.removeOne(filename);
	MainWindow::writeNEditDB();
}

/*
** Update the Previously Opened Files menu of a single window to reflect the
** current state of the list.
*/
void MainWindow::updatePrevOpenMenu() {

	if (Preferences::GetPrefMaxPrevOpenFiles() < 1) {
		ui.action_Open_Previous->setEnabled(false);
		return;
	}

	//  Read history file to get entries written by other sessions.
	MainWindow::readNEditDB();

	// Sort the previously opened file list if requested
	QVector<QString> prevOpenSorted = PrevOpen;

	if (Preferences::GetPrefSortOpenPrevMenu()) {
		std::sort(prevOpenSorted.begin(), prevOpenSorted.end());
	}

	int count = prevOpenSorted.size();

	for (int i = 0; i < count; ++i) {
		QString filename = prevOpenSorted[i];
		previousOpenFilesList_[i]->setText(filename);
		previousOpenFilesList_[i]->setData(filename);
		previousOpenFilesList_[i]->setVisible(true);
	}

	for (int i = count; i < previousOpenFilesList_.size(); ++i) {
		previousOpenFilesList_[i]->setVisible(false);
	}

	ui.action_Open_Previous->setEnabled(count != 0);
}

/**
 * @brief MainWindow::tabWidget_tabCountChanged
 * @param count
 */
void MainWindow::tabWidget_tabCountChanged(int count) {
	ui.action_Detach_Tab->setEnabled(count > 1);
}

/**
 * @brief MainWindow::tabWidget_currentChanged
 * @param index
 */
void MainWindow::tabWidget_currentChanged(int index) {
	if (index != -1) {
		if (DocumentWidget *document = documentAt(index)) {
			endISearch();
			document->documentRaised();
		}
	}
}

/**
 * @brief MainWindow::tabWidget_customContextMenuRequested
 * @param pos
 */
void MainWindow::tabWidget_customContextMenuRequested(const QPoint &pos) {
	const int index = ui.tabWidget->tabBar()->tabAt(pos);
	if (index != -1) {
		auto *menu              = new QMenu(this);
		QAction *const newTab   = menu->addAction(tr("New Tab"));
		QAction *const closeTab = menu->addAction(tr("Close Tab"));
		menu->addSeparator();
		QAction *const detachTab = menu->addAction(tr("Detach Tab"));
		QAction *const moveTab   = menu->addAction(tr("Move Tab To..."));

		// make sure that these are always in sync with the primary UI
		detachTab->setEnabled(ui.action_Detach_Tab->isEnabled());
		moveTab->setEnabled(ui.action_Move_Tab_To->isEnabled());
		closeTab->setEnabled(ui.action_Close->isEnabled());

		// make the icons the same too :-P
		newTab->setIcon(ui.action_New->icon());
		closeTab->setIcon(ui.action_Close->icon());
		detachTab->setIcon(ui.action_Detach_Tab->icon());
		moveTab->setIcon(ui.action_Move_Tab_To->icon());

		if (QAction *const selected = menu->exec(ui.tabWidget->tabBar()->mapToGlobal(pos))) {

			if (DocumentWidget *document = documentAt(index)) {

				if (selected == newTab) {
					action_New(document, NewMode::Prefs);
				} else if (selected == closeTab) {
					action_Close(document, CloseMode::Prompt);
				} else if (selected == detachTab) {
					action_Detach_Document(document);
				} else if (selected == moveTab) {
					action_Move_Tab_To(document);
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
	const QString selected = document->getAnySelection();
	if (!selected.isEmpty()) {
		openFile(document, selected);
	} else {
		QApplication::beep();
	}

	MainWindow::checkCloseEnableState();
}

/**
 * @brief MainWindow::action_Open_Selected_triggered
 */
void MainWindow::action_Open_Selected_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Open_Selected(document);
	}
}

/**
 * @brief MainWindow::action_Shift_Open_Selected
 */
void MainWindow::action_Shift_Open_Selected() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Open_Selected(document);
	}
}

/**
 * @brief MainWindow::action_Shift_Open_Selected
 * @param document
 */
void MainWindow::action_Shift_Open_Selected(DocumentWidget *document) {

	emit_event("open_copied");

	QString copied;

	// Get the clipboard text, if there's nothing copied, do nothing
	const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Clipboard);
	if (mimeData->hasText()) {
		copied = mimeData->text();
	}

	if (!copied.isEmpty()) {
		openFile(document, copied);
	} else {
		QApplication::beep();
	}

	MainWindow::checkCloseEnableState();
}

QFileInfoList MainWindow::openFileHelperSystem(DocumentWidget *document, const QRegularExpressionMatch &match, QString *searchPath, QString *searchName) const {
	const QStringList includeDirs = Preferences::GetPrefIncludePaths();

	QString text = match.captured(1);
	if (QFileInfo(text).isAbsolute()) {
		return openFileHelperString(document, text, searchPath, searchName);
	}

	QFileInfoList results;

	for (const QString &includeDir : includeDirs) {
		// we need to do this because someone could write #include <path/to/file.h>
		// which confuses QDir..
		QFileInfo fullPath = QStringLiteral("%1/%2").arg(includeDir, match.captured(1));
		QString filename   = fullPath.fileName();
		QString filepath   = fullPath.path();

		filepath = NormalizePathname(filepath);

		if (searchPath->isNull()) {
			*searchPath = filepath;
		}

		if (searchName->isNull()) {
			*searchName = filename;
		}

		QDir dir(filepath);
		QStringList name_filters    = {filename};
		QFileInfoList localFileList = dir.entryInfoList(name_filters, QDir::NoDotAndDotDot | QDir::Hidden | QDir::Files);
		results.append(localFileList);
	}

	return results;
}

QFileInfoList MainWindow::openFileHelperLocal(DocumentWidget *document, const QRegularExpressionMatch &match, QString *searchPath, QString *searchName) const {
	return openFileHelperString(document, match.captured(1), searchPath, searchName);
}

QFileInfoList MainWindow::openFileHelperString(DocumentWidget *document, const QString &text, QString *searchPath, QString *searchName) const {

	QFileInfoList results;

	if (QFileInfo(text).isAbsolute()) {
		QFileInfo fullPath = text;
		QString filename   = fullPath.fileName();
		QString filepath   = fullPath.path();

		filepath = NormalizePathname(filepath);

		*searchPath = filepath;
		*searchName = filename;

		QDir dir(filepath);
		QStringList name_filters    = {filename};
		QFileInfoList localFileList = dir.entryInfoList(name_filters, QDir::NoDotAndDotDot | QDir::Hidden | QDir::Files);
		results.append(localFileList);
	} else {
		// we need to do this because someone could write #include "path/to/file.h"
		// which confuses QDir..
		QFileInfo fullPath = QStringLiteral("%1/%2").arg(document->path(), text);
		QString filename   = fullPath.fileName();
		QString filepath   = fullPath.path();

		filepath = NormalizePathname(filepath);

		*searchPath = filepath;
		*searchName = filename;

		QDir dir(filepath);
		QStringList name_filters    = {filename};
		QFileInfoList localFileList = dir.entryInfoList(name_filters, QDir::NoDotAndDotDot | QDir::Hidden | QDir::Files);
		results.append(localFileList);
	}

	return results;
}

QFileInfoList MainWindow::openFileHelper(DocumentWidget *document, const QString &text, QString *searchPath, QString *searchName) const {

	static const QRegularExpression reSystem(QLatin1String("#include\\s*<([^>]+)>"));
	static const QRegularExpression reLocal(QLatin1String("#include\\s*\"([^\"]+)\""));

	{
		QRegularExpressionMatch match = reSystem.match(text);
		if (match.hasMatch()) {
			return openFileHelperSystem(document, match, searchPath, searchName);
		}
	}

	{
		QRegularExpressionMatch match = reLocal.match(text);
		if (match.hasMatch()) {
			return openFileHelperLocal(document, match, searchPath, searchName);
		}
	}

	return openFileHelperString(document, text, searchPath, searchName);
}

/**
 * @brief MainWindow::openFile
 * @param document
 * @param text
 */
void MainWindow::openFile(DocumentWidget *document, const QString &text) {

	// get the string, or skip if we can't get the selection data
	if (text.isEmpty()) {
		QApplication::beep();
		return;
	}

	const bool openInTab = Preferences::GetPrefOpenInTab();

	QString searchName;
	QString searchPath;

	QFileInfoList fileList = openFileHelper(document, text, &searchPath, &searchName);

	if (!searchPath.endsWith(QLatin1Char('/'))) {
		searchPath.append(QLatin1Char('/'));
	}

	// No match in any of the search directories, just tell the user the file wasn't found
	if (fileList.isEmpty()) {
		QMessageBox::critical(
			this,
			tr("Error opening File"),
			tr("Could not open %1%2:\n%3").arg(searchPath, searchName, tr("No such file or directory")));
		return;
	}

	// OK, we've got some things to try to open, let's go for it!
	for (const QFileInfo &file : fileList) {

		const PathInfo fi = parseFilename(file.absoluteFilePath());
		DocumentWidget::editExistingFile(
			openInTab ? document : nullptr,
			fi.filename,
			fi.pathname,
			0,
			QString(),
			/*iconic*/ false,
			QString(),
			openInTab,
			/*bgOpen=*/false);
	}

	MainWindow::checkCloseEnableState();
}

/**
 * @brief MainWindow::action_Shift_Left
 * @param document
 */
void MainWindow::action_Shift_Left(DocumentWidget *document) {

	emit_event("shift_left");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		shiftSelection(document, area, ShiftDirection::Left, /*byTab=*/false);
	}
}

/**
 * @brief MainWindow::action_Shift_Left_triggered
 */
void MainWindow::action_Shift_Left_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Left(document);
	}
}

/**
 * @brief MainWindow::action_Shift_Right
 * @param document
 */
void MainWindow::action_Shift_Right(DocumentWidget *document) {

	emit_event("shift_right");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		shiftSelection(document, area, ShiftDirection::Right, /*byTab=*/false);
	}
}

/**
 * @brief MainWindow::action_Shift_Right_triggered
 */
void MainWindow::action_Shift_Right_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Right(document);
	}
}

/**
 * @brief MainWindow::action_Shift_Left_Tabs
 * @param document
 */
void MainWindow::action_Shift_Left_Tabs(DocumentWidget *document) {

	emit_event("shift_left_by_tab");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		shiftSelection(document, area, ShiftDirection::Left, /*byTab=*/true);
	}
}

/**
 * @brief MainWindow::action_Shift_Left_Tabs
 */
void MainWindow::action_Shift_Left_Tabs() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Left_Tabs(document);
	}
}

/**
 * @brief MainWindow::action_Shift_Right_Tabs
 * @param document
 */
void MainWindow::action_Shift_Right_Tabs(DocumentWidget *document) {

	emit_event("shift_right_by_tab");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		shiftSelection(document, area, ShiftDirection::Right, /*byTab=*/true);
	}
}

/**
 * @brief MainWindow::action_Shift_Right_Tabs
 */
void MainWindow::action_Shift_Right_Tabs() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Right_Tabs(document);
	}
}

/**
 * @brief MainWindow::action_Lower_case
 * @param document
 */
void MainWindow::action_Lower_case(DocumentWidget *document) {

	emit_event("lowercase");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		downcaseSelection(document, area);
	}
}

/**
 * @brief MainWindow::action_Lower_case_triggered
 */
void MainWindow::action_Lower_case_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Lower_case(document);
	}
}

/**
 * @brief MainWindow::action_Upper_case
 * @param document
 */
void MainWindow::action_Upper_case(DocumentWidget *document) {

	emit_event("uppercase");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		upcaseSelection(document, area);
	}
}

/**
 * @brief MainWindow::action_Upper_case_triggered
 */
void MainWindow::action_Upper_case_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Upper_case(document);
	}
}

/**
 * @brief MainWindow::action_Fill_Paragraph
 * @param document
 */
void MainWindow::action_Fill_Paragraph(DocumentWidget *document) {

	emit_event("fill_paragraph");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		fillSelection(document, area);
	}
}

/**
 * @brief MainWindow::action_Fill_Paragraph_triggered
 */
void MainWindow::action_Fill_Paragraph_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Fill_Paragraph(document);
	}
}

/**
 * @brief MainWindow::action_Insert_Form_Feed_triggered
 */
void MainWindow::action_Insert_Form_Feed_triggered() {

	if (QPointer<TextArea> area = lastFocus()) {
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

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		area->insertStringAP(str);
	}
}

/**
 * @brief MainWindow::action_Insert_Ctrl_Code
 * @param document
 */
void MainWindow::action_Insert_Ctrl_Code(DocumentWidget *document) {
	if (document->checkReadOnly()) {
		return;
	}

	bool ok;
	int n = QInputDialog::getInt(this,
								 tr("Insert Ctrl Code"),
								 tr("ASCII Character Code:"),
								 0,
								 0,
								 255,
								 1,
								 &ok);
	if (ok) {
		QString str(QChar::fromLatin1(static_cast<char>(n)));
		action_Insert_Ctrl_Code(document, str);
	}
}

/**
 * @brief MainWindow::action_Insert_Ctrl_Code_triggered
 */
void MainWindow::action_Insert_Ctrl_Code_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Insert_Ctrl_Code(document);
	}
}

/**
 * @brief MainWindow::action_Goto_Line_Number
 * @param document
 * @param s
 */
void MainWindow::action_Goto_Line_Number(DocumentWidget *document, const QString &s) {

	emit_event("goto_line_number", s);

	/* Accept various formats:
		  [line]:[column]   (menu action)
		  line              (macro call)
		  line, column      (macro call) */
	boost::optional<Location> loc = StringToLineAndCol(s);
	if (!loc) {
		QApplication::beep();
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		document->gotoAP(area, loc->line, loc->column);
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
		tr("Goto Line (and/or Column) Number:"),
		QLineEdit::Normal,
		QString(),
		&ok);

	if (!ok) {
		return;
	}

	action_Goto_Line_Number(document, text);
}

/**
 * @brief MainWindow::action_Goto_Line_Number_triggered
 */
void MainWindow::action_Goto_Line_Number_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Goto_Line_Number(document);
	}
}

/**
 * @brief MainWindow::action_Goto_Selected
 * @param document
 */
void MainWindow::action_Goto_Selected(DocumentWidget *document) {

	emit_event("goto_selected");

	const QString selected = document->getAnySelection();
	if (selected.isEmpty()) {
		QApplication::beep();
		return;
	}

	action_Goto_Line_Number(document, selected);
}

/**
 * @brief MainWindow::action_Goto_Selected_triggered
 */
void MainWindow::action_Goto_Selected_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Goto_Selected(document);
	}
}

/**
 * @brief MainWindow::action_Find_Dialog
 * @param document
 * @param direction
 * @param type
 * @param keepDialog
 */
void MainWindow::action_Find_Dialog(DocumentWidget *document, Direction direction, SearchType type, bool keepDialog) {

	if (!dialogFind_) {
		dialogFind_ = new DialogFind(this, document);
	}

	dialogFind_->setTextFieldFromDocument(document);

	if (dialogFind_->isVisible()) {
		dialogFind_->raise();
		dialogFind_->activateWindow();
		return;
	}

	// Set the initial search type
	dialogFind_->initToggleButtons(type);

	dialogFind_->setDirection(direction);
	dialogFind_->setKeepDialog(keepDialog);
	dialogFind_->updateFindButton();

	fHistIndex_ = 0;

	dialogFind_->show();
	dialogFind_->raise();
	dialogFind_->activateWindow();
}

/**
 * @brief MainWindow::action_Shift_Find
 * @param document
 */
void MainWindow::action_Shift_Find(DocumentWidget *document) {
	action_Find_Dialog(
		document,
		Direction::Backward,
		Preferences::GetPrefSearch(),
		Preferences::GetPrefKeepSearchDlogs());
}

/**
 * @brief MainWindow::action_Shift_Find
 * @param document
 */
void MainWindow::action_Shift_Find() {
	if (DocumentWidget *document = currentDocument()) {
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

	if (QPointer<TextArea> area = lastFocus()) {
		searchAndSelectSame(
			document,
			area,
			direction,
			wrap);
	}
}

/**
 * @brief MainWindow::action_Shift_Find_Again
 * @param document
 */
void MainWindow::action_Shift_Find_Again(DocumentWidget *document) {
	action_Find_Again(
		document,
		Direction::Backward,
		Preferences::GetPrefSearchWraps());
}

/**
 * @brief MainWindow::action_Shift_Find_Again
 * @param document
 */
void MainWindow::action_Shift_Find_Again() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Find_Again(document);
	}
}

/**
 * @brief MainWindow::action_Find_Selection
 * @param document
 * @param direction
 * @param type
 * @param wrap
 */
void MainWindow::action_Find_Selection(DocumentWidget *document, Direction direction, SearchType type, WrapMode wrap) {

	emit_event("find_selection", to_string(direction), to_string(type), to_string(wrap));

	if (QPointer<TextArea> area = lastFocus()) {
		searchForSelected(
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
		Preferences::GetPrefSearch(),
		Preferences::GetPrefSearchWraps());
}

/**
 * @brief MainWindow::action_Shift_Find_Selection
 */
void MainWindow::action_Shift_Find_Selection() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Find_Selection(document);
	}
}

/*
** Called when user types in the incremental search line. Redoes the
** search for the new search string.
*/
void MainWindow::editIFind_textChanged(const QString &text) {

	const SearchType searchType = [this]() {
		if (ui.checkIFindCase->isChecked()) {
			if (ui.checkIFindRegex->isChecked()) {
				return SearchType::Regex;
			} else {
				return SearchType::CaseSense;
			}
		} else {
			if (ui.checkIFindRegex->isChecked()) {
				return SearchType::RegexNoCase;
			} else {
				return SearchType::Literal;
			}
		}
	}();

	const Direction direction = ui.checkIFindReverse->isChecked() ? Direction::Backward : Direction::Forward;

	/* If the search type is a regular expression, test compile it.  If it
	   fails, silently skip it.  (This allows users to compose the expression
	   in peace when they have unfinished syntax, but still get beeps when
	   correct syntax doesn't match) */
	if (Search::isRegexType(searchType)) {
		try {
			auto compiledRE = make_regex(text, Search::defaultRegexFlags(searchType));
		} catch (const RegexError &) {
			return;
		}
	}

	/* Call the incremental search handler to do the searching and
	   selecting (this allows it to be recorded for learn/replay).  If
	   there's an incremental search already in progress, mark the operation
	   as "continued" so the search routine knows to re-start the search
	   from the original starting position */
	if (DocumentWidget *document = currentDocument()) {
		action_Find_Incremental(document,
								text,
								direction,
								searchType,
								Preferences::GetPrefSearchWraps(),
								iSearchStartPos_ != -1);
	}
}

/**
 * @brief MainWindow::action_Find_Incremental_triggered
 */
void MainWindow::action_Find_Incremental_triggered() {
	beginISearch(Direction::Forward);
}

/**
 * @brief MainWindow::action_Shift_Find_Incremental
 */
void MainWindow::action_Shift_Find_Incremental() {
	beginISearch(Direction::Backward);
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

	if (QPointer<TextArea> area = lastFocus()) {
		searchAndSelectIncremental(document,
								   area,
								   searchString,
								   direction,
								   searchType,
								   searchWraps,
								   isContinue);
	}
}

/**
 * @brief MainWindow::buttonIFind_clicked
 */
void MainWindow::buttonIFind_clicked() {
	// same as pressing return
	editIFind_returnPressed();
}

/**
 * User pressed return in the incremental search bar. Do a new search with the
 * search string displayed.  The direction of the search is toggled if the Ctrl
 * key or the Shift key is pressed when the text field is activated.
 *
 * @brief MainWindow::editIFind_returnPressed
 */
void MainWindow::editIFind_returnPressed() {

	/* Fetch the string, search type and direction from the incremental
	   search bar widgets at the top of the window */
	QString searchString = ui.editIFind->text();

	const SearchType searchType = [this]() {
		if (ui.checkIFindCase->isChecked()) {
			if (ui.checkIFindRegex->isChecked()) {
				return SearchType::Regex;
			} else {
				return SearchType::CaseSense;
			}
		} else {
			if (ui.checkIFindRegex->isChecked()) {
				return SearchType::RegexNoCase;
			} else {
				return SearchType::Literal;
			}
		}
	}();

	Direction direction = ui.checkIFindReverse->isChecked() ? Direction::Backward : Direction::Forward;

	// Reverse the search direction if the Ctrl or Shift key was pressed
	if (QApplication::keyboardModifiers() & (Qt::CTRL | Qt::SHIFT)) {
		direction = !direction;
	}

	// find the text and mark it
	if (DocumentWidget *document = currentDocument()) {
		action_Find(document, searchString, direction, searchType, Preferences::GetPrefSearchWraps());
	}
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
void MainWindow::checkIFindCase_toggled(bool searchCaseSense) {

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (ui.checkIFindRegex->isChecked()) {
		iSearchLastRegexCase_ = searchCaseSense;
	} else {
		iSearchLastLiteralCase_ = searchCaseSense;
	}

	// When search parameters (direction or search type), redo the search
	editIFind_textChanged(ui.editIFind->text());
}

/**
 * @brief MainWindow::checkIFindRegex_toggled
 * @param searchRegex
 */
void MainWindow::checkIFindRegex_toggled(bool searchRegex) {

	bool searchCaseSense = ui.checkIFindCase->isChecked();

	// In sticky mode, restore the state of the Case Sensitive button
	if (Preferences::GetPrefStickyCaseSenseBtn()) {
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
	editIFind_textChanged(ui.editIFind->text());
}

/**
 * @brief MainWindow::checkIFindReverse_toggled
 * @param value
 */
void MainWindow::checkIFindReverse_toggled(bool value) {

	Q_UNUSED(value)

	// When search parameters (direction or search type), redo the search
	editIFind_textChanged(ui.editIFind->text());
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
void MainWindow::beginISearch(Direction direction) {

	iSearchStartPos_ = TextCursor(-1);
	ui.editIFind->setText(QString());
	no_signals(ui.checkIFindReverse)->setChecked(direction == Direction::Backward);

	/* Note: in contrast to the replace and find dialogs, the regex and
	   case toggles are not reset to their default state when the incremental
	   search bar is redisplayed. I'm not sure whether this is the best
	   choice. If not, an initToggleButtons() call should be inserted
	   here. But in that case, it might be appropriate to have different
	   default search modes for i-search and replace/find. */

	tempShowISearch(true);
	ui.editIFind->setFocus();
}

/*
** Incremental searching is anchored at the position where the cursor
** was when the user began typing the search string.  Call this routine
** to forget about this original anchor, and if the search bar is not
** permanently up, pop it down.
*/
void MainWindow::endISearch() {

	/* Note: Please maintain this such that it can be freely peppered in
	   mainline code, without callers having to worry about performance
	   or visual glitches.  */

	// Forget the starting position used for the current run of searches
	iSearchStartPos_ = TextCursor(-1);

	// Mark the end of incremental search history overwriting
	Search::saveSearchHistory(QString(), QString(), SearchType::Literal, /*isIncremental=*/false);

	// Pop down the search line (if it's not pegged up in Preferences)
	tempShowISearch(false);
}

/**
 * @brief MainWindow::action_Replace_Find_Again
 * @param document
 * @param direction
 */
void MainWindow::action_Replace_Find_Again(DocumentWidget *document, Direction direction, WrapMode wrap) {
	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {

		const Search::HistoryEntry *entry = Search::HistoryByIndex(1);
		if (!entry) {
			QApplication::beep();
			return;
		}

		replaceAndSearch(
			document,
			area,
			entry->search,
			entry->replace,
			direction,
			entry->type,
			wrap);
	}
}

/**
 * @brief MainWindow::action_Replace_Find_Again_triggered
 */
void MainWindow::action_Replace_Find_Again_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Replace_Find_Again(document, Direction::Forward, Preferences::GetPrefSearchWraps());
	}
}

/**
 * @brief MainWindow::action_Shift_Replace_Find_Again
 * @param document
 */
void MainWindow::action_Shift_Replace_Find_Again(DocumentWidget *document) {
	action_Replace_Find_Again(document, Direction::Backward, Preferences::GetPrefSearchWraps());
}

/**
 * @brief MainWindow::action_Replace_Again
 * @param document
 * @param direction
 * @param wrap
 */
void MainWindow::action_Replace_Again(DocumentWidget *document, Direction direction, WrapMode wrap) {

	emit_event("replace_again", to_string(direction), to_string(wrap));

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		replaceSame(document,
					area,
					direction,
					wrap);
	}
}

/**
 * @brief MainWindow::action_Replace_Again_triggered
 */
void MainWindow::action_Replace_Again_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Replace_Again(document,
							 Direction::Forward,
							 Preferences::GetPrefSearchWraps());
	}
}

/**
 * @brief MainWindow::action_Shift_Replace_Again_triggered
 */
void MainWindow::action_Shift_Replace_Again() {

	if (DocumentWidget *document = currentDocument()) {
		action_Replace_Again(document,
							 Direction::Backward,
							 Preferences::GetPrefSearchWraps());
	}
}

/**
 * @brief MainWindow::action_Mark
 * @param mark
 */
void MainWindow::action_Mark(DocumentWidget *document, const QString &mark) {

	// NOTE(eteran): as per discussion#212, bookmarks from macros
	// might not be letters! So we have looser requirements here than
	// the UI instigated version.
	if (mark.size() != 1) {
		qWarning("NEdit: action requires a single-character label");
		QApplication::beep();
		return;
	}

	emit_event("mark", mark);

	const QChar ch = mark[0];
	if (QPointer<TextArea> area = lastFocus()) {
		document->addMark(area, ch);
	}
}

/**
 * @brief MainWindow::action_Mark
 * @param document
 */
void MainWindow::action_Mark(DocumentWidget *document) {
	bool ok;
	const QString result = QInputDialog::getText(
		this,
		tr("Mark"),
		tr("Enter a single letter label to use for recalling\n"
		   "the current selection and cursor position.\n\n"
		   "(To skip this dialog, use the accelerator key,\n"
		   "followed immediately by a letter key (a-z))"),
		QLineEdit::Normal,
		QString(),
		&ok);

	if (!ok) {
		return;
	}

	if (result.size() != 1 || !result[0].isLetter()) {
		qWarning("NEdit: action requires a single-letter label");
		QApplication::beep();
		return;
	}

	action_Mark(document, result);
}

/**
 * @brief MainWindow::action_Mark_triggered
 */
void MainWindow::action_Mark_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Mark(document);
	}
}

/**
 * @brief MainWindow::action_Mark_Shortcut_triggered
 */
void MainWindow::action_Mark_Shortcut() {

	if (auto shortcut = qobject_cast<QShortcut *>(sender())) {
		const QKeySequence sequence = shortcut->key();

		if (DocumentWidget *document = currentDocument()) {

			const int key = sequence[1];
			if (key >= Qt::Key_A && key <= Qt::Key_Z) {
				QString keyseq(key);
				action_Mark(document, keyseq);
			} else {
				QApplication::beep();
			}
		}
	}
}

void MainWindow::action_Goto_Mark(DocumentWidget *document, const QString &mark, bool extend) {

	emit_event("goto_mark", mark);

	// NOTE(eteran): as per discussion#212, bookmarks from macros
	// might not be letters! So we have looser requirements here than
	// the UI instigated version.
	if (mark.size() != 1) {
		qWarning("NEdit: action requires a single-character label");
		QApplication::beep();
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		document->gotoMark(area, mark[0], extend);
	}
}

/**
 * @brief MainWindow::action_Goto_Mark_Dialog
 * @param extend
 */
void MainWindow::action_Goto_Mark_Dialog(DocumentWidget *document, bool extend) {
	bool ok;
	const QString result = QInputDialog::getText(
		this,
		tr("Goto Mark"),
		tr("Enter the single letter label used to mark\n"
		   "the selection and/or cursor position.\n\n"
		   "(To skip this dialog, use the accelerator\n"
		   "key, followed immediately by the letter)"),
		QLineEdit::Normal,
		QString(),
		&ok);

	if (!ok) {
		return;
	}

	if (result.size() != 1 || !result[0].isLetter()) {
		qWarning("NEdit: action requires a single-letter label");
		QApplication::beep();
		return;
	}

	action_Goto_Mark(document, result, extend);
}

/**
 * @brief MainWindow::action_Goto_Mark_triggered
 */
void MainWindow::action_Goto_Mark_triggered() {

	const bool extend = (QApplication::keyboardModifiers() & Qt::SHIFT);

	if (DocumentWidget *document = currentDocument()) {
		action_Goto_Mark_Dialog(document, extend);
	}
}

/**
 * @brief MainWindow::action_Goto_Mark_Shortcut_Helper
 * @param shifted
 */
void MainWindow::action_Goto_Mark_Shortcut_Helper(bool shifted) {
	if (auto shortcut = qobject_cast<QShortcut *>(sender())) {
		const QKeySequence sequence = shortcut->key();

		if (DocumentWidget *document = currentDocument()) {

			const int key = sequence[1];
			if (key >= Qt::Key_A && key <= Qt::Key_Z) {
				QString keyseq(key);
				action_Goto_Mark(document, keyseq, shifted);
			} else {
				QApplication::beep();
			}
		}
	}
}

/**
 * @brief MainWindow::action_Shift_Goto_Mark_Shortcut
 */
void MainWindow::action_Shift_Goto_Mark_Shortcut() {
	action_Goto_Mark_Shortcut_Helper(/*shifted=*/true);
}

/**
 * @brief MainWindow::action_Goto_Mark_Shortcut
 */
void MainWindow::action_Goto_Mark_Shortcut() {
	action_Goto_Mark_Shortcut_Helper(/*shifted=*/false);
}

/**
 * @brief MainWindow::action_Goto_Matching
 * @param document
 */
void MainWindow::action_Goto_Matching(DocumentWidget *document) {

	emit_event("goto_matching");
	if (QPointer<TextArea> area = lastFocus()) {
		document->gotoMatchingCharacter(area, /*select=*/false);
	}
}

/**
 * @brief MainWindow::action_Goto_Matching_triggered
 */
void MainWindow::action_Goto_Matching_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Goto_Matching(document);
	}
}

/**
 * @brief MainWindow::action_Shift_Goto_Matching
 * @param document
 */
void MainWindow::action_Shift_Goto_Matching(DocumentWidget *document) {

	emit_event("select_to_matching");
	if (QPointer<TextArea> area = lastFocus()) {
		document->gotoMatchingCharacter(area, /*select=*/true);
	}
}

/**
 * @brief MainWindow::action_Shift_Goto_Matching
 */
void MainWindow::action_Shift_Goto_Matching() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Goto_Matching(document);
	}
}

/**
 * @brief MainWindow::updateTipsFileMenu
 */
void MainWindow::updateTipsFileMenu() {

	auto tipsMenu = new QMenu(this);

	for (Tags::File &tf : Tags::TipsFileList) {
		const QString &filename = tf.filename;
		QAction *action         = tipsMenu->addAction(filename);
		action->setData(filename);
	}

	ui.action_Unload_Calltips_File->setMenu(tipsMenu);

	connect(tipsMenu, &QMenu::triggered, this, [this](QAction *action) {
		auto filename = action->data().toString();
		if (!filename.isEmpty()) {
			if (DocumentWidget *document = currentDocument()) {
				action_Unload_Tips_File(document, filename);
			}
		}
	});
}

/**
 * @brief MainWindow::updateTagsFileMenu
 */
void MainWindow::updateTagsFileMenu() {

	auto tagsMenu = new QMenu(this);

	for (Tags::File &tf : Tags::TagsFileList) {
		const QString &filename = tf.filename;
		QAction *action         = tagsMenu->addAction(filename);
		action->setData(filename);
	}

	ui.action_Unload_Tags_File->setMenu(tagsMenu);

	connect(tagsMenu, &QMenu::triggered, this, [this](QAction *action) {
		auto filename = action->data().toString();
		if (!filename.isEmpty()) {
			if (DocumentWidget *document = currentDocument()) {
				action_Unload_Tags_File(document, filename);
			}
		}
	});
}

/**
 * @brief MainWindow::action_Unload_Tips_File
 * @param document
 * @param filename
 */
void MainWindow::action_Unload_Tips_File(DocumentWidget *document, const QString &filename) {

	Q_UNUSED(document)
	emit_event("unload_tips_file", filename);

	if (Tags::deleteTagsFile(filename, Tags::SearchMode::TIP, /*force_unload=*/true)) {
		for (MainWindow *window : MainWindow::allWindows()) {
			window->updateTipsFileMenu();
		}
	}
}

/**
 * @brief MainWindow::action_Unload_Tags_File
 * @param document
 * @param filename
 */
void MainWindow::action_Unload_Tags_File(DocumentWidget *document, const QString &filename) {

	Q_UNUSED(document)
	emit_event("unload_tags_file", filename);

	if (Tags::deleteTagsFile(filename, Tags::SearchMode::TAG, /*force_unload=*/true)) {
		for (MainWindow *window : MainWindow::allWindows()) {
			window->updateTagsFileMenu();
		}
	}
}

/**
 * @brief MainWindow::action_Load_Tips_File
 * @param document
 * @param filename
 */
void MainWindow::action_Load_Tips_File(DocumentWidget *document, const QString &filename) {

	Q_UNUSED(document)
	emit_event("load_tips_file", filename);

	if (!Tags::addTagsFile(filename, Tags::SearchMode::TIP)) {
		QMessageBox::warning(
			this,
			tr("Error Reading File"),
			tr("Error reading tips file:\n'%1'\ntips not loaded").arg(filename));
	}
}

/**
 * @brief MainWindow::action_Load_Calltips_File
 * @param document
 */
void MainWindow::action_Load_Calltips_File(DocumentWidget *document) {
	QStringList filenames = promptForExistingFiles(this, document->path(), tr("Load Calltips File"), QFileDialog::ExistingFile);
	if (filenames.isEmpty()) {
		return;
	}

	action_Load_Tips_File(document, filenames[0]);
	updateTipsFileMenu();
}

/**
 * @brief MainWindow::action_Load_Calltips_File_triggered
 */
void MainWindow::action_Load_Calltips_File_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Load_Calltips_File(document);
	}
}

/**
 * @brief MainWindow::action_Load_Tags_File
 * @param document
 * @param filename
 */
void MainWindow::action_Load_Tags_File(DocumentWidget *document, const QString &filename) {

	emit_event("load_tags_file", filename);

	if (!Tags::addTagsFile(filename, Tags::SearchMode::TAG)) {
		QMessageBox::warning(
			document,
			tr("Error Reading File"),
			tr("Error reading ctags file:\n'%1'\ntags not loaded").arg(filename));
	}
}

/**
 * @brief MainWindow::action_Load_Tags_File
 * @param document
 */
void MainWindow::action_Load_Tags_File(DocumentWidget *document) {
	QStringList filenames = promptForExistingFiles(this, document->path(), tr("Load Tags File"), QFileDialog::ExistingFile);
	if (filenames.isEmpty()) {
		return;
	}

	action_Load_Tags_File(document, filenames[0]);
	updateTagsFileMenu();
}

/**
 * @brief MainWindow::action_Load_Tags_File_triggered
 */
void MainWindow::action_Load_Tags_File_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Load_Tags_File(document);
	}
}

/**
 * @brief MainWindow::action_Load_Macro_File
 * @param document
 * @param filename
 */
void MainWindow::action_Load_Macro_File(DocumentWidget *document, const QString &filename) {

	emit_event("load_macro_file", filename);
	document->readMacroFile(filename, true);
}

/**
 * @brief MainWindow::action_Load_Macro_File
 * @param document
 */
void MainWindow::action_Load_Macro_File(DocumentWidget *document) {
	QStringList filenames = promptForExistingFiles(this, document->path(), tr("Load Macro File"), QFileDialog::ExistingFile);
	if (filenames.isEmpty()) {
		return;
	}

	action_Load_Macro_File(document, filenames[0]);
}

/**
 * @brief MainWindow::action_Load_Macro_File_triggered
 */
void MainWindow::action_Load_Macro_File_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Load_Macro_File(document);
	}
}

/**
 * @brief MainWindow::action_Print
 * @param document
 */
void MainWindow::action_Print(DocumentWidget *document) {

	emit_event("print");
	if (QPointer<TextArea> area = lastFocus()) {
		document->printWindow(area, /*selectedOnly=*/false);
	}
}

/**
 * @brief MainWindow::action_Print_triggered
 */
void MainWindow::action_Print_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Print(document);
	}
}

/**
 * @brief MainWindow::action_Print_Selection
 * @param document
 */
void MainWindow::action_Print_Selection(DocumentWidget *document) {

	emit_event("print_selection");
	if (QPointer<TextArea> area = lastFocus()) {
		document->printWindow(area, /*selectedOnly=*/true);
	}
}

/**
 * @brief MainWindow::action_Print_Selection_triggered
 */
void MainWindow::action_Print_Selection_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Print_Selection(document);
	}
}

/**
 * @brief MainWindow::action_Split_Pane
 * @param document
 */
void MainWindow::action_Split_Pane(DocumentWidget *document) {

	emit_event("split_pane");
	document->splitPane();
	ui.action_Close_Pane->setEnabled(document->textPanesCount() > 1);
}

/**
 * @brief MainWindow::action_Split_Pane_triggered
 */
void MainWindow::action_Split_Pane_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Split_Pane(document);
	}
}

/**
 * @brief MainWindow::action_Close_Pane
 * @param document
 */
void MainWindow::action_Close_Pane(DocumentWidget *document) {

	emit_event("close_pane");
	document->closePane();
	ui.action_Close_Pane->setEnabled(document->textPanesCount() > 1);
}

/**
 * @brief MainWindow::action_Close_Pane_triggered
 */
void MainWindow::action_Close_Pane_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Close_Pane(document);
	}
}

/**
 * @brief MainWindow::action_Move_Tab_To
 * @param document
 */
void MainWindow::action_Move_Tab_To(DocumentWidget *document) {

	emit_event("move_document_dialog");
	document->moveDocument(this);
}

/**
 * @brief MainWindow::action_Move_Tab_To_triggered
 */
void MainWindow::action_Move_Tab_To_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Move_Tab_To(document);
	}
}

/**
 * @brief MainWindow::action_About_Qt_triggered
 */
void MainWindow::action_About_Qt_triggered() {
	QMessageBox::aboutQt(this);
}

/**
 * @brief MainWindow::currentDocument
 * @return
 */
DocumentWidget *MainWindow::currentDocument() const {
	return qobject_cast<DocumentWidget *>(ui.tabWidget->currentWidget());
}

/**
 * @brief MainWindow::documentAt
 * @param index
 * @return
 */
DocumentWidget *MainWindow::documentAt(int index) const {
	return qobject_cast<DocumentWidget *>(ui.tabWidget->widget(index));
}

/**
 * @brief MainWindow::action_Statistics_Line_toggled
 * @param state
 */
void MainWindow::action_Statistics_Line_toggled(bool state) {

	for (DocumentWidget *document : openDocuments()) {
		document->showStatsLine(state);

		if (document->isTopDocument()) {
			updateStatus(document, nullptr);
		}
	}
}

/*
** Turn on and off the continuing display of the incremental search line
** (when off, it is popped up and down as needed via TempShowISearch)
*/
void MainWindow::action_Incremental_Search_Line_toggled(bool state) {
	showISearchLine_ = state;
	ui.incrementalSearchFrame->setVisible(state);
}

/**
 * @brief MainWindow::action_Show_Line_Numbers_toggled
 * @param state
 */
void MainWindow::action_Show_Line_Numbers_toggled(bool state) {
	showLineNumbers(state);
}

/**
 * @brief MainWindow::action_Set_Auto_Indent
 * @param document
 * @param state
 */
void MainWindow::action_Set_Auto_Indent(DocumentWidget *document, IndentStyle state) {
	emit_event("set_auto_indent", to_string(state));
	document->setAutoIndent(state);
}

/**
 * @brief MainWindow::indentGroupTriggered
 * @param action
 */
void MainWindow::indentGroupTriggered(QAction *action) {
	if (DocumentWidget *document = currentDocument()) {
		if (action == ui.action_Indent_Off) {
			action_Set_Auto_Indent(document, IndentStyle::None);
		} else if (action == ui.action_Indent_On) {
			action_Set_Auto_Indent(document, IndentStyle::Auto);
		} else if (action == ui.action_Indent_Smart) {
			action_Set_Auto_Indent(document, IndentStyle::Smart);
		} else {
			qWarning("NEdit: set_auto_indent invalid argument");
		}
	}
}

/**
 * @brief MainWindow::action_Set_Auto_Wrap
 * @param document
 * @param state
 */
void MainWindow::action_Set_Auto_Wrap(DocumentWidget *document, WrapStyle state) {
	emit_event("set_wrap_text", to_string(state));
	document->setAutoWrap(state);
}

/**
 * @brief MainWindow::wrapGroupTriggered
 * @param action
 */
void MainWindow::wrapGroupTriggered(QAction *action) {
	if (DocumentWidget *document = currentDocument()) {
		if (action == ui.action_Wrap_None) {
			action_Set_Auto_Wrap(document, WrapStyle::None);
		} else if (action == ui.action_Wrap_Auto_Newline) {
			action_Set_Auto_Wrap(document, WrapStyle::Newline);
		} else if (action == ui.action_Wrap_Continuous) {
			action_Set_Auto_Wrap(document, WrapStyle::Continuous);
		} else {
			qWarning("NEdit: set_wrap_text invalid argument");
		}
	}
}

/**
 * @brief MainWindow::action_Wrap_Margin_triggered
 */
void MainWindow::action_Wrap_Margin_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		auto dialog = std::make_unique<DialogWrapMargin>(document, this);
		dialog->exec();
	}
}

/**
 * @brief MainWindow::action_Tab_Stops_triggered
 */
void MainWindow::action_Tab_Stops_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		auto dialog = std::make_unique<DialogTabs>(document, this);
		dialog->exec();
	}
}

/**
 * @brief MainWindow::action_Text_Fonts_triggered
 */
void MainWindow::action_Text_Fonts_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		auto dialogFonts = std::make_unique<DialogFonts>(document, this);
		dialogFonts->exec();
	}
}

/**
 * @brief MainWindow::action_Highlight_Syntax_toggled
 * @param state
 */
void MainWindow::action_Highlight_Syntax_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {

		document->highlightSyntax_ = state;

		if (document->highlightSyntax_) {
			document->startHighlighting(Verbosity::Verbose);
		} else {
			document->stopHighlighting();
		}
	}
}

/**
 * @brief MainWindow::action_Apply_Backlighting_toggled
 * @param state
 */
void MainWindow::action_Apply_Backlighting_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {
		document->setBacklightChars(state ? Preferences::GetPrefBacklightCharTypes() : QString());
	}
}

/**
 * @brief MainWindow::action_Make_Backup_Copy_toggled
 * @param state
 */
void MainWindow::action_Make_Backup_Copy_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {
		document->info_->saveOldVersion = state;
	}
}

/**
 * @brief MainWindow::action_Incremental_Backup_toggled
 * @param state
 */
void MainWindow::action_Incremental_Backup_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {
		document->info_->autoSave = state;
	}
}

/**
 * @brief MainWindow::matchingGroupTriggered
 * @param action
 */
void MainWindow::matchingGroupTriggered(QAction *action) {

	if (DocumentWidget *document = currentDocument()) {
		if (action == ui.action_Matching_Off) {
			document->setShowMatching(ShowMatchingStyle::None);
		} else if (action == ui.action_Matching_Delimiter) {
			document->setShowMatching(ShowMatchingStyle::Delimiter);
		} else if (action == ui.action_Matching_Range) {
			document->setShowMatching(ShowMatchingStyle::Range);
		} else {
			qWarning("NEdit: Invalid argument for set_show_matching");
		}
	}
}

/**
 * @brief MainWindow::action_Matching_Syntax_toggled
 * @param state
 */
void MainWindow::action_Matching_Syntax_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {
		document->info_->matchSyntaxBased = state;
	}
}

/**
 * @brief MainWindow::action_Overtype_toggled
 * @param state
 */
void MainWindow::action_Overtype_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {
		document->setOverstrike(state);
	}
}

/**
 * @brief MainWindow::action_Read_Only_toggled
 * @param state
 */
void MainWindow::action_Read_Only_toggled(bool state) {
	if (DocumentWidget *document = currentDocument()) {
		document->info_->lockReasons.setUserLocked(state);
		updateWindowTitle(document);
		updateWindowReadOnly(document);
	}
}

/**
 * @brief MainWindow::action_Save_Defaults_triggered
 */
void MainWindow::action_Save_Defaults_triggered() {
	Preferences::SaveNEditPrefs(this, Verbosity::Verbose);
}

/**
 * @brief MainWindow::action_Default_Language_Modes_triggered
 */
void MainWindow::action_Default_Language_Modes_triggered() {
	auto dialog = std::make_unique<DialogLanguageModes>(nullptr, this);
	dialog->exec();
}

/**
 * @brief MainWindow::defaultIndentGroupTriggered
 * @param action
 */
void MainWindow::defaultIndentGroupTriggered(QAction *action) {

	if (action == ui.action_Default_Indent_Off) {
		Preferences::SetPrefAutoIndent(IndentStyle::None);
	} else if (action == ui.action_Default_Indent_On) {
		Preferences::SetPrefAutoIndent(IndentStyle::Auto);
	} else if (action == ui.action_Default_Indent_Smart) {
		Preferences::SetPrefAutoIndent(IndentStyle::Smart);
	} else {
		qWarning("NEdit: invalid default indent");
	}
}

/**
 * @brief MainWindow::action_Default_Program_Smart_Indent_triggered
 */
void MainWindow::action_Default_Program_Smart_Indent_triggered() {

	if (!SmartIndent::SmartIndentDialog) {
		// We pass this document so that the dialog can show the information for the currently
		// active language mode
		DocumentWidget *document = currentDocument();
		if (!document) {
			qWarning("Nedit: no current document");
			return;
		}

		SmartIndent::SmartIndentDialog = new DialogSmartIndent(document, this);
	}

	if (Preferences::LanguageModeName(0).isNull()) {
		QMessageBox::warning(this,
							 tr("Language Mode"),
							 tr("No Language Modes defined"));
		return;
	}

	// NOTE(eteran): GCC 7+ false positive
	// it is impossible to get here and have the pointer be null.
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_GCC("-Wnull-dereference")
	Q_ASSERT(SmartIndent::SmartIndentDialog);
	SmartIndent::SmartIndentDialog->exec();
	QT_WARNING_POP
}

/**
 * @brief MainWindow::defaultWrapGroupTriggered
 * @param action
 */
void MainWindow::defaultWrapGroupTriggered(QAction *action) {

	if (action == ui.action_Default_Wrap_None) {
		Preferences::SetPrefWrap(WrapStyle::None);
	} else if (action == ui.action_Default_Wrap_Auto_Newline) {
		Preferences::SetPrefWrap(WrapStyle::Newline);
	} else if (action == ui.action_Default_Wrap_Continuous) {
		Preferences::SetPrefWrap(WrapStyle::Continuous);
	} else {
		qWarning("NEdit: invalid default wrap");
	}
}

/**
 * @brief MainWindow::action_Default_Wrap_Margin_triggered
 */
void MainWindow::action_Default_Wrap_Margin_triggered() {
	auto dialog = std::make_unique<DialogWrapMargin>(nullptr, this);
	dialog->exec();
}

/**
 * @brief MainWindow::defaultTagCollisionsGroupTriggered
 * @param action
 */
void MainWindow::defaultTagCollisionsGroupTriggered(QAction *action) {
	if (action == ui.action_Default_Tag_Show_All) {
		Preferences::SetPrefSmartTags(false);
	} else if (action == ui.action_Default_Tag_Smart) {
		Preferences::SetPrefSmartTags(true);
	} else {
		qWarning("NEdit: invalid default collisions");
	}
}

/**
 * @brief MainWindow::action_Default_Command_Shell_triggered
 */
void MainWindow::action_Default_Command_Shell_triggered() {
	bool ok;
	QString shell = QInputDialog::getText(this,
										  tr("Command Shell"),
										  tr("Enter shell path:"),
										  QLineEdit::Normal,
										  Preferences::GetPrefShell(),
										  &ok);

	if (ok && !shell.isEmpty()) {

		if (!QFile::exists(shell)) {
			int resp = QMessageBox::warning(
				this,
				tr("Command Shell"),
				tr("The selected shell is not available.\nDo you want to use it anyway?"),
				QMessageBox::Ok | QMessageBox::Cancel);
			if (resp == QMessageBox::Cancel) {
				return;
			}
		}

		Preferences::SetPrefShell(shell);
	}
}

/**
 * @brief MainWindow::action_Default_Tab_Stops_triggered
 */
void MainWindow::action_Default_Tab_Stops_triggered() {
	auto dialog = std::make_unique<DialogTabs>(nullptr, this);
	dialog->exec();
}

/**
 * @brief MainWindow::action_Default_Text_Fonts_triggered
 */
void MainWindow::action_Default_Text_Fonts_triggered() {
	auto dialog = std::make_unique<DialogFonts>(nullptr, this);
	dialog->exec();
}

/**
 * @brief MainWindow::action_Default_Colors_triggered
 */
void MainWindow::action_Default_Colors_triggered() {
	auto dialog = std::make_unique<DialogColors>(this);
	dialog->exec();
}

/*
** Present a dialog for editing the user specified commands in the shell menu
*/
void MainWindow::action_Default_Shell_Menu_triggered() {
	if (!dialogShellMenu_) {
		dialogShellMenu_ = new DialogShellMenu(this);
	}

	// TODO(eteran): do we want to take any measures to prevent
	// more than one of these being shown?
	dialogShellMenu_->show();
}

/*
** Present a dialogs for editing the user specified commands in the Macro
** and background menus
*/
void MainWindow::action_Default_Macro_Menu_triggered() {
	if (!dialogMacros_) {
		dialogMacros_ = new DialogMacros(this);
	}

	// TODO(eteran): do we want to take any measures to prevent
	// more than one of these being shown?
	dialogMacros_->show();
}

/**
 * @brief MainWindow::action_Default_Window_Background_Menu_triggered
 */
void MainWindow::action_Default_Window_Background_Menu_triggered() {
	if (!dialogWindowBackgroundMenu_) {
		dialogWindowBackgroundMenu_ = new DialogWindowBackgroundMenu(this);
	}

	// TODO(eteran): do we want to take any measures to prevent
	// more than one of these being shown?
	dialogWindowBackgroundMenu_->show();
}

/**
 * @brief MainWindow::action_Default_Sort_Open_Prev_Menu_toggled
 * @param state
 */
void MainWindow::action_Default_Sort_Open_Prev_Menu_toggled(bool state) {

	// Set the preference, make the other windows' menus agree
	Preferences::SetPrefSortOpenPrevMenu(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Sort_Open_Prev_Menu)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Show_Path_In_Windows_Menu_toggled
 * @param state
 */
void MainWindow::action_Default_Show_Path_In_Windows_Menu_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefShowPathInWindowsMenu(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Show_Path_In_Windows_Menu)->setChecked(state);
	}

	MainWindow::updateWindowMenus();
}

/**
 * @brief MainWindow::action_Default_Customize_Window_Title_triggered
 */
void MainWindow::action_Default_Customize_Window_Title_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		auto dialog = std::make_unique<DialogWindowTitle>(document, this);
		dialog->exec();
	}
}

/**
 * @brief MainWindow::action_Default_Search_Verbose_toggled
 * @param state
 */
void MainWindow::action_Default_Search_Verbose_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefSearchDlogs(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Search_Verbose)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Search_Wrap_Around_toggled
 * @param state
 */
void MainWindow::action_Default_Search_Wrap_Around_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefSearchWraps(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Search_Wrap_Around)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Search_Beep_On_Search_Wrap_toggled
 * @param state
 */
void MainWindow::action_Default_Search_Beep_On_Search_Wrap_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefBeepOnSearchWrap(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Search_Beep_On_Search_Wrap)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Search_Keep_Dialogs_Up_toggled
 * @param state
 */
void MainWindow::action_Default_Search_Keep_Dialogs_Up_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefKeepSearchDlogs(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Search_Keep_Dialogs_Up)->setChecked(state);
	}
}

/**
 * @brief MainWindow::defaultSearchGroupTriggered
 * @param action
 */
void MainWindow::defaultSearchGroupTriggered(QAction *action) {

	std::vector<MainWindow *> windows = MainWindow::allWindows();

	if (action == ui.action_Default_Search_Literal) {
		Preferences::SetPrefSearch(SearchType::Literal);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Search_Literal)->setChecked(true);
		}
	} else if (action == ui.action_Default_Search_Literal_Case_Sensitive) {
		Preferences::SetPrefSearch(SearchType::CaseSense);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Search_Literal_Case_Sensitive)->setChecked(true);
		}
	} else if (action == ui.action_Default_Search_Literal_Whole_Word) {
		Preferences::SetPrefSearch(SearchType::LiteralWord);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Search_Literal_Whole_Word)->setChecked(true);
		}
	} else if (action == ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word) {
		Preferences::SetPrefSearch(SearchType::CaseSenseWord);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Search_Literal_Case_Sensitive_Whole_Word)->setChecked(true);
		}
	} else if (action == ui.action_Default_Search_Regular_Expression) {
		Preferences::SetPrefSearch(SearchType::Regex);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Search_Regular_Expression)->setChecked(true);
		}
	} else if (action == ui.action_Default_Search_Regular_Expresison_Case_Insensitive) {
		Preferences::SetPrefSearch(SearchType::RegexNoCase);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Search_Regular_Expresison_Case_Insensitive)->setChecked(true);
		}
	}
}

/**
 * @brief MainWindow::defaultSyntaxGroupTriggered
 * @param action
 */
void MainWindow::defaultSyntaxGroupTriggered(QAction *action) {

	std::vector<MainWindow *> windows = MainWindow::allWindows();

	if (action == ui.action_Default_Syntax_Off) {
		Preferences::SetPrefHighlightSyntax(false);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Syntax_Off)->setChecked(true);
		}
	} else if (action == ui.action_Default_Syntax_On) {
		Preferences::SetPrefHighlightSyntax(true);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Syntax_On)->setChecked(true);
		}
	}
}

/**
 * @brief MainWindow::action_Default_Syntax_Recognition_Patterns_triggered
 */
void MainWindow::action_Default_Syntax_Recognition_Patterns_triggered() {
	editHighlightPatterns();
}

/**
 * @brief MainWindow::action_Default_Syntax_Text_Drawing_Styles_triggered
 */
void MainWindow::action_Default_Syntax_Text_Drawing_Styles_triggered() {
	editHighlightStyles(QString());
}

/**
 * @brief MainWindow::action_Default_Apply_Backlighting_toggled
 * @param state
 */
void MainWindow::action_Default_Apply_Backlighting_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefBacklightChars(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Apply_Backlighting)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Tab_Open_File_In_New_Tab_toggled
 * @param state
 */
void MainWindow::action_Default_Tab_Open_File_In_New_Tab_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefOpenInTab(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Tab_Open_File_In_New_Tab)->setChecked(state);

		if (!Preferences::GetPrefOpenInTab()) {
			window->ui.action_New_Window->setText(tr("New &Tab"));
		} else {
			window->ui.action_New_Window->setText(tr("New &Window"));
		}
	}
}

/**
 * @brief MainWindow::action_Default_Tab_Show_Tab_Bar_toggled
 * @param state
 */
void MainWindow::action_Default_Tab_Show_Tab_Bar_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefTabBar(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Tab_Show_Tab_Bar)->setChecked(state);
		window->tabWidget()->tabBar()->setVisible(state);
	}
}

/**
 * @brief MainWindow::action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open_toggled
 * @param state
 */
void MainWindow::action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefTabBarHideOne(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Tab_Hide_Tab_Bar_When_Only_One_Document_is_Open)->setChecked(state);
		window->tabWidget()->setTabBarAutoHide(state);
	}
}

/**
 * @brief MainWindow::action_Default_Tab_Next_Prev_Tabs_Across_Windows_toggled
 * @param state
 */
void MainWindow::action_Default_Tab_Next_Prev_Tabs_Across_Windows_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefGlobalTabNavigate(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Tab_Next_Prev_Tabs_Across_Windows)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Tab_Sort_Tabs_Alphabetically_toggled
 * @param state
 */
void MainWindow::action_Default_Tab_Sort_Tabs_Alphabetically_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefSortTabs(state);

	std::vector<MainWindow *> windows = MainWindow::allWindows();

	for (MainWindow *window : windows) {
		no_signals(window->ui.action_Default_Tab_Sort_Tabs_Alphabetically)->setChecked(state);
	}

	// If we just enabled sorting, sort all tabs
	if (state) {
		for (MainWindow *window : windows) {
			window->sortTabBar();
			window->tabWidget()->tabBar()->setMovable(false);
		}
	} else {
		for (MainWindow *window : windows) {
			window->tabWidget()->tabBar()->setMovable(true);
		}
	}
}

/**
 * @brief MainWindow::action_Default_Show_Tooltips_toggled
 * @param state
 */
void MainWindow::action_Default_Show_Tooltips_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefToolTips(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Show_Tooltips)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Statistics_Line_toggled
 * @param state
 */
void MainWindow::action_Default_Statistics_Line_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefStatsLine(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Statistics_Line)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Incremental_Search_Line_toggled
 * @param state
 */
void MainWindow::action_Default_Incremental_Search_Line_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefISearchLine(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Incremental_Search_Line)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Show_Line_Numbers_toggled
 * @param state
 */
void MainWindow::action_Default_Show_Line_Numbers_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefLineNums(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Show_Line_Numbers)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Make_Backup_Copy_toggled
 * @param state
 */
void MainWindow::action_Default_Make_Backup_Copy_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefSaveOldVersion(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Make_Backup_Copy)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Incremental_Backup_toggled
 * @param state
 */
void MainWindow::action_Default_Incremental_Backup_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefAutoSave(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Incremental_Backup)->setChecked(state);
	}
}

/**
 * @brief MainWindow::defaultMatchingGroupTriggered
 * @param action
 */
void MainWindow::defaultMatchingGroupTriggered(QAction *action) {

	std::vector<MainWindow *> windows = MainWindow::allWindows();

	if (action == ui.action_Default_Matching_Off) {
		Preferences::SetPrefShowMatching(ShowMatchingStyle::None);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Matching_Off)->setChecked(true);
		}
	} else if (action == ui.action_Default_Matching_Delimiter) {
		Preferences::SetPrefShowMatching(ShowMatchingStyle::Delimiter);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Matching_Delimiter)->setChecked(true);
		}
	} else if (action == ui.action_Default_Matching_Range) {
		Preferences::SetPrefShowMatching(ShowMatchingStyle::Range);
		for (MainWindow *window : windows) {
			no_signals(window->ui.action_Default_Matching_Range)->setChecked(true);
		}
	}
}

/**
 * @brief MainWindow::action_Default_Matching_Syntax_Based_toggled
 * @param state
 */
void MainWindow::action_Default_Matching_Syntax_Based_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefMatchSyntaxBased(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Matching_Syntax_Based)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Terminate_with_Line_Break_on_Save_toggled
 * @param state
 */
void MainWindow::action_Default_Terminate_with_Line_Break_on_Save_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefAppendLF(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Terminate_with_Line_Break_on_Save)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Popups_Under_Pointer_toggled
 * @param state
 */
void MainWindow::action_Default_Popups_Under_Pointer_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefRepositionDialogs(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Popups_Under_Pointer)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Auto_Scroll_Near_Window_Top_Bottom_toggled
 * @param state
 */
void MainWindow::action_Default_Auto_Scroll_Near_Window_Top_Bottom_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefAutoScroll(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Auto_Scroll_Near_Window_Top_Bottom)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Warnings_Files_Modified_Externally_toggled
 * @param state
 */
void MainWindow::action_Default_Warnings_Files_Modified_Externally_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefWarnFileMods(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Warnings_Files_Modified_Externally)->setChecked(state);
		window->ui.action_Default_Warnings_Check_Modified_File_Contents->setEnabled(Preferences::GetPrefWarnFileMods());
	}
}

/**
 * @brief MainWindow::action_Default_Warnings_Check_Modified_File_Contents_toggled
 * @param state
 */
void MainWindow::action_Default_Warnings_Check_Modified_File_Contents_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefWarnRealFileMods(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Warnings_Check_Modified_File_Contents)->setChecked(state);
	}
}

/**
 * @brief MainWindow::action_Default_Warnings_On_Exit_toggled
 * @param state
 */
void MainWindow::action_Default_Warnings_On_Exit_toggled(bool state) {

	// Set the preference and make the other windows' menus agree
	Preferences::SetPrefWarnExit(state);
	for (MainWindow *window : MainWindow::allWindows()) {
		no_signals(window->ui.action_Default_Warnings_On_Exit)->setChecked(state);
	}
}

/**
 * @brief MainWindow::defaultSizeGroupTriggered
 * @param action
 */
void MainWindow::defaultSizeGroupTriggered(QAction *action) {
	if (action == ui.action_Default_Size_24_x_80) {
		setWindowSizeDefault(24, 80);
	} else if (action == ui.action_Default_Size_40_x_80) {
		setWindowSizeDefault(40, 80);
	} else if (action == ui.action_Default_Size_60_x_80) {
		setWindowSizeDefault(60, 80);
	} else if (action == ui.action_Default_Size_80_x_80) {
		setWindowSizeDefault(80, 80);
	} else if (action == ui.action_Default_Size_Custom) {
		auto dialog = std::make_unique<DialogWindowSize>(this);
		dialog->exec();
		updateWindowSizeMenus();
	}
}

/**
 * @brief MainWindow::setWindowSizeDefault
 * @param rows
 * @param cols
 */
void MainWindow::setWindowSizeDefault(int rows, int cols) {
	Preferences::SetPrefRows(rows);
	Preferences::SetPrefCols(cols);
	updateWindowSizeMenus();
}

/**
 * @brief MainWindow::updateWindowSizeMenus
 */
void MainWindow::updateWindowSizeMenus() {
	for (MainWindow *window : MainWindow::allWindows()) {
		window->updateWindowSizeMenu();
	}
}

/**
 * @brief MainWindow::updateWindowSizeMenu
 */
void MainWindow::updateWindowSizeMenu() {
	const int rows = Preferences::GetPrefRows();
	const int cols = Preferences::GetPrefCols();

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

/**
 * @brief MainWindow::action_Next_Document
 */
void MainWindow::action_Next_Document() {

	emit_event("next_document");

	bool crossWindows = Preferences::GetPrefGlobalTabNavigate();
	int currentIndex  = ui.tabWidget->currentIndex();
	int nextIndex     = currentIndex + 1;
	int tabCount      = ui.tabWidget->count();

	if (!crossWindows) {
		if (nextIndex == tabCount) {
			ui.tabWidget->setCurrentIndex(0);
		} else {
			ui.tabWidget->setCurrentIndex(nextIndex);
		}
	} else {
		if (nextIndex == tabCount) {

			std::vector<MainWindow *> windows = MainWindow::allWindows();
			auto currentIter                  = std::find(windows.begin(), windows.end(), this);
			auto nextIter                     = std::next(currentIter);

			if (nextIter == windows.end()) {
				nextIter = windows.begin();
			}

			// raise the window set the focus to the first document in it
			MainWindow *nextWindow      = *nextIter;
			DocumentWidget *firstWidget = nextWindow->documentAt(0);

			Q_ASSERT(firstWidget);

			if (auto document = firstWidget) {
				document->raiseFocusDocumentWindow(true);
				nextWindow->tabWidget()->setCurrentWidget(document);
			}

		} else {
			ui.tabWidget->setCurrentIndex(nextIndex);
		}
	}
}

/**
 * @brief MainWindow::action_Prev_Document
 */
void MainWindow::action_Prev_Document() {

	emit_event("previous_document");

	bool crossWindows = Preferences::GetPrefGlobalTabNavigate();
	int currentIndex  = ui.tabWidget->currentIndex();
	int prevIndex     = currentIndex - 1;
	int tabCount      = ui.tabWidget->count();

	if (!crossWindows) {
		if (currentIndex == 0) {
			ui.tabWidget->setCurrentIndex(tabCount - 1);
		} else {
			ui.tabWidget->setCurrentIndex(prevIndex);
		}
	} else {
		if (currentIndex == 0) {

			std::vector<MainWindow *> windows = MainWindow::allWindows();
			auto currentIter                  = std::find(windows.rbegin(), windows.rend(), this);
			auto nextIter                     = std::next(currentIter);

			if (nextIter == windows.rend()) {
				nextIter = windows.rbegin();
			}

			// raise the window set the focus to the first document in it
			MainWindow *nextWindow     = *nextIter;
			DocumentWidget *lastWidget = nextWindow->documentAt(nextWindow->tabWidget()->count() - 1);

			Q_ASSERT(lastWidget);

			if (auto document = lastWidget) {
				document->raiseFocusDocumentWindow(true);
				nextWindow->tabWidget()->setCurrentWidget(document);
			}

		} else {
			ui.tabWidget->setCurrentIndex(prevIndex);
		}
	}
}

/**
 * @brief MainWindow::action_Last_Document
 */
void MainWindow::action_Last_Document() {

	emit_event("last_document");

	if (lastFocusDocument) {
		lastFocusDocument->raiseFocusDocumentWindow(/*focus=*/true);
	}
}

DocumentWidget *MainWindow::editNewFile(MainWindow *window, const QString &geometry, bool iconic, const QString &languageMode) {
	return editNewFile(window, geometry, iconic, languageMode, QDir::currentPath());
}

DocumentWidget *MainWindow::editNewFile(MainWindow *window, const QString &geometry, bool iconic, const QString &languageMode, const QDir &defaultPath) {
	DocumentWidget *document;

	// Find a (relatively) unique name for the new file
	QString name = MainWindow::uniqueUntitledName();

	// create new window/document
	if (window) {
		document = window->createDocument(name);
	} else {
		window   = new MainWindow();
		document = window->createDocument(name);

		window->parseGeometry(geometry);

		if (iconic) {
			window->showMinimized();
		} else {
			window->show();
		}
	}

	Q_ASSERT(window);
	Q_ASSERT(document);

	document->setFilename(name);
	document->setPath(defaultPath);

	// TODO(eteran): I'd like basically all of this to be done with signals
	// on an as needed basis
	document->setWindowModified(false);
	document->info_->lockReasons.clear();
	window->updateWindowReadOnly(document);
	window->updateStatus(document, document->firstPane());
	window->updateWindowTitle(document);
	window->updateWindowHints(document);

	// NOTE(eteran): addresses issue #224. When a new tab causes a reordering
	// AND a focus event will trigger a "save/cancel" dialog
	// we can end up with the wrong names on tabs!
	// so we sort BEFORE we update the tab state just to be sure everything
	// is in the right spots!
	window->sortTabBar();

	document->refreshTabState();

	if (languageMode.isNull()) {
		document->determineLanguageMode(true);
	} else {
		document->action_Set_Language_Mode(languageMode, true);
	}

	if (iconic && window->isMinimized()) {
		document->raiseDocument();
	} else {
		document->raiseDocumentWindow();
	}

	return document;
}

/**
 * @brief MainWindow::allDocumentsBusy
 * @param message
 */
void MainWindow::allDocumentsBusy(const QString &message) {

	std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

	if (!currentlyBusy) {
		busyStartTime  = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
		modeMessageSet = false;

		/*
		 * We don't the display message here yet, but defer it for a while.
		 * If the wait is short, we don't want to have it flash on and off
		 * the screen. However, we can't use a time since in generally we
		 * are in a tight loop, so it's up to the caller to make sure that
		 * this routine is called at regular intervals.
		 */
		QApplication::setOverrideCursor(Qt::WaitCursor);

	} else if (!modeMessageSet && !message.isNull() && (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - busyStartTime) > 1000) {

		// Show the mode message when we've been busy for more than a second
		for (DocumentWidget *document : documents) {
			document->setModeMessage(message);
		}
		modeMessageSet = true;
	}

	/* Keep UI alive while loading large files */
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
	currentlyBusy = true;
}

/**
 * @brief MainWindow::allDocumentsUnbusy
 */
void MainWindow::allDocumentsUnbusy() {

	for (DocumentWidget *document : DocumentWidget::allDocuments()) {
		document->clearModeMessage();
	}

	currentlyBusy  = false;
	modeMessageSet = false;
	busyStartTime  = 0;

	QApplication::restoreOverrideCursor();
}

/**
 * @brief MainWindow::action_Save
 * @param document
 */
void MainWindow::action_Save(DocumentWidget *document) {

	emit_event("save");

	if (document->checkReadOnly()) {
		return;
	}

	document->saveDocument();
}

/**
 * @brief MainWindow::action_Save_triggered
 */
void MainWindow::action_Save_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Save(document);
	}
}

/**
 * @brief promptForExistingFiles
 * @param parent
 * @param path
 * @param prompt
 * @param mode
 * @param nameFilter
 * @param defaultSelection
 * @return
 */
QStringList MainWindow::promptForExistingFiles(QWidget *parent, const QString &path, const QString &prompt, QFileDialog::FileMode mode, const QString &nameFilter, const QString &defaultSelection) {

	QFileDialog dialog(parent, prompt);
	dialog.setOptions(QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons);
	dialog.setFileMode(mode);
	dialog.setFilter(QDir::AllDirs | QDir::AllEntries | QDir::Hidden | QDir::System);

	if (!path.isEmpty()) {
		dialog.setDirectory(path);
	}

	if (!nameFilter.isEmpty()) {
		dialog.setNameFilter(nameFilter);
	}

	if (!defaultSelection.isEmpty()) {
		dialog.selectFile(defaultSelection);
	}

	if (dialog.exec()) {
		return dialog.selectedFiles();
	}

	return QStringList();
}

/**
 * @brief MainWindow::promptForNewFile
 * @param document
 * @param prompt
 * @param fileFormat
 * @param addWrap
 * @return
 */
QString MainWindow::promptForNewFile(DocumentWidget *document, FileFormats *format, bool *addWrap) {

	Q_ASSERT(format);
	Q_ASSERT(addWrap);

	QString filename;
	QFileDialog dialog(document, tr("Save File As"));

	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setDirectory(document->path());
	dialog.setOptions(QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons);
	dialog.setFilter(QDir::AllDirs | QDir::AllEntries | QDir::Hidden | QDir::System);

	if (auto layout = qobject_cast<QGridLayout *>(dialog.layout())) {
		if (layout->rowCount() == 4 && layout->columnCount() == 3) {
			auto boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);

			auto unixCheck = new QRadioButton(tr("&Unix"));
			auto dosCheck  = new QRadioButton(tr("D&OS"));
			auto macCheck  = new QRadioButton(tr("&Macintosh"));

			switch (document->fileFormat()) {
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
			if (*addWrap) {
				wrapCheck->setChecked(true);
			}

			QObject::connect(wrapCheck, &QCheckBox::toggled, document, [wrapCheck, document](bool checked) {
				if (checked) {
					int ret = QMessageBox::information(document, tr("Add Wrap"),
													   tr("This operation adds permanent line breaks to match the automatic wrapping done by the Continuous Wrap mode Preferences Option.\n\n"
														  "*** This Option is Irreversable ***\n\n"
														  "Once newlines are inserted, continuous wrapping will no longer work automatically on these lines"),
													   QMessageBox::Ok | QMessageBox::Cancel);

					if (ret != QMessageBox::Ok) {
						wrapCheck->setChecked(false);
					}
				}
			});

			if (document->wrapMode() == WrapStyle::Continuous) {
				layout->addWidget(wrapCheck, row, 1, 1, 1);
			}

			if (dialog.exec()) {
				if (dosCheck->isChecked()) {
					document->setFileFormat(FileFormats::Dos);
				} else if (macCheck->isChecked()) {
					document->setFileFormat(FileFormats::Mac);
				} else if (unixCheck->isChecked()) {
					document->setFileFormat(FileFormats::Unix);
				}

				*addWrap = wrapCheck->isChecked();
				*format  = document->fileFormat();

				const QStringList files = dialog.selectedFiles();
				filename                = files[0];
			}
		}
	}

	return filename;
}

/**
 * @brief MainWindow::action_Save_As
 * @param document
 * @param filename
 * @param wrapped
 */
void MainWindow::action_Save_As(DocumentWidget *document, const QString &filename, bool wrapped) {

	if (wrapped) {
		emit_event("save_as", filename, QLatin1String("wrapped"));
	} else {
		emit_event("save_as", filename);
	}

	document->saveDocumentAs(filename, wrapped);
}

/**
 * @brief MainWindow::action_Save_As
 * @param document
 */
void MainWindow::action_Save_As(DocumentWidget *document) {
	bool addWrap           = false;
	FileFormats fileFormat = FileFormats::Unix;

	QString fullname = promptForNewFile(document, &fileFormat, &addWrap);
	if (fullname.isNull()) {
		return;
	}

	document->setFileFormat(fileFormat);
	action_Save_As(document, fullname, addWrap);
}

/**
 * @brief MainWindow::action_Save_As_triggered
 */
void MainWindow::action_Save_As_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Save_As(document);
	}
}

/**
 * @brief MainWindow::action_Revert_to_Saved
 */
void MainWindow::action_Revert_to_Saved(DocumentWidget *document) {
	emit_event("revert_to_saved");
	document->revertToSaved();
}

/**
 * @brief MainWindow::action_Revert_to_Saved_triggered
 */
void MainWindow::action_Revert_to_Saved_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		// re-reading file is irreversible, prompt the user first
		if (document->fileChanged()) {

			int result = QMessageBox::question(
				this,
				tr("Discard Changes"),
				tr("Discard changes to\n%1%2?").arg(document->path(), document->filename()),
				QMessageBox::Ok | QMessageBox::Cancel);
			if (result == QMessageBox::Cancel) {
				return;
			}
		} else {

			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Reload File"));
			messageBox.setIcon(QMessageBox::Question);
			messageBox.setText(tr("Re-load file\n%1%2?").arg(document->path(), document->filename()));
			QPushButton *buttonOk     = messageBox.addButton(tr("Re-read"), QMessageBox::AcceptRole);
			QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
			Q_UNUSED(buttonOk)

			messageBox.exec();
			if (messageBox.clickedButton() == buttonCancel) {
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
	MainWindow::editNewFile(Preferences::GetPrefOpenInTab() ? nullptr : this, QString(), /*iconic=*/false, QString(), QDir(document->path()));
	MainWindow::checkCloseEnableState();
}

/**
 * @brief MainWindow::action_New_Window_triggered
 */
void MainWindow::action_New_Window_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_New_Window(document);
	}
}

/**
 * @brief MainWindow::action_Exit
 * @param document
 */
void MainWindow::action_Exit(DocumentWidget *document) {

	emit_event("exit");

	Q_UNUSED(document)

	std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

	if (!checkPrefsChangesSaved()) {
		return;
	}

	/* If this is not the last document (more than one document is open),
	   confirm with the user before exiting. */
	if (Preferences::GetPrefWarnExit() && documents.size() >= 2) {

		auto exitMsg = tr("Editing: ");

		/* List the windows being edited and make sure the
		   user really wants to exit */
		// This code assembles a list of document names being edited and elides as necessary
		for (size_t i = 0; i < documents.size(); ++i) {
			DocumentWidget *const document = documents[i];

			auto filename = tr("%1%2").arg(document->filename(), document->fileChanged() ? tr("*") : QString());

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
		Q_UNUSED(buttonExit)

		messageBox.exec();
		if (messageBox.clickedButton() == buttonCancel) {
			return;
		}
	}

	// Close all files and exit when the last one is closed
	if (MainWindow::closeAllFilesAndWindows()) {
		QApplication::quit();
	}
}

/**
 * @brief MainWindow::action_Exit_triggered
 */
void MainWindow::action_Exit_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Exit(document);
	}
}

/*
** Check if preferences have changed, and if so, ask the user if he wants
** to re-save.  Returns false if user requests cancelation of Exit (or whatever
** operation triggered this call to be made).
*/
bool MainWindow::checkPrefsChangesSaved() {

	if (!Preferences::PreferencesChanged()) {
		return true;
	}

	QString importedFile = Preferences::ImportedSettingsFile();

	QMessageBox messageBox(this);
	messageBox.setWindowTitle(tr("Default Preferences"));
	messageBox.setIcon(QMessageBox::Question);

	messageBox.setText((importedFile.isNull())
						   ? tr("Default Preferences have changed.\nSave changes to NEdit preference file?")
						   : tr("Default Preferences have changed.\nSAVING CHANGES WILL INCORPORATE ADDITIONAL SETTINGS FROM FILE: %s").arg(importedFile));

	QPushButton *buttonSave     = messageBox.addButton(QMessageBox::Save);
	QPushButton *buttonDontSave = messageBox.addButton(tr("Don't Save"), QMessageBox::AcceptRole);
	QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
	Q_UNUSED(buttonCancel)

	messageBox.exec();
	if (messageBox.clickedButton() == buttonSave) {
		Preferences::SaveNEditPrefs(this, Verbosity::Silent);
		return true;
	} else if (messageBox.clickedButton() == buttonDontSave) {
		return true;
	} else {
		return false;
	}
}

/*
** close all the documents in a window
*/
bool MainWindow::closeAllDocumentsInWindow() {

	if (tabCount() == 1) {
		// only one document in the window
		if (DocumentWidget *document = currentDocument()) {
			return document->closeFileAndWindow(CloseMode::Prompt);
		}
	} else {

		// close all _modified_ documents belong to this window
		for (DocumentWidget *document : openDocuments()) {
			if (document->fileChanged()) {
				if (!document->closeFileAndWindow(CloseMode::Prompt)) {
					return false;
				}
			}
		}

		// NOTE(eteran): suppress tab bar updates until this is done to speed things up!
		if (auto suppress_signals = no_signals(ui.tabWidget)) {
			// if there's still documents left in the window...
			for (DocumentWidget *document : openDocuments()) {
				if (!document->closeFileAndWindow(CloseMode::Prompt)) {
					return false;
				}
			}
		}
	}

	return true;
}

/**
 * @brief MainWindow::closeEvent
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event) {

	std::vector<MainWindow *> windows = MainWindow::allWindows();
	if (windows.size() == 1) {
		// this is only window, then this is the same as exit
		event->ignore();
		action_Exit_triggered();

	} else {

		if (tabCount() == 1) {
			if (DocumentWidget *document = currentDocument()) {
				document->closeFileAndWindow(CloseMode::Prompt);
			}
		} else {
			int resp = QMessageBox::Close;
			if (Preferences::GetPrefWarnExit()) {
				resp = QMessageBox::question(this,
											 tr("Close Window"),
											 tr("Close ALL documents in this window?"),
											 QMessageBox::Close | QMessageBox::Cancel);
			}

			if (resp == QMessageBox::Close) {
				closeAllDocumentsInWindow();
				event->accept();
			} else {
				event->ignore();
			}
		}
	}
}

/**
 * @brief MainWindow::action_Execute_Command_Line
 * @param document
 */
void MainWindow::action_Execute_Command_Line(DocumentWidget *document) {

	emit_event("execute_command_line");

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		document->execCursorLine(area, CommandSource::User);
	}
}

/**
 * @brief MainWindow::action_Execute_Command_Line_triggered
 */
void MainWindow::action_Execute_Command_Line_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Execute_Command_Line(document);
	}
}

/**
 * @brief MainWindow::action_Cancel_Shell_Command_triggered
 */
void MainWindow::action_Cancel_Shell_Command_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		document->abortShellCommand();
	}
}

/**
 * @brief MainWindow::action_Learn_Keystrokes_triggered
 */
void MainWindow::action_Learn_Keystrokes_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		document->beginLearn();
	}
}

/**
 * @brief MainWindow::action_Finish_Learn_triggered
 */
void MainWindow::action_Finish_Learn_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		document->finishLearning();
	}
}

/**
 * @brief MainWindow::action_Replay_Keystrokes_triggered
 */
void MainWindow::action_Replay_Keystrokes_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		document->replay();
	}
}

/**
 * @brief MainWindow::action_Cancel_Learn_triggered
 */
void MainWindow::action_Cancel_Learn_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		document->cancelMacroOrLearn();
	}
}

/*
** Close all files and windows, leaving one untitled window
*/
bool MainWindow::closeAllFilesAndWindows() {

	for (MainWindow *window : MainWindow::allWindows()) {
		if (!window->closeAllDocumentsInWindow()) {
			return false;
		}

		window->deleteLater();
	}

	return true;
}

/**
 * @brief MainWindow::action_Repeat
 * @param document
 */
void MainWindow::action_Repeat(DocumentWidget *document) {
	QString lastCommand = CommandRecorder::instance()->lastCommand();

	if (lastCommand.isNull()) {
		QMessageBox::warning(
			this,
			tr("Repeat Macro"),
			tr("No previous commands or learn/replay sequences to repeat"));
		return;
	}

	auto dialog = std::make_unique<DialogRepeat>(document, this);
	if (!dialog->setCommand(lastCommand)) {
		return;
	}
	dialog->exec();
}

/**
 * @brief MainWindow::action_Repeat_triggered
 */
void MainWindow::action_Repeat_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Repeat(document);
	}
}

/**
 * @brief MainWindow::focusChanged
 * @param from
 * @param to
 */
void MainWindow::focusChanged(QWidget *from, QWidget *to) {

	if (auto area = qobject_cast<TextArea *>(from)) {
		if (auto document = DocumentWidget::fromArea(area)) {
			lastFocusDocument = document;
		}
	}

	if (auto area = qobject_cast<TextArea *>(to)) {
		if (auto document = DocumentWidget::fromArea(area)) {
			if (auto window = MainWindow::fromDocument(document)) {
				// record which window pane last had the keyboard focus
				window->lastFocus_ = area;
			}

			// update line number statistic to reflect current focus pane
			updateStatus(document, area);

			// finish off the current incremental search
			endISearch();

			// Check for changes to read-only status and/or file modifications
			Q_EMIT checkForChangesToFile(document);
		}
	}
}

/**
 * @brief MainWindow::action_Help_triggered
 */
void MainWindow::action_Help_triggered() {
	Help::displayTopic(Help::Topic::Start);
}

/**
 * @brief MainWindow::eventFilter
 * @param object
 * @param event
 * @return
 */
bool MainWindow::eventFilter(QObject *object, QEvent *ev) {

	if (object == ui.editIFind && ev->type() == QEvent::KeyPress) {

		auto event = static_cast<QKeyEvent *>(ev);

		// only process up and down arrow keys
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down && event->key() != Qt::Key_Escape) {
			return false;
		}

		int index = iSearchHistIndex_;

		// allow escape key to cancel search
		if (event->key() == Qt::Key_Escape) {
			endISearch();
			return true;
		}

		// increment or decrement the index depending on which arrow was pressed
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return
		if (index != 0 && Search::historyIndex(index) == -1) {
			QApplication::beep();
			return true;
		}

		QString searchStr;
		SearchType searchType;

		// determine the strings and button settings to use
		if (index == 0) {
			searchStr  = QString();
			searchType = Preferences::GetPrefSearch();
		} else {
			const Search::HistoryEntry *entry = Search::HistoryByIndex(index);
			Q_ASSERT(entry);
			searchStr  = entry->search;
			searchType = entry->type;
		}

		iSearchHistIndex_ = index;
		initToggleButtonsiSearch(searchType);

		// Beware the value changed callback is processed as part of this call
		int previousPosition = ui.editIFind->cursorPosition();
		ui.editIFind->setText(searchStr);
		ui.editIFind->setCursorPosition(previousPosition);
		return true;
	}

	if (object == ui.tabWidget->tabBar() && ev->type() == QEvent::ToolTip) {
		if (Preferences::GetPrefToolTips()) {
			auto event  = static_cast<QHelpEvent *>(ev);
			auto tabBar = ui.tabWidget->tabBar();

			int index = tabBar->tabAt(static_cast<QHelpEvent *>(event)->pos());
			if (index != -1) {

				if (DocumentWidget *document = documentAt(index)) {

					QString labelString;
					QString filename = document->filename();

					/* Set tab label to document's filename. Position of "*" (modified)
					 * will change per label alignment setting */
					QStyle *const style = ui.tabWidget->tabBar()->style();
					const int alignment = style->styleHint(QStyle::SH_TabBar_Alignment);

					if (alignment != Qt::AlignRight) {
						labelString = tr("%1%2").arg(document->fileChanged() ? tr("*") : QString(), filename);
					} else {
						labelString = tr("%2%1").arg(document->fileChanged() ? tr("*") : QString(), filename);
					}

					QString tipString;
					if (Preferences::GetPrefShowPathInWindowsMenu() && document->filenameSet()) {
						tipString = tr("%1 - %2").arg(labelString, document->path());
					} else {
						tipString = labelString;
					}

					QToolTip::showText(static_cast<QHelpEvent *>(event)->globalPos(), tipString, this);
				}
			}
		}
		return true;
	}

	return false;
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

	if (QPointer<TextArea> area = lastFocus()) {
		searchAndSelect(
			document,
			area,
			string,
			direction,
			type,
			searchWrap);
	}
}

/**
 * @brief MainWindow::action_Find_triggered
 */
void MainWindow::action_Find_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Find_Dialog(
			document,
			Direction::Forward,
			Preferences::GetPrefSearch(),
			Preferences::GetPrefKeepSearchDlogs());
	}
}

/**
 * @brief MainWindow::action_Find_Again_triggered
 */
void MainWindow::action_Find_Again_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Find_Again(
			document,
			Direction::Forward,
			Preferences::GetPrefSearchWraps());
	}
}

/**
 * @brief MainWindow::action_Find_Selection_triggered
 */
void MainWindow::action_Find_Selection_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Find_Selection(
			document,
			Direction::Forward,
			Preferences::GetPrefSearch(),
			Preferences::GetPrefSearchWraps());
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

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		searchAndReplace(
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

	if (document->checkReadOnly()) {
		return;
	}

	if (!dialogReplace_) {
		dialogReplace_ = new DialogReplace(this, document);
	}

	dialogReplace_->setTextFieldFromDocument(document);

	// If the window is already up, just pop it to the top
	if (dialogReplace_->isVisible()) {
		dialogReplace_->raise();
		dialogReplace_->activateWindow();
		return;
	}

	dialogReplace_->setReplaceText(QString());
	dialogReplace_->initToggleButtons(type);
	dialogReplace_->setDirection(direction);
	dialogReplace_->setKeepDialog(keepDialog);
	dialogReplace_->UpdateReplaceActionButtons();

	// Start the search history mechanism at the current history item
	rHistIndex_ = 0;

	dialogReplace_->show();
	dialogReplace_->raise();
	dialogReplace_->activateWindow();
}

/**
 * @brief MainWindow::action_Shift_Replace
 * @param document
 */
void MainWindow::action_Shift_Replace(DocumentWidget *document) {
	action_Replace_Dialog(
		document,
		Direction::Backward,
		Preferences::GetPrefSearch(),
		Preferences::GetPrefKeepSearchDlogs());
}

/**
 * @brief MainWindow::action_Shift_Replace
 */
void MainWindow::action_Shift_Replace() {
	if (DocumentWidget *document = currentDocument()) {
		action_Shift_Replace(document);
	}
}

/**
 * @brief MainWindow::action_Replace_triggered
 */
void MainWindow::action_Replace_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Replace_Dialog(document,
							  Direction::Forward,
							  Preferences::GetPrefSearch(),
							  Preferences::GetPrefKeepSearchDlogs());
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

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		replaceAll(document,
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
	if (!argument.isEmpty()) {
		emit_event("show_tip", argument);
	} else {
		emit_event("show_tip");
	}

	if (QPointer<TextArea> area = lastFocus()) {
		document->findDefinitionCalltip(area, argument);
	}
}

/**
 * @brief MainWindow::action_Find_Definition
 * @param argument
 */
void MainWindow::action_Find_Definition(DocumentWidget *document, const QString &argument) {
	if (!argument.isEmpty()) {
		emit_event("find_definition", argument);
	} else {
		emit_event("find_definition");
	}

	if (QPointer<TextArea> area = lastFocus()) {
		document->findDefinition(area, argument);
	}
}

/**
 * @brief MainWindow::action_Find_Definition
 * @param document
 */
void MainWindow::action_Find_Definition(DocumentWidget *document) {
	action_Find_Definition(document, QString());
}

/**
 * @brief MainWindow::action_Find_Definition_triggered
 */
void MainWindow::action_Find_Definition_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Find_Definition(document);
	}
}

/**
 * @brief MainWindow::action_Show_Calltip
 * @param document
 */
void MainWindow::action_Show_Calltip(DocumentWidget *document) {
	action_Show_Tip(document, QString());
}

/**
 * @brief MainWindow::action_Show_Calltip_triggered
 */
void MainWindow::action_Show_Calltip_triggered() {
	if (DocumentWidget *document = currentDocument()) {
		action_Show_Calltip(document);
	}
}

/**
 * @brief MainWindow::action_Filter_Selection
 * @param document
 */
void MainWindow::action_Filter_Selection(DocumentWidget *document, CommandSource source) {

	if (document->checkReadOnly()) {
		return;
	}

	if (!document->buffer()->primary.hasSelection()) {
		QApplication::beep();
		return;
	}

	auto dialog = std::make_unique<DialogFilter>(this);
	int r       = dialog->exec();
	if (!r) {
		return;
	}

	QString filterText = dialog->currentText();
	action_Filter_Selection(document, filterText, source);
}

/**
 * @brief MainWindow::action_Filter_Selection_triggered
 */
void MainWindow::action_Filter_Selection_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Filter_Selection(document, CommandSource::User);
	}
}

/**
 * @brief action_Filter_Selection
 * @param filter
 */
void MainWindow::action_Filter_Selection(DocumentWidget *document, const QString &filter, CommandSource source) {

	emit_event("filter_selection", filter);

	if (document->checkReadOnly()) {
		return;
	}

	if (!document->buffer()->primary.hasSelection()) {
		QApplication::beep();
		return;
	}

	if (!filter.isEmpty()) {
		document->filterSelection(filter, source);
	}
}

/**
 * @brief MainWindow::action_Execute_Command
 * @param command
 */
void MainWindow::action_Execute_Command(DocumentWidget *document, const QString &command) {

	emit_event("execute_command", command);

	if (document->checkReadOnly()) {
		return;
	}

	if (!command.isEmpty()) {
		if (QPointer<TextArea> area = lastFocus()) {
			document->execAP(area, command);
		}
	}
}

/**
 * @brief MainWindow::action_Execute_Command
 * @param document
 */
void MainWindow::action_Execute_Command(DocumentWidget *document) {

	if (document->checkReadOnly()) {
		return;
	}

	auto dialog = std::make_unique<DialogExecuteCommand>(this);
	int r       = dialog->exec();
	if (!r) {
		return;
	}

	QString commandText = dialog->currentText();
	dialog->addHistoryItem(commandText);
	action_Execute_Command(document, commandText);
}

/**
 * @brief MainWindow::action_Execute_Command_triggered
 */
void MainWindow::action_Execute_Command_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Execute_Command(document);
	}
}

/**
 * @brief MainWindow::shellTriggered
 * @param action
 */
void MainWindow::shellTriggered(QAction *action) {

	QVariant data = action->data();
	if (!data.isNull()) {
		const auto index   = data.toUInt();
		const QString name = ShellMenuData[index].item.name;

		if (DocumentWidget *document = currentDocument()) {
			action_Shell_Menu_Command(document, name);
		}
	}
}

/**
 * @brief MainWindow::action_Shell_Menu_Command
 * @param command
 */
void MainWindow::action_Shell_Menu_Command(DocumentWidget *document, const QString &name) {
	emit_event("shell_menu_command", name);
	if (QPointer<TextArea> area = lastFocus()) {
		execNamedShellMenuCmd(document, area, name, CommandSource::User);
	}
}

/**
 * @brief MainWindow::macroTriggered
 * @param action
 */
void MainWindow::macroTriggered(QAction *action) {

	QVariant actionIndex = action->data();
	if (!actionIndex.isNull()) {
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
		if (DocumentWidget *document = currentDocument()) {
			if (document->macroCmdData_) {
				QApplication::beep();
				return;
			}

			const auto index   = actionIndex.toUInt();
			const QString name = MacroMenuData[index].item.name;
			action_Macro_Menu_Command(document, name);
		}
	}
}

/**
 * @brief MainWindow::action_Macro_Menu_Command
 * @param name
 */
void MainWindow::action_Macro_Menu_Command(DocumentWidget *document, const QString &name) {
	emit_event("macro_menu_command", name);
	if (QPointer<TextArea> area = lastFocus()) {
		execNamedMacroMenuCmd(document, area, name, CommandSource::User);
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

/**
 * @brief MainWindow::action_Detach_Tab_triggered
 */
void MainWindow::action_Detach_Tab_triggered() {

	if (DocumentWidget *document = currentDocument()) {
		action_Detach_Document(document);
	}
}

/**
 * @brief MainWindow::action_Detach_Document
 * @param document
 */
void MainWindow::action_Detach_Document(DocumentWidget *document) {

	emit_event("detach_document");
	if (tabCount() > 1) {
		auto new_window = new MainWindow();

		document->updateSignals(this, new_window);

		new_window->tabWidget()->addTab(document, document->filename());
		document->refreshTabState();

		new_window->parseGeometry(QString());
		new_window->show();

		// NOTE(eteran): this needs to be AFTER we show the window
		// because internally, it only includes visible windows
		// Perhaps, we should support an "ALL" flag that can be
		// propagated down
		updateWindowMenus();

		// NOTE(eteran): fix for issue #194
		for (MainWindow *window : MainWindow::allWindows()) {
			window->updateTagsFileMenu();
			window->updateTipsFileMenu();
		}
		MainWindow::updateMenuItems();
	}
}

/**
 * @brief MainWindow::action_Detach_Document_Dialog
 * @param document
 */
void MainWindow::action_Detach_Document_Dialog(DocumentWidget *document) {

	if (tabCount() > 1) {

		int result = QMessageBox::question(
			nullptr,
			tr("Detach Tab"),
			tr("Detach %1?").arg(document->filename()),
			QMessageBox::Yes | QMessageBox::No);

		if (result == QMessageBox::Yes) {
			action_Detach_Document(document);
		}
	}
}

/**
 * @brief MainWindow::fromDocument
 * @param document
 * @return
 */
MainWindow *MainWindow::fromDocument(const DocumentWidget *document) {
	if (document) {
		return qobject_cast<MainWindow *>(document->window());
	}

	return nullptr;
}

/*
** Present a dialog for editing highlight style information
*/
void MainWindow::editHighlightStyles(const QString &initialStyle) {

	auto DrawingStyles = std::make_unique<DialogDrawingStyles>(nullptr, Highlight::HighlightStyles, this);
	DrawingStyles->setStyleByName(initialStyle);
	DrawingStyles->exec();
}

/*
** Present a dialog for editing highlight pattern information
*/
void MainWindow::editHighlightPatterns() {

	if (Preferences::LanguageModeName(0).isNull()) {

		QMessageBox::warning(this,
							 tr("No Language Modes"),
							 tr("No Language Modes available for syntax highlighting\n"
								"Add language modes under Preferenses->Language Modes"));
		return;
	}

	if (DocumentWidget *document = currentDocument()) {
		QString languageName = Preferences::LanguageModeName((document->languageMode_ == PLAIN_LANGUAGE_MODE) ? 0 : document->languageMode_);

		auto SyntaxPatterns = std::make_unique<DialogSyntaxPatterns>(this);
		SyntaxPatterns->setLanguageName(languageName);
		SyntaxPatterns->exec();
	}
}

/*
** Change the language mode name of pattern sets for language "oldName" to
** "newName" in both the stored patterns, and the pattern set currently being
** edited in the dialog.
*/
void MainWindow::renameHighlightPattern(const QString &oldName, const QString &newName) {

	for (PatternSet &patternSet : Highlight::PatternSets) {
		if (patternSet.languageMode == oldName) {
			patternSet.languageMode = newName;
		}
	}
}

/**
 * @brief MainWindow::getIncrementalSearchLine
 * @return
 */
bool MainWindow::getIncrementalSearchLine() const {
	return showISearchLine_;
}

/**
 * @brief MainWindow::setIncrementalSearchLine
 * @param value
 */
void MainWindow::setIncrementalSearchLine(bool value) {

	emit_event("set_incremental_search_line", QString::number(value));

	showISearchLine_ = value;
	no_signals(ui.action_Incremental_Search_Line)->setChecked(value);
}

/**
 * @brief MainWindow::searchWindow
 * @param document
 * @param searchString
 * @param direction
 * @param searchType
 * @param searchWrap
 * @param beginPos
 * @param searchResult
 * @return
 */
bool MainWindow::searchWindow(DocumentWidget *document, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap, int64_t beginPos, Search::Result *searchResult) {

	TextBuffer *buffer    = document->buffer();
	const int64_t fileEnd = buffer->length() - 1;

	// reject empty string
	if (searchString.isEmpty()) {
		return false;
	}

	// get the entire text buffer from the text area widget
	view::string_view fileString = buffer->BufAsString();

	/* If we're already outside the boundaries, we must consider wrapping
	   immediately (Note: fileEnd+1 is a valid starting position. Consider
	   searching for $ at the end of a file ending with \n.) */
	bool outsideBounds = ((direction == Direction::Forward && beginPos > fileEnd + 1) || (direction == Direction::Backward && beginPos < 0));

	/* search the string and present dialogs, or just beep.
	 * iSearchStartPos_ is not a perfect indicator that an incremental search
	 * is in progress.  A parameter would be better. */

	bool found;
	if (iSearchStartPos_ == -1) { // normal search

		found = !outsideBounds && Search::SearchString(
									  fileString,
									  searchString,
									  direction,
									  searchType,
									  WrapMode::NoWrap,
									  beginPos,
									  searchResult,
									  document->getWindowDelimiters());

		if (dialogFind_) {
			if (!dialogFind_->keepDialog()) {
				dialogFind_->hide();
			}
		}

		if (dialogReplace_) {
			if (!dialogReplace_->keepDialog()) {
				dialogReplace_->hide();
			}
		}

		if (!found) {
			if (searchWrap == WrapMode::Wrap) {
				if (direction == Direction::Forward && beginPos != 0) {
					if (Preferences::GetPrefBeepOnSearchWrap()) {
						QApplication::beep();
					} else if (Preferences::GetPrefSearchDlogs()) {

						QMessageBox messageBox(document);
						messageBox.setWindowTitle(tr("Wrap Search"));
						messageBox.setIcon(QMessageBox::Question);
						messageBox.setText(tr("Continue search from beginning of file?"));
						QPushButton *buttonContinue = messageBox.addButton(tr("Continue"), QMessageBox::YesRole);
						QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
						Q_UNUSED(buttonContinue)

						messageBox.exec();
						if (messageBox.clickedButton() == buttonCancel) {
							return false;
						}
					}

					found = Search::SearchString(
						fileString,
						searchString,
						direction,
						searchType,
						WrapMode::NoWrap,
						0,
						searchResult,
						document->getWindowDelimiters());

				} else if (direction == Direction::Backward && beginPos != fileEnd) {
					if (Preferences::GetPrefBeepOnSearchWrap()) {
						QApplication::beep();
					} else if (Preferences::GetPrefSearchDlogs()) {

						QMessageBox messageBox(document);
						messageBox.setWindowTitle(tr("Wrap Search"));
						messageBox.setIcon(QMessageBox::Question);
						messageBox.setText(tr("Continue search from end of file?"));
						QPushButton *buttonContinue = messageBox.addButton(tr("Continue"), QMessageBox::YesRole);
						QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
						Q_UNUSED(buttonContinue)

						messageBox.exec();
						if (messageBox.clickedButton() == buttonCancel) {
							return false;
						}
					}

					found = Search::SearchString(
						fileString,
						searchString,
						direction,
						searchType,
						WrapMode::NoWrap,
						fileEnd + 1,
						searchResult,
						document->getWindowDelimiters());
				}
			}

			if (!found) {
				if (Preferences::GetPrefSearchDlogs()) {
					QMessageBox::information(document, tr("String not found"), tr("String was not found"));
				} else {
					QApplication::beep();
				}
			}
		}
	} else { // incremental search
		if (outsideBounds && searchWrap == WrapMode::Wrap) {
			beginPos      = (direction == Direction::Forward) ? 0 : fileEnd + 1;
			outsideBounds = false;
		}

		found = !outsideBounds && Search::SearchString(
									  fileString,
									  searchString,
									  direction,
									  searchType,
									  searchWrap,
									  beginPos,
									  searchResult,
									  document->getWindowDelimiters());

		if (found) {
			iSearchTryBeepOnWrap(direction, TextCursor(beginPos), TextCursor(searchResult->start));
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
bool MainWindow::searchAndSelect(DocumentWidget *document, TextArea *area, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap) {

	TextCursor beginPos;
	TextRange selectionRange;
	bool movedFwd = false;

	// Save a copy of searchString in the search history
	Search::saveSearchHistory(searchString, QString(), searchType, /*isIncremental=*/false);

	/* set the position to start the search so we don't find the same
	   string that was found on the last search */
	if (searchMatchesSelection(document, searchString, searchType, &selectionRange, nullptr, nullptr)) {
		// selection matches search string, start before or after sel.
		if (direction == Direction::Backward) {
			beginPos = selectionRange.start - 1;
		} else {
			beginPos = selectionRange.start + 1;
			movedFwd = true;
		}
	} else {
		selectionRange.start = TextCursor(-1);
		selectionRange.end   = TextCursor(-1);
		// no selection, or no match, search relative cursor

		TextCursor cursorPos = area->cursorPos();
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
	iSearchRecordLastBeginPos(direction, beginPos);

	Search::Result searchResult;

	// do the search.  SearchWindow does appropriate dialogs and beeps
	if (!searchWindow(document, searchString, direction, searchType, searchWrap, to_integer(beginPos), &searchResult)) {
		return false;
	}

	auto startPos = TextCursor(searchResult.start);
	auto endPos   = TextCursor(searchResult.end);

	/* if the search matched an empty string (possible with regular exps)
	   beginning at the start of the search, go to the next occurrence,
	   otherwise repeated finds will get "stuck" at zero-length matches */
	if (direction == Direction::Forward && beginPos == startPos && beginPos == endPos) {
		if (!movedFwd && !searchWindow(document, searchString, direction, searchType, searchWrap, to_integer(beginPos + 1), &searchResult)) {
			return false;
		}

		startPos = TextCursor(searchResult.start);
		endPos   = TextCursor(searchResult.end);
	}

	// if matched text is already selected, just beep
	if (selectionRange.start == startPos && selectionRange.end == endPos) {
		QApplication::beep();
		return false;
	}

	// select the text found string
	document->buffer()->BufSelect(startPos, endPos);
	document->makeSelectionVisible(area);
	area->TextSetCursorPos(endPos);
	return true;
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  If
** "continued" is true and a prior incremental search starting position is
** recorded, search from that original position, otherwise, search from the
** current cursor position.
*/
bool MainWindow::searchAndSelectIncremental(DocumentWidget *document, TextArea *area, const QString &searchString, Direction direction, SearchType searchType, WrapMode searchWrap, bool continued) {

	/* If there's a search in progress, start the search from the original
	   starting position, otherwise search from the cursor position. */
	if (!continued || iSearchStartPos_ == -1) {
		iSearchStartPos_ = area->cursorPos();
		iSearchRecordLastBeginPos(direction, iSearchStartPos_);
	}

	TextCursor beginPos = iSearchStartPos_;

	/* If the search string is empty, beep eventually if text wrapped
	   back to the initial position, re-init iSearchLastBeginPos,
	   clear the selection, set the cursor back to what would be the
	   beginning of the search, and return. */
	if (searchString.isEmpty()) {
		TextCursor beepBeginPos = (direction == Direction::Backward) ? beginPos - 1 : beginPos;
		iSearchTryBeepOnWrap(direction, beepBeginPos, beepBeginPos);
		iSearchRecordLastBeginPos(direction, iSearchStartPos_);

		document->buffer()->BufUnselect();
		area->TextSetCursorPos(beginPos);
		return true;
	}

	/* Save the string in the search history, unless we're cycling thru
	   the search history itself, which can be detected by matching the
	   search string with the search string of the current history index. */
	if (!(iSearchHistIndex_ > 1 && (searchString == Search::HistoryByIndex(iSearchHistIndex_)->search))) {
		Search::saveSearchHistory(searchString, QString(), searchType, /*isIncremental=*/true);
		// Reset the incremental search history pointer to the beginning
		iSearchHistIndex_ = 1;
	}

	// begin at insert position - 1 for backward searches
	if (direction == Direction::Backward) {
		--beginPos;
	}

	Search::Result searchResult;

	// do the search.  SearchWindow does appropriate dialogs and beeps
	if (!searchWindow(document, searchString, direction, searchType, searchWrap, to_integer(beginPos), &searchResult)) {
		return false;
	}

	auto startPos = TextCursor(searchResult.start);
	auto endPos   = TextCursor(searchResult.end);

	iSearchLastBeginPos_ = startPos;

	/* if the search matched an empty string (possible with regular exps)
	   beginning at the start of the search, go to the next occurrence,
	   otherwise repeated finds will get "stuck" at zero-length matches */
	if (direction == Direction::Forward && beginPos == startPos && beginPos == endPos) {
		if (!searchWindow(document, searchString, direction, searchType, searchWrap, to_integer(beginPos + 1), &searchResult)) {
			return false;
		}

		startPos = TextCursor(searchResult.start);
		endPos   = TextCursor(searchResult.end);
	}

	iSearchLastBeginPos_ = startPos;

	// select the text found string
	document->buffer()->BufSelect(startPos, endPos);
	document->makeSelectionVisible(area);
	area->TextSetCursorPos(endPos);
	return true;
}

/*
** Replace selection with "replaceString" and search for string "searchString"
** in window "window", using algorithm "searchType" and direction "direction"
*/
bool MainWindow::replaceAndSearch(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWrap) {

	TextRange selectionRange;
	TextCursor extentBW;
	TextCursor extentFW;
	TextBuffer *buffer = document->buffer();

	// Save a copy of search and replace strings in the search history
	Search::saveSearchHistory(searchString, replaceString, searchType, /*isIncremental=*/false);

	bool replaced = false;

	// Replace the selected text only if it matches the search string
	if (searchMatchesSelection(document, searchString, searchType, &selectionRange, &extentBW, &extentFW)) {

		int64_t replaceLen = 0;

		// replace the text
		if (Search::isRegexType(searchType)) {
			std::string replaceResult;
			const std::string foundString = buffer->BufGetRange(extentBW, extentFW + 1);

			Search::replaceUsingRE(
				searchString,
				replaceString,
				foundString,
				selectionRange.start - extentBW,
				replaceResult,
				selectionRange.start == 0 ? -1 : buffer->BufGetCharacter(selectionRange.start - 1),
				document->getWindowDelimiters(),
				Search::defaultRegexFlags(searchType));

			buffer->BufReplace(selectionRange, replaceResult);
			replaceLen = static_cast<int64_t>(replaceResult.size());
		} else {
			buffer->BufReplace(selectionRange, replaceString.toStdString());
			replaceLen = replaceString.size();
		}

		// Position the cursor so the next search will work correctly based
		// on the direction of the search
		area->TextSetCursorPos(selectionRange.start + ((direction == Direction::Forward) ? replaceLen : 0));
		replaced = true;
	}

	// do the search; beeps/dialogs are taken care of
	searchAndSelect(document, area, searchString, direction, searchType, searchWrap);
	return replaced;
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** return search type in "searchType", and returns true.
** Otherwise, returns false.
*/
bool MainWindow::searchAndSelectSame(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {

	const Search::HistoryEntry *entry = Search::HistoryByIndex(1);
	if (!entry) {
		QApplication::beep();
		return false;
	}

	return searchAndSelect(
		document,
		area,
		entry->search,
		direction,
		entry->type,
		searchWrap);
}

/*
** Search for string "searchString" in window "window", using algorithm
** "searchType" and direction "direction", and replace it with "replaceString"
** Also adds the search and replace strings to the global search history.
*/
bool MainWindow::searchAndReplace(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWrap) {

	/* NOTE(eteran): OK, the whole point of extentBW, and extentFW
	 * are to help with regex search/replace operations involving look-ahead and
	 * look-behind. For example, if the buffer contains "ABCDEF-A" and we want
	 * the regex to match "(?<=-)[A-Z]" (a captial letter preceded by a dash).
	 * The regex is matching the "A" and thus the start position of the match is
	 * offset 7. However, the extentBW is offset 6 because the "-" was
	 * required for the match.
	 *
	 * We "need" this because essentially what the replace does is extract the
	 * text that matched (in this case "-A") and does the regex replace on THAT.
	 *
	 * Then it will substitute the altered text into the
	 * buffer. There probably is a way to do this without the need to extract
	 * the text in a way that is dependant on this searchExtent concept.
	 */
	TextCursor extentBW;
	TextCursor extentFW;

	// Save a copy of search and replace strings in the search history
	Search::saveSearchHistory(searchString, replaceString, searchType, /*isIncremental=*/false);

	// If the text selected in the window matches the search string,
	// the user is probably using search then replace method, so
	// replace the selected text regardless of where the cursor is.
	// Otherwise, search for the string.
	TextRange selectionRange;
	if (!searchMatchesSelection(document, searchString, searchType, &selectionRange, &extentBW, &extentFW)) {
		// get the position to start the search

		TextCursor beginPos;
		TextCursor cursorPos = area->cursorPos();
		if (direction == Direction::Backward) {
			// use the insert position - 1 for backward searches
			beginPos = cursorPos - 1;
		} else {
			// use the insert position for forward searches
			beginPos = cursorPos;
		}

		Search::Result searchResult;

		// do the search
		bool found = searchWindow(
			document,
			searchString,
			direction,
			searchType,
			searchWrap,
			to_integer(beginPos),
			&searchResult);

		selectionRange.start = TextCursor(searchResult.start);
		selectionRange.end   = TextCursor(searchResult.end);
		extentBW             = TextCursor(searchResult.extentBW);
		extentFW             = TextCursor(searchResult.extentFW);

		if (!found) {
			return false;
		}
	}

	TextBuffer *buffer = document->buffer();

	int64_t replaceLen;

	// replace the text
	if (Search::isRegexType(searchType)) {
		std::string replaceResult;
		const std::string foundString = buffer->BufGetRange(extentBW, extentFW + 1);

		Search::replaceUsingRE(
			searchString,
			replaceString,
			foundString,
			selectionRange.start - extentBW,
			replaceResult,
			selectionRange.start == 0 ? -1 : buffer->BufGetCharacter(selectionRange.start - 1),
			document->getWindowDelimiters(),
			Search::defaultRegexFlags(searchType));

		buffer->BufReplace(selectionRange, replaceResult);
		replaceLen = static_cast<int64_t>(replaceResult.size());
	} else {
		buffer->BufReplace(selectionRange, replaceString.toStdString());
		replaceLen = replaceString.size();
	}

	/* after successfully completing a replace, selected text attracts
	   attention away from the area of the replacement, particularly
	   when the selection represents a previous search. so deselect */
	buffer->BufUnselect();

	/* temporarily shut off autoShowInsertPos before setting the cursor
	   position so MakeSelectionVisible gets a chance to place the replaced
	   string at a pleasing position on the screen (otherwise, the cursor would
	   be automatically scrolled on screen and MakeSelectionVisible would do
	   nothing) */
	area->setAutoShowInsertPos(false);

	area->TextSetCursorPos(selectionRange.start + ((direction == Direction::Forward) ? replaceLen : 0));
	document->makeSelectionVisible(area);
	area->setAutoShowInsertPos(true);
	return true;
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool MainWindow::replaceSame(DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {

	const Search::HistoryEntry *entry = Search::HistoryByIndex(1);
	if (!entry) {
		QApplication::beep();
		return false;
	}

	return searchAndReplace(
		document,
		area,
		entry->search,
		entry->replace,
		direction,
		entry->type,
		searchWrap);
}

/**
 * @brief MainWindow::action_Replace_Find
 * @param document
 * @param searchString
 * @param replaceString
 * @param direction
 * @param searchType
 * @param searchWraps
 */
void MainWindow::action_Replace_Find(DocumentWidget *document, const QString &searchString, const QString &replaceString, Direction direction, SearchType searchType, WrapMode searchWraps) {

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		replaceAndSearch(
			document,
			area,
			searchString,
			replaceString,
			direction,
			searchType,
			searchWraps);
	}
}

/**
 * @brief MainWindow::searchForSelected
 * @param document
 * @param area
 * @param direction
 * @param searchType
 * @param searchWrap
 */
void MainWindow::searchForSelected(DocumentWidget *document, TextArea *area, Direction direction, SearchType searchType, WrapMode searchWrap) {

	const QString selected = document->getAnySelection();
	if (selected.isEmpty()) {
		if (Preferences::GetPrefSearchDlogs()) {
			QMessageBox::warning(document, tr("Wrong Selection"), tr("Selection not appropriate for searching"));
		} else {
			QApplication::beep();
		}
		return;
	}

	if (selected.isEmpty()) {
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
	searchAndSelect(
		document,
		area,
		selected,
		direction,
		searchType,
		searchWrap);
}

/**
 * @brief MainWindow::action_Replace_In_Selection
 * @param document
 * @param searchString
 * @param replaceString
 * @param type
 */
void MainWindow::action_Replace_In_Selection(DocumentWidget *document, const QString &searchString, const QString &replaceString, SearchType type) {

	emit_event("replace_in_selection", searchString, replaceString, to_string(type));

	if (document->checkReadOnly()) {
		return;
	}

	if (QPointer<TextArea> area = lastFocus()) {
		replaceInSelection(
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
void MainWindow::replaceInSelection(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType) {

	std::string fileString;
	bool substSuccess = false;
	bool anyFound     = false;
	bool cancelSubst  = true;

	// save a copy of search and replace strings in the search history
	Search::saveSearchHistory(searchString, replaceString, searchType, /*isIncremental=*/false);

	TextBuffer *buffer = document->buffer();

	// find out where the selection is
	boost::optional<SelectionPos> pos = buffer->BufGetSelectionPos();
	if (!pos) {
		return;
	}

	// get the selected text
	if (pos->isRect) {
		pos->start = buffer->BufStartOfLine(pos->start);
		pos->end   = buffer->BufEndOfLine(pos->end);
		fileString = buffer->BufGetRange(pos->start, pos->end);
	} else {
		fileString = buffer->BufGetSelectionText();
	}

	/* create a temporary buffer in which to do the replacements to hide the
	   intermediate steps from the display routines, and so everything can
	   be undone in a single operation */
	TextBuffer tempBuf;
	tempBuf.BufSetSyncXSelection(false);
	tempBuf.BufSetAll(fileString);

	// search the string and do the replacements in the temporary buffer
	int64_t replaceLen   = replaceString.size();
	TextCursor beginPos  = {};
	TextCursor cursorPos = {};
	int64_t realOffset   = 0;

	Q_FOREVER {
		boost::optional<Search::Result> searchResult = Search::SearchString(
			fileString,
			searchString,
			Direction::Forward,
			searchType,
			WrapMode::NoWrap,
			to_integer(beginPos),
			document->getWindowDelimiters());

		if (!searchResult) {
			break;
		}

		anyFound = true;
		/* if the selection is rectangular, verify that the found
		   string is in the rectangle */
		if (pos->isRect) {
			TextCursor lineStart = buffer->BufStartOfLine(pos->start + searchResult->start);
			if (buffer->BufCountDispChars(lineStart, pos->start + searchResult->start) < pos->rectStart || buffer->BufCountDispChars(lineStart, pos->start + searchResult->end) > pos->rectEnd) {

				if (static_cast<size_t>(searchResult->end) == fileString.size()) {
					break;
				}

				/* If the match starts before the left boundary of the
				   selection, and extends past it, we should not continue
				   search after the end of the (false) match, because we
				   could miss a valid match starting between the left boundary
				   and the end of the false match. */
				if (buffer->BufCountDispChars(lineStart, pos->start + searchResult->start) < pos->rectStart && buffer->BufCountDispChars(lineStart, pos->start + searchResult->end) > pos->rectStart) {
					beginPos += 1;
				} else {
					beginPos = (searchResult->start == searchResult->end) ? TextCursor(searchResult->end + 1) : TextCursor(searchResult->end);
				}
				continue;
			}
		}

		/* Make sure the match did not start past the end (regular expressions
		   can consider the artificial end of the range as the end of a line,
		   and match a fictional whole line beginning there) */
		if (searchResult->start == (pos->end - pos->start)) {
			break;
		}

		// replace the string and compensate for length change
		if (Search::isRegexType(searchType)) {
			std::string replaceResult;
			const std::string foundString = tempBuf.BufGetRange(TextCursor(searchResult->extentBW + realOffset), TextCursor(searchResult->extentFW + realOffset + 1));

			substSuccess = Search::replaceUsingRE(
				searchString,
				replaceString,
				foundString,
				searchResult->start - searchResult->extentBW,
				replaceResult,
				(searchResult->start + realOffset) == 0 ? -1 : tempBuf.BufGetCharacter(TextCursor(searchResult->start + realOffset - 1)),
				document->getWindowDelimiters(),
				Search::defaultRegexFlags(searchType));

			if (!substSuccess) {
				cancelSubst = prefOrUserCancelsSubst(document);

				if (cancelSubst) {
					//  No point in trying other substitutions.
					break;
				}
			}

			tempBuf.BufReplace(TextCursor(searchResult->start + realOffset), TextCursor(searchResult->end + realOffset), replaceResult);
			replaceLen = static_cast<int64_t>(replaceResult.size());
		} else {
			// at this point plain substitutions (should) always work
			tempBuf.BufReplace(TextCursor(searchResult->start + realOffset), TextCursor(searchResult->end + realOffset), replaceString.toStdString());
			substSuccess = true;
		}

		realOffset += replaceLen - (searchResult->end - searchResult->start);
		// start again after match unless match was empty, then endPos+1
		beginPos  = (searchResult->start == searchResult->end) ? TextCursor(searchResult->end + 1) : TextCursor(searchResult->end);
		cursorPos = TextCursor(searchResult->end);

		if (static_cast<size_t>(searchResult->end) == fileString.size()) {
			break;
		}
	}

	if (anyFound) {
		if (substSuccess || !cancelSubst) {
			/*  Either the substitution was successful (the common case) or the
				user does not care and wants to have a faulty replacement.  */

			// replace the selected range in the real buffer
			buffer->BufReplace(pos->start, pos->end, tempBuf.BufAsString());

			// set the insert point at the end of the last replacement
			area->TextSetCursorPos(pos->start + to_integer(cursorPos) + realOffset);

			/* leave non-rectangular selections selected (rect. ones after replacement
			   are less useful since left/right positions are randomly adjusted) */
			if (!pos->isRect) {
				buffer->BufSelect(pos->start, pos->end + realOffset);
			}
		}
	} else {
		//  Nothing found, tell the user about it
		if (Preferences::GetPrefSearchDlogs()) {

			if (dialogFind_) {
				if (!dialogFind_->keepDialog()) {
					dialogFind_->hide();
				}
			}

			if (dialogReplace_) {
				if (!dialogReplace_->keepDialog()) {
					dialogReplace_->hide();
				}
			}

			QMessageBox::information(document, tr("String not found"), tr("String was not found"));
		} else {
			QApplication::beep();
		}
	}
}

/*
**  Uses the setting nedit.truncSubstitution to determine how to deal with
**  regex failures and asks or warns the user depending on that.
**
**  One could argue that the dialoging should be determined by the setting
**  'searchDlogs'. However, the incomplete substitution is not just a question
**  of verbosity, but of data loss. The search is successful, only the
**  replacement fails due to an internal limitation of NEdit.
**
**  The result is either predetermined by the setting or the user's choice.
*/
bool MainWindow::prefOrUserCancelsSubst(DocumentWidget *document) {

	bool cancel = true;

	switch (Preferences::GetPrefTruncSubstitution()) {
	case TruncSubstitution::Silent:
		//  silently fail the operation
		cancel = true;
		break;

	case TruncSubstitution::Ignore:
		//  silently conclude the operation; THIS WILL DESTROY DATA.
		cancel = false;
		break;

	case TruncSubstitution::Fail:
		//  fail the operation and pop up a dialog informing the user
		QApplication::beep();

		QMessageBox::information(document,
								 tr("Substitution Failed"),
								 tr("The result length of the substitution exceeded an internal limit.\n"
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
			Q_UNUSED(buttonLose)

			messageBox.exec();
			cancel = (messageBox.clickedButton() == buttonCancel);
		}
		break;
	}

	return cancel;
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString".
** Also adds the search and replace strings to the global search history.
*/
bool MainWindow::replaceAll(DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType) {

	int64_t copyStart;
	int64_t copyEnd;

	// reject empty string
	if (searchString.isEmpty()) {
		return false;
	}

	// save a copy of search and replace strings in the search history
	Search::saveSearchHistory(searchString, replaceString, searchType, /*isIncremental=*/false);

	TextBuffer *buffer = document->buffer();

	// view the entire text buffer from the text area widget as a string
	view::string_view fileString = buffer->BufAsString();

	QString delimieters = document->getWindowDelimiters();

	boost::optional<std::string> newFileString = Search::ReplaceAllInString(
		fileString,
		searchString,
		replaceString,
		searchType,
		&copyStart,
		&copyEnd,
		delimieters);

	if (!newFileString) {
		if (document->multiFileBusy_) {
			// only needed during multi-file replacements
			document->replaceFailed_ = true;
		} else if (Preferences::GetPrefSearchDlogs()) {

			if (dialogFind_) {
				if (!dialogFind_->keepDialog()) {
					dialogFind_->hide();
				}
			}

			if (dialogReplace_) {
				if (!dialogReplace_->keepDialog()) {
					dialogReplace_->hide();
				}
			}

			QMessageBox::information(document, tr("String not found"), tr("String was not found"));
		} else {
			QApplication::beep();
		}

		return false;
	}

	// replace the contents of the text widget with the substituted text
	buffer->BufReplace(TextCursor(copyStart), TextCursor(copyEnd), *newFileString);

	// Move the cursor to the end of the last replacement
	area->TextSetCursorPos(TextCursor(copyStart + static_cast<int64_t>(newFileString->size())));
	return true;
}

/*
** Reset iSearchLastBeginPos_ to the resulting initial
** search begin position for incremental searches.
*/
void MainWindow::iSearchRecordLastBeginPos(Direction direction, TextCursor initPos) {
	iSearchLastBeginPos_ = initPos;
	if (direction == Direction::Backward) {
		--iSearchLastBeginPos_;
	}
}

/*
** If this is an incremental search and BeepOnSearchWrap is on:
** Emit a beep if the search wrapped over BOF/EOF compared to
** the last startPos of the current incremental search.
*/
void MainWindow::iSearchTryBeepOnWrap(Direction direction, TextCursor beginPos, TextCursor startPos) {
	if (Preferences::GetPrefBeepOnSearchWrap()) {
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
** Return true if "searchString" exactly matches the text in the window's
** current primary selection using search algorithm "searchType".  If true,
** also return the position of the selection in "left" and "right".
*/
bool MainWindow::searchMatchesSelection(DocumentWidget *document, const QString &searchString, SearchType searchType, TextRange *textRange, TextCursor *extentBW, TextCursor *extentFW) {

	const int regexLookContext = Search::isRegexType(searchType) ? 1000 : 0;

	int64_t selLen;
	TextCursor selStart;
	TextCursor selEnd;
	int64_t beginPos;
	int64_t rectStart;
	int64_t rectEnd;
	TextCursor lineStart = {};
	std::string string;
	bool isRect;

	TextBuffer *buffer = document->buffer();

	// find length of selection, give up on no selection or too long
	if (!buffer->BufGetEmptySelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		return false;
	}

	// if the selection is rectangular, don't match if it spans lines
	if (isRect) {
		lineStart = buffer->BufStartOfLine(selStart);
		if (lineStart != buffer->BufStartOfLine(selEnd)) {
			return false;
		}
	}

	/* get the selected text plus some additional context for regular
	   expression lookahead */
	if (isRect) {
		TextCursor stringStart = lineStart + rectStart - regexLookContext;
		if (stringStart < 0) {
			stringStart = TextCursor();
		}

		string   = buffer->BufGetRange(stringStart, lineStart + rectEnd + regexLookContext);
		selLen   = rectEnd - rectStart;
		beginPos = to_integer(lineStart) + (rectStart - to_integer(stringStart));
	} else {
		TextCursor stringStart = selStart - regexLookContext;
		if (stringStart < 0) {
			stringStart = TextCursor();
		}

		string   = buffer->BufGetRange(stringStart, selEnd + regexLookContext);
		selLen   = selEnd - selStart;
		beginPos = selStart - stringStart;
	}

	if (string.empty()) {
		return false;
	}

	// search for the string in the selection (we are only interested
	// in an exact match, but the procedure SearchString does important
	// stuff like applying the correct matching algorithm)
	boost::optional<Search::Result> searchResult = Search::SearchString(
		string,
		searchString,
		Direction::Forward,
		searchType,
		WrapMode::NoWrap,
		beginPos,
		document->getWindowDelimiters());

	// decide if it is an exact match
	if (!searchResult) {
		return false;
	}

	if (searchResult->start != beginPos || (searchResult->end - beginPos) != selLen) {
		return false;
	}

	// return the start and end of the selection
	if (isRect) {
		buffer->GetSimpleSelection(textRange);
	} else {
		textRange->start = selStart;
		textRange->end   = selEnd;
	}

	if (extentBW) {
		*extentBW = textRange->start - (searchResult->start - searchResult->extentBW);
	}

	if (extentFW) {
		*extentFW = textRange->end + (searchResult->extentFW - searchResult->end);
	}

	return true;
}

/*
** Update the "Find Definition", "Unload Tags File", "Show Calltip",
** and "Unload Calltips File" menu items in the existing windows.
*/
void MainWindow::updateMenuItems() {
	const bool tipStat = !Tags::TipsFileList.empty();
	const bool tagStat = !Tags::TagsFileList.empty();

	for (MainWindow *window : MainWindow::allWindows(true)) {
		window->ui.action_Unload_Calltips_File->setEnabled(tipStat);
		window->ui.action_Unload_Tags_File->setEnabled(tagStat);
		window->ui.action_Show_Calltip->setEnabled(tipStat || tagStat);
		window->ui.action_Find_Definition->setEnabled(tagStat);
	}
}

/*
** Search through the shell menu and execute the first command with menu item
** name "itemName".  Returns true on successs and false on failure.
*/
bool MainWindow::execNamedShellMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, CommandSource source) {

	if (MenuData *p = find_menu_item(name, CommandTypes::Shell)) {

		if (p->item.output == TO_SAME_WINDOW && document->checkReadOnly()) {
			return false;
		}

		document->doShellMenuCmd(
			this,
			area,
			p->item,
			source);

		return true;
	}

	return false;
}

/*
** Search through the Macro or background menu and execute the first command
** with menu item name "itemName".  Returns true on successs and false on
** failure.
*/
bool MainWindow::execNamedMacroMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, CommandSource source) {

	Q_UNUSED(source)
	Q_UNUSED(area)

	if (MenuData *p = find_menu_item(name, CommandTypes::Macro)) {
		document->doMacro(
			p->item.cmd,
			tr("macro menu command"));

		return true;
	}

	return false;
}

/**
 * @brief MainWindow::execNamedBGMenuCmd
 * @param document
 * @param area
 * @param name
 * @param fromMacro
 * @return
 */
bool MainWindow::execNamedBGMenuCmd(DocumentWidget *document, TextArea *area, const QString &name, CommandSource source) {

	Q_UNUSED(source)
	Q_UNUSED(area)

	if (MenuData *p = find_menu_item(name, CommandTypes::Context)) {
		document->doMacro(
			p->item.cmd,
			tr("background menu macro"));

		return true;
	}

	return false;
}

/**
 * @brief MainWindow::setShowLineNumbers
 * @param show
 */
void MainWindow::setShowLineNumbers(bool show) {

	emit_event("set_show_line_numbers", show ? QLatin1String("1") : QLatin1String("0"));

	no_signals(ui.action_Show_Line_Numbers)->setChecked(show);
	showLineNumbers_ = show;
}

/**
 * @brief MainWindow::getShowLineNumbers
 * @return
 */
bool MainWindow::getShowLineNumbers() const {
	return showLineNumbers_;
}

/**
 * @brief MainWindow::tabWidget
 * @return
 */
QTabWidget *MainWindow::tabWidget() const {
	return ui.tabWidget;
}

/**
 * @brief MainWindow::updateStatus
 * @param area
 */
void MainWindow::updateStatus(DocumentWidget *document, TextArea *area) {
	if (!document->isTopDocument()) {
		return;
	}

	/* This routine is called for each character typed, so its performance
	   affects overall editor perfomance.  Only update if the line is on. */
	if (!document->showStats_) {
		return;
	}

	if (!area) {
		area = lastFocus();
		if (!area) {
			area = document->firstPane();
			Q_ASSERT(area);
		}
	}

	// Compose the string to display. If line # isn't available, leave it off
	const TextCursor pos = area->cursorPos();

	const QString format = [document]() {
		switch (document->fileFormat()) {
		case FileFormats::Dos:
			return tr(" DOS");
		case FileFormats::Mac:
			return tr(" Mac");
		default:
			return QString();
		}
	}();

	QString string;
	QString slinecol;
	const int64_t length = document->buffer()->length();

	if (boost::optional<Location> loc = area->positionToLineAndCol(pos)) {
		slinecol = tr("L: %1  C: %2").arg(loc->line).arg(loc->column);
		if (showLineNumbers_) {
			string = tr("%1%2%3 byte %4 of %5").arg(document->path(), document->filename(), format).arg(to_integer(pos)).arg(length);
		} else {
			string = tr("%1%2%3 %4 bytes").arg(document->path(), document->filename(), format).arg(length);
		}
	} else {
		string   = tr("%1%2%3 %4 bytes").arg(document->path(), document->filename(), format).arg(length);
		slinecol = tr("L: ---  C: ---");
	}

	// Update the line/column number
	document->ui.labelStats->setText(slinecol);

	// Don't clobber the line if there's a special message being displayed
	if (!document->modeMessageDisplayed()) {
		document->ui.labelFileAndSize->setText(string);
	}
}

/**
 * @brief Update window geometry hints to allow for incremented resize.
 * @param document
 */
void MainWindow::updateWindowHints(DocumentWidget *document) {
	Q_UNUSED(document);
	// NOTE(eteran): I'm not against supporting this, but it breaks being able to resize
	// the font nicely, at least in my WM... so NERFing this code until I can figure out
	// a nice way to handle it.
#if 0
	QFontMetrics fm(document->defaultFont());
	QSize increment(fm.averageCharWidth(), fm.height());
	setSizeIncrement(increment);
#endif
}

/*
** Update the read-only state of the text area(s) in the document, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void MainWindow::updateWindowReadOnly(DocumentWidget *document) {
	const bool state = document->lockReasons().isAnyLocked();

	for (TextArea *area : document->textPanes()) {
		area->setReadOnly(state);
	}

	if (document->isTopDocument()) {
		no_signals(ui.action_Read_Only)->setChecked(state);
		ui.action_Read_Only->setEnabled(!document->lockReasons().isAnyLockedIgnoringUser());
	}
}

/**
 * @brief MainWindow::updateWindowTitle
 * @param document
 */
void MainWindow::updateWindowTitle(DocumentWidget *document) {

	QString title = DialogWindowTitle::formatWindowTitle(
		document,
		ClearCase::GetViewTag(),
		Preferences::GetPrefServerName(),
		IsServer,
		Preferences::GetPrefTitleFormat());

	setWindowTitle(title);

	QString iconTitle = document->filename();

	if (document->fileChanged()) {
		iconTitle.append(QLatin1Char('*'));
	}

	setWindowIconText(iconTitle);

	/* If there's a find or replace dialog up in "Keep Up" mode, with a
	   file name in the title, update it too */
	if (dialogFind_) {
		dialogFind_->setDocument(document);
	}

	if (dialogReplace_) {
		dialogReplace_->setDocument(document);
	}

	// Update the Windows menus with the new name
	MainWindow::updateWindowMenus();
}

#ifdef PER_TAB_CLOSE
/**
 * @brief MainWindow::tabWidget_tabCloseRequested
 * @param index
 */
void MainWindow::tabWidget_tabCloseRequested(int index) {
	if (DocumentWidget *document = documentAt(index)) {
		action_Close(document, CloseMode::Prompt);
	}
}
#endif

/**
 * @brief MainWindow::lastFocus
 * @return
 */
QPointer<TextArea> MainWindow::lastFocus() {

	if (DocumentWidget *document = currentDocument()) {
		if (!lastFocus_) {
			lastFocus_ = document->firstPane();
		}

		// NOTE(eteran): fix for issue #114, fix up where the last focus is
		// if pointing to a tab which is not longer the "top document"
		if (lastFocus_->document() != document) {
			lastFocus_ = document->firstPane();
		}
	}

	return lastFocus_;
}
