
#include "DocumentWidget.h"
#include "CommandRecorder.h"
#include "DialogDuplicateTags.h"
#include "DialogMoveDocument.h"
#include "DialogOutput.h"
#include "DialogPrint.h"
#include "DialogReplace.h"
#include "DragEndEvent.h"
#include "EditFlags.h"
#include "Font.h"
#include "Highlight.h"
#include "HighlightData.h"
#include "HighlightStyle.h"
#include "MainWindow.h"
#include "PatternSet.h"
#include "Preferences.h"
#include "Search.h"
#include "Settings.h"
#include "SignalBlocker.h"
#include "SmartIndent.h"
#include "SmartIndentEntry.h"
#include "SmartIndentEvent.h"
#include "Style.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "Util/ClearCase.h"
#include "Util/FileSystem.h"
#include "Util/Input.h"
#include "Util/User.h"
#include "Util/regex.h"
#include "Util/utils.h"
#include "WindowHighlightData.h"
#include "WindowMenuEvent.h"
#include "X11Colors.h"
#include "interpret.h"
#include "macro.h"
#include "parse.h"
#include "userCmds.h"

#include <QButtonGroup>
#include <QClipboard>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>
#include <QRadioButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryFile>
#include <QTimer>
#include <qplatformdefs.h>

#include <chrono>

#if defined(Q_OS_WIN)
#define FDOPEN _fdopen
#else
#define FDOPEN fdopen
#endif

// NOTE(eteran): generally, this class reaches out to MainWindow FAR too much
// it would be better to create some fundamental signals that MainWindow could
// listen on and update itself as needed. This would reduce a lot fo the heavy
// coupling seen in this class :-/.

/* data attached to window during shell command execution with information for
 * controling and communicating with the process */
struct ShellCommandData {
	QTimer bannerTimer;
	QByteArray standardError;
	QByteArray standardOutput;
	QProcess *process;
	TextArea *area;
	TextCursor leftPos;
	TextCursor rightPos;
	CommandSource source;
	int flags;
	bool bannerIsUp;
};

DocumentWidget *DocumentWidget::LastCreated = nullptr;

namespace {

// Max # of ADDITIONAL text editing panes  that can be added to a window
constexpr int MaxPanes = 6;

// how long to wait (msec) before putting up Shell Command Executing... banner
constexpr int BannerWaitTime = 6000;

constexpr int FlashInterval = 1500;

enum : int {
	ACCUMULATE        = 1,
	ERROR_DIALOGS     = 2,
	REPLACE_SELECTION = 4,
	RELOAD_FILE_AFTER = 8,
	OUTPUT_TO_DIALOG  = 16,
	OUTPUT_TO_STRING  = 32
};

struct CharMatchTable {
	char ch;
	char match;
	Direction direction;
};

constexpr CharMatchTable MatchingChars[] = {
	{'{', '}', Direction::Forward},
	{'}', '{', Direction::Backward},
	{'(', ')', Direction::Forward},
	{')', '(', Direction::Backward},
	{'[', ']', Direction::Forward},
	{']', '[', Direction::Backward},
	{'<', '>', Direction::Forward},
	{'>', '<', Direction::Backward},
	{'/', '/', Direction::Forward},
	{'"', '"', Direction::Forward},
	{'\'', '\'', Direction::Forward},
	{'`', '`', Direction::Forward},
	{'\\', '\\', Direction::Forward},
};

constexpr CharMatchTable FlashingChars[] = {
	{'{', '}', Direction::Forward},
	{'}', '{', Direction::Backward},
	{'(', ')', Direction::Forward},
	{')', '(', Direction::Backward},
	{'[', ']', Direction::Forward},
	{']', '[', Direction::Backward},
};

/**
 * @brief isAdministrator
 * @return
 */
bool isAdministrator() {
#ifdef Q_OS_UNIX
	return getuid() == 0;
#else
	return false;
#endif
}

/**
 * @brief errorString
 * @param error
 * @return The result of strerror(error), but as a QString
 */
QString errorString(int error) {
	return QString::fromLatin1(strerror(error));
}

/**
 * @brief modifiedCB
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param nRestyled
 * @param deletedText
 * @param user
 */
void modifiedCB(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user) {
	if (auto document = static_cast<DocumentWidget *>(user)) {
		document->modifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText);
	}
}

/**
 * @brief smartIndentCB
 * @param area
 * @param data
 * @param user
 */
void smartIndentCB(TextArea *area, SmartIndentEvent *data, void *user) {
	if (auto document = static_cast<DocumentWidget *>(user)) {
		document->smartIndentCallback(area, data);
	}
}

/**
 * @brief movedCB
 * @param area
 * @param user
 */
void movedCB(TextArea *area, void *user) {
	if (auto document = static_cast<DocumentWidget *>(user)) {
		document->movedCallback(area);
	}
}

/**
 * @brief dragStartCB
 * @param area
 * @param user
 */
void dragStartCB(TextArea *area, void *user) {
	if (auto document = static_cast<DocumentWidget *>(user)) {
		document->dragStartCallback(area);
	}
}

/**
 * @brief dragEndCB
 * @param area
 * @param data
 * @param user
 */
void dragEndCB(TextArea *area, const DragEndEvent *data, void *user) {
	if (auto document = static_cast<DocumentWidget *>(user)) {
		document->dragEndCallback(area, data);
	}
}

/**
 * @brief handleUnparsedRegionCB
 * @param area
 * @param pos
 * @param user
 */
void handleUnparsedRegionCB(const TextArea *area, TextCursor pos, const void *user) {
	if (auto document = static_cast<const DocumentWidget *>(user)) {
		document->handleUnparsedRegion(area->styleBuffer(), pos);
	}
}

/*
** Buffer replacement wrapper routine to be used for inserting output from
** a command into the buffer, which takes into account that the buffer may
** have been shrunken by the user (eg, by Undo). If necessary, the starting
** and ending positions (part of the state of the command) are corrected.
*/
// TODO(eteran): I think the bufReplaceEx function already sanitizes its input,
//               so it's possible that this function is redundant.
void safeBufReplace(TextBuffer *buf, TextCursor *start, TextCursor *end, view::string_view text) {

	const TextCursor last = buf->BufEndOfBuffer();

	*start = std::min(last, *start);
	*end   = std::min(last, *end);

	buf->BufReplace(*start, *end, text);
}

/*
** Update a position across buffer modifications specified by
** "modPos", "nDeleted", and "nInserted".
*/
void maintainPosition(TextCursor &position, TextCursor modPos, int64_t nInserted, int64_t nDeleted) {
	if (modPos > position) {
		return;
	}

	if (modPos + nDeleted <= position) {
		position += nInserted - nDeleted;
	} else {
		position = modPos;
	}
}

/*
** Update a selection across buffer modifications specified by
** "pos", "nDeleted", and "nInserted".
*/
void maintainSelection(TextBuffer::Selection &sel, TextCursor pos, int64_t nInserted, int64_t nDeleted) {
	if (!sel.hasSelection() || pos > sel.end()) {
		return;
	}

	maintainPosition(sel.start_, pos, nInserted, nDeleted);
	maintainPosition(sel.end_, pos, nInserted, nDeleted);

	if (sel.end() <= sel.start()) {
		sel.selected_ = false;
	}
}

/**
 * @brief determineUndoType
 * @param nInserted
 * @param nDeleted
 * @return
 */
UndoTypes determineUndoType(int64_t nInserted, int64_t nDeleted) {

	const bool textDeleted  = (nDeleted > 0);
	const bool textInserted = (nInserted > 0);

	if (textInserted && !textDeleted) {
		// Insert
		if (nInserted == 1)
			return ONE_CHAR_INSERT;
		else
			return BLOCK_INSERT;
	} else if (textInserted && textDeleted) {
		// Replace
		if (nInserted == 1)
			return ONE_CHAR_REPLACE;
		else
			return BLOCK_REPLACE;
	} else if (!textInserted && textDeleted) {
		// Delete
		if (nDeleted == 1)
			return ONE_CHAR_DELETE;
		else
			return BLOCK_DELETE;
	} else {
		// Nothing deleted or inserted
		return UNDO_NOOP;
	}
}

/**
 * @brief createRepeatMacro
 * @param how
 * @return
 */
QLatin1String createRepeatMacro(int how) {

	/* TODO(eteran): move this to the interpreter library as it is specific
	 * to the Nedit macro language
	 */

	switch (how) {
	case REPEAT_TO_END:
		return QLatin1String("lastCursor=-1\nstartPos=$cursor\n"
							 "while($cursor>=startPos&&$cursor!=lastCursor){\nlastCursor=$cursor\n%1\n}\n");
	case REPEAT_IN_SEL:
		return QLatin1String("selStart = $selection_start\nif (selStart == -1)\nreturn\n"
							 "selEnd = $selection_end\nset_cursor_pos(selStart)\nselect(0,0)\n"
							 "boundText = get_range(selEnd, selEnd+10)\n"
							 "while($cursor >= selStart && $cursor < selEnd && \\\n"
							 "get_range(selEnd, selEnd+10) == boundText) {\n"
							 "startLength = $text_length\n%1\n"
							 "selEnd += $text_length - startLength\n}\n");
	default:
		return QLatin1String("for(i=0;i<%1;i++){\n%2\n}\n");
	}
}

/**
** Replaces '%' with <fullName> and '#' with <lineNumber>. But '%%' and '##'
** are replaced by '%' and '#'.
*/
QString escapeCommand(const QString &command, const QString &fullName, int lineNumber) {
	QString ret;
	for (int i = 0; i < command.size(); ++i) {
		auto c = command[i];
		if (c == QLatin1Char('%') || c == QLatin1Char('#')) {
			if (i + 1 < command.size() && command[i + 1] == c) {
				ret.append(c);
				i += 1;
			} else if (c == QLatin1Char('%')) {
				ret.append(fullName);
			} else if (c == QLatin1Char('#')) {
				ret.append(QString::number(lineNumber));
			} else {
				assert(0);
			}
		} else {
			ret.append(command[i]);
		}
	}
	return ret;
}

}

/*
** Open an existing file specified by name and path.  Use the document
** inDocument unless inDocument is nullptr or points to a document which is
** already in use (displays a file other than Untitled, or is Untitled but
** modified). Flags can be any of:
**
**  CREATE:               If file is not found, (optionally) prompt the user
**                        whether to create
**  SUPPRESS_CREATE_WARN  When creating a file, don't ask the user
**  PREF_READ_ONLY        Make the file read-only regardless
**
** If languageMode is passed as QString(), it will be determined automatically
** from the file extension or file contents.
**
** If bgOpen is true, then the file will be open in background. This
** works in association with the SetLanguageMode() function that has
** the syntax highlighting deferred, in order to speed up the file-
** opening operation when multiple files are being opened in succession.
*/
DocumentWidget *DocumentWidget::editExistingFile(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool background) {

	// first look to see if file is already displayed in a window
	if (DocumentWidget *document = MainWindow::findWindowWithFile(name, path)) {
		if (!background) {
			if (iconic) {
				document->raiseDocument();
			} else {
				document->raiseDocumentWindow();
			}
		}
		return document;
	}

	// helper local function to reduce code duplication
	auto createInNewWindow = [&name, &geometry, iconic]() {
		auto win                 = new MainWindow();
		DocumentWidget *document = win->createDocument(name);
		if (iconic) {
			win->showMinimized();
		} else {
			win->showNormal();
		}
		win->parseGeometry(geometry);

		return document;
	};

	/* If an existing document isn't specified; or the document is already in
	 * use (not Untitled or Untitled and modified), or is currently busy
	 * running a macro; create the document */
	DocumentWidget *document = nullptr;
	if (!inDocument) {
		document = createInNewWindow();
	} else if (inDocument->info_->filenameSet || inDocument->info_->fileChanged || inDocument->macroCmdData_) {
		if (tabbed) {
			if (MainWindow *win = MainWindow::fromDocument(inDocument)) {
				document = win->createDocument(name);
			} else {
				return nullptr;
			}
		} else {
			document = createInNewWindow();
		}
	} else {
		// open file in untitled document
		document = inDocument;
		document->setPath(path);
		document->info_->filename = name;

		if (!iconic && !background) {
			document->raiseDocumentWindow();
		}
	}

	MainWindow *win = MainWindow::fromDocument(document);
	if (!win) {
		return nullptr;
	}

	// Open the file
	if (!document->doOpen(name, path, flags)) {
		document->closeDocument();
		return nullptr;
	}

	win->forceShowLineNumbers();

	// Decide what language mode to use, trigger language specific actions
	if (languageMode.isNull()) {
		document->determineLanguageMode(/*forceNewDefaults=*/true);
	} else {
		document->action_Set_Language_Mode(languageMode, /*forceNewDefaults=*/true);
	}

	// update tab label and tooltip
	document->refreshTabState();
	win->sortTabBar();

	if (!background) {
		document->raiseDocument();
	}

	/* Bring the title bar and statistics line up to date, doOpen does
	   not necessarily set the window title or read-only status */
	Q_EMIT document->updateWindowTitle(document);
	Q_EMIT document->updateWindowReadOnly(document);
	Q_EMIT document->updateStatus(document, nullptr);

	// Add the name to the convenience menu of previously opened files
	auto fullname = QStringLiteral("%1%2").arg(path, name);

	if (Preferences::GetPrefAlwaysCheckRelTagsSpecs()) {
		Tags::addRelTagsFile(Preferences::GetPrefTagFile(), path, Tags::SearchMode::TAG);
		for (MainWindow *window : MainWindow::allWindows()) {
			window->updateTagsFileMenu();
		}
	}

	MainWindow::addToPrevOpenMenu(fullname);
	return document;
}

/**
 * Used for creating a clone of a document, not quite ready yet...
 *
 * @brief DocumentWidget::DocumentWidget
 * @param info_ptr
 * @param parent
 * @param f
 */
DocumentWidget::DocumentWidget(std::shared_ptr<DocumentInfo> &info_ptr, QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f), info_(info_ptr) {

	Q_ASSERT(info_ptr);

	ui.setupUi(this);

	// track what the last created document was so that focus_document("last")
	// works correctly
	LastCreated = this;

	// create the text widget
	if (Settings::splitHorizontally) {
		splitter_ = new QSplitter(Qt::Horizontal, this);
	} else {
		splitter_ = new QSplitter(Qt::Vertical, this);
	}

	splitter_->setChildrenCollapsible(false);
	ui.verticalLayout->addWidget(splitter_);

	// NOTE(eteran): I'm not sure why this is necessary to make things look right :-/
#ifdef Q_OS_MACOS
	ui.verticalLayout->setContentsMargins(0, 5, 0, 0);
#elif defined(Q_OS_WIN)
	ui.verticalLayout->setContentsMargins(2, 2, 2, 2);
#else
	ui.verticalLayout->setContentsMargins(0, 0, 0, 0);
#endif

	// initialize the members
	highlightSyntax_ = Preferences::GetPrefHighlightSyntax();
	backlightChars_  = Preferences::GetPrefBacklightChars();
	fontName_        = Preferences::GetPrefFontName();
	font_            = Preferences::GetPrefDefaultFont();
	showStats_       = Preferences::GetPrefStatsLine();

	if (backlightChars_) {
		QString cTypes = Preferences::GetPrefBacklightCharTypes();
		if (!cTypes.isNull()) {
			backlightCharTypes_ = cTypes;
		}
	}

	showStatsLine(showStats_);

	flashTimer_ = new QTimer(this);
	flashTimer_->setInterval(FlashInterval);
	flashTimer_->setSingleShot(true);

	connect(flashTimer_, &QTimer::timeout, this, [this]() {
		eraseFlash();
	});

	auto area = createTextArea(info_->buffer);

	info_->buffer->BufAddModifyCB(modifiedCB, this);

	static int n = 0;
	area->setObjectName(tr("TextArea_Clone_%1").arg(n++));
	area->setBacklightCharTypes(backlightCharTypes_);
	splitter_->addWidget(area);
}

/**
 * @brief DocumentWidget::DocumentWidget
 * @param name
 * @param parent
 * @param f
 */
DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);

	info_           = std::make_shared<DocumentInfo>();
	info_->filename = name;

	// track what the last created document was so that focus_document("last")
	// works correctly
	LastCreated = this;
	setAcceptDrops(true);

	// Every document has a backing buffer
	info_->buffer = std::make_shared<TextBuffer>();
	info_->buffer->BufAddModifyCB(Highlight::SyntaxHighlightModifyCB, this);

	// create the text widget
	if (Settings::splitHorizontally) {
		splitter_ = new QSplitter(Qt::Horizontal, this);
	} else {
		splitter_ = new QSplitter(Qt::Vertical, this);
	}
	splitter_->setChildrenCollapsible(false);
	ui.verticalLayout->addWidget(splitter_);

	// NOTE(eteran): I'm not sure why this is necessary to make things look right :-/
#ifdef Q_OS_MACOS
	ui.verticalLayout->setContentsMargins(0, 5, 0, 0);
#elif defined(Q_OS_WIN)
	ui.verticalLayout->setContentsMargins(2, 2, 2, 2);
#else
	ui.verticalLayout->setContentsMargins(0, 0, 0, 0);
#endif

	// initialize the members
	info_->indentStyle       = Preferences::GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	info_->autoSave          = Preferences::GetPrefAutoSave();
	info_->saveOldVersion    = Preferences::GetPrefSaveOldVersion();
	info_->wrapMode          = Preferences::GetPrefWrap(PLAIN_LANGUAGE_MODE);
	info_->showMatchingStyle = Preferences::GetPrefShowMatching();
	info_->matchSyntaxBased  = Preferences::GetPrefMatchSyntaxBased();
	highlightSyntax_         = Preferences::GetPrefHighlightSyntax();
	backlightChars_          = Preferences::GetPrefBacklightChars();
	fontName_                = Preferences::GetPrefFontName();
	font_                    = Preferences::GetPrefDefaultFont();
	showStats_               = Preferences::GetPrefStatsLine();

	if (backlightChars_) {
		QString cTypes = Preferences::GetPrefBacklightCharTypes();
		if (!cTypes.isNull()) {
			backlightCharTypes_ = cTypes;
		}
	}

	showStatsLine(showStats_);

	flashTimer_ = new QTimer(this);
	flashTimer_->setInterval(FlashInterval);
	flashTimer_->setSingleShot(true);

	connect(flashTimer_, &QTimer::timeout, this, [this]() {
		eraseFlash();
	});

	auto area = createTextArea(info_->buffer);

	info_->buffer->BufAddModifyCB(modifiedCB, this);

	// Set the requested hardware tab distance and useTabs in the text buffer
	info_->buffer->BufSetTabDistance(Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE), true);
	info_->buffer->BufSetUseTabs(Preferences::GetPrefInsertTabs(PLAIN_LANGUAGE_MODE));

	static int n = 0;
	area->setObjectName(tr("TextArea_%1").arg(n++));
	area->setBacklightCharTypes(backlightCharTypes_);
	splitter_->addWidget(area);
}

/**
 * @brief DocumentWidget::~DocumentWidget
 */
DocumentWidget::~DocumentWidget() {

	// first delete all of the text area's so that they can properly
	// remove themselves from the buffer's callbacks
	const std::vector<TextArea *> textAreas = textPanes();
	qDeleteAll(textAreas);

	// And delete the rangeset table too for the same reasons
	rangesetTable_ = nullptr;

	// Free syntax highlighting patterns, if any. w/o redisplaying
	freeHighlightingData();

	info_->buffer->BufRemoveModifyCB(modifiedCB, this);
	info_->buffer->BufRemoveModifyCB(Highlight::SyntaxHighlightModifyCB, this);
}

/**
 * @brief DocumentWidget::createTextArea
 * @param buffer
 * @return
 */
TextArea *DocumentWidget::createTextArea(const std::shared_ptr<TextBuffer> &buffer) {

	auto area = new TextArea(this,
							 buffer.get(),
							 Preferences::GetPrefDefaultFont());

	area->setCursorVPadding(Preferences::GetVerticalAutoScroll());
	area->setEmulateTabs(Preferences::GetPrefEmTabDist(PLAIN_LANGUAGE_MODE));

	const QColor textFg   = X11Colors::fromString(Preferences::GetPrefColorName(TEXT_FG_COLOR));
	const QColor textBg   = X11Colors::fromString(Preferences::GetPrefColorName(TEXT_BG_COLOR));
	const QColor selectFg = X11Colors::fromString(Preferences::GetPrefColorName(SELECT_FG_COLOR));
	const QColor selectBg = X11Colors::fromString(Preferences::GetPrefColorName(SELECT_BG_COLOR));
	const QColor hiliteFg = X11Colors::fromString(Preferences::GetPrefColorName(HILITE_FG_COLOR));
	const QColor hiliteBg = X11Colors::fromString(Preferences::GetPrefColorName(HILITE_BG_COLOR));
	const QColor lineNoFg = X11Colors::fromString(Preferences::GetPrefColorName(LINENO_FG_COLOR));
	const QColor lineNoBg = X11Colors::fromString(Preferences::GetPrefColorName(LINENO_BG_COLOR));
	const QColor cursorFg = X11Colors::fromString(Preferences::GetPrefColorName(CURSOR_FG_COLOR));

	area->setColors(
		textFg,
		textBg,
		selectFg,
		selectBg,
		hiliteFg,
		hiliteBg,
		lineNoFg,
		lineNoBg,
		cursorFg);

	// add focus, drag, cursor tracking, and smart indent callbacks
	area->addCursorMovementCallback(movedCB, this);
	area->addDragStartCallback(dragStartCB, this);
	area->addDragEndCallback(dragEndCB, this);
	area->addSmartIndentCallback(smartIndentCB, this);

	// NOTE(eteran): we kinda cheat here. We want to have a custom context menu
	// but we don't want it to fire if the user is pressing Ctrl when they right click
	// so TextArea captures the default context menu event, then if appropriate
	// fires off a customContextMenuRequested event manually. So no need to set the
	// policy here, in fact, that would break things.
	//area->setContextMenuPolicy(Qt::CustomContextMenu);

	// just emit an event that will later be caught by the higher layer informing it of the context menu request
	connect(area, &TextArea::customContextMenuRequested, this, [this](const QPoint &pos) {
		Q_EMIT contextMenuRequested(this, pos);
	});

	return area;
}

/*
** Change the window appearance and the window to show that the file it
** contains has been modified
*/
void DocumentWidget::setWindowModified(bool modified) {
	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (!info_->fileChanged && modified) {
		win->ui.action_Close->setEnabled(true);
		info_->fileChanged = true;
	} else if (info_->fileChanged && !modified) {
		info_->fileChanged = false;
	}

	Q_EMIT updateWindowTitle(this);
	refreshTabState();
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void DocumentWidget::refreshTabState() {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	QTabWidget *tabWidget = win->tabWidget();
	int index             = tabWidget->indexOf(this);

	QString labelString;
	QString filename = info_->filename;
	if (Settings::truncateLongNamesInTabs != 0) {

		const int absTruncate = std::abs(Settings::truncateLongNamesInTabs);

		if (absTruncate > 3) {
			if (filename.size() > absTruncate) {
				if (Settings::truncateLongNamesInTabs > 0) {
					filename = QStringLiteral("%1%2").arg(QLatin1String("..."), filename.right(absTruncate - 3));
				} else {
					filename = QStringLiteral("%1%2").arg(filename.left(absTruncate - 3), QLatin1String("..."));
				}
			}
		} else {
			qDebug("NEdit: tab truncation is set to an unreasonably short value, try a value with a magnitude greater than 3");
		}
	}

	static const auto saveIcon = QIcon::fromTheme(QLatin1String("document-save"));
	if (!saveIcon.isNull()) {
		tabWidget->setTabIcon(index, info_->fileChanged ? saveIcon : QIcon());
		labelString = filename;
	} else {
		/* Set tab label to document's filename. Position of "*" (modified)
		 * will change per label alignment setting */
		QStyle *const style = tabWidget->tabBar()->style();
		const int alignment = style->styleHint(QStyle::SH_TabBar_Alignment);

		if (alignment != Qt::AlignRight) {
			labelString = QStringLiteral("%1%2").arg(info_->fileChanged ? tr("*") : QString(), filename);
		} else {
			labelString = QStringLiteral("%2%1").arg(info_->fileChanged ? tr("*") : QString(), filename);
		}
	}

	tabWidget->setTabText(index, labelString);
}

/*
** Apply language mode matching criteria and set languageMode_ to
** the appropriate mode for the current file, trigger language mode
** specific actions (turn on/off highlighting), and update the language
** mode menu item.  If forceNewDefaults is true, re-establish default
** settings for language-specific preferences regardless of whether
** they were previously set by the user.
*/
void DocumentWidget::determineLanguageMode(bool forceNewDefaults) {
	setLanguageMode(matchLanguageMode(), forceNewDefaults);
}

/**
 * @brief DocumentWidget::getLanguageMode
 * @return
 */
size_t DocumentWidget::getLanguageMode() const {
	return languageMode_;
}

/*
** Set the language mode for the window, update the menu and trigger language
** mode specific actions (turn on/off highlighting).  If forceNewDefaults is
** true, re-establish default settings for language-specific preferences
** regardless of whether they were previously set by the user.
*/
void DocumentWidget::setLanguageMode(size_t mode, bool forceNewDefaults) {

	// Do mode-specific actions
	reapplyLanguageMode(mode, forceNewDefaults);

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	// Select the correct language mode in the sub-menu
	QList<QAction *> languages = win->ui.action_Language_Mode->menu()->actions();

	auto it = std::find_if(languages.begin(), languages.end(), [mode](QAction *action) {
		return action->data().value<qulonglong>() == mode;
	});

	if (it != languages.end()) {
		(*it)->setChecked(true);
	}
}

/**
 * @brief DocumentWidget::action_Set_Language_Mode
 * @param languageMode
 * @param forceNewDefaults
 */
void DocumentWidget::action_Set_Language_Mode(const QString &languageMode, bool forceNewDefaults) {
	emit_event("set_language_mode", languageMode);
	setLanguageMode(Preferences::FindLanguageMode(languageMode), forceNewDefaults);
}

/**
 * @brief DocumentWidget::action_Set_Language_Mode
 * @param languageMode
 */
void DocumentWidget::action_Set_Language_Mode(const QString &languageMode) {
	action_Set_Language_Mode(languageMode, /*forceNewDefaults=*/false);
}

/**
 * @brief DocumentWidget::matchLanguageMode
 * @return
 */
size_t DocumentWidget::matchLanguageMode() const {

	/*... look for an explicit mode statement first */

	// Do a regular expression search on for recognition pattern
	const std::string first200 = info_->buffer->BufGetRange(info_->buffer->BufStartOfBuffer(), info_->buffer->BufStartOfBuffer() + 200);
	if (!first200.empty()) {
		for (size_t i = 0; i < Preferences::LanguageModes.size(); i++) {
			if (!Preferences::LanguageModes[i].recognitionExpr.isNull()) {

				boost::optional<Search::Result> searchResult = Search::SearchString(
					first200,
					Preferences::LanguageModes[i].recognitionExpr,
					Direction::Forward,
					SearchType::Regex,
					WrapMode::NoWrap,
					0,
					QString());

				if (searchResult) {
					return i;
				}
			}
		}
	}

	/* Look at file extension ("@@/" starts a ClearCase version extended path,
	   which gets appended after the file extension, and therefore must be
	   stripped off to recognize the extension to make ClearCase users happy) */
	int fileNameLen = info_->filename.size();

	const int versionExtendedPathIndex = ClearCase::GetVersionExtendedPathIndex(info_->filename);
	if (versionExtendedPathIndex != -1) {
		fileNameLen = versionExtendedPathIndex;
	}

	const QString file = info_->filename.left(fileNameLen);

	for (size_t i = 0; i < Preferences::LanguageModes.size(); i++) {
		Q_FOREACH (const QString &ext, Preferences::LanguageModes[i].extensions) {
			if (file.endsWith(ext)) {
				return i;
			}
		}
	}

	// no appropriate mode was found
	return PLAIN_LANGUAGE_MODE;
}

/**
 * @brief DocumentWidget::movedCallback
 * @param area
 */
void DocumentWidget::movedCallback(TextArea *area) {

	if (info_->ignoreModify) {
		return;
	}

	// update line and column nubers in statistics line
	Q_EMIT updateStatus(this, area);

	// Check the character before the cursor for matchable characters
	flashMatchingChar(area);

	// Check for changes to read-only status and/or file modifications
	checkForChangesToFile();

	if (QTimer *const blinkTimer = area->cursorBlinkTimer()) {
		if (!blinkTimer->isActive()) {
			//  Start blinking the caret again.
			blinkTimer->start();
		}
	}
}

/**
 * @brief DocumentWidget::dragStartCallback
 * @param area
 */
void DocumentWidget::dragStartCallback(TextArea *area) {
	Q_UNUSED(area)
	// don't record all of the intermediate drag steps for undo
	info_->ignoreModify = true;
}

/*
** Keep the marks in the windows book-mark table up to date across
** changes to the underlying buffer
*/
void DocumentWidget::updateMarkTable(TextCursor pos, int64_t nInserted, int64_t nDeleted) {

	for (auto &entry : markTable_) {
		Bookmark &bookmark = entry.second;
		maintainSelection(bookmark.sel, pos, nInserted, nDeleted);
		maintainPosition(bookmark.cursorPos, pos, nInserted, nDeleted);
	}
}

/**
 * @brief DocumentWidget::modifiedCallback
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param nRestyled
 * @param deletedText
 * @param area
 */
void DocumentWidget::modifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, TextArea *area) {
	Q_UNUSED(nRestyled)

	// number of distinct chars the user can typebefore NEdit gens. new backup file
	constexpr int AutoSaveCharLimit = 80;

	// number of distinct editing operations user can do before NEdit gens. new backup file
	constexpr int AutoSaveOpLimit = 8;

	const bool selected = info_->buffer->primary.hasSelection();

	// update the table of bookmarks
	if (!info_->ignoreModify) {
		updateMarkTable(pos, nInserted, nDeleted);
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	// Check and dim/undim selection related menu items
	if (info_->wasSelected != selected) {
		info_->wasSelected = selected;

		/* do not refresh window-level items (window, menu-bar etc) when
		 * motifying non-top document */
		if (isTopDocument()) {
			win->selectionChanged(selected);

			updateSelectionSensitiveMenus(selected);

			if (auto dialog = win->dialogReplace_) {
				dialog->UpdateReplaceActionButtons();
			}
		}
	}

	/* When the program needs to make a change to a text area without without
	   recording it for undo or marking file as changed it sets ignoreModify */
	if (info_->ignoreModify || (nDeleted == 0 && nInserted == 0)) {
		return;
	}

	// Make sure line number display is sufficient for new data
	win->updateLineNumDisp();

	/* Save information for undoing this operation (this call also counts
	   characters and editing operations for triggering autosave */
	saveUndoInformation(pos, nInserted, nDeleted, deletedText);

	// Trigger automatic backup if operation or character limits reached
	if (info_->autoSave && (info_->autoSaveCharCount > AutoSaveCharLimit || info_->autoSaveOpCount > AutoSaveOpLimit)) {
		writeBackupFile();
		info_->autoSaveCharCount = 0;
		info_->autoSaveOpCount   = 0;
	}

	// Indicate that the window has now been modified
	setWindowModified(true);

	// Update # of bytes, and line and col statistics
	Q_EMIT updateStatus(this, area);

	// Check if external changes have been made to file and warn user
	checkForChangesToFile();
}

/**
 * @brief DocumentWidget::modifiedCallback
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param nRestyled
 * @param deletedText
 */
void DocumentWidget::modifiedCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText) {
	modifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText, nullptr);
}

/**
 * @brief DocumentWidget::dragEndCallback
 * @param area
 * @param data
 */
void DocumentWidget::dragEndCallback(TextArea *area, const DragEndEvent *event) {

	// restore recording of undo information
	info_->ignoreModify = false;

	// Do nothing if drag operation was canceled
	if (event->nCharsInserted == 0) {
		return;
	}

	/* Save information for undoing this operation not saved while undo
	 * recording was off */
	modifiedCallback(event->startPos, event->nCharsInserted, event->nCharsDeleted, 0, event->deletedText, area);
}

/**
 * @brief DocumentWidget::smartIndentCallback
 * @param area
 * @param data
 */
void DocumentWidget::smartIndentCallback(TextArea *area, SmartIndentEvent *event) {

	Q_UNUSED(area)

	if (!info_->smartIndentData) {
		return;
	}

	switch (event->reason) {
	case CHAR_TYPED:
		executeModMacro(event);
		break;
	case NEWLINE_INDENT_NEEDED:
		executeNewlineMacro(event);
		break;
	}
}

/*
** raise the document and its window and optionally focus.
*/
void DocumentWidget::raiseFocusDocumentWindow(bool focus) {
	raiseDocument();

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (!win->isMaximized()) {
		win->showNormal();
	}

	win->raise();
	win->activateWindow();

	if (focus) {
		setFocus();
	}
}

/**
 * raise the document and its shell window and focus depending on pref.
 *
 * @brief DocumentWidget::RaiseDocumentWindow
 */
void DocumentWidget::raiseDocumentWindow() {
	raiseFocusDocumentWindow(Preferences::GetPrefFocusOnRaise());
}

/**
 * @brief DocumentWidget::documentRaised
 */
void DocumentWidget::documentRaised() {
	// Turn on syntax highlight that might have been deferred.
	if (highlightSyntax_ && !highlightData_) {
		startHighlighting(Verbosity::Silent);
	}

	refreshWindowStates();

	/* Make sure that the "In Selection" button tracks the presence of a
	   selection and that the this inherits the proper search scope. */
	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (auto dialog = win->dialogReplace_) {
		dialog->UpdateReplaceActionButtons();
	}
}

void DocumentWidget::raiseDocument() {
	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	win->tabWidget()->setCurrentWidget(this);
}

/*
** Change the language mode to the one indexed by "mode", reseting word
** delimiters, syntax highlighting and other mode specific parameters
*/
void DocumentWidget::reapplyLanguageMode(size_t mode, bool forceDefaults) {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	const size_t oldMode = languageMode_;

	/* If the mode is the same, and changes aren't being forced (as might
	   happen with Save As...), don't mess with already correct settings */
	if (languageMode_ == mode && !forceDefaults) {
		return;
	}

	// Change the mode name stored in the window
	languageMode_ = mode;

	// Decref oldMode's default calltips file if needed
	if (oldMode != PLAIN_LANGUAGE_MODE && !Preferences::LanguageModes[oldMode].defTipsFile.isNull()) {
		Tags::deleteTagsFile(Preferences::LanguageModes[oldMode].defTipsFile, Tags::SearchMode::TIP, false);
	}

	// Set delimiters for all text widgets
	QString delimiters;
	if (mode == PLAIN_LANGUAGE_MODE || Preferences::LanguageModes[mode].delimiters.isNull()) {
		delimiters = Preferences::GetPrefDelimiters();
	} else {
		delimiters = Preferences::LanguageModes[mode].delimiters;
	}

	const std::vector<TextArea *> textAreas = textPanes();
	for (TextArea *area : textAreas) {
		area->setWordDelimiters(delimiters.toStdString());
	}

	/* Decide on desired values for language-specific parameters.  If a
	   parameter was set to its default value, set it to the new default,
	   otherwise, leave it alone */
	const bool wrapModeIsDef  = (info_->wrapMode == Preferences::GetPrefWrap(oldMode));
	const bool tabDistIsDef   = (info_->buffer->BufGetTabDistance() == Preferences::GetPrefTabDist(oldMode));
	const bool tabInsertIsDef = (static_cast<int>(info_->buffer->BufGetUseTabs()) == Preferences::GetPrefInsertTabs(oldMode));

	const int oldEmTabDist            = textAreas[0]->getEmulateTabs();
	const QString oldlanguageModeName = Preferences::LanguageModeName(oldMode);

	const bool emTabDistIsDef   = oldEmTabDist == Preferences::GetPrefEmTabDist(oldMode);
	const bool indentStyleIsDef = info_->indentStyle == Preferences::GetPrefAutoIndent(oldMode) || (Preferences::GetPrefAutoIndent(oldMode) == IndentStyle::Smart && info_->indentStyle == IndentStyle::Auto && !SmartIndent::smartIndentMacrosAvailable(Preferences::LanguageModeName(oldMode)));
	const bool highlightIsDef   = highlightSyntax_ == Preferences::GetPrefHighlightSyntax() || (Preferences::GetPrefHighlightSyntax() && Highlight::FindPatternSet(oldlanguageModeName) == nullptr);
	const WrapStyle wrapMode    = wrapModeIsDef || forceDefaults ? Preferences::GetPrefWrap(mode) : info_->wrapMode;
	const int tabDist           = tabDistIsDef || forceDefaults ? Preferences::GetPrefTabDist(mode) : info_->buffer->BufGetTabDistance();
	const int insertTabs        = tabInsertIsDef || forceDefaults ? Preferences::GetPrefInsertTabs(mode) : info_->buffer->BufGetUseTabs();
	const int emTabDist         = emTabDistIsDef || forceDefaults ? Preferences::GetPrefEmTabDist(mode) : oldEmTabDist;
	IndentStyle indentStyle     = indentStyleIsDef || forceDefaults ? Preferences::GetPrefAutoIndent(mode) : info_->indentStyle;
	bool highlight              = highlightIsDef || forceDefaults ? Preferences::GetPrefHighlightSyntax() : highlightSyntax_;

	/* Dim/undim smart-indent and highlighting menu items depending on
	   whether patterns/macros are available */
	QString languageModeName   = Preferences::LanguageModeName(mode);
	bool haveHighlightPatterns = Highlight::FindPatternSet(languageModeName);
	bool haveSmartIndentMacros = SmartIndent::smartIndentMacrosAvailable(Preferences::LanguageModeName(mode));

	const bool topDocument = isTopDocument();

	if (topDocument) {
		win->ui.action_Highlight_Syntax->setEnabled(haveHighlightPatterns);
		win->ui.action_Indent_Smart->setEnabled(haveSmartIndentMacros);
	}

	// Turn off requested options which are not available
	highlight = haveHighlightPatterns && highlight;
	if (indentStyle == IndentStyle::Smart && !haveSmartIndentMacros) {
		indentStyle = IndentStyle::Auto;
	}

	// Change highlighting
	highlightSyntax_ = highlight;

	no_signals(win->ui.action_Highlight_Syntax)->setChecked(highlight);

	stopHighlighting();

	// we defer highlighting to RaiseDocument() if doc is hidden
	if (topDocument && highlight) {
		startHighlighting(Verbosity::Silent);
	}

	// Force a change of smart indent macros (SetAutoIndent will re-start)
	if (info_->indentStyle == IndentStyle::Smart) {
		endSmartIndent();
		info_->indentStyle = IndentStyle::Auto;
	}

	// set requested wrap, indent, and tabs
	setAutoWrap(wrapMode);
	setAutoIndent(indentStyle);
	setTabDistance(tabDist);
	setEmTabDistance(emTabDist);
	setInsertTabs(insertTabs);

	// Load calltips files for new mode
	if (mode != PLAIN_LANGUAGE_MODE && !Preferences::LanguageModes[mode].defTipsFile.isNull()) {
		Tags::addTagsFile(Preferences::LanguageModes[mode].defTipsFile, Tags::SearchMode::TIP);
	}

	// Add/remove language specific menu items
	win->updateUserMenus(this);
}

/**
 * @brief DocumentWidget::setTabDistance
 * @param distance
 */
void DocumentWidget::setTabDistance(int distance) {

	emit_event("set_tab_dist", QString::number(distance));

	if (info_->buffer->BufGetTabDistance() != distance) {
		TextCursor saveCursorPositions[MaxPanes];
		int saveVScrollPositions[MaxPanes];
		int saveHScrollPositions[MaxPanes];

		info_->ignoreModify = true;

		const std::vector<TextArea *> textAreas = textPanes();
		const size_t paneCount                  = textAreas.size();

		Q_ASSERT(paneCount <= MaxPanes);

		for (size_t index = 0; index < paneCount; ++index) {
			TextArea *area = textAreas[index];

			saveVScrollPositions[index] = area->verticalScrollBar()->value();
			saveHScrollPositions[index] = area->horizontalScrollBar()->value();
			saveCursorPositions[index]  = area->cursorPos();
			area->setModifyingTabDist(true);
		}

		info_->buffer->BufSetTabDistance(distance, true);

		for (size_t index = 0; index < paneCount; ++index) {
			TextArea *area = textAreas[index];

			area->setModifyingTabDist(false);
			area->TextSetCursorPos(saveCursorPositions[index]);
			area->verticalScrollBar()->setValue(saveVScrollPositions[index]);
			area->horizontalScrollBar()->setValue(saveHScrollPositions[index]);
		}

		info_->ignoreModify = false;
	}
}

/**
 * @brief DocumentWidget::setEmTabDistance
 * @param distance
 */
void DocumentWidget::setEmTabDistance(int distance) {

	emit_event("set_em_tab_dist", QString::number(distance));

	for (TextArea *area : textPanes()) {
		area->setEmulateTabs(distance);
	}
}

/**
 * @brief DocumentWidget::setInsertTabs
 * @param value
 */
void DocumentWidget::setInsertTabs(bool value) {

	emit_event("set_insert_tabs", QString::number(value));
	info_->buffer->BufSetUseTabs(value);
}

/*
** Set autoindent state to one of None, Auto, or Smart.
*/
void DocumentWidget::setAutoIndent(IndentStyle indentStyle) {

	const bool autoIndent  = (indentStyle == IndentStyle::Auto);
	const bool smartIndent = (indentStyle == IndentStyle::Smart);

	if (info_->indentStyle == IndentStyle::Smart && !smartIndent) {
		endSmartIndent();
	} else if (smartIndent && info_->indentStyle != IndentStyle::Smart) {
		beginSmartIndent(Verbosity::Verbose);
	}

	info_->indentStyle = indentStyle;

	for (TextArea *area : textPanes()) {
		area->setAutoIndent(autoIndent);
		area->setSmartIndent(smartIndent);
	}

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	no_signals(win->ui.action_Indent_Smart)->setChecked(smartIndent);
	no_signals(win->ui.action_Indent_On)->setChecked(autoIndent);
	no_signals(win->ui.action_Indent_Off)->setChecked(indentStyle == IndentStyle::None);
}

/*
** Select auto-wrap mode, one of None, Newline, or Continuous
*/
void DocumentWidget::setAutoWrap(WrapStyle wrapStyle) {

	emit_event("set_wrap_text", to_string(wrapStyle));

	const bool autoWrap = (wrapStyle == WrapStyle::Newline);
	const bool contWrap = (wrapStyle == WrapStyle::Continuous);

	for (TextArea *area : textPanes()) {
		area->setAutoWrap(autoWrap);
		area->setContinuousWrap(contWrap);
	}

	info_->wrapMode = wrapStyle;

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	no_signals(win->ui.action_Wrap_Auto_Newline)->setChecked(autoWrap);
	no_signals(win->ui.action_Wrap_Continuous)->setChecked(contWrap);
	no_signals(win->ui.action_Wrap_None)->setChecked(wrapStyle == WrapStyle::None);
}

/**
 * @brief DocumentWidget::textPanesCount
 * @return
 */
int DocumentWidget::textPanesCount() const {
	return splitter_->count();
}

/**
 * @brief DocumentWidget::textPanes
 * @return
 */
std::vector<TextArea *> DocumentWidget::textPanes() const {

	const int count = textPanesCount();

	std::vector<TextArea *> list;
	list.reserve(static_cast<size_t>(count));

	for (int i = 0; i < count; ++i) {
		if (auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
			list.push_back(area);
		}
	}

	return list;
}

/**
 * @brief DocumentWidget::isTopDocument
 * @return
 */
bool DocumentWidget::isTopDocument() const {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return false;
	}

	return win->currentDocument() == this;
}

/**
 * @brief DocumentWidget::getWindowsMenuEntry
 * @return
 */
QString DocumentWidget::getWindowsMenuEntry() const {

	auto fullTitle = tr("%1%2").arg(info_->filename, info_->fileChanged ? tr("*") : QString());

	if (Preferences::GetPrefShowPathInWindowsMenu() && info_->filenameSet) {
		fullTitle.append(tr(" - %1").arg(info_->path));
	}

	return fullTitle;
}

/*
** Turn off syntax highlighting and free style buffer, compiled patterns, and
** related data.
*/
void DocumentWidget::stopHighlighting() {
	if (!highlightData_) {
		return;
	}

	// Free and remove the highlight data from the window
	highlightData_ = nullptr;

	/* Remove and detach style buffer and style table from all text
	   display(s) of window, and redisplay without highlighting */
	for (TextArea *area : textPanes()) {
		area->removeWidgetHighlight();
	}
}

/**
 * @brief DocumentWidget::firstPane
 * @return
 */
TextArea *DocumentWidget::firstPane() const {

	for (int i = 0; i < splitter_->count(); ++i) {
		if (auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
			return area;
		}
	}

	return nullptr;
}

/*
** Get the set of word delimiters for the language mode set in the current
** document.  Returns QString() when no language mode is set (it would be easy
** to return the default delimiter set when the current language mode is
** "Plain", or the mode doesn't have its own delimiters, but this is usually
** used to supply delimiters for RE searching.
*/
QString DocumentWidget::documentDelimiters() const {
	if (languageMode_ == PLAIN_LANGUAGE_MODE) {
		return QString();
	} else {
		return Preferences::LanguageModes[languageMode_].delimiters;
	}
}

/*
** Disable/enable user programmable menu items which depend on there being
** a selection in their associated window.
*/
void DocumentWidget::updateSelectionSensitiveMenus(bool enabled) {

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	updateSelectionSensitiveMenu(win->ui.menu_Shell, ShellMenuData, enabled);
	updateSelectionSensitiveMenu(win->ui.menu_Macro, MacroMenuData, enabled);
	updateSelectionSensitiveMenu(win->contextMenu_, BGMenuData, enabled);
}

void DocumentWidget::updateSelectionSensitiveMenu(QMenu *menu, const gsl::span<MenuData> &menuList, bool enabled) {

	if (menu) {
		const QList<QAction *> actions = menu->actions();
		for (QAction *action : actions) {
			if (QMenu *subMenu = action->menu()) {
				updateSelectionSensitiveMenu(subMenu, menuList, enabled);
			} else {
				if (action->data().isValid()) {
					size_t index = action->data().toUInt();
					if (index >= menuList.size()) {
						return;
					}

					if (menuList[index].item.input == FROM_SELECTION) {
						action->setEnabled(enabled);
					}
				}
			}
		}
	}
}

/*
** saveUndoInformation stores away the changes made to the text buffer.  As a
** side effect, it also increments the autoSave operation and character counts
** since it needs to do the classification anyhow.
**
** Note: This routine must be kept efficient.  It is called for every
**       character typed.
*/
void DocumentWidget::saveUndoInformation(TextCursor pos, int64_t nInserted, int64_t nDeleted, view::string_view deletedText) {

	const int isUndo = (!info_->undo.empty() && info_->undo.front().inUndo);
	const int isRedo = (!info_->redo.empty() && info_->redo.front().inUndo);

	/* redo operations become invalid once the user begins typing or does
	   other editing.  If this is not a redo or undo operation and a redo
	   list still exists, clear it and dim the redo menu item */
	if (!(isUndo || isRedo) && !info_->redo.empty()) {
		clearRedoList();
	}

	/* figure out what kind of editing operation this is, and recall
	   what the last one was */
	const UndoTypes newType = determineUndoType(nInserted, nDeleted);
	if (newType == UNDO_NOOP) {
		return;
	}

	UndoInfo *const currentUndo = info_->undo.empty() ? nullptr : &info_->undo.front();

	const UndoTypes oldType = (!currentUndo || isUndo) ? UNDO_NOOP : currentUndo->type;

	/*
	** Check for continuations of single character operations.  These are
	** accumulated so a whole insertion or deletion can be undone, rather
	** than just the last character that the user typed.  If the document
	** is currently in an unmodified state, don't accumulate operations
	** across the save, so the user can undo back to the unmodified state.
	*/
	if (info_->fileChanged) {

		// normal sequential character insertion
		if (((oldType == ONE_CHAR_INSERT || oldType == ONE_CHAR_REPLACE) && newType == ONE_CHAR_INSERT) && (pos == currentUndo->endPos)) {
			++currentUndo->endPos;
			++info_->autoSaveCharCount;
			return;
		}

		// overstrike mode replacement
		if ((oldType == ONE_CHAR_REPLACE && newType == ONE_CHAR_REPLACE) && (pos == currentUndo->endPos)) {
			appendDeletedText(deletedText, nDeleted, Direction::Forward);
			++currentUndo->endPos;
			++info_->autoSaveCharCount;
			return;
		}

		// forward delete
		if ((oldType == ONE_CHAR_DELETE && newType == ONE_CHAR_DELETE) && (pos == currentUndo->startPos)) {
			appendDeletedText(deletedText, nDeleted, Direction::Forward);
			return;
		}

		// reverse delete
		if ((oldType == ONE_CHAR_DELETE && newType == ONE_CHAR_DELETE) && (pos == currentUndo->startPos - 1)) {
			appendDeletedText(deletedText, nDeleted, Direction::Backward);
			--currentUndo->startPos;
			--currentUndo->endPos;
			return;
		}
	}

	/*
	** The user has started a new operation, create a new undo record
	** and save the new undo data.
	*/
	UndoInfo undo(newType, pos, pos + nInserted);

	// if text was deleted, save it
	if (nDeleted > 0) {
		undo.oldText = deletedText.to_string();
	}

	// increment the operation count for the autosave feature
	++info_->autoSaveOpCount;

	/* if the this is currently unmodified, remove the previous
	   restoresToSaved marker, and set it on this record */

	// TODO(eteran): there probably is some data structure we could use
	// to avoid having to loop here.
	// as we have more entries in the undo/redo lists, the longer these loops
	// get
	if (!info_->fileChanged) {
		undo.restoresToSaved = true;

		for (UndoInfo &u : info_->undo) {
			u.restoresToSaved = false;
		}

		for (UndoInfo &u : info_->redo) {
			u.restoresToSaved = false;
		}
	}

	/* Add the new record to the undo list unless saveUndoInformation is
	   saving information generated by an Undo operation itself, in
	   which case, add the new record to the redo list. */
	if (isUndo) {
		addRedoItem(std::move(undo));
	} else {
		addUndoItem(std::move(undo));
	}
}

/*
** clearUndoList, clearRedoList
**
** Functions for clearing all of the information off of the undo or redo
** lists and adjusting the edit menu accordingly
*/
void DocumentWidget::clearUndoList() {

	info_->undo.clear();
	Q_EMIT canUndoChanged(!info_->undo.empty());
}

void DocumentWidget::clearRedoList() {

	info_->redo.clear();
	Q_EMIT canRedoChanged(!info_->redo.empty());
}

/*
** Add deleted text to the beginning or end
** of the text saved for undoing the last operation.  This routine is intended
** for continuing of a string of one character deletes or replaces, but will
** work with more than one character.
*/
void DocumentWidget::appendDeletedText(view::string_view deletedText, int64_t deletedLen, Direction direction) {
	UndoInfo &undo = info_->undo.front();

	// re-allocate, adding space for the new character(s)
	std::string comboText;
	comboText.reserve(undo.oldText.size() + static_cast<size_t>(deletedLen));

	// copy the new character and the already deleted text to the new memory
	if (direction == Direction::Forward) {
		comboText.append(undo.oldText);
		comboText.append(deletedText.begin(), deletedText.end());
	} else {
		comboText.append(deletedText.begin(), deletedText.end());
		comboText.append(undo.oldText);
	}

	// replace the old saved text and attach the new
	undo.oldText = std::move(comboText);
}

/*
** Add an undo record to the this's undo
** list if the item pushes the undo operation or character counts past the
** limits, trim the undo list to an acceptable length.
*/
void DocumentWidget::addUndoItem(UndoInfo &&undo) {

	info_->undo.emplace_front(std::move(undo));

	// Trim the list if it exceeds any of the limits
	if (info_->undo.size() > UNDO_OP_LIMIT) {
		trimUndoList(UNDO_OP_TRIMTO);
	}

	Q_EMIT canUndoChanged(!info_->undo.empty());
}

/*
** Add an item (already allocated by the caller) to the this's redo list.
*/
void DocumentWidget::addRedoItem(UndoInfo &&redo) {

	info_->redo.emplace_front(std::move(redo));
	Q_EMIT canRedoChanged(!info_->redo.empty());
}

/*
** Pop the current undo record from the undo list
*/
void DocumentWidget::removeUndoItem() {

	if (info_->undo.empty()) {
		return;
	}

	info_->undo.pop_front();
	Q_EMIT canUndoChanged(!info_->undo.empty());
}

/*
** Pop the current redo record from the redo list
*/
void DocumentWidget::removeRedoItem() {

	if (info_->redo.empty()) {
		return;
	}

	info_->redo.pop_front();
	Q_EMIT canRedoChanged(!info_->redo.empty());
}

/*
** Trim records off of the END of the undo list to reduce it to length
** maxLength
*/
void DocumentWidget::trimUndoList(size_t maxLength) {

	if (info_->undo.size() <= maxLength) {
		return;
	}

	auto it = info_->undo.begin();
	std::advance(it, maxLength);

	// Trim off all subsequent entries
	info_->undo.erase(it, info_->undo.end());
}

void DocumentWidget::undo() {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (info_->undo.empty()) {
		return;
	}

	UndoInfo &undo = info_->undo.front();

	/* BufReplaceEx will eventually call SaveUndoInformation.  This is mostly
	   good because it makes accumulating redo operations easier, however
	   SaveUndoInformation needs to know that it is being called in the context
	   of an undo.  The inUndo field in the undo record indicates that this
	   record is in the process of being undone. */
	undo.inUndo = true;

	// use the saved undo information to reverse changes
	info_->buffer->BufReplace(undo.startPos, undo.endPos, undo.oldText);

	const auto restoredTextLength = static_cast<int64_t>(undo.oldText.size());
	if (!info_->buffer->primary.hasSelection() || Preferences::GetPrefUndoModifiesSelection()) {
		/* position the cursor in the focus pane after the changed text
		   to show the user where the undo was done */
		if (QPointer<TextArea> area = win->lastFocus()) {
			area->TextSetCursorPos(undo.startPos + restoredTextLength);
		}
	}

	if (Preferences::GetPrefUndoModifiesSelection()) {
		if (restoredTextLength > 0) {
			info_->buffer->BufSelect(undo.startPos, undo.startPos + restoredTextLength);
		} else {
			info_->buffer->BufUnselect();
		}
	}

	if (QPointer<TextArea> area = win->lastFocus()) {
		makeSelectionVisible(area);
	}

	/* restore the file's unmodified status if the file was unmodified
	   when the change being undone was originally made.  Also, remove
	   the backup file, since the text in the buffer is now identical to
	   the original file */
	if (undo.restoresToSaved) {
		setWindowModified(false);
		removeBackupFile();
	}

	// free the undo record and remove it from the chain
	removeUndoItem();
}

void DocumentWidget::redo() {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (info_->redo.empty()) {
		return;
	}

	UndoInfo &redo = info_->redo.front();

	/* BufReplaceEx will eventually call SaveUndoInformation.  To indicate
	   to SaveUndoInformation that this is the context of a redo operation,
	   we set the inUndo indicator in the redo record */
	redo.inUndo = true;

	// use the saved redo information to reverse changes
	info_->buffer->BufReplace(redo.startPos, redo.endPos, redo.oldText);

	const auto restoredTextLength = static_cast<int64_t>(redo.oldText.size());
	if (!info_->buffer->primary.hasSelection() || Preferences::GetPrefUndoModifiesSelection()) {
		/* position the cursor in the focus pane after the changed text
		   to show the user where the undo was done */
		if (QPointer<TextArea> area = win->lastFocus()) {
			area->TextSetCursorPos(redo.startPos + restoredTextLength);
		}
	}

	if (Preferences::GetPrefUndoModifiesSelection()) {

		if (restoredTextLength > 0) {
			info_->buffer->BufSelect(redo.startPos, redo.startPos + restoredTextLength);
		} else {
			info_->buffer->BufUnselect();
		}
	}

	if (QPointer<TextArea> area = win->lastFocus()) {
		makeSelectionVisible(area);
	}

	/* restore the file's unmodified status if the file was unmodified
	   when the change being redone was originally made. Also, remove
	   the backup file, since the text in the buffer is now identical to
	   the original file */
	if (redo.restoresToSaved) {
		setWindowModified(/*modified=*/false);
		removeBackupFile();
	}

	// remove the redo record from the chain and free it
	removeRedoItem();
}

/**
 * @brief DocumentWidget::isReadOnly
 * @return
 */
bool DocumentWidget::isReadOnly() const {
	return info_->lockReasons.isAnyLocked();
}

/*
** Check the read-only or locked status of the window and beep and return
** false if the window should not be written in.
*/
bool DocumentWidget::checkReadOnly() const {
	if (isReadOnly()) {
		QApplication::beep();
		return true;
	}
	return false;
}

/*
** If the selection (or cursor position if there's no selection) is not
** fully shown, scroll to bring it in to view.
*/
void DocumentWidget::makeSelectionVisible(TextArea *area) {
	area->makeSelectionVisible();
	Q_EMIT updateStatus(this, area);
}

/*
** Remove the backup file associated with this window
*/
void DocumentWidget::removeBackupFile() const {

	// Don't delete backup files when backups aren't activated.
	if (!info_->autoSave) {
		return;
	}

	QFile::remove(backupFileName());
}

/*
** Generate the name of the backup file for this window from the filename
** and path in the window data structure & write into name
*/
QString DocumentWidget::backupFileName() const {

	if (info_->filenameSet) {
		return QStringLiteral("%1~%2").arg(info_->path, info_->filename);
	} else {
		return PrependHome(QStringLiteral("~%1").arg(info_->filename));
	}
}

/*
** Check if the file in the window was changed by an external source.
** and put up a warning dialog if it has.
*/
void DocumentWidget::checkForChangesToFile() {

	/* Maximum frequency of checking for external modifications. The periodic
	 * check is only performed on buffer modification, and the check interval is
	 * only to prevent checking on every keystroke in case of a file system which
	 * is slow to process stat requests (which I'm not sure exists) */
	constexpr auto CheckInterval = std::chrono::milliseconds(3000);

	// TODO(eteran): 2.0, this concept can probably be reworked in terms of QFileSystemWatcher
	static QPointer<DocumentWidget> lastCheckWindow;
	static std::chrono::high_resolution_clock::time_point lastCheckTime;

	if (!info_->filenameSet) {
		return;
	}

	// If last check was very recent, don't impact performance
	auto timestamp = std::chrono::high_resolution_clock::now();
	if (this == lastCheckWindow && (timestamp - lastCheckTime) < CheckInterval) {
		return;
	}

	lastCheckWindow = this;
	lastCheckTime   = timestamp;

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	/* Update the status, but don't pop up a dialog if we're called from a
	 * place where the window might be iconic (e.g., from the replace dialog)
	 * or on another desktop.
	 */
	const bool silent = (!isTopDocument() || !win->isVisible());

	// Get the file mode and modification time
	QString fullname = fullPath();

	QT_STATBUF statbuf;
	if (QT_STAT(fullname.toUtf8().data(), &statbuf) != 0) {

		const int error = errno;

		// Return if we've already warned the user or we can't warn them now
		if (info_->fileMissing || silent) {
			return;
		}

		/* Can't stat the file --
		 * maybe it's been deleted. The filename is now invalid */
		info_->fileMissing      = true;
		info_->statbuf.st_mtime = 1;
		info_->statbuf.st_dev   = 0;
		info_->statbuf.st_ino   = 0;

		/* Warn the user, if they like to be warned (Maybe this should be
		 * its own preference setting: GetPrefWarnFileDeleted()) */
		if (Preferences::GetPrefWarnFileMods()) {
			bool save = false;

			//  Set title, message body and button to match stat()'s error.
			switch (error) {
			case ENOENT: {
				// A component of the path file_name does not exist.
				int resp = QMessageBox::critical(
					this,
					tr("File not Found"),
					tr("File '%1' (or directory in its path) no longer exists.\n"
					   "Another program may have deleted or moved it.")
						.arg(info_->filename),
					QMessageBox::Save | QMessageBox::Cancel);
				save = (resp == QMessageBox::Save);
			} break;
			case EACCES: {
				// Search permission denied for a path component. We add one to the response because Re-Save wouldn't really make sense here.
				int resp = QMessageBox::critical(
					this,
					tr("Permission Denied"),
					tr("You no longer have access to file '%1'.\n"
					   "Another program may have changed the permissions of one of its parent directories.")
						.arg(info_->filename),
					QMessageBox::Save | QMessageBox::Cancel);
				save = (resp == QMessageBox::Save);
			} break;
			default: {
				// Everything else. This hints at an internal error (eg. ENOTDIR) or at some bad state at the host.
				int resp = QMessageBox::critical(
					this,
					tr("File not Accessible"),
					tr("Error while checking the status of file '%1':\n"
					   "    '%2'\n"
					   "Please make sure that no data is lost before closing this window.")
						.arg(info_->filename, errorString(error)),
					QMessageBox::Save | QMessageBox::Cancel);
				save = (resp == QMessageBox::Save);
			} break;
			}

			if (save) {
				saveDocument();
			}
		}

		// A missing or (re-)saved file can't be read-only.
		// NOTE: A document without a file can be locked though.
		// Make sure that the window was not destroyed behind our back!
		info_->lockReasons.setPermLocked(false);
		Q_EMIT updateWindowTitle(this);
		Q_EMIT updateWindowReadOnly(this);
		return;
	}

	/* Check that the file's read-only status is still correct (but
	   only if the file can still be opened successfully in read mode) */
	if (info_->statbuf.st_mode != statbuf.st_mode || info_->statbuf.st_uid != statbuf.st_uid || info_->statbuf.st_gid != statbuf.st_gid) {

		info_->statbuf.st_mode = statbuf.st_mode;
		info_->statbuf.st_uid  = statbuf.st_uid;
		info_->statbuf.st_gid  = statbuf.st_gid;

		QFile fp(fullname);
		if (fp.open(QIODevice::ReadWrite) || fp.open(QIODevice::ReadOnly)) {
			const bool readOnly = !fp.isWritable();
			fp.close();

			if (info_->lockReasons.isPermLocked() != readOnly) {
				info_->lockReasons.setPermLocked(readOnly);
				Q_EMIT updateWindowTitle(this);
				Q_EMIT updateWindowReadOnly(this);
			}
		}
	}

	/* Warn the user if the file has been modified, unless checking is
	 * turned off or the user has already been warned. */
	if (!silent && ((info_->statbuf.st_mtime != 0 && info_->statbuf.st_mtime != statbuf.st_mtime) || info_->fileMissing)) {

		info_->statbuf.st_mtime = 0; // Inhibit further warnings
		info_->fileMissing      = false;
		if (!Preferences::GetPrefWarnFileMods()) {
			return;
		}

		if (Preferences::GetPrefWarnRealFileMods() && !compareDocumentToFile(fullname)) {
			// Contents hasn't changed. Update the modification time.
			info_->statbuf.st_mtime = statbuf.st_mtime;
			return;
		}

		QMessageBox messageBox(this);
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setWindowTitle(tr("File modified externally"));
		QPushButton *buttonReload = messageBox.addButton(tr("Reload"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);

		Q_UNUSED(buttonCancel)

		if (info_->fileChanged) {
			messageBox.setText(tr("%1 has been modified by another program.  Reload?\n\nWARNING: Reloading will discard changes made in this editing session!").arg(info_->filename));
		} else {
			messageBox.setText(tr("%1 has been modified by another program.  Reload?").arg(info_->filename));
		}

		messageBox.exec();
		if (messageBox.clickedButton() == buttonReload) {
			revertToSaved();
		}
	}
}

/**
 * @brief DocumentWidget::fullPath
 * @return
 */
QString DocumentWidget::fullPath() const {

	if (info_->path.isEmpty()) {
		return QString();
	}

	Q_ASSERT(info_->path.endsWith(QLatin1Char('/')));
	return QStringLiteral("%1%2").arg(info_->path, info_->filename);
}

/**
 * @brief DocumentWidget::filename
 * @return
 */
QString DocumentWidget::filename() const {
	return info_->filename;
}

/**
 * @brief DocumentWidget::setFilename
 * @param filename
 */
void DocumentWidget::setFilename(const QString &filename) {
	info_->filename = filename;
}

/**
 * @brief DocumentWidget::path
 * @return
 */
QString DocumentWidget::path() const {
	return info_->path;
}

/*
 * Check if the contents of the TextBuffer is equal
 * the contens of the file named fileName. The format of
 * the file (UNIX/DOS/MAC) is handled properly.
 *
 * Return values
 * false: no difference found
 * true : difference found or could not compare contents.
 */
bool DocumentWidget::compareDocumentToFile(const QString &fileName) const {

	// Number of bytes read at once
	constexpr auto PREFERRED_CMPBUF_LEN = static_cast<int64_t>(0x8000);

	char pendingCR = '\0';

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		return true;
	}

	int64_t fileLen = file.size();

	// For UNIX/macOS files, we can do a quick check to see if the on disk file
	// has a different length, but for DOS files, it's not that simple...
	switch (info_->fileFormat) {
	case FileFormats::Unix:
	case FileFormats::Mac:
		if (fileLen != info_->buffer->length()) {
			return true;
		}
		break;
	case FileFormats::Dos:
		// However, if a DOS file is smaller on disk, it's certainly different
		if (fileLen < info_->buffer->length()) {
			return true;
		}
		break;
	}

	int64_t restLen = std::min(PREFERRED_CMPBUF_LEN, fileLen);

	TextCursor bufPos = {};
	int filePos       = 0;
	char fileString[PREFERRED_CMPBUF_LEN + 2];

	/* For large files, the comparison can take a while. If it takes too long,
	   the user should be given a clue about what is happening. */
	MainWindow::allDocumentsBusy(tr("Comparing externally modified %1 ...").arg(info_->filename));

	// make sure that we unbusy the windows when we're done
	auto _ = gsl::finally([]() {
		MainWindow::allDocumentsUnbusy();
	});

	while (restLen > 0) {

		size_t offset = 0;
		if (pendingCR) {
			fileString[offset++] = pendingCR;
		}

		int64_t nRead = file.read(fileString + offset, restLen);

		if (nRead != restLen) {
			return true;
		}

		filePos += nRead;
		nRead += offset;

		// check for on-disk file format changes, but only for the first chunk
		if (bufPos == 0 && info_->fileFormat != FormatOfFile(view::string_view(fileString, static_cast<size_t>(nRead)))) {
			return true;
		}

		switch (info_->fileFormat) {
		case FileFormats::Mac:
			ConvertFromMac(fileString, nRead);
			break;
		case FileFormats::Dos:
			ConvertFromDos(fileString, &nRead, &pendingCR);
			break;
		case FileFormats::Unix:
			break;
		}

		if (int rv = info_->buffer->compare(bufPos, fileString, nRead)) {
			return rv;
		}

		bufPos += nRead;
		restLen = std::min(fileLen - filePos, PREFERRED_CMPBUF_LEN);
	}

	if (pendingCR) {
		if (int rv = info_->buffer->compare(bufPos, pendingCR)) {
			return rv;
		}
		bufPos += 1;
	}

	if (bufPos != info_->buffer->length()) {
		return true;
	}

	return false;
}

void DocumentWidget::revertToSaved() {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	// Can't revert untitled windows
	if (!info_->filenameSet) {
		QMessageBox::warning(
			this,
			tr("Error"),
			tr("Window '%1' was never saved, can't re-read").arg(info_->filename));
		return;
	}

	const std::vector<TextArea *> textAreas = textPanes();
	const size_t panesCount                 = textAreas.size();

	Q_ASSERT(panesCount <= MaxPanes);

	TextCursor insertPositions[MaxPanes];
	int topLines[MaxPanes];
	int horizOffsets[MaxPanes];

	// save insert & scroll positions of all of the panes to restore later
	for (size_t i = 0; i < panesCount; i++) {
		TextArea *area     = textAreas[i];
		insertPositions[i] = area->cursorPos();
		topLines[i]        = area->verticalScrollBar()->value();
		horizOffsets[i]    = area->horizontalScrollBar()->value();
	}

	// re-read the file, update the window title if new file is different
	const QString name = info_->filename;
	const QString path = info_->path;

	removeBackupFile();
	clearUndoList();

	const int openFlags = info_->lockReasons.isUserLocked() ? EditFlags::PREF_READ_ONLY : 0;
	if (!doOpen(name, path, openFlags)) {
		/* This is a bit sketchy.  The only error in doOpen that irreperably
		   damages the window is "too much binary data".  It should be
		   pretty rare to be reverting something that was fine only to find
		   that now it has too much binary data. */
		if (!info_->fileMissing) {
			closeDocument();
		} else {
			// Treat it like an externally modified file
			info_->statbuf.st_mtime = 0;
			info_->fileMissing      = false;
		}
		return;
	}

	win->forceShowLineNumbers();
	Q_EMIT updateWindowTitle(this);
	Q_EMIT updateWindowReadOnly(this);

	// restore the insert and scroll positions of each pane
	for (size_t i = 0; i < panesCount; i++) {
		TextArea *area = textAreas[i];
		area->TextSetCursorPos(insertPositions[i]);
		area->verticalScrollBar()->setValue(topLines[i]);
		area->horizontalScrollBar()->setValue(horizOffsets[i]);
	}
}

/*
** Create a backup file for the current document.  The name for the backup file
** is generated using the name and path stored in the window and adding a
** tilde (~) on UNIX.
*/
bool DocumentWidget::writeBackupFile() {
	FILE *fp;

	// Generate a name for the autoSave file
	const QString name = backupFileName();

	// remove the old backup file. Well, this might fail - we'll notice later however.
	QFile::remove(name);

	/* open the file, set more restrictive permissions (using default
	   permissions was somewhat of a security hole, because permissions were
	   independent of those of the original file being edited */
#ifdef Q_OS_WIN
	int fd = QT_OPEN(name.toUtf8().data(), QT_OPEN_CREAT | O_EXCL | QT_OPEN_WRONLY, _S_IREAD | _S_IWRITE);
#else
	int fd = QT_OPEN(name.toUtf8().data(), QT_OPEN_CREAT | O_EXCL | QT_OPEN_WRONLY, S_IRUSR | S_IWUSR);
#endif
	if (fd < 0 || (fp = FDOPEN(fd, "w")) == nullptr) {

		QMessageBox::warning(
			this,
			tr("Error writing Backup"),
			tr("Unable to save backup for %1:\n%2\nAutomatic backup is now off").arg(info_->filename, errorString(errno)));

		info_->autoSave = false;

		if (auto win = MainWindow::fromDocument(this)) {
			no_signals(win->ui.action_Incremental_Backup)->setChecked(false);
		}
		return false;
	}

	// get the text buffer contents
	std::string fileString = info_->buffer->BufGetAll();

	// add a terminating newline if the file doesn't already have one
	if (Preferences::GetPrefAppendLF()) {
		if (!fileString.empty() && fileString.back() != '\n') {
			fileString.append("\n");
		}
	}

	auto _ = gsl::finally([fp] { ::fclose(fp); });

	// write out the file
	::fwrite(fileString.data(), 1, fileString.size(), fp);
	if (::ferror(fp)) {
		QMessageBox::critical(
			this,
			tr("Error saving Backup"),
			tr("Error while saving backup for %1:\n%2\nAutomatic backup is now off").arg(info_->filename, errorString(errno)));

		QFile::remove(name);
		info_->autoSave = false;
		return false;
	}

	return true;
}

/**
 * @brief DocumentWidget::saveDocument
 * @return
 */
bool DocumentWidget::saveDocument() {

	// Try to ensure our information is up-to-date
	checkForChangesToFile();

	/* Return success if the file is normal & unchanged or is a
		read-only file. */
	if ((!info_->fileChanged && !info_->fileMissing && info_->statbuf.st_mtime > 0) || info_->lockReasons.isAnyLockedIgnoringPerm()) {
		return true;
	}

	// Prompt for a filename if this is an Untitled window
	if (!info_->filenameSet) {
		// empty string signals a prompt for filename
		return saveDocumentAs(QString(), /*addWrap=*/false);
	}

	// Check for external modifications and warn the user
	if (Preferences::GetPrefWarnFileMods() && fileWasModifiedExternally()) {

		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Save File"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("%1 has been modified by another program.\n\n"
							  "Continuing this operation will overwrite any external\n"
							  "modifications to the file since it was opened in NEdit,\n"
							  "and your work or someone else's may potentially be lost.\n\n"
							  "To preserve the modified file, cancel this operation and\n"
							  "use Save As... to save this file under a different name,\n"
							  "or Revert to Saved to revert to the modified version.")
							   .arg(info_->filename));

		QPushButton *buttonContinue = messageBox.addButton(tr("Continue"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonCancel)

		messageBox.exec();
		if (messageBox.clickedButton() != buttonContinue) {
			// Cancel and mark file as externally modified
			info_->statbuf.st_mtime = 0;
			info_->fileMissing      = false;
			return false;
		}
	}

	if (writeBckVersion()) {
		return false;
	}

	const bool status = doSave();
	if (status) {
		removeBackupFile();
	}

	return status;
}

bool DocumentWidget::doSave() {

	QString fullname = fullPath();

	/*  Check for root and warn him if he wants to write to a file with
		none of the write bits set.  */
	if (isAdministrator()) {
		QFileInfo fi(fullname);
		if (fi.exists() && ((fi.permissions() & (QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup)) == 0)) {
			int result = QMessageBox::warning(
				this,
				tr("Writing Read-only File"),
				tr("File '%1' is marked as read-only.\nDo you want to save anyway?").arg(info_->filename),
				QMessageBox::Save | QMessageBox::Cancel);

			if (result != QMessageBox::Save) {
				return true;
			}
		}
	}

	/* add a terminating newline if the file doesn't already have one for
	   Unix utilities which get confused otherwise
	   NOTE: this must be done _before_ we create/open the file, because the
			 (potential) buffer modification can trigger a check for file
			 changes. If the file is created for the first time, it has
			 zero size on disk, and the check would falsely conclude that the
			 file has changed on disk, and would pop up a warning dialog */
	if (Preferences::GetPrefAppendLF() && !info_->buffer->BufIsEmpty() && info_->buffer->back() != '\n') {
		info_->buffer->BufAppend('\n');
	}

	// open the file
	QFile file(fullname);
	if (!file.open(QIODevice::WriteOnly)) {
		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Error saving File"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("Unable to save %1:\n%2\n\nSave as a new file?").arg(info_->filename, file.errorString()));

		QPushButton *buttonSaveAs = messageBox.addButton(tr("Save As..."), QMessageBox::AcceptRole);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonCancel)

		messageBox.exec();
		if (messageBox.clickedButton() == buttonSaveAs) {
			// empty string signals a prompt for filename
			return saveDocumentAs(QString(), /*addWrap=*/false);
		}

		return false;
	}

	// get the text buffer contents and its length
	std::string text = info_->buffer->BufGetAll();

	// If the file is to be saved in DOS or Macintosh format, reconvert
	switch (info_->fileFormat) {
	case FileFormats::Dos:
		ConvertToDos(text);
		break;
	case FileFormats::Mac:
		ConvertToMac(text);
		break;
	default:
		break;
	}

	// write to the file
	if (file.write(text.data(), static_cast<int64_t>(text.size())) == -1) {
		QMessageBox::critical(this, tr("Error saving File"), tr("%1 not saved:\n%2").arg(info_->filename, file.errorString()));
		file.close();
		file.remove();
		return false;
	}

	// success, file was written
	setWindowModified(false);

	// update the modification time
	QT_STATBUF statbuf;
	if (QT_STAT(fullname.toUtf8().data(), &statbuf) == 0) {
		info_->statbuf.st_mtime = statbuf.st_mtime;
		info_->fileMissing      = false;
		info_->statbuf.st_dev   = statbuf.st_dev;
		info_->statbuf.st_ino   = statbuf.st_ino;
	} else {
		// This needs to produce an error message -- the file can't be accessed!
		info_->statbuf.st_mtime = 0;
		info_->fileMissing      = true;
		info_->statbuf.st_dev   = 0;
		info_->statbuf.st_ino   = 0;
	}

	return true;
}

/**
 * @brief DocumentWidget::saveDocumentAs
 * @param newName
 * @param addWrap
 * @return
 */
bool DocumentWidget::saveDocumentAs(const QString &newName, bool addWrap) {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return false;
	}

	QString fullname;

	if (newName.isNull()) {
		fullname = MainWindow::promptForNewFile(this, &info_->fileFormat, &addWrap);
		if (fullname.isNull()) {
			return false;
		}
	} else {
		fullname = newName;
	}

	// Add newlines if requested
	if (addWrap) {
		addWrapNewlines();
	}

	const PathInfo fi = parseFilename(fullname);

	// If the requested file is this file, just save it and return
	if (info_->filename == fi.filename && info_->path == fi.pathname) {
		if (writeBckVersion()) {
			return false;
		}

		return doSave();
	}

	// If the file is open in another window, make user close it.
	if (DocumentWidget *otherWindow = MainWindow::findWindowWithFile(fi)) {

		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("File open"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("%1 is open in another NEdit window").arg(fi.filename));
		QPushButton *buttonCloseOther = messageBox.addButton(tr("Close Other Window"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel     = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonCloseOther)

		messageBox.exec();
		if (messageBox.clickedButton() == buttonCancel) {
			return false;
		}

		/*
		 * after doing the dialog, check again whether the window still
		 * exists in case the user somehow closed the window
		 */
		if (otherWindow == MainWindow::findWindowWithFile(fi)) {
			if (!otherWindow->closeFileAndWindow(CloseMode::Prompt)) {
				return false;
			}
		}
	}

	// Change the name of the file and save it under the new name
	removeBackupFile();
	setPath(fi.pathname);
	info_->filename        = fi.filename;
	info_->statbuf.st_mode = 0;
	info_->statbuf.st_uid  = 0;
	info_->statbuf.st_gid  = 0;

	info_->lockReasons.clear();
	const int retVal = doSave();
	Q_EMIT updateWindowReadOnly(this);
	refreshTabState();

	// Add the name to the convenience menu of previously opened files
	MainWindow::addToPrevOpenMenu(fullname);

	/*  If name has changed, language mode may have changed as well, unless
		it's an Untitled window for which the user already set a language
		mode; it's probably the right one.  */
	if (languageMode_ == PLAIN_LANGUAGE_MODE || info_->filenameSet) {
		determineLanguageMode(false);
	}
	info_->filenameSet = true;

	// Update the stats line and window title with the new filename
	Q_EMIT updateWindowTitle(this);
	Q_EMIT updateStatus(this, nullptr);

	win->sortTabBar();
	return retVal;
}

/*
** Change a document created in NEdit's continuous wrap mode to the more
** conventional Unix format of embedded newlines.  Indicate to the user
** by turning off Continuous Wrap mode.
*/
void DocumentWidget::addWrapNewlines() {

	TextCursor insertPositions[MaxPanes];
	int topLines[MaxPanes];

	const std::vector<TextArea *> textAreas = textPanes();
	const size_t paneCount                  = textAreas.size();

	Q_ASSERT(paneCount <= MaxPanes);

	// save the insert and scroll positions of each pane
	for (size_t i = 0; i < paneCount; ++i) {
		TextArea *area     = textAreas[i];
		insertPositions[i] = area->cursorPos();
		topLines[i]        = area->verticalScrollBar()->value();
	}

	// Modify the buffer to add wrapping
	TextArea *area         = textAreas[0];
	std::string fileString = area->TextGetWrapped(info_->buffer->BufStartOfBuffer(), info_->buffer->BufEndOfBuffer());

	info_->buffer->BufSetAll(fileString);

	// restore the insert and scroll positions of each pane
	for (size_t i = 0; i < paneCount; ++i) {
		TextArea *area = textAreas[i];
		area->TextSetCursorPos(insertPositions[i]);
		area->verticalScrollBar()->setValue(topLines[i]);
		area->horizontalScrollBar()->setValue(0);
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	/* Show the user that something has happened by turning off
	   Continuous Wrap mode */
	no_signals(win->ui.action_Wrap_Continuous)->setChecked(false);
}

/*
** If saveOldVersion is on, copies the existing version of the file to
** <filename>.bck in anticipation of a new version being saved.  Returns
** true if backup fails and user requests that the new file not be written.
*/
bool DocumentWidget::writeBckVersion() {

	struct BackupError {
		QString filename;
		QString message;
	};

	try {
		// TODO(eteran): 2.0, this is essentially just a QFile::copy("filename", "filename.bck");
		// with error reporting

		// Do only if version backups are turned on
		if (!info_->saveOldVersion) {
			return false;
		}

		// Get the full name of the file
		QString fullname = fullPath();

		// Generate name for old version
		auto bckname = QStringLiteral("%1.bck").arg(fullname);

		// Delete the old backup file
		// Errors are ignored; we'll notice them later.
		QFile::remove(bckname);

		/* open the file being edited.  If there are problems with the
		 * old file, don't bother the user, just skip the backup */
		QFile inputFile(fullname);
		if (!inputFile.open(QIODevice::ReadOnly)) {
			return false;
		}

		/* Get permissions of the file.
		 * We preserve the normal permissions but not ownership, extended
		 * attributes, et cetera. */
		QT_STATBUF statbuf;
		if (QT_FSTAT(inputFile.handle(), &statbuf) != 0) {
			return false;
		}

		// open the destination file exclusive and with restrictive permissions.
#ifdef Q_OS_WIN
		int out_fd = QT_OPEN(bckname.toUtf8().data(), QT_OPEN_CREAT | O_EXCL | QT_OPEN_TRUNC | QT_OPEN_WRONLY, _S_IREAD | _S_IWRITE);
#else
		int out_fd = QT_OPEN(bckname.toUtf8().data(), QT_OPEN_CREAT | O_EXCL | QT_OPEN_TRUNC | QT_OPEN_WRONLY, S_IRUSR | S_IWUSR);
#endif
		if (out_fd < 0) {
			Raise<BackupError>(bckname, tr("Error open backup file"));
		}

		auto _2 = gsl::finally([out_fd]() {
			QT_CLOSE(out_fd);
		});

#ifdef Q_OS_UNIX
		// Set permissions on new file
		if (::fchmod(out_fd, statbuf.st_mode) != 0) {
			QFile::remove(bckname);
			Raise<BackupError>(bckname, tr("fchmod() failed"));
		}
#endif

		// Allocate I/O buffer
		constexpr size_t IO_BUFFER_SIZE = (1024 * 1024);
		auto io_buffer                  = std::make_unique<char[]>(IO_BUFFER_SIZE);

		// copy loop
		for (;;) {
			qint64 bytes_read = inputFile.read(&io_buffer[0], IO_BUFFER_SIZE);

			if (bytes_read < 0) {
				QFile::remove(bckname);
				Raise<BackupError>(info_->filename, tr("read() error"));
			}

			if (bytes_read == 0) {
				break; // EOF
			}

			// write to the file
			qint64 bytes_written = QT_WRITE(out_fd, &io_buffer[0], static_cast<size_t>(bytes_read));
			if (bytes_written != bytes_read) {
				QFile::remove(bckname);
				Raise<BackupError>(bckname, errorString(errno));
			}
		}

		return false;
	} catch (const BackupError &e) {
		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Error writing Backup"));
		messageBox.setIcon(QMessageBox::Critical);
		messageBox.setText(tr("Couldn't write .bck (last version) file.\n%1: %2").arg(e.filename, e.message));

		QPushButton *buttonCancelSave = messageBox.addButton(tr("Cancel Save"), QMessageBox::RejectRole);
		QPushButton *buttonTurnOff    = messageBox.addButton(tr("Turn off Backups"), QMessageBox::AcceptRole);
		QPushButton *buttonContinue   = messageBox.addButton(tr("Continue"), QMessageBox::AcceptRole);

		Q_UNUSED(buttonContinue)

		messageBox.exec();
		if (messageBox.clickedButton() == buttonCancelSave) {
			return true;
		}

		if (messageBox.clickedButton() == buttonTurnOff) {
			info_->saveOldVersion = false;

			if (auto win = MainWindow::fromDocument(this)) {
				no_signals(win->ui.action_Make_Backup_Copy)->setChecked(false);
			}
		}

		return false;
	}
}

/*
** Return true if the file displayed in window has been modified externally
** to nedit.  This should return false if the file has been deleted or is
** unavailable.
*/
bool DocumentWidget::fileWasModifiedExternally() const {

	if (!info_->filenameSet) {
		return false;
	}

	QString fullname = fullPath();

	QT_STATBUF statbuf;
	if (QT_STAT(fullname.toLocal8Bit().data(), &statbuf) != 0) {
		return false;
	}

	if (info_->statbuf.st_mtime == statbuf.st_mtime) {
		return false;
	}

	if (Preferences::GetPrefWarnRealFileMods() && !compareDocumentToFile(fullname)) {
		return false;
	}

	return true;
}

bool DocumentWidget::closeFileAndWindow(CloseMode preResponse) {

	/* If the window is a normal & unmodified file or an empty new file,
	   or if the user wants to ignore external modifications then
	   just close it.  Otherwise ask for confirmation first. */
	if (!info_->fileChanged &&
		/* Normal File */
		((!info_->fileMissing && info_->statbuf.st_mtime > 0) ||
		 /* New File */
		 (info_->fileMissing && info_->statbuf.st_mtime == 0) ||
		 /* File deleted/modified externally, ignored by user. */
		 !Preferences::GetPrefWarnFileMods())) {

		closeDocument();
		// up-to-date windows don't have outstanding backup files to close
	} else {

		int response = QMessageBox::Yes;
		switch (preResponse) {
		case CloseMode::Prompt:
			response = QMessageBox::warning(
				this,
				tr("Save File"),
				tr("Save %1 before closing?").arg(info_->filename),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
			break;
		case CloseMode::Save:
			response = QMessageBox::Yes;
			break;
		case CloseMode::NoSave:
			response = QMessageBox::No;
			break;
		}

		switch (response) {
		case QMessageBox::Yes:
			// Save
			if (saveDocument()) {
				closeDocument();
			} else {
				return false;
			}
			break;
		case QMessageBox::No:
			// Don't Save
			removeBackupFile();
			closeDocument();
			break;
		default:
			return false;
		}
	}

	return true;
}

/*
** Close a document, or an editor window
*/
void DocumentWidget::closeDocument() {

	// Free smart indent macro programs
	endSmartIndent();

	/* Clean up macro references to the doomed window.  If a macro is
	   executing, stop it.  If macro is calling this (closing its own
	   window), leave the window alive until the macro completes */
	const bool keepWindow = !macroWindowCloseActions();

	// Kill shell sub-process
	abortShellCommand();

	// Unload the default tips files for this language mode if necessary
	unloadLanguageModeTipsFile();

	/* if this is the last window, or must be kept alive temporarily because
	   it's running the macro calling us, don't close it, make it Untitled */
	const size_t windowCount   = MainWindow::allWindows().size();
	const size_t documentCount = DocumentWidget::allDocuments().size();

	Q_EMIT documentClosed();

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (keepWindow || (windowCount == 1 && documentCount == 1)) {

		// clearing the existing name first ensures that uniqueUntitledName()
		// will find the actual first available untitled name
		info_->filename.clear();

		QString name = MainWindow::uniqueUntitledName();
		info_->lockReasons.clear();

		info_->statbuf.st_mode  = 0;
		info_->statbuf.st_uid   = 0;
		info_->statbuf.st_gid   = 0;
		info_->statbuf.st_mtime = 0;
		info_->statbuf.st_dev   = 0;
		info_->statbuf.st_ino   = 0;
		info_->filename         = name;
		setPath(QString());

		markTable_.clear();

		// clear the buffer, but ignore changes
		info_->ignoreModify = true;
		info_->buffer->BufSetAll(view::string_view());
		info_->ignoreModify = false;

		info_->filenameSet = false;
		info_->fileChanged = false;
		info_->fileMissing = true;
		info_->fileFormat  = FileFormats::Unix;

		stopHighlighting();
		endSmartIndent();
		Q_EMIT updateWindowTitle(this);
		Q_EMIT updateWindowReadOnly(this);
		win->ui.action_Close->setEnabled(false);
		win->ui.action_Read_Only->setEnabled(true);
		win->ui.action_Read_Only->setChecked(false);
		clearUndoList();
		clearRedoList();
		Q_EMIT updateStatus(this, nullptr);
		determineLanguageMode(true);
		refreshTabState();
		win->updateLineNumDisp();
		return;
	}

	// deallocate the document data structure

	// NOTE(eteran): removing the tab does not automatically delete the widget
	// so we also schedule it for deletion later
	win->ui.tabWidget->removeTab(win->ui.tabWidget->indexOf(this));
	this->deleteLater();

	// Close of window running a macro may have been disabled.
	// NOTE(eteran): this may be redundant...
	MainWindow::checkCloseEnableState();
	MainWindow::updateWindowMenus();

	// if we deleted the last tab, then we can close the window too
	if (win->tabCount() == 0) {
		win->deleteLater();
		win->setVisible(false);
	}

	// The number of open windows may have changed...
	const std::vector<MainWindow *> windows = MainWindow::allWindows();
	const bool enabled                      = windows.size() > 1;
	for (MainWindow *window : windows) {
		window->ui.action_Move_Tab_To->setEnabled(enabled);
	}
}

/**
 * @brief DocumentWidget::fromArea
 * @param area
 * @return
 */
DocumentWidget *DocumentWidget::fromArea(TextArea *area) {

	if (!area) {
		return nullptr;
	}

	return area->document();
}

/**
 * @brief DocumentWidget::open
 * @param fullpath
 */
DocumentWidget *DocumentWidget::open(const QString &fullpath) {

	const PathInfo fi        = parseFilename(fullpath);
	DocumentWidget *document = DocumentWidget::editExistingFile(
		this,
		fi.filename,
		fi.pathname,
		0,
		QString(),
		/*iconic=*/false,
		QString(),
		Preferences::GetPrefOpenInTab(),
		/*bgOpen=*/false);

	MainWindow::checkCloseEnableState();

	return document;
}

/**
 * @brief DocumentWidget::doOpen
 * @param name
 * @param path
 * @param flags
 * @return
 */
bool DocumentWidget::doOpen(const QString &name, const QString &path, int flags) {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return false;
	}

	// initialize lock reasons
	info_->lockReasons.clear();

	// Update the window data structure
	setPath(path);
	info_->filename    = name;
	info_->filenameSet = true;
	info_->fileMissing = true;

	FILE *fp = nullptr;

	// Get the full name of the file
	const QString fullname = fullPath();

	// Open the file
	/* The only advantage of this is if you use clearcase,
	   which messes up the mtime of files opened with r+,
	   even if they're never actually written.
	   To avoid requiring special builds for clearcase users,
	   this is now the default.
	*/
	{
		if ((fp = ::fopen(fullname.toUtf8().data(), "r"))) {

			// detect if the file is readable, but not writable
			QFile file(fullname);
			if (file.open(QIODevice::ReadWrite) || file.open(QIODevice::ReadOnly)) {
				info_->lockReasons.setPermLocked(!file.isWritable());
			}

		} else if (flags & EditFlags::CREATE && errno == ENOENT) {
			// Give option to create (or to exit if this is the only window)
			if (!(flags & EditFlags::SUPPRESS_CREATE_WARN)) {

				QMessageBox msgbox(this);
				msgbox.setIcon(QMessageBox::Warning);
				msgbox.setWindowTitle(tr("New File"));
				msgbox.setText(tr("Can't open %1:\n%2").arg(fullname, errorString(errno)));

				QAbstractButton *exitButton   = nullptr;
				QAbstractButton *cancelButton = msgbox.addButton(QMessageBox::Cancel);
				QAbstractButton *newButton    = msgbox.addButton(tr("New File"), QMessageBox::AcceptRole);

				// ask user for next action if file not found
				std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

				if (documents.size() == 1 && documents.front() == this) {
					exitButton = msgbox.addButton(tr("Exit NEdit"), QMessageBox::RejectRole);
				}

				Q_UNUSED(newButton)

				msgbox.exec();

				if (msgbox.clickedButton() == cancelButton) {
					return false;
				} else if (msgbox.clickedButton() == exitButton) {
					// NOTE(eteran): we use ::exit here because if we got here
					// it's because we failed to open the initial documents on
					// launch and the user has opted to no longer continue
					// running nedit-ng
					::exit(0);
				}
			}

			// Test if new file can be created
			const int fd = QT_OPEN(fullname.toUtf8().data(), QT_OPEN_CREAT | QT_OPEN_WRONLY | QT_OPEN_TRUNC, 0666);
			if (fd == -1) {
				QMessageBox::critical(this, tr("Error creating File"), tr("Can't create %1:\n%2").arg(fullname, errorString(errno)));
				return false;
			} else {
				QT_CLOSE(fd);
				QFile::remove(fullname);
			}

			setWindowModified(false);
			if ((flags & EditFlags::PREF_READ_ONLY) != 0) {
				info_->lockReasons.setUserLocked(true);
			}

			Q_EMIT updateWindowReadOnly(this);
			return true;
		} else {
			// A true error
			QMessageBox::critical(this, tr("Error opening File"), tr("Could not open %1%2:\n%3").arg(path, name, errorString(errno)));
			return false;
		}
	}

	auto _ = gsl::finally([fp] { ::fclose(fp); });

	/* Get the length of the file, the protection mode, and the time of the
	   last modification to the file */
	QT_STATBUF statbuf;
	if (QT_FSTAT(QT_FILENO(fp), &statbuf) != 0) {
		info_->filenameSet = false; // Temp. prevent check for changes.
		QMessageBox::critical(this, tr("Error opening File"), tr("Error opening %1").arg(name));
		info_->filenameSet = true;
		return false;
	}

#ifdef Q_OS_WIN
// Copied from linux libc sys/stat.h:
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif

	if (S_ISDIR(statbuf.st_mode)) {
		info_->filenameSet = false; // Temp. prevent check for changes.
		QMessageBox::critical(this, tr("Error opening File"), tr("Can't open directory %1").arg(name));
		info_->filenameSet = true;
		return false;
	}

#ifdef S_ISBLK
	if (S_ISBLK(statbuf.st_mode)) {
		info_->filenameSet = false; // Temp. prevent check for changes.
		QMessageBox::critical(this, tr("Error opening File"), tr("Can't open block device %1").arg(name));
		info_->filenameSet = true;
		return false;
	}
#endif

	if (statbuf.st_size > (0x100000000ll)) {
		info_->filenameSet = false; // Temp. prevent check for changes.
		QMessageBox::critical(this, tr("Error opening File"), tr("File size too large %1").arg(name));
		info_->filenameSet = true;
		return false;
	}

	// Allocate space for the whole contents of the file (unfortunately)
	try {
		QFile file;
		// TODO(eteran): error checking on this open?
		file.open(fp, QIODevice::ReadOnly);

		std::string text;

		if (file.size() != 0) {
			uchar *memory = file.map(0, file.size());
			if (!memory) {
				info_->filenameSet = false; // Temp. prevent check for changes.
				QMessageBox::critical(this, tr("Error while opening File"), tr("Error reading %1\n%2").arg(name, file.errorString()));
				info_->filenameSet = true;
				return false;
			}

			text = std::string{reinterpret_cast<char *>(memory), static_cast<size_t>(file.size())};
			file.unmap(memory);
		}

		/* Any errors that happen after this point leave the window in a
		 * "broken" state, and thus RevertToSaved will abandon the window if
		 * info_->fileMissing is false and doOpen fails. */
		info_->statbuf.st_mode  = statbuf.st_mode;
		info_->statbuf.st_uid   = statbuf.st_uid;
		info_->statbuf.st_gid   = statbuf.st_gid;
		info_->statbuf.st_mtime = statbuf.st_mtime;
		info_->statbuf.st_dev   = statbuf.st_dev;
		info_->statbuf.st_ino   = statbuf.st_ino;
		info_->fileMissing      = false;

		// Detect and convert DOS and Macintosh format files
		if (Preferences::GetPrefForceOSConversion()) {
			info_->fileFormat = FormatOfFile(text);
			switch (info_->fileFormat) {
			case FileFormats::Dos:
				ConvertFromDos(text);
				break;
			case FileFormats::Mac:
				ConvertFromMac(text);
				break;
			case FileFormats::Unix:
				break;
			}
		}

		// Display the file contents in the text widget
		info_->ignoreModify = true;
		info_->buffer->BufSetAll(text);
		info_->ignoreModify = false;

		// Set window title and file changed flag
		if ((flags & EditFlags::PREF_READ_ONLY) != 0) {
			info_->lockReasons.setUserLocked(true);
		}

		if (info_->lockReasons.isPermLocked()) {
			info_->fileChanged = false;
			Q_EMIT updateWindowTitle(this);
		} else {
			setWindowModified(false);
			if (info_->lockReasons.isAnyLocked()) {
				Q_EMIT updateWindowTitle(this);
			}
		}

		Q_EMIT updateWindowReadOnly(this);
		return true;
	} catch (const std::bad_alloc &) {
		info_->filenameSet = false; // Temp. prevent check for changes.
		QMessageBox::critical(this, tr("Error while opening File"), tr("File is too large to edit"));
		info_->filenameSet = true;
		return false;
	}
}

/*
** refresh window state for this document
*/
void DocumentWidget::refreshWindowStates() {

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (modeMessageDisplayed()) {
		ui.labelFileAndSize->setText(modeMessage_);
	} else {
		Q_EMIT updateStatus(this, nullptr);
	}

	Q_EMIT updateWindowReadOnly(this);
	Q_EMIT updateWindowTitle(this);
	Q_EMIT fontChanged(this);

	// show/hide statsline as needed
	if (modeMessageDisplayed() && !ui.statusFrame->isVisible()) {
		// turn on statline to display mode message
		showStatsLine(true);
	} else if (showStats_ && !ui.statusFrame->isVisible()) {
		// turn on statsline since it is enabled
		showStatsLine(true);
	} else if (!showStats_ && !modeMessageDisplayed() && ui.statusFrame->isVisible()) {
		// turn off statsline since there's nothing to show
		showStatsLine(false);
	}

	// signal if macro/shell is running
	if (shellCmdData_ || macroCmdData_) {
		setCursor(Qt::WaitCursor);
	} else {
		setCursor(Qt::ArrowCursor);
	}

	refreshMenuBar();
	win->updateLineNumDisp();
}

/*
** Refresh the various settings/state of the shell window per the
** settings of the top document.
*/
void DocumentWidget::refreshMenuBar() {
	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	refreshMenuToggleStates();

	// Add/remove language specific menu items
	win->updateUserMenus(this);

	// refresh selection-sensitive menus
	updateSelectionSensitiveMenus(info_->wasSelected);
}

/*
** Refresh the menu entries per the settings of the
** top document.
*/
void DocumentWidget::refreshMenuToggleStates() {

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	// File menu
	win->ui.action_Print_Selection->setEnabled(info_->wasSelected);

	// Edit menu
	win->ui.action_Undo->setEnabled(!info_->undo.empty());
	win->ui.action_Redo->setEnabled(!info_->redo.empty());
	win->ui.action_Cut->setEnabled(info_->wasSelected);
	win->ui.action_Copy->setEnabled(info_->wasSelected);
	win->ui.action_Delete->setEnabled(info_->wasSelected);

	// Preferences menu
	no_signals(win->ui.action_Statistics_Line)->setChecked(showStats_);
	no_signals(win->ui.action_Incremental_Search_Line)->setChecked(win->showISearchLine_);
	no_signals(win->ui.action_Show_Line_Numbers)->setChecked(win->showLineNumbers_);
	no_signals(win->ui.action_Highlight_Syntax)->setChecked(highlightSyntax_);
	no_signals(win->ui.action_Apply_Backlighting)->setChecked(backlightChars_);
	no_signals(win->ui.action_Make_Backup_Copy)->setChecked(info_->saveOldVersion);
	no_signals(win->ui.action_Incremental_Backup)->setChecked(info_->autoSave);
	no_signals(win->ui.action_Overtype)->setChecked(info_->overstrike);
	no_signals(win->ui.action_Matching_Syntax)->setChecked(info_->matchSyntaxBased);
	no_signals(win->ui.action_Read_Only)->setChecked(info_->lockReasons.isUserLocked());

	win->ui.action_Indent_Smart->setEnabled(SmartIndent::smartIndentMacrosAvailable(Preferences::LanguageModeName(languageMode_)));
	win->ui.action_Highlight_Syntax->setEnabled(languageMode_ != PLAIN_LANGUAGE_MODE);

	setAutoIndent(info_->indentStyle);
	setAutoWrap(info_->wrapMode);
	setShowMatching(info_->showMatchingStyle);
	setLanguageMode(languageMode_, /*forceNewDefaults=*/false);

	// Windows Menu
	win->ui.action_Split_Pane->setEnabled(textPanesCount() < MaxPanes);
	win->ui.action_Close_Pane->setEnabled(textPanesCount() > 1);

	std::vector<MainWindow *> windows = MainWindow::allWindows(/*includeInvisible=*/true);
	win->ui.action_Move_Tab_To->setEnabled(windows.size() > 1);
}

/*
** Run the newline macro with information from the smart-indent callback
** structure passed by the widget
*/
void DocumentWidget::executeNewlineMacro(SmartIndentEvent *event) {

	if (const std::unique_ptr<SmartIndentData> &winData = info_->smartIndentData) {

		DataValue result;
		QString errMsg;

		/* Beware of recursion: the newline macro may insert a string which
		   triggers the newline macro to be called again and so on. Newline
		   macros shouldn't insert strings, but nedit must not crash either if
		   they do. */
		if (winData->inNewLineMacro) {
			return;
		}

		// Call newline macro with the position at which to add newline/indent
		std::array<DataValue, 1> args = {
			make_value(to_integer(event->pos))};

		++(winData->inNewLineMacro);

		std::shared_ptr<MacroContext> continuation;
		int stat = executeMacro(this, winData->newlineMacro.get(), args, &result, continuation, &errMsg);

		// Don't allow preemption or time limit.  Must get return value
		while (stat == MACRO_TIME_LIMIT) {
			stat = continueMacro(continuation, &result, &errMsg);
		}

		--(winData->inNewLineMacro);

		// Process errors in macro execution
		if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
			QMessageBox::critical(
				this,
				tr("Smart Indent"),
				tr("Error in smart indent macro:\n%1").arg(stat == MACRO_ERROR ? errMsg : tr("dialogs and shell commands not permitted")));
			endSmartIndent();
			return;
		}

		// Validate and return the result
		if (!is_integer(result) || to_integer(result) < -1 || to_integer(result) > 1000) {
			QMessageBox::critical(this, tr("Smart Indent"), tr("Smart indent macros must return integer indent distance"));
			endSmartIndent();
			return;
		}

		event->request = to_integer(result);
	}
}

/*
** Set showMatching state to one of None, Delimeter or Range.
** Update the menu to reflect the change of state.
*/
void DocumentWidget::setShowMatching(ShowMatchingStyle state) {

	emit_event("set_show_matching", to_string(state));

	info_->showMatchingStyle = state;
	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	switch (state) {
	case ShowMatchingStyle::None:
		no_signals(win->ui.action_Matching_Off)->setChecked(true);
		break;
	case ShowMatchingStyle::Delimiter:
		no_signals(win->ui.action_Matching_Delimiter)->setChecked(true);
		break;
	case ShowMatchingStyle::Range:
		no_signals(win->ui.action_Matching_Range)->setChecked(true);
		break;
	}
}

/*
** Run the modification macro with information from the smart-indent callback
** structure passed by the widget
*/
void DocumentWidget::executeModMacro(SmartIndentEvent *event) {

	if (const std::unique_ptr<SmartIndentData> &winData = info_->smartIndentData) {

		DataValue result;
		QString errMsg;

		/* Check for inappropriate calls and prevent re-entering if the macro
		   makes a buffer modification */
		if (!winData->modMacro || winData->inModMacro > 0) {
			return;
		}

		/* Call modification macro with the position of the modification,
		   and the character(s) inserted.  Don't allow
		   preemption or time limit.  Execution must not overlap or re-enter */
		std::array<DataValue, 2> args = {
			make_value(to_integer(event->pos)),
			make_value(event->charsTyped)};

		++(winData->inModMacro);

		std::shared_ptr<MacroContext> continuation;
		int stat = executeMacro(this, winData->modMacro.get(), args, &result, continuation, &errMsg);

		while (stat == MACRO_TIME_LIMIT) {
			stat = continueMacro(continuation, &result, &errMsg);
		}

		--(winData->inModMacro);

		// Process errors in macro execution
		if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
			QMessageBox::critical(
				this,
				tr("Smart Indent"),
				tr("Error in smart indent modification macro:\n%1").arg((stat == MACRO_ERROR) ? errMsg : tr("dialogs and shell commands not permitted")));

			endSmartIndent();
			return;
		}
	}
}

/**
 * @brief DocumentWidget::macroBannerTimeoutProc
 */
void DocumentWidget::macroBannerTimeoutProc() {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	macroCmdData_->bannerIsUp = true;

	// Extract accelerator text from menu PushButtons
	QString cCancel = win->ui.action_Cancel_Learn->shortcut().toString();

	// Create message
	if (cCancel.isEmpty()) {
		setModeMessage(tr("Macro Command in Progress"));
	} else {
		setModeMessage(tr("Macro Command in Progress -- Press %1 to Cancel").arg(cCancel));
	}
}

/**
 * @brief DocumentWidget::shellBannerTimeoutProc
 */
void DocumentWidget::shellBannerTimeoutProc() {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	shellCmdData_->bannerIsUp = true;

	// Extract accelerator text from menu PushButtons
	QString cCancel = win->ui.action_Cancel_Shell_Command->shortcut().toString();

	// Create message
	if (cCancel.isEmpty()) {
		setModeMessage(tr("Shell Command in Progress"));
	} else {
		setModeMessage(tr("Shell Command in Progress -- Press %1 to Cancel").arg(cCancel));
	}
}

void DocumentWidget::actionClose(CloseMode mode) {

	closeFileAndWindow(mode);
	MainWindow::checkCloseEnableState();
	MainWindow::updateWindowMenus();
}

bool DocumentWidget::includeFile(const QString &name) {

	if (checkReadOnly()) {
		return false;
	}

	// TODO(eteran): this code is *almost* identical to a block of code in doOpen, we should
	// abstract this out into a function and reuse it.
	QFile file(name);
	if (!file.open(QFile::ReadOnly)) {
		QMessageBox::critical(this, tr("Error opening File"), file.errorString());
		return false;
	}

	if (file.size() != 0) {
		uchar *memory = file.map(0, file.size());
		if (!memory) {
			QMessageBox::critical(this, tr("Error opening File"), file.errorString());
			return false;
		}

		auto text = std::string{reinterpret_cast<char *>(memory), static_cast<size_t>(file.size())};
		file.unmap(memory);

		// Detect and convert DOS and Macintosh format files
		switch (FormatOfFile(text)) {
		case FileFormats::Dos:
			ConvertFromDos(text);
			break;
		case FileFormats::Mac:
			ConvertFromMac(text);
			break;
		case FileFormats::Unix:
			//  Default is Unix, no conversion necessary.
			break;
		}

		/* insert the contents of the file in the selection or at the insert
		   position in the window if no selection exists */
		if (info_->buffer->primary.hasSelection()) {
			info_->buffer->BufReplaceSelected(text);
		} else {
			if (auto win = MainWindow::fromDocument(this)) {
				info_->buffer->BufInsert(win->lastFocus()->cursorPos(), text);
			}
		}
	}

	return true;
}

boost::optional<TextCursor> DocumentWidget::findMatchingChar(char toMatch, Style styleToMatch, TextCursor charPos, TextCursor startLimit, TextCursor endLimit) {

	Style style;
	bool matchSyntaxBased = info_->matchSyntaxBased;

	// If we don't match syntax based, fake a matching style.
	if (!matchSyntaxBased) {
		style = styleToMatch;
	}

	// Look up the matching character and match direction
	auto matchIt = std::find_if(std::begin(MatchingChars), std::end(MatchingChars), [toMatch](const CharMatchTable &entry) {
		return entry.ch == toMatch;
	});

	if (matchIt == std::end(MatchingChars)) {
		return boost::none;
	}

	const char matchChar      = matchIt->match;
	const Direction direction = matchIt->direction;

	// find it in the buffer

	int nestDepth = 1;

	switch (direction) {
	case Direction::Forward: {
		const TextCursor beginPos = charPos + 1;

		for (TextCursor pos = beginPos; pos < endLimit; ++pos) {
			const char ch = info_->buffer->BufGetCharacter(pos);
			if (ch == matchChar) {
				if (matchSyntaxBased) {
					style = getHighlightInfo(pos);
				}

				if (style == styleToMatch) {
					--nestDepth;
					if (nestDepth == 0) {
						return pos;
					}
				}
			} else if (ch == toMatch) {
				if (matchSyntaxBased) {
					style = getHighlightInfo(pos);
				}

				if (style == styleToMatch) {
					++nestDepth;
				}
			}
		}
		break;
	}
	case Direction::Backward:
		if (charPos != startLimit) {
			const TextCursor beginPos = charPos - 1;

			for (TextCursor pos = beginPos; pos >= startLimit; --pos) {
				const char ch = info_->buffer->BufGetCharacter(pos);
				if (ch == matchChar) {
					if (matchSyntaxBased) {
						style = getHighlightInfo(pos);
					}

					if (style == styleToMatch) {
						--nestDepth;
						if (nestDepth == 0) {
							return pos;
						}
					}
				} else if (ch == toMatch) {
					if (matchSyntaxBased) {
						style = getHighlightInfo(pos);
					}

					if (style == styleToMatch) {
						++nestDepth;
					}
				}
			}
		}
		break;
	}

	return boost::none;
}

void DocumentWidget::gotoMatchingCharacter(TextArea *area, bool select) {

	/* get the character to match and its position from the selection, or
	   the character before the insert point if nothing is selected.
	   Give up if too many characters are selected */
	TextRange range;
	if (!info_->buffer->GetSimpleSelection(&range)) {

		range.end = area->cursorPos();
		if (info_->overstrike) {
			range.end += 1;
		}

		if (range.end == 0) {
			QApplication::beep();
			return;
		}

		range.start = range.end - 1;
	}

	if ((range.end - range.start) != 1) {
		QApplication::beep();
		return;
	}

	// Search for it in the buffer
	boost::optional<TextCursor> matchPos = findMatchingChar(
		info_->buffer->BufGetCharacter(range.start),
		getHighlightInfo(range.start),
		range.start,
		info_->buffer->BufStartOfBuffer(),
		info_->buffer->BufEndOfBuffer());
	if (!matchPos) {
		QApplication::beep();
		return;
	}

	if (select) {
		const TextCursor startPos = (*matchPos > range.start) ? range.start : *matchPos;
		const TextCursor endPos   = (*matchPos > range.start) ? *matchPos : range.start;

		/* temporarily shut off autoShowInsertPos before setting the cursor
		   position so MakeSelectionVisible gets a chance to place the cursor
		   string at a pleasing position on the screen (otherwise, the cursor would
		   be automatically scrolled on screen and MakeSelectionVisible would do
		   nothing) */
		area->setAutoShowInsertPos(false);
		info_->buffer->BufSelect(startPos, endPos + 1);
		makeSelectionVisible(area);
		area->setAutoShowInsertPos(true);
	} else {

		/* temporarily shut off autoShowInsertPos before setting the cursor
		   position so MakeSelectionVisible gets a chance to place the cursor
		   string at a pleasing position on the screen (otherwise, the cursor would
		   be automatically scrolled on screen and MakeSelectionVisible would do
		   nothing) */
		area->setAutoShowInsertPos(false);
		area->TextSetCursorPos(*matchPos + 1);
		makeSelectionVisible(area);
		area->setAutoShowInsertPos(true);
	}
}

/*
** This code path is followed if the request came from either
** findDefinition or findDefCalltip.  This should probably be refactored.
*/
int DocumentWidget::findDefinitionHelperCommon(TextArea *area, const QString &value, Tags::SearchMode search_type) {

	// TODO(eteran): change status to a more type safe success/fail

	int status = 0;

	Tags::searchMode = search_type;

	// make sure that the whole value is ascii
	auto p = std::find_if(value.begin(), value.end(), [](QChar ch) {
		return !safe_ctype<::isascii>(ch.toLatin1());
	});

	if (p == value.end()) {

		// See if we can find the tip/tag
		status = findAllMatches(area, value);

		// If we didn't find a requested calltip, see if we can use a tag
		if (status == 0 && search_type == Tags::SearchMode::TIP && !Tags::TagsFileList.empty()) {
			Tags::searchMode = Tags::SearchMode::TIP_FROM_TAG;
			status           = findAllMatches(area, value);
		}

		if (status == 0) {
			// Didn't find any matches
			if (Tags::searchMode == Tags::SearchMode::TIP_FROM_TAG || Tags::searchMode == Tags::SearchMode::TIP) {
				Tags::tagsShowCalltip(area, tr("No match for \"%1\" in calltips or tags.").arg(Tags::tagName));
			} else {
				QMessageBox::warning(this, tr("Tags"), tr("\"%1\" not found in tags %2").arg(Tags::tagName, (Tags::TagsFileList.size() > 1) ? tr("files") : tr("file")));
			}
		}
	} else {
		qWarning("NEdit: Can't handle non 8-bit text");
		QApplication::beep();
	}

	return status;
}

/*
** Lookup the definition for the current primary selection the currently
** loaded tags file and bring up the file and line that the tags file
** indicates.
*/
void DocumentWidget::findDefinitionHelper(TextArea *area, const QString &arg, Tags::SearchMode search_type) {
	if (!arg.isNull()) {
		findDefinitionHelperCommon(area, arg, search_type);
	} else {
		Tags::searchMode = search_type;

		QString selected = getAnySelection(ErrorSound::Silent);
		if (selected.isEmpty()) {
			return;
		}

		findDefinitionHelperCommon(area, selected, search_type);
	}
}

/**
 * @brief DocumentWidget::findDefinitionCalltip
 * @param area
 * @param tipName
 */
void DocumentWidget::findDefinitionCalltip(TextArea *area, const QString &tipName) {

	// Reset calltip parameters to reasonable defaults
	Tags::globAnchored  = false;
	Tags::globPos       = -1;
	Tags::globHAlign    = TipHAlignMode::Left;
	Tags::globVAlign    = TipVAlignMode::Below;
	Tags::globAlignMode = TipAlignMode::Sloppy;

	findDefinitionHelper(area, tipName, Tags::SearchMode::TIP);
}

/**
 * @brief DocumentWidget::findDefinition
 * @param area
 * @param tagName
 */
void DocumentWidget::findDefinition(TextArea *area, const QString &tagName) {
	findDefinitionHelper(area, tagName, Tags::SearchMode::TAG);
}

/**
 * @brief DocumentWidget::execAP
 * @param area
 * @param command
 */
void DocumentWidget::execAP(TextArea *area, const QString &command) {

	if (checkReadOnly()) {
		return;
	}

	executeShellCommand(area, command, CommandSource::User);
}

/*
** Execute shell command "command", depositing the result at the current
** insert position or in the current selection if the window has a
** selection.
*/
void DocumentWidget::executeShellCommand(TextArea *area, const QString &command, CommandSource source) {
	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	int flags = 0;

	// Can't do two shell commands at once in the same document
	if (shellCmdData_) {
		QApplication::beep();
		return;
	}

	// get the selection or the insert position
	const TextCursor pos = area->cursorPos();

	TextRange range;
	if (info_->buffer->GetSimpleSelection(&range)) {
		flags = ACCUMULATE | REPLACE_SELECTION;
	} else {
		range.start = pos;
		range.end   = pos;
	}

	/* Substitute the current file name for % and the current line number
	   for # in the shell command */
	QString fullName = fullPath();

	const boost::optional<Location> loc = area->positionToLineAndCol(pos);

	QString substitutedCommand = escapeCommand(command, fullName, loc->line);

	if (substitutedCommand.isNull()) {
		QMessageBox::critical(this, tr("Shell Command"), tr("Shell command is too long due to\n"
															"filename substitutions with '%%' or\n"
															"line number substitutions with '#'"));
		return;
	}

	issueCommand(
		win,
		area,
		substitutedCommand,
		QString(),
		flags,
		range.start,
		range.end,
		source);
}

/**
 * @brief DocumentWidget::printWindow
 * @param area
 * @param selectedOnly
 */
void DocumentWidget::printWindow(TextArea *area, bool selectedOnly) {

	std::string fileString;

	/* get the contents of the text buffer from the text area widget.  Add
	   wrapping newlines if necessary to make it match the displayed text */
	if (selectedOnly) {

		const TextBuffer::Selection *sel = &info_->buffer->primary;

		if (!sel->hasSelection()) {
			QApplication::beep();
			return;
		}

		if (sel->isRectangular()) {
			fileString = info_->buffer->BufGetSelectionText();
		} else {
			fileString = area->TextGetWrapped(sel->start(), sel->end());
		}
	} else {
		fileString = area->TextGetWrapped(info_->buffer->BufStartOfBuffer(), info_->buffer->BufEndOfBuffer());
	}

	// add a terminating newline if the file doesn't already have one
	if (!fileString.empty() && fileString.back() != '\n') {
		fileString.push_back('\n');
	}

	// Print the string
	printString(fileString, info_->filename);
}

/**
 * Print a string
 *
 * @brief DocumentWidget::printString
 * @param string
 * @param jobname
 */
void DocumentWidget::printString(const std::string &string, const QString &jobname) {

	auto dialog = std::make_unique<DialogPrint>(
		QString::fromStdString(string),
		jobname,
		this,
		this);

	dialog->exec();
}

/**
 * @brief DocumentWidget::splitPane
 */
void DocumentWidget::splitPane() {

	if (splitter_->count() >= MaxPanes) {
		return;
	}

	auto area = createTextArea(info_->buffer);

	if (auto activeArea = qobject_cast<TextArea *>(splitter_->widget(0))) {
		area->setLineNumCols(activeArea->getLineNumCols());
		area->setBacklightCharTypes(backlightCharTypes_);
		area->setFont(font_);

		const int scrollPosition = activeArea->verticalScrollBar()->value();

		// NOTE(eteran): annoyingly, on windows the viewrect isn't resized until it is shown
		// so if we try to do this now, it will try to scroll to a spot outside of
		// then "lineStarts_" array. The simplest thing to do to make it work correctly
		// is to schedule the scrolling for after the event is fully processed
		// FIXES https://github.com/eteran/nedit-ng/issues/239
		QTimer::singleShot(0, [area, scrollPosition]() {
			area->verticalScrollBar()->setValue(scrollPosition);
		});
	}

	attachHighlightToWidget(area);

	splitter_->addWidget(area);
	area->setFocus();
}

/*
** Close the window pane that last had the keyboard focus.
*/
void DocumentWidget::closePane() {

	if (splitter_->count() <= 1) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	TextArea *lastFocus = win->lastFocus();
	Q_ASSERT(lastFocus);

	for (int i = 0; i < splitter_->count(); ++i) {
		if (auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
			if (area == lastFocus) {
				area->deleteLater();
				return;
			}
		}
	}

	// if we got here, that means that the last focus isn't even in this
	// document, so we'll just nominate the last one
	std::vector<TextArea *> panes = textPanes();

	Q_ASSERT(!panes.empty());
	delete panes.back();
}

/*
** Turn on smart-indent (well almost).  Unfortunately, this doesn't do
** everything.  It requires that the smart indent callback (SmartIndentCB)
** is already attached to all of the text widgets in the window, and that the
** smartIndent resource must be turned on in the widget.  These are done
** separately, because they are required per-text widget, and therefore must
** be repeated whenever a new text widget is created within this window
** (a split-window command).
*/
void DocumentWidget::beginSmartIndent(Verbosity verbosity) {

	static bool initialized = false;

	// Find the window's language mode.  If none is set, warn the user
	QString modeName = Preferences::LanguageModeName(languageMode_);
	if (modeName.isNull()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(
				this,
				tr("Smart Indent"),
				tr("No language-specific mode has been set for this file.\n\nTo use smart indent in this window, please select a language from the Preferences -> Language Modes menu."));
		}
		return;
	}

	// Look up the appropriate smart-indent macros for the language
	const SmartIndentEntry *indentMacros = SmartIndent::findIndentSpec(modeName);
	if (!indentMacros) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(
				this,
				tr("Smart Indent"),
				tr("Smart indent is not available in languagemode\n%1.\n\nYou can create new smart indent macros in the\nPreferences -> Default Settings -> Smart Indent\ndialog, or choose a different language mode from:\nPreferences -> Language Mode.").arg(modeName));
		}
		return;
	}

	/* Make sure that the initial macro file is loaded before we execute
	   any of the smart-indent macros. Smart-indent macros may reference
	   routines defined in that file. */
	readMacroInitFile();

	/* Compile and run the common and language-specific initialization macros
	   (Note that when these return, the immediate commands in the file have not
	   necessarily been executed yet.  They are only SCHEDULED for execution) */
	if (!initialized) {
		if (!readMacroString(SmartIndent::CommonMacros, tr("smart indent common initialization macros"))) {
			return;
		}

		initialized = true;
	}

	if (!indentMacros->initMacro.isNull()) {
		if (!readMacroString(indentMacros->initMacro, tr("smart indent initialization macro"))) {
			return;
		}
	}

	// Compile the newline and modify macros and attach them to the window
	int stoppedAt;
	QString errMsg;

	auto siData          = std::make_unique<SmartIndentData>();
	siData->newlineMacro = std::unique_ptr<Program>(compileMacro(indentMacros->newlineMacro, &errMsg, &stoppedAt));

	if (!siData->newlineMacro) {
		Preferences::reportError(this, indentMacros->newlineMacro, stoppedAt, tr("newline macro"), errMsg);
		return;
	}

	if (indentMacros->modMacro.isNull()) {
		siData->modMacro = nullptr;
	} else {
		siData->modMacro = std::unique_ptr<Program>(compileMacro(indentMacros->modMacro, &errMsg, &stoppedAt));
		if (!siData->modMacro) {

			siData->newlineMacro = nullptr;
			Preferences::reportError(this, indentMacros->modMacro, stoppedAt, tr("smart indent modify macro"), errMsg);
			return;
		}
	}

	info_->smartIndentData = std::move(siData);
}

/**
 * @brief DocumentWidget::updateSignals
 * @param from
 * @param to
 */
void DocumentWidget::updateSignals(MainWindow *from, MainWindow *to) {

	disconnect(from);

	connect(this, &DocumentWidget::canUndoChanged, to, &MainWindow::undoAvailable);
	connect(this, &DocumentWidget::canRedoChanged, to, &MainWindow::redoAvailable);
	connect(this, &DocumentWidget::updateStatus, to, &MainWindow::updateStatus);
	connect(this, &DocumentWidget::updateWindowReadOnly, to, &MainWindow::updateWindowReadOnly);
	connect(this, &DocumentWidget::updateWindowTitle, to, &MainWindow::updateWindowTitle);
	connect(this, &DocumentWidget::fontChanged, to, &MainWindow::updateWindowHints);
	connect(this, &DocumentWidget::contextMenuRequested, to, &MainWindow::handleContextMenuEvent);
}

/*
** present dialog for selecting a target window to move this document
** into. Do nothing if there is only one shell window opened.
*/
void DocumentWidget::moveDocument(MainWindow *fromWindow) {

	// all windows
	std::vector<MainWindow *> allWindows = MainWindow::allWindows();

	// except for the source window
	allWindows.erase(std::remove(allWindows.begin(), allWindows.end(), fromWindow), allWindows.end());

	// stop here if there's no other window to move to
	if (allWindows.empty()) {
		return;
	}

	auto dialog = std::make_unique<DialogMoveDocument>(this);

	// load them into the dialog
	for (MainWindow *win : allWindows) {
		dialog->addItem(win);
	}

	// reset the dialog and display it
	dialog->resetSelection();
	dialog->setLabel(info_->filename);
	dialog->setMultipleDocuments(fromWindow->tabCount() > 1);
	int r = dialog->exec();

	if (r == QDialog::Accepted) {

		int selection = dialog->selectionIndex();

		// get the this to move document into
		MainWindow *targetWindow = allWindows[static_cast<size_t>(selection)];

		// move top document
		if (dialog->moveAllSelected()) {
			// move all documents
			for (DocumentWidget *document : fromWindow->openDocuments()) {

				targetWindow->tabWidget()->addTab(document, document->filename());

				document->updateSignals(fromWindow, targetWindow);

				raiseFocusDocumentWindow(true);
				targetWindow->show();
			}
		} else {
			targetWindow->tabWidget()->addTab(this, info_->filename);

			updateSignals(fromWindow, targetWindow);

			raiseFocusDocumentWindow(true);
			targetWindow->show();
		}

		// if we just emptied the window, then delete it
		if (fromWindow->tabCount() == 0) {
			fromWindow->deleteLater();
		}

		refreshTabState();

		if (Preferences::GetPrefSortTabs()) {
			targetWindow->sortTabBar();
		}
	}
}

/*
** Turn on and off the display of the statistics line
*/
void DocumentWidget::showStatsLine(bool state) {

	/* In continuous wrap mode, text widgets must be told to keep track of
	   the top line number in absolute (non-wrapped) lines, because it can
	   be a costly calculation, and is only needed for displaying line
	   numbers, either in the stats line, or along the left margin */
	for (TextArea *area : textPanes()) {
		area->TextDMaintainAbsLineNum(state);
	}

	showStats_ = state;
	ui.statusFrame->setVisible(state);
}

/**
 * @brief DocumentWidget::setWrapMargin
 * @param margin
 */
void DocumentWidget::setWrapMargin(int margin) {
	for (TextArea *area : textPanes()) {
		area->setWrapMargin(margin);
	}
}

/*
** Set the fonts for "window" from a font name, and updates the display.
*/
void DocumentWidget::action_Set_Fonts(const QString &fontName) {

	emit_event("set_fonts", fontName);

	// Check which fonts have changed
	if (fontName == fontName_) {
		return;
	}

	fontName_ = fontName;
	font_     = Font::fromString(fontName);

	// Change the primary font in all the widgets
	for (TextArea *area : textPanes()) {
		area->setFont(font_);
	}

	Q_EMIT fontChanged(this);
}

/*
** Set the backlight character class string
*/
void DocumentWidget::setBacklightChars(const QString &applyBacklightTypes) {

	backlightChars_     = !applyBacklightTypes.isNull();
	backlightCharTypes_ = applyBacklightTypes;

	for (TextArea *area : textPanes()) {
		area->setBacklightCharTypes(backlightCharTypes_);
	}
}

/**
 * @brief DocumentWidget::backlightCharTypes
 * @return
 */
QString DocumentWidget::backlightCharTypes() const {
	return backlightCharTypes_;
}

/**
 * @brief DocumentWidget::backlightChars
 * @return
 */
bool DocumentWidget::backlightChars() const {
	return backlightChars_;
}

/**
 * @brief DocumentWidget::setShowStatisticsLine
 * @param value
 */
void DocumentWidget::setShowStatisticsLine(bool value) {

	emit_event("set_statistics_line", value ? QLatin1String("1") : QLatin1String("0"));

	// stats line is a shell-level item, so we toggle the button state
	// regardless of it's 'topness'
	if (auto win = MainWindow::fromDocument(this)) {
		no_signals(win->ui.action_Statistics_Line)->setChecked(value);
	}

	showStats_ = value;
}

/**
 * @brief DocumentWidget::showStatisticsLine
 * @return
 */
bool DocumentWidget::showStatisticsLine() const {
	return showStats_;
}

/**
 * @brief DocumentWidget::matchSyntaxBased
 * @return
 */
bool DocumentWidget::matchSyntaxBased() const {
	return info_->matchSyntaxBased;
}

/**
 * @brief DocumentWidget::setMatchSyntaxBased
 * @param value
 */
void DocumentWidget::setMatchSyntaxBased(bool value) {

	emit_event("set_match_syntax_based", value ? QLatin1String("1") : QLatin1String("0"));

	if (isTopDocument()) {
		if (auto win = MainWindow::fromDocument(this)) {
			no_signals(win->ui.action_Matching_Syntax)->setChecked(value);
		}
	}

	info_->matchSyntaxBased = value;
}

/**
 * @brief DocumentWidget::overstrike
 * @return
 */
bool DocumentWidget::overstrike() const {
	return info_->overstrike;
}

/*
** Set insert/overstrike mode
*/
void DocumentWidget::setOverstrike(bool overstrike) {

	emit_event("set_overtype_mode", overstrike ? QLatin1String("1") : QLatin1String("0"));

	if (isTopDocument()) {
		if (auto win = MainWindow::fromDocument(this)) {
			no_signals(win->ui.action_Overtype)->setChecked(overstrike);
		}
	}

	for (TextArea *area : textPanes()) {
		area->setOverstrike(overstrike);
	}

	info_->overstrike = overstrike;
}

/**
 * @brief DocumentWidget::gotoAP
 * @param area
 * @param lineNum
 * @param column
 */
void DocumentWidget::gotoAP(TextArea *area, int line, int column) {

	TextCursor position;

	// User specified column, but not line number
	if (line == -1) {
		position = area->cursorPos();

		boost::optional<Location> loc = area->positionToLineAndCol(position);
		if (!loc) {
			return;
		}

		line = loc->line;
	} else if (column == -1) {
		// User didn't specify a column
		selectNumberedLine(area, line);
		return;
	}

	position = area->lineAndColToPosition(line, column);
	if (position == -1) {
		return;
	}

	area->TextSetCursorPos(position);
}

/**
 * @brief DocumentWidget::setColors
 * @param textFg
 * @param textBg
 * @param selectFg
 * @param selectBg
 * @param hiliteFg
 * @param hiliteBg
 * @param lineNoFg
 * @param lineNoBg
 * @param cursorFg
 */
void DocumentWidget::setColors(const QColor &textFg, const QColor &textBg, const QColor &selectFg, const QColor &selectBg, const QColor &hiliteFg, const QColor &hiliteBg, const QColor &lineNoFg, const QColor &lineNoBg, const QColor &cursorFg) {

	for (TextArea *area : textPanes()) {
		area->setColors(textFg,
						textBg,
						selectFg,
						selectBg,
						hiliteFg,
						hiliteBg,
						lineNoFg,
						lineNoBg,
						cursorFg);
	}
}

/**
 * @brief DocumentWidget::modeMessageDisplayed
 * @return
 */
bool DocumentWidget::modeMessageDisplayed() const {
	return !modeMessage_.isNull();
}

/*
** Display a special message in the stats line (show the stats line if it
** is not currently shown).
*/
void DocumentWidget::setModeMessage(const QString &message) {

	if (message.isNull()) {
		return;
	}

	/* this document may be hidden (not on top) or later made hidden,
	   so we save a copy of the mode message, so we can restore the
	   statsline when the document is raised to top again */
	modeMessage_ = message;

	ui.labelFileAndSize->setText(message);

	// Don't invoke the stats line again, if stats line is already displayed.
	if (!showStats_) {
		showStatsLine(true);
	}
}

/*
** Clear special statistics line message set in setModeMessage, returns
** the statistics line to its original state as set in showStats_
*/
void DocumentWidget::clearModeMessage() {

	if (!modeMessageDisplayed()) {
		return;
	}

	modeMessage_ = QString();

	/*
	 * Remove the stats line only if indicated by it's window state.
	 */
	if (!showStats_) {
		showStatsLine(false);
	}

	Q_EMIT updateStatus(this, nullptr);
}

// Decref the default calltips file(s) for this window
void DocumentWidget::unloadLanguageModeTipsFile() {

	const size_t mode = languageMode_;
	if (mode != PLAIN_LANGUAGE_MODE && !Preferences::LanguageModes[mode].defTipsFile.isNull()) {
		Tags::deleteTagsFile(Preferences::LanguageModes[mode].defTipsFile, Tags::SearchMode::TIP, false);
	}
}

/*
** Issue a shell command and feed it the string "input".  Output can be
** directed either to text widget "textW" where it replaces the text between
** the positions "replaceLeft" and "replaceRight", to a separate pop-up dialog
** (OUTPUT_TO_DIALOG), or to a macro-language string (OUTPUT_TO_STRING).  If
** "input" is nullptr, no input is fed to the process.  If an input string is
** provided, it is freed when the command completes.  Flags:
**
**   ACCUMULATE         Causes output from the command to be saved up until
**                      the command completes.
**   ERROR_DIALOGS      Presents stderr output separately in popup a dialog,
**                      and also reports failed exit status as a popup dialog
**                      including the command output.
**   REPLACE_SELECTION  Causes output to replace the selection in textW.
**   RELOAD_FILE_AFTER  Causes the file to be completely reloaded after the
**                      command completes.
**   OUTPUT_TO_DIALOG   Send output to a pop-up dialog instead of textW
**   OUTPUT_TO_STRING   Output to a macro-language string instead of a text
**                      widget or dialog.
**
** REPLACE_SELECTION, ERROR_DIALOGS, and OUTPUT_TO_STRING can only be used
** along with ACCUMULATE (these operations can't be done incrementally).
*/
void DocumentWidget::issueCommand(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, TextCursor replaceLeft, TextCursor replaceRight, CommandSource source) {

	// verify consistency of input parameters
	if ((flags & ERROR_DIALOGS || flags & REPLACE_SELECTION || flags & OUTPUT_TO_STRING) && !(flags & ACCUMULATE)) {
		return;
	}

	DocumentWidget *document = this;

	/* a shell command called from a macro must be executed in the same
	   window as the macro, regardless of where the output is directed,
	   so the user can cancel them as a unit */
	if (source == CommandSource::Macro) {
		document = MacroRunDocument();
	}

	if (source == CommandSource::User) {
		setCursor(Qt::WaitCursor);
	}

	if (source == CommandSource::User) {
		window->ui.action_Cancel_Shell_Command->setEnabled(true);
	}

	auto process = new QProcess(this);
	process->setWorkingDirectory(document->path());
	connect(process,
			static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
			document,
			&DocumentWidget::processFinished);

	// support for merged output if we are not using ERROR_DIALOGS
	if (flags & ERROR_DIALOGS) {
		connect(process, &QProcess::readyReadStandardError, this, [this]() {
			if (shellCmdData_) {
				QByteArray dataErr = shellCmdData_->process->readAllStandardError();
				shellCmdData_->standardError.append(dataErr);
			}
		});

		connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
			if (shellCmdData_) {
				QByteArray dataOut = shellCmdData_->process->readAllStandardOutput();
				shellCmdData_->standardOutput.append(dataOut);
			}
		});
	} else {
		process->setProcessChannelMode(QProcess::MergedChannels);

		connect(process, &QProcess::readyRead, this, [this]() {
			if (shellCmdData_) {
				QByteArray dataAll = shellCmdData_->process->readAll();
				shellCmdData_->standardOutput.append(dataAll);
			}
		});
	}

	// start it off!
	QStringList args;
	args << QLatin1String("-c");
	args << command;
	process->start(Preferences::GetPrefShell(), args);

	// if there's nothing to write to the process' stdin, close it now, otherwise
	// write it to the process
	if (!input.isEmpty()) {
		process->write(input.toLocal8Bit());
	}

	process->closeWriteChannel();

	/* Create a data structure for passing process information around
	   amongst the callback routines which will process i/o and completion */
	auto cmdData        = std::make_unique<ShellCommandData>();
	cmdData->process    = process;
	cmdData->flags      = flags;
	cmdData->area       = area;
	cmdData->bannerIsUp = false;
	cmdData->source     = source;
	cmdData->leftPos    = replaceLeft;
	cmdData->rightPos   = replaceRight;

	document->shellCmdData_ = std::move(cmdData);

	// Set up timer proc for putting up banner when process takes too long
	if (source == CommandSource::User) {
		connect(&document->shellCmdData_->bannerTimer, &QTimer::timeout, document, &DocumentWidget::shellBannerTimeoutProc);
		document->shellCmdData_->bannerTimer.setSingleShot(true);
		document->shellCmdData_->bannerTimer.start(BannerWaitTime);
	}

	/* If this was called from a macro, preempt the macro until shell
	   command completes */
	if (source == CommandSource::Macro) {
		preemptMacro();
	}
}

/*
** Clean up after the execution of a shell command sub-process and present
** the output/errors to the user as requested in the initial issueCommand
** call.  If "terminatedOnError" is true, don't bother trying to read the
** output, just close the i/o descriptors, free the memory, and restore the
** user interface state.
*/
void DocumentWidget::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	if (!shellCmdData_) {
		return;
	}

	const bool fromMacro = (shellCmdData_->source == CommandSource::Macro);

	// Cancel pending timeouts
	shellCmdData_->bannerTimer.stop();

	// Clean up waiting-for-shell-command-to-complete mode
	if (shellCmdData_->source == CommandSource::User) {
		setCursor(Qt::ArrowCursor);
		win->ui.action_Cancel_Shell_Command->setEnabled(false);
		if (shellCmdData_->bannerIsUp) {
			clearModeMessage();
		}
	}

	// when this function ends, do some cleanup
	auto _ = gsl::finally([this, fromMacro] {
		delete shellCmdData_->process;
		shellCmdData_ = nullptr;

		if (fromMacro) {
			resumeMacroExecution();
		}
	});

	QString errText;
	QString outText;

	// If the process was killed or became inaccessable, give up
	if (exitStatus != QProcess::NormalExit) {
		return;
	}

	// if we have terminated the process, let's be 100% sure we've gotten all input
	shellCmdData_->process->waitForReadyRead();

	/* Assemble the output from the process' stderr and stdout streams into
	   strings */
	if (shellCmdData_->flags & ERROR_DIALOGS) {
		// make sure we got the rest and convert it to a string
		QByteArray dataErr = shellCmdData_->process->readAllStandardError();
		shellCmdData_->standardError.append(dataErr);
		errText = QString::fromLocal8Bit(shellCmdData_->standardError);

		QByteArray dataOut = shellCmdData_->process->readAllStandardOutput();
		shellCmdData_->standardOutput.append(dataOut);
		outText = QString::fromLocal8Bit(shellCmdData_->standardOutput);
	} else {

		QByteArray dataAll = shellCmdData_->process->readAll();
		shellCmdData_->standardOutput.append(dataAll);
		outText = QString::fromLocal8Bit(shellCmdData_->standardOutput);
	}

	static const QRegularExpression trailingNewlines(QLatin1String("\\n+$"));

	/* Present error and stderr-information dialogs.  If a command returned
	   error output, or if the process' exit status indicated failure,
	   present the information to the user. */
	if (shellCmdData_->flags & ERROR_DIALOGS) {
		bool cancel = false;
		// NOTE(eteran): assumes UNIX return code style!
		bool failure     = exitCode != 0;
		bool errorReport = !errText.isEmpty();

		static constexpr int MaxMessageLength = 4096;

		if (failure && errorReport) {
			errText.remove(trailingNewlines);
			errText.truncate(MaxMessageLength);

			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("Warning"));
			msgBox.setText(errText);
			msgBox.setIcon(QMessageBox::Warning);
			msgBox.setStandardButtons(QMessageBox::Cancel);
			auto accept = new QPushButton(tr("Proceed"));
			msgBox.addButton(accept, QMessageBox::AcceptRole);
			msgBox.setDefaultButton(accept);
			cancel = (msgBox.exec() == QMessageBox::Cancel);

		} else if (failure) {
			outText.truncate(MaxMessageLength);

			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("Command Failure"));
			msgBox.setText(tr("Command reported failed exit status.\nOutput from command:\n%1").arg(outText));
			msgBox.setIcon(QMessageBox::Warning);
			msgBox.setStandardButtons(QMessageBox::Cancel);
			auto accept = new QPushButton(tr("Proceed"));
			msgBox.addButton(accept, QMessageBox::AcceptRole);
			msgBox.setDefaultButton(accept);
			cancel = (msgBox.exec() == QMessageBox::Cancel);

		} else if (errorReport) {

			errText.remove(trailingNewlines);
			errText.truncate(MaxMessageLength);

			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("Information"));
			msgBox.setText(errText);
			msgBox.setIcon(QMessageBox::Information);
			msgBox.setStandardButtons(QMessageBox::Cancel);
			auto accept = new QPushButton(tr("Proceed"));
			msgBox.addButton(accept, QMessageBox::AcceptRole);
			msgBox.setDefaultButton(accept);
			cancel = (msgBox.exec() == QMessageBox::Cancel);
		}

		if (cancel) {
			return;
		}
	}

	{
		/* If output is to a dialog, present the dialog.  Otherwise insert the
		   (remaining) output in the text widget as requested, and move the
		   insert point to the end */
		if (shellCmdData_->flags & OUTPUT_TO_DIALOG) {
			outText.remove(trailingNewlines);

			if (!outText.isEmpty()) {
				auto dialog = new DialogOutput(this);
				dialog->setText(outText);
				dialog->show();
			}
		} else if (shellCmdData_->flags & OUTPUT_TO_STRING) {
			returnShellCommandOutput(this, outText, exitCode);
		} else {

			std::string output_string = outText.toStdString();

			auto area       = shellCmdData_->area;
			TextBuffer *buf = area->buffer();

			if (shellCmdData_->flags & REPLACE_SELECTION) {
				TextCursor reselectStart = buf->primary.isRectangular() ? TextCursor(-1) : buf->primary.start();
				buf->BufReplaceSelected(output_string);

				area->TextSetCursorPos(buf->BufCursorPosHint());

				if (reselectStart != -1) {
					buf->BufSelect(reselectStart, reselectStart + static_cast<int64_t>(output_string.size()));
				}
			} else {
				safeBufReplace(buf, &shellCmdData_->leftPos, &shellCmdData_->rightPos, output_string);
				area->TextSetCursorPos(shellCmdData_->leftPos + outText.size());
			}
		}

		// If the command requires the file to be reloaded afterward, reload it
		if (shellCmdData_->flags & RELOAD_FILE_AFTER) {
			revertToSaved();
		}
	}
}

/*
** Cancel the shell command in progress
*/
void DocumentWidget::abortShellCommand() {
	if (shellCmdData_) {
		if (QProcess *process = shellCmdData_->process) {
			process->kill();
			shellCmdData_ = nullptr;
		}
	}
}

/*
** Execute the line of text where the the insertion cursor is positioned
** as a shell command.
*/
void DocumentWidget::execCursorLine(TextArea *area, CommandSource source) {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	TextCursor insertPos;

	// Can't do two shell commands at once in the same window
	if (shellCmdData_) {
		QApplication::beep();
		return;
	}

	// get all of the text on the line with the insert position
	TextCursor pos = area->cursorPos();

	TextRange range;
	if (!info_->buffer->GetSimpleSelection(&range)) {
		range.start = info_->buffer->BufStartOfLine(pos);
		range.end   = info_->buffer->BufEndOfLine(pos);
		insertPos   = range.end;
	} else {
		insertPos = info_->buffer->BufEndOfLine(range.end);
	}

	std::string cmdText = info_->buffer->BufGetRange(range);

	// insert a newline after the entire line
	info_->buffer->BufInsert(insertPos, '\n');

	/* Substitute the current file name for % and the current line number
	   for # in the shell command */
	const boost::optional<Location> loc = area->positionToLineAndCol(pos);

	auto substitutedCommand = escapeCommand(QString::fromStdString(cmdText), fullPath(), loc->line);

	if (substitutedCommand.isNull()) {
		QMessageBox::critical(
			this,
			tr("Shell Command"),
			tr("Shell command is too long due to\n"
			   "filename substitutions with '%%' or\n"
			   "line number substitutions with '#'"));
		return;
	}

	issueCommand(
		win,
		area,
		substitutedCommand,
		QString(),
		0,
		insertPos + 1,
		insertPos + 1,
		source);
}

/*
** Filter the current selection through shell command "command".  The selection
** is removed, and replaced by the output from the command execution.  Failed
** command status and output to stderr are presented in dialog form.
*/
void DocumentWidget::filterSelection(const QString &command, CommandSource source) {

	if (checkReadOnly()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	// Can't do two shell commands at once in the same window
	if (shellCmdData_) {
		QApplication::beep();
		return;
	}

	/* Get the selection and the range in character positions that it
	   occupies.  Beep and return if no selection */
	const std::string text = info_->buffer->BufGetSelectionText();
	if (text.empty()) {
		QApplication::beep();
		return;
	}

	const TextCursor left  = info_->buffer->primary.start();
	const TextCursor right = info_->buffer->primary.end();

	issueCommand(
		win,
		win->lastFocus(),
		command,
		QString::fromStdString(text),
		ACCUMULATE | ERROR_DIALOGS | REPLACE_SELECTION,
		left,
		right,
		source);
}

/*
** Do a shell command, with the options allowed to users (input source,
** output destination, save first and load after) in the shell commands
** menu.
*/
void DocumentWidget::doShellMenuCmd(MainWindow *inWindow, TextArea *area, const MenuItem &item, CommandSource source) {
	doShellMenuCmd(
		inWindow,
		area,
		item.cmd,
		item.input,
		item.output,
		item.repInput,
		item.saveFirst,
		item.loadAfter,
		source);
}

void DocumentWidget::doShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, CommandSource source) {

	int flags = 0;

	// Can't do two shell commands at once in the same window
	if (shellCmdData_) {
		QApplication::beep();
		return;
	}

	/* Substitute the current file name for % and the current line number
	   for # in the shell command */
	TextCursor pos = area->cursorPos();

	const boost::optional<Location> loc = area->positionToLineAndCol(pos);

	QString substitutedCommand = escapeCommand(command, fullPath(), loc ? loc->line : 0);

	/* Get the command input as a text string.  If there is input, errors
	  shouldn't be mixed in with output, so set flags to ERROR_DIALOGS */
	std::string text;
	switch (input) {
	case FROM_SELECTION:
		text = info_->buffer->BufGetSelectionText();
		if (text.empty()) {
			QApplication::beep();
			return;
		}
		flags |= ACCUMULATE | ERROR_DIALOGS;
		break;
	case FROM_WINDOW:
		text = info_->buffer->BufGetAll();
		flags |= ACCUMULATE | ERROR_DIALOGS;
		break;
	case FROM_EITHER:
		text = info_->buffer->BufGetSelectionText();
		if (text.empty()) {
			text = info_->buffer->BufGetAll();
		}
		flags |= ACCUMULATE | ERROR_DIALOGS;
		break;
	case FROM_NONE:
		text = std::string();
		break;
	}

	/* Assign the output destination.  If output is to a new window,
	   create it, and run the command from it instead of the current
	   one, to free the current one from waiting for lengthy execution */
	TextArea *outWidget = nullptr;
	TextRange range;
	switch (output) {
	case TO_DIALOG:
		flags |= OUTPUT_TO_DIALOG;
		range.start = TextCursor();
		range.end   = TextCursor();
		break;
	case TO_NEW_WINDOW:
		if (DocumentWidget *document = MainWindow::editNewFile(
				Preferences::GetPrefOpenInTab() ? inWindow : nullptr,
				QString(),
				false,
				QString(),
				QDir(info_->path))) {

			inWindow    = MainWindow::fromDocument(document);
			outWidget   = document->firstPane();
			range.start = TextCursor();
			range.end   = TextCursor();
			MainWindow::checkCloseEnableState();
		}
		break;
	case TO_SAME_WINDOW:
		outWidget = area;
		if (outputReplacesInput && input != FROM_NONE) {
			if (input == FROM_WINDOW) {
				range.start = TextCursor();
				range.end   = info_->buffer->BufEndOfBuffer();
			} else if (input == FROM_SELECTION) {
				info_->buffer->GetSimpleSelection(&range);
				flags |= ACCUMULATE | REPLACE_SELECTION;
			} else if (input == FROM_EITHER) {
				if (info_->buffer->GetSimpleSelection(&range)) {
					flags |= ACCUMULATE | REPLACE_SELECTION;
				} else {
					range.start = TextCursor();
					range.end   = info_->buffer->BufEndOfBuffer();
				}
			}
		} else {
			if (info_->buffer->GetSimpleSelection(&range)) {
				flags |= ACCUMULATE | REPLACE_SELECTION;
			} else {
				range.start = range.end = area->cursorPos();
			}
		}
		break;
	}

	// If the command requires the file be saved first, save it
	if (saveFirst) {
		if (!saveDocument()) {
			if (input != FROM_NONE) {
				return;
			}
		}
	}

	/* If the command requires the file to be reloaded after execution, set
	   a flag for issueCommand to deal with it when execution is complete */
	if (loadAfter) {
		flags |= RELOAD_FILE_AFTER;
	}

	// issue the command
	issueCommand(
		inWindow,
		outWidget,
		substitutedCommand,
		QString::fromStdString(text),
		flags,
		range.start,
		range.end,
		source);
}

/**
 * @brief DocumentWidget::widgetToPaneIndex
 * @param area
 * @return
 */
int DocumentWidget::widgetToPaneIndex(TextArea *area) const {
	return splitter_->indexOf(area);
}

/*
** Execute shell command "command", on input string "input", depositing the
** in a macro string (via a call back to ReturnShellCommandOutput).
*/
void DocumentWidget::shellCmdToMacroString(const QString &command, const QString &input) {

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	// fork the command and begin processing input/output
	issueCommand(
		win,
		nullptr,
		command,
		input,
		ACCUMULATE | OUTPUT_TO_STRING,
		TextCursor(),
		TextCursor(),
		CommandSource::Macro);
}

/*
** Set the auto-scroll margin
*/
void DocumentWidget::setAutoScroll(int margin) {

	for (TextArea *area : textPanes()) {
		area->setCursorVPadding(margin);
	}
}

/**
 * @brief DocumentWidget::repeatMacro
 * @param macro
 * @param how REPEAT_IN_SEL, REPEAT_TO_END or a non-negative integer value
 *
 * Dispatches a macro to which repeats macro command in "command", either
 * an integer number of times ("how" == positive integer), or within a
 * selected range ("how" == REPEAT_IN_SEL), or to the end of the window
 * ("how" == REPEAT_TO_END).
 *
 * Note that as with most macro routines, this returns BEFORE the macro is
 * finished executing
 */
void DocumentWidget::repeatMacro(const QString &macro, int how) {
	// Wrap a for loop and counter/tests around the command
	QString loopMacro = createRepeatMacro(how);
	QString loopedCmd;

	if (how == REPEAT_TO_END || how == REPEAT_IN_SEL) {
		loopedCmd = loopMacro.arg(macro);
	} else {
		loopedCmd = loopMacro.arg(how).arg(macro);
	}

	// Parse the resulting macro into an executable program "prog"
	QString errMsg;
	int stoppedAt;
	Program *const prog = compileMacro(loopedCmd, &errMsg, &stoppedAt);
	if (!prog) {
		qWarning("NEdit: internal error, repeat macro syntax wrong: %s", qPrintable(errMsg));
		return;
	}

	// run the executable program
	runMacro(prog);
}

/**
 * @brief DocumentWidget::allDocuments
 * @return
 */
std::vector<DocumentWidget *> DocumentWidget::allDocuments() {
	std::vector<MainWindow *> windows = MainWindow::allWindows();
	std::vector<DocumentWidget *> documents;

	for (MainWindow *window : windows) {

		std::vector<DocumentWidget *> openDocuments = window->openDocuments();

		documents.reserve(documents.size() + openDocuments.size());
		documents.insert(documents.end(), openDocuments.begin(), openDocuments.end());
	}

	return documents;
}

/**
 * @brief DocumentWidget::beginLearn
 */
void DocumentWidget::beginLearn() {

	// If we're already in learn mode, return
	if (CommandRecorder::instance()->isRecording()) {
		return;
	}

	MainWindow *thisWindow = MainWindow::fromDocument(this);
	if (!thisWindow) {
		return;
	}

	// dim the inappropriate menus and items, and undim finish and cancel
	for (MainWindow *window : MainWindow::allWindows()) {
		window->ui.action_Learn_Keystrokes->setEnabled(false);
	}

	thisWindow->ui.action_Finish_Learn->setEnabled(true);
	thisWindow->ui.action_Cancel_Learn->setText(tr("Cancel Learn"));
	thisWindow->ui.action_Cancel_Learn->setEnabled(true);

	// Add the action hook for recording the actions
	CommandRecorder::instance()->startRecording(this);

	// Extract accelerator texts from menu PushButtons
	QString cFinish = thisWindow->ui.action_Finish_Learn->shortcut().toString();
	QString cCancel = thisWindow->ui.action_Cancel_Learn->shortcut().toString();

	if (cFinish.isEmpty()) {
		if (cCancel.isEmpty()) {
			setModeMessage(tr("Learn Mode -- Use menu to finish or cancel"));
		} else {
			setModeMessage(tr("Learn Mode -- Use menu to finish, press %1 to cancel").arg(cCancel));
		}
	} else {
		if (cCancel.isEmpty()) {
			setModeMessage(tr("Learn Mode -- Press %1 to finish, use menu to cancel").arg(cFinish));
		} else {
			setModeMessage(tr("Learn Mode -- Press %1 to finish, %2 to cancel").arg(cFinish, cCancel));
		}
	}
}

/*
** Read an NEdit macro file.  Extends the syntax of the macro parser with
** define keyword, and allows intermixing of defines with immediate actions.
*/
bool DocumentWidget::readMacroFile(const QString &fileName, bool warnNotExist) {

	/* read-in macro file and force a terminating \n, to prevent syntax
	** errors with statements on the last line
	*/
	QString text = ReadAnyTextFile(fileName, /*forceNL=*/true);
	if (text.isNull()) {
		if (warnNotExist) {
			QMessageBox::critical(this,
								  tr("Read Macro"),
								  tr("Error reading macro file %1: %2").arg(fileName, errorString(errno)));
		}
		return false;
	}

	// Parse text
	return readCheckMacroString(this, text, this, fileName, nullptr);
}

/*
** Run a pre-compiled macro, changing the interface state to reflect that
** a macro is running, and handling preemption, resumption, and cancellation.
** frees prog when macro execution is complete;
*/
void DocumentWidget::runMacro(Program *prog) {

	/* If a macro is already running, just call the program as a subroutine,
	   instead of starting a new one, so we don't have to keep a separate
	   context, and the macros will serialize themselves automatically */
	if (macroCmdData_) {
		RunMacroAsSubrCall(prog);
		return;
	}

	// put up a watch cursor over the waiting window
	setCursor(Qt::BusyCursor);

	// enable the cancel menu item
	if (auto win = MainWindow::fromDocument(this)) {
		win->ui.action_Cancel_Learn->setText(tr("Cancel Macro"));
		win->ui.action_Cancel_Learn->setEnabled(true);
	}

	/* Create a data structure for passing macro execution information around
	   amongst the callback routines which will process i/o and completion */
	auto cmdData               = std::make_unique<MacroCommandData>();
	cmdData->bannerIsUp        = false;
	cmdData->closeOnCompletion = false;
	cmdData->program           = std::unique_ptr<Program>(prog);
	cmdData->context           = nullptr;

	macroCmdData_ = std::move(cmdData);

	// Set up timer proc for putting up banner when macro takes too long
	QObject::connect(&macroCmdData_->bannerTimer, &QTimer::timeout, this, &DocumentWidget::macroBannerTimeoutProc);
	macroCmdData_->bannerTimer.setSingleShot(true);
	macroCmdData_->bannerTimer.start(BannerWaitTime);

	// Begin macro execution
	DataValue result;
	QString errMsg;
	const int stat = executeMacro(this, prog, {}, &result, macroCmdData_->context, &errMsg);

	switch (stat) {
	case MACRO_ERROR:
		finishMacroCmdExecution();
		QMessageBox::critical(this, tr("Macro Error"), tr("Error executing macro: %1").arg(errMsg));
		break;
	case MACRO_DONE:
		finishMacroCmdExecution();
		break;
	case MACRO_TIME_LIMIT:
		resumeMacroExecution();
		break;
	case MACRO_PREEMPT:
		break;
	}
}

/*
** Clean up after the execution of a macro command: free memory, and restore
** the user interface state.
*/
void DocumentWidget::finishMacroCmdExecution() {

	const bool closeOnCompletion = macroCmdData_->closeOnCompletion;

	// Cancel pending timeout and work proc
	macroCmdData_->bannerTimer.stop();
	macroCmdData_->continuationTimer.stop();

	// Clean up waiting-for-macro-command-to-complete mode
	setCursor(Qt::ArrowCursor);

	// enable the cancel menu item
	if (auto win = MainWindow::fromDocument(this)) {
		win->ui.action_Cancel_Learn->setText(tr("Cancel Learn"));
		win->ui.action_Cancel_Learn->setEnabled(false);
	}

	if (macroCmdData_->bannerIsUp) {
		clearModeMessage();
	}

	// Free execution information
	macroCmdData_ = nullptr;

	/* If macro closed its own window, window was made empty and untitled,
	   but close was deferred until completion.  This is completion, so if
	   the window is still empty, do the close */
	if (closeOnCompletion && !info_->filenameSet && !info_->fileChanged) {
		closeDocument();
	}
}

/**
 * @brief DocumentWidget::continueWorkProcEx
 * @return
 */
DocumentWidget::MacroContinuationCode DocumentWidget::continueWorkProc() {

	// on the last loop, it may have been set to nullptr!
	if (!macroCmdData_) {
		return MacroContinuationCode::Stop;
	}

	QString errMsg;
	DataValue result;
	const ExecReturnCodes stat = continueMacro(macroCmdData_->context, &result, &errMsg);

	switch (stat) {
	case MACRO_ERROR:
		finishMacroCmdExecution();
		QMessageBox::critical(this, tr("Macro Error"), tr("Error executing macro: %1").arg(errMsg));
		return MacroContinuationCode::Stop;
	case MACRO_DONE:
		finishMacroCmdExecution();
		return MacroContinuationCode::Stop;
	case MACRO_PREEMPT:
		return MacroContinuationCode::Stop;
	case MACRO_TIME_LIMIT:
		// Macro exceeded time slice, re-schedule it
		return MacroContinuationCode::Continue;
	}

	return MacroContinuationCode::Stop;
}

/*
** Continue with macro execution after preemption.  Called by the routines
** whose actions cause preemption when they have completed their lengthy tasks.
** Re-establishes macro execution work proc.
*/
void DocumentWidget::resumeMacroExecution() {

	if (const std::shared_ptr<MacroCommandData> cmdData = macroCmdData_) {

		// create a background task that will run so long as the function returns false
		connect(&cmdData->continuationTimer, &QTimer::timeout, this, [cmdData, this]() {
			if (continueWorkProc() == MacroContinuationCode::Stop) {
				cmdData->continuationTimer.stop();
				cmdData->continuationTimer.disconnect();
			} else {
			}
		});

		// a timeout of 0 means "run whenever the event loop is idle"
		cmdData->continuationTimer.start(0);
	}
}

/*
** Check the character before the insertion cursor of textW and flash
** matching parenthesis, brackets, or braces, by temporarily highlighting
** the matching character (a timer procedure is scheduled for removing the
** highlights)
*/
void DocumentWidget::flashMatchingChar(TextArea *area) {

	// if a marker is already drawn, erase it and cancel the timeout
	if (flashTimer_->isActive()) {
		eraseFlash();
		flashTimer_->stop();
	}

	// no flashing required
	if (info_->showMatchingStyle == ShowMatchingStyle::None) {
		return;
	}

	// don't flash matching characters if there's a selection
	if (info_->buffer->primary.hasSelection()) {
		return;
	}

	// get the character to match and the position to start from
	const TextCursor currentPos = area->cursorPos();
	if (currentPos == 0) {
		return;
	}

	const TextCursor pos = currentPos - 1;

	const char ch = info_->buffer->BufGetCharacter(pos);

	Style style = getHighlightInfo(pos);

	// is the character one we want to flash?
	auto matchIt = std::find_if(std::begin(FlashingChars), std::end(FlashingChars), [ch](const CharMatchTable &entry) {
		return entry.ch == ch;
	});

	if (matchIt == std::end(FlashingChars)) {
		return;
	}

	/* constrain the search to visible text only when in single-pane mode
	   AND using delimiter flashing (otherwise search the whole buffer) */
	bool constrain = (textPanes().empty() && (info_->showMatchingStyle == ShowMatchingStyle::Delimiter));

	TextCursor startPos;
	TextCursor endPos;
	TextCursor searchPos;

	if (matchIt->direction == Direction::Backward) {
		startPos  = constrain ? area->firstVisiblePos() : info_->buffer->BufStartOfBuffer();
		endPos    = pos;
		searchPos = endPos;
	} else {
		startPos  = pos;
		endPos    = constrain ? area->TextLastVisiblePos() : info_->buffer->BufEndOfBuffer();
		searchPos = startPos;
	}

	// do the search
	boost::optional<TextCursor> matchPos = findMatchingChar(ch, style, searchPos, startPos, endPos);
	if (!matchPos) {
		return;
	}

	if (info_->showMatchingStyle == ShowMatchingStyle::Delimiter) {
		// Highlight either the matching character ...
		info_->buffer->BufHighlight(*matchPos, *matchPos + 1);
	} else {
		// ... or the whole range.
		if (matchIt->direction == Direction::Backward) {
			info_->buffer->BufHighlight(*matchPos, pos + 1);
		} else {
			info_->buffer->BufHighlight(*matchPos + 1, pos);
		}
	}

	flashTimer_->start();
}

/*
** Erase the marker drawn on a matching parenthesis bracket or brace
** character.
*/
void DocumentWidget::eraseFlash() {
	info_->buffer->BufUnhighlight();
}

/*
** Find the pattern set matching the window's current language mode, or
** tell the user if it can't be done (if warn is true) and return nullptr.
*/
PatternSet *DocumentWidget::findPatternsForWindow(Verbosity verbosity) {

	// Find the window's language mode.  If none is set, warn user
	QString modeName = Preferences::LanguageModeName(languageMode_);
	if (modeName.isNull()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this,
								 tr("Language Mode"),
								 tr("No language-specific mode has been set for this file.\n\n"
									"To use syntax highlighting in this window, please select a\n"
									"language from the Preferences -> Language Modes menu.\n\n"
									"New language modes and syntax highlighting patterns can be\n"
									"added via Preferences -> Default Settings -> Language Modes,\n"
									"and Preferences -> Default Settings -> Syntax Highlighting."));
		}
		return nullptr;
	}

	// Look up the appropriate pattern for the language
	PatternSet *const patterns = Highlight::FindPatternSet(modeName);
	if (!patterns) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this,
								 tr("Language Mode"),
								 tr("Syntax highlighting is not available in language\n"
									"mode %1.\n\n"
									"You can create new syntax highlight patterns in the\n"
									"Preferences -> Default Settings -> Syntax Highlighting\n"
									"dialog, or choose a different language mode from:\n"
									"Preferences -> Language Mode.")
									 .arg(modeName));
			return nullptr;
		}
	}

	return patterns;
}

/*
** Free highlighting data from a window destined for destruction, without
** redisplaying.
*/
void DocumentWidget::freeHighlightingData() {

	if (!highlightData_) {
		return;
	}

	highlightData_ = nullptr;

	/* The text display may make a last desperate attempt to access highlight
	   information when it is destroyed, which would be a disaster. */
	for (TextArea *area : textPanes()) {
		area->setStyleBuffer(nullptr);
	}
}

/*
** Change highlight fonts and/or styles in a highlighted window, without
** re-parsing.
*/
void DocumentWidget::updateHighlightStyles() {

	std::unique_ptr<WindowHighlightData> &oldHighlightData = highlightData_;

	// Do nothing if window not highlighted
	if (!oldHighlightData) {
		return;
	}

	// Find the pattern set for the window's current language mode
	PatternSet *patterns = findPatternsForWindow(Verbosity::Silent);
	if (!patterns) {
		stopHighlighting();
		return;
	}

	// Build new patterns
	std::unique_ptr<WindowHighlightData> newHighlightData = createHighlightData(patterns);
	if (!newHighlightData) {
		stopHighlighting();
		return;
	}

	/* Update highlight pattern data in the window data structure, but
	   preserve all of the effort that went in to parsing the buffer
	   by swapping it with the empty one in highlightData */
	newHighlightData->styleBuffer = std::move(oldHighlightData->styleBuffer);

	highlightData_ = std::move(newHighlightData);

	/* Attach new highlight information to text widgets in each pane
	   (and redraw) */
	for (TextArea *area : textPanes()) {
		attachHighlightToWidget(area);
	}
}

/*
** Find the HighlightPattern structure with a given name in the window.
*/
HighlightPattern *DocumentWidget::findPatternOfWindow(const QString &name) const {

	if (const std::unique_ptr<WindowHighlightData> &hData = highlightData_) {
		if (PatternSet *set = hData->patternSetForWindow) {

			for (HighlightPattern &pattern : set->patterns) {
				if (pattern.name == name) {
					return &pattern;
				}
			}
		}
	}
	return nullptr;
}

/**
 * @brief DocumentWidget::highlightBGColorOfCode
 * @param hCode
 * @return
 */
QColor DocumentWidget::highlightBGColorOfCode(size_t hCode) const {
	StyleTableEntry *entry = styleTableEntryOfCode(hCode);

	if (entry && !entry->bgColorName.isNull()) {
		return entry->bgColor;
	} else {
		// pick up background color of the (first) text widget of the window
		return firstPane()->getBackgroundColor();
	}
}

/*
** Returns the highlight style of the character at a given position of a
** window. To avoid breaking encapsulation, the highlight style is converted
** to a Style object (no other module has to know that characters are used
** to represent highlight styles; that would complicate future extensions).
** Returns Style() if the window has highlighting turned off.
** The only guarantee that this function offers, is that when the same
** value is returned for two positions, the corresponding characters have
** the same highlight style.
**/
Style DocumentWidget::getHighlightInfo(TextCursor pos) {

	HighlightData *pattern                                    = nullptr;
	const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_;
	if (!highlightData) {
		return Style();
	}

	// Be careful with signed/unsigned conversions. NO conversion here!
	int style = highlightData->styleBuffer->BufGetCharacter(pos);

	// Beware of unparsed regions.
	if (style == UNFINISHED_STYLE) {
		handleUnparsedRegion(highlightData->styleBuffer, pos);
		style = highlightData->styleBuffer->BufGetCharacter(pos);
	}

	if (highlightData->pass1Patterns) {
		pattern = Highlight::patternOfStyle(highlightData->pass1Patterns, style);
	}

	if (!pattern && highlightData->pass2Patterns) {
		pattern = Highlight::patternOfStyle(highlightData->pass2Patterns, style);
	}

	if (!pattern) {
		return Style();
	}

	return Style(reinterpret_cast<void *>(pattern->userStyleIndex));
}

/*
** Returns the length over which a particular style applies, starting at pos.
** If the initial code value *checkCode is zero, the highlight code of pos
** is used.
*/
int64_t DocumentWidget::styleLengthOfCodeFromPos(TextCursor pos) const {

	const TextCursor oldPos = pos;

	if (const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {
		if (const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer) {

			auto hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
			if (!hCode) {
				return 0;
			}

			if (hCode == UNFINISHED_STYLE) {
				// encountered "unfinished" style, trigger parsing
				handleUnparsedRegion(highlightData->styleBuffer, pos);
				hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
			}

			StyleTableEntry *entry = styleTableEntryOfCode(hCode);
			if (!entry) {
				return 0;
			}

			QString checkStyleName = entry->styleName;

			while (hCode == UNFINISHED_STYLE || ((entry = styleTableEntryOfCode(hCode)) && entry->styleName == checkStyleName)) {
				if (hCode == UNFINISHED_STYLE) {
					// encountered "unfinished" style, trigger parsing, then loop
					handleUnparsedRegion(highlightData->styleBuffer, pos);
					hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
				} else {
					// advance the position and get the new code
					hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(++pos));
				}
			}
		}
	}

	return pos - oldPos;
}

/*
** Functions to return style information from the highlighting style table.
*/
QString DocumentWidget::highlightNameOfCode(size_t hCode) const {
	if (StyleTableEntry *entry = styleTableEntryOfCode(hCode)) {
		return entry->highlightName;
	}

	return QString();
}

QString DocumentWidget::highlightStyleOfCode(size_t hCode) const {
	if (StyleTableEntry *entry = styleTableEntryOfCode(hCode)) {
		return entry->styleName;
	}

	return QString();
}

QColor DocumentWidget::highlightColorValueOfCode(size_t hCode) const {
	if (StyleTableEntry *entry = styleTableEntryOfCode(hCode)) {
		return entry->color;
	}

	// pick up foreground color of the (first) text widget of the window
	return firstPane()->getForegroundColor();
}

/*
** Returns a pointer to the entry in the style table for the entry of code
** hCode (if any).
*/
StyleTableEntry *DocumentWidget::styleTableEntryOfCode(size_t hCode) const {
	const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_;

	hCode -= UNFINISHED_STYLE; // get the correct index value
	if (!highlightData || hCode >= highlightData->styleTable.size()) {
		return nullptr;
	}

	return &highlightData->styleTable[hCode];
}

/*
** Picks up the entry in the style buffer for the position (if any).
** Returns the style code or zero.
*/
size_t DocumentWidget::highlightCodeOfPos(TextCursor pos) const {

	size_t hCode = 0;
	if (const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {

		if (const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer) {

			hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
			if (hCode == UNFINISHED_STYLE) {
				// encountered "unfinished" style, trigger parsing
				handleUnparsedRegion(highlightData->styleBuffer, pos);
				hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
			}
		}
	}

	return hCode;
}

/*
** Returns the length over which a particular highlight code applies, starting
** at pos.
*/
int64_t DocumentWidget::highlightLengthOfCodeFromPos(TextCursor pos) const {

	const TextCursor oldPos = pos;
	size_t checkCode        = 0;

	if (const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {

		if (const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer) {

			auto hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
			if (!hCode) {
				return 0;
			}

			if (hCode == UNFINISHED_STYLE) {
				// encountered "unfinished" style, trigger parsing
				handleUnparsedRegion(highlightData->styleBuffer, pos);
				hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
			}

			if (checkCode == 0) {
				checkCode = hCode;
			}

			while (hCode == checkCode || hCode == UNFINISHED_STYLE) {
				if (hCode == UNFINISHED_STYLE) {
					// encountered "unfinished" style, trigger parsing, then loop
					handleUnparsedRegion(highlightData->styleBuffer, pos);
					hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
				} else {
					// advance the position and get the new code
					hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(++pos));
				}
			}
		}
	}

	return pos - oldPos;
}

/*
** Callback to parse an "unfinished" region of the buffer.  "unfinished" means
** that the buffer has been parsed with pass 1 patterns, but this section has
** not yet been exposed, and thus never had pass 2 patterns applied.  This
** callback is invoked when the text widget's display routines encounter one
** of these unfinished regions.  "pos" is the first position encountered which
** needs re-parsing.  This routine applies pass 2 patterns to a chunk of
** the buffer of size PASS_2_REPARSE_CHUNK_SIZE beyond pos.
*/
void DocumentWidget::handleUnparsedRegion(TextBuffer *styleBuf, TextCursor pos) const {
	TextBuffer *buf                                           = info_->buffer.get();
	const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_;

	const ReparseContext &context                         = highlightData->contextRequirements;
	const std::unique_ptr<HighlightData[]> &pass2Patterns = highlightData->pass2Patterns;

	if (!pass2Patterns) {
		return;
	}

	int firstPass2Style = pass2Patterns[1].style;

	/* If there are no pass 2 patterns to process, do nothing (but this
	   should never be triggered) */

	/* Find the point at which to begin parsing to ensure that the character at
	   pos is parsed correctly (beginSafety), at most one context distance back
	   from pos, unless there is a pass 1 section from which to start */
	TextCursor beginParse  = pos;
	TextCursor beginSafety = Highlight::backwardOneContext(buf, context, beginParse);

	for (TextCursor p = beginParse; p >= beginSafety; --p) {
		char ch = styleBuf->BufGetCharacter(p);
		if (ch != UNFINISHED_STYLE && ch != PLAIN_STYLE && static_cast<uint8_t>(ch) < firstPass2Style) {
			beginSafety = p + 1;
			break;
		}
	}

	/* Decide where to stop (endParse), and the extra distance (endSafety)
	   necessary to ensure that the changes at endParse are correct.  Stop at
	   the end of the unfinished region, or a max. of PASS_2_REPARSE_CHUNK_SIZE
	   characters forward from the requested position */
	TextCursor endParse  = std::min(buf->BufEndOfBuffer(), pos + PASS_2_REPARSE_CHUNK_SIZE);
	TextCursor endSafety = Highlight::forwardOneContext(buf, context, endParse);

	for (TextCursor p = pos; p < endSafety; ++p) {
		char ch = styleBuf->BufGetCharacter(p);
		if (ch != UNFINISHED_STYLE && ch != PLAIN_STYLE && static_cast<uint8_t>(ch) < firstPass2Style) {
			endParse  = std::min(endParse, p);
			endSafety = p;
			break;
		} else if (ch != UNFINISHED_STYLE && p < endParse) {
			endParse = p;
			if (static_cast<uint8_t>(ch) < firstPass2Style) {
				endSafety = p;
			} else {
				endSafety = Highlight::forwardOneContext(buf, context, endParse);
			}
			break;
		}
	}

	// Copy the buffer range into a string
	std::string str    = buf->BufGetRange(beginSafety, endSafety);
	const char *string = &str[0];

	std::string styleStr    = styleBuf->BufGetRange(beginSafety, endSafety);
	char *const styleString = &styleStr[0];
	char *stylePtr          = &styleStr[0];

	// Parse it with pass 2 patterns
	int prev_char = Highlight::getPrevChar(buf, beginSafety);
	Highlight::ParseContext ctx;
	ctx.prev_char  = &prev_char;
	ctx.delimiters = documentDelimiters();
	ctx.text       = str;

	Highlight::parseString(
		&pass2Patterns[0],
		string,
		stylePtr,
		endParse - beginSafety,
		&ctx,
		nullptr,
		nullptr);

	/* Update the style buffer the new style information, but only between
	   beginParse and endParse.  Skip the safety region */
	auto view = view::string_view(&styleString[beginParse - beginSafety], static_cast<size_t>(endParse - beginParse));
	styleBuf->BufReplace(beginParse, endParse, view);
}

/**
 * @brief DocumentWidget::handleUnparsedRegion
 * @param styleBuf
 * @param pos
 */
void DocumentWidget::handleUnparsedRegion(const std::shared_ptr<TextBuffer> &styleBuf, TextCursor pos) const {
	handleUnparsedRegion(styleBuf.get(), pos);
}

/*
** Turn on syntax highlighting.  If "warn" is true, warn the user when it
** can't be done, otherwise, just return.
*/
void DocumentWidget::startHighlighting(Verbosity verbosity) {

	/* Find the pattern set matching the window's current
	   language mode, tell the user if it can't be done */
	PatternSet *patterns = findPatternsForWindow(verbosity);
	if (!patterns) {
		return;
	}

	// Compile the patterns
	std::unique_ptr<WindowHighlightData> highlightData = createHighlightData(patterns);
	if (!highlightData) {
		return;
	}

	// Prepare for a long delay, refresh display and put up a watch cursor
	const QCursor prevCursor = cursor();
	setCursor(Qt::WaitCursor);

	const int64_t bufLength = info_->buffer->length();

	/* Parse the buffer with pass 1 patterns.  If there are none, initialize
	   the style buffer to all UNFINISHED_STYLE to trigger parsing later */
	std::string style_buffer(static_cast<size_t>(bufLength), UNFINISHED_STYLE);
	if (highlightData->pass1Patterns) {
		char *stylePtr = &style_buffer[0];

		int prev_char = -1;
		Highlight::ParseContext ctx;
		ctx.prev_char         = &prev_char;
		ctx.delimiters        = documentDelimiters();
		ctx.text              = info_->buffer->BufAsString();
		const char *stringPtr = &ctx.text[0];

		Highlight::parseString(
			&highlightData->pass1Patterns[0],
			stringPtr,
			stylePtr,
			bufLength,
			&ctx,
			nullptr,
			nullptr);
	}

	highlightData->styleBuffer->BufSetAll(style_buffer);

	// install highlight pattern data in the window data structure
	highlightData_ = std::move(highlightData);

	// Attach highlight information to text widgets in each pane
	for (TextArea *area : textPanes()) {
		attachHighlightToWidget(area);
	}

	setCursor(prevCursor);
}

/*
** Attach style information from a window's highlight data to a
** text widget and redisplay.
*/
void DocumentWidget::attachHighlightToWidget(TextArea *area) {
	if (const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {
		area->attachHighlightData(
			highlightData->styleBuffer.get(),
			highlightData->styleTable,
			UNFINISHED_STYLE,
			handleUnparsedRegionCB,
			this);
	}
}

/*
** Create complete syntax highlighting information from "patternSrc", using
** highlighting fonts from "window", includes pattern compilation.  If errors
** are encountered, warns user with a dialog and returns nullptr.
*/
std::unique_ptr<WindowHighlightData> DocumentWidget::createHighlightData(PatternSet *patternSet, Verbosity verbosity) {

	std::vector<HighlightPattern> &patterns = patternSet->patterns;

	// The highlighting code can't handle empty pattern sets, quietly say no
	if (patterns.empty()) {
		return nullptr;
	}

	// Check that the styles and parent pattern names actually exist
	if (!Highlight::NamedStyleExists(QLatin1String("Plain"))) {
		QMessageBox::warning(this, tr("Highlight Style"), tr("Highlight style \"Plain\" is missing"));
		return nullptr;
	}

	for (const HighlightPattern &pattern : patterns) {
		if (!pattern.subPatternOf.isNull() && Highlight::indexOfNamedPattern(patterns, pattern.subPatternOf) == PATTERN_NOT_FOUND) {
			QMessageBox::warning(
				this,
				tr("Parent Pattern"),
				tr("Parent field \"%1\" in pattern \"%2\"\ndoes not match any highlight patterns in this set").arg(pattern.subPatternOf, pattern.name));
			return nullptr;
		}
	}

	for (const HighlightPattern &pattern : patterns) {
		if (!Highlight::NamedStyleExists(pattern.style)) {
			QMessageBox::warning(
				this,
				tr("Highlight Style"),
				tr("Style \"%1\" named in pattern \"%2\"\ndoes not match any existing style").arg(pattern.style, pattern.name));
			return nullptr;
		}
	}

	/* Make DEFER_PARSING flags agree with top level patterns (originally,
	   individual flags had to be correct and were checked here, but dialog now
	   shows this setting only on top patterns which is much less confusing) */
	{
		size_t i = 0;
		for (HighlightPattern &pattern : patterns) {

			if (!pattern.subPatternOf.isNull()) {
				const size_t parentindex = Highlight::findTopLevelParentIndex(patterns, i);
				if (parentindex == PATTERN_NOT_FOUND) {
					QMessageBox::warning(
						this,
						tr("Parent Pattern"),
						tr("Pattern \"%1\" does not have valid parent").arg(pattern.name));
					return nullptr;
				}

				if (patterns[parentindex].flags & DEFER_PARSING) {
					pattern.flags |= DEFER_PARSING;
				} else {
					pattern.flags &= ~DEFER_PARSING;
				}
			}

			++i;
		}
	}

	/* Sort patterns into those to be used in pass 1 parsing, and those to
	   be used in pass 2, and add default pattern (0) to each list */
	std::vector<HighlightPattern> pass1PatternSrc;
	std::vector<HighlightPattern> pass2PatternSrc;

	auto p1Ptr = std::back_inserter(pass1PatternSrc);
	auto p2Ptr = std::back_inserter(pass2PatternSrc);

	*p1Ptr++ = HighlightPattern(QLatin1String("Plain"));
	*p2Ptr++ = HighlightPattern(QLatin1String("Plain"));

	for (const HighlightPattern &pattern : patterns) {
		if (pattern.flags & DEFER_PARSING) {
			*p2Ptr++ = pattern;
		} else {
			*p1Ptr++ = pattern;
		}
	}

	/* If a particular pass is empty except for the default pattern, don't
	   bother compiling it or setting up styles */
	if (pass1PatternSrc.size() == 1) {
		pass1PatternSrc.clear();
	}

	if (pass2PatternSrc.size() == 1) {
		pass2PatternSrc.clear();
	}

	std::unique_ptr<HighlightData[]> pass1Pats;
	std::unique_ptr<HighlightData[]> pass2Pats;

	// Compile patterns
	if (!pass1PatternSrc.empty()) {
		pass1Pats = compilePatterns(pass1PatternSrc, verbosity);
		if (!pass1Pats) {
			return nullptr;
		}
	}

	if (!pass2PatternSrc.empty()) {
		pass2Pats = compilePatterns(pass2PatternSrc, verbosity);
		if (!pass2Pats) {
			return nullptr;
		}
	}

	/* Set pattern styles.  If there are pass 2 patterns, pass 1 pattern
	   0 should have a default style of UNFINISHED_STYLE.  With no pass 2
	   patterns, unstyled areas of pass 1 patterns should be PLAIN_STYLE
	   to avoid triggering re-parsing every time they are encountered */
	const bool zeroPass1 = (pass1PatternSrc.empty());
	const bool zeroPass2 = (pass2PatternSrc.empty());

	if (zeroPass2) {
		Q_ASSERT(pass1Pats);
		pass1Pats[0].style = PLAIN_STYLE;
	} else if (zeroPass1) {
		Q_ASSERT(pass2Pats);
		pass2Pats[0].style = PLAIN_STYLE;
	} else {
		Q_ASSERT(pass1Pats);
		Q_ASSERT(pass2Pats);
		pass1Pats[0].style = UNFINISHED_STYLE;
		pass2Pats[0].style = PLAIN_STYLE;
	}

	for (size_t i = 1; i < pass1PatternSrc.size(); i++) {
		pass1Pats[i].style = gsl::narrow<uint8_t>(PLAIN_STYLE + i);
	}

	for (size_t i = 1; i < pass2PatternSrc.size(); i++) {
		pass2Pats[i].style = gsl::narrow<uint8_t>(PLAIN_STYLE + (zeroPass1 ? 0 : pass1PatternSrc.size() - 1) + i);
	}

	// Create table for finding parent styles
	std::vector<uint8_t> parentStyles;
	parentStyles.reserve(pass1PatternSrc.size() + pass2PatternSrc.size() + 2);

	auto parentStylesPtr = std::back_inserter(parentStyles);

	*parentStylesPtr++ = '\0';
	*parentStylesPtr++ = '\0';

	for (size_t i = 1; i < pass1PatternSrc.size(); i++) {
		const HighlightPattern &pattern = pass1PatternSrc[i];

		if (pattern.subPatternOf.isNull()) {
			*parentStylesPtr++ = PLAIN_STYLE;
		} else {
			*parentStylesPtr++ = pass1Pats[Highlight::indexOfNamedPattern(pass1PatternSrc, pattern.subPatternOf)].style;
		}
	}

	for (size_t i = 1; i < pass2PatternSrc.size(); i++) {
		const HighlightPattern &pattern = pass2PatternSrc[i];

		if (pattern.subPatternOf.isNull()) {
			*parentStylesPtr++ = PLAIN_STYLE;
		} else {
			*parentStylesPtr++ = pass2Pats[Highlight::indexOfNamedPattern(pass2PatternSrc, pattern.subPatternOf)].style;
		}
	}

	// Set up table for mapping colors and fonts to syntax
	std::vector<StyleTableEntry> styleTable;
	styleTable.reserve(pass1PatternSrc.size() + pass2PatternSrc.size());

	auto it = std::back_inserter(styleTable);

	auto createStyleTableEntry = [](HighlightPattern *pat) {
		StyleTableEntry p;

		p.isUnderlined  = false;
		p.highlightName = pat->name;
		p.styleName     = pat->style;
		p.colorName     = Highlight::FgColorOfNamedStyle(pat->style);
		p.bgColorName   = Highlight::BgColorOfNamedStyle(pat->style);
		p.isBold        = Highlight::FontOfNamedStyleIsBold(pat->style);
		p.isItalic      = Highlight::FontOfNamedStyleIsItalic(pat->style);

		// And now for the more physical stuff
		p.color = X11Colors::fromString(p.colorName);

		if (!p.bgColorName.isNull()) {
			p.bgColor = X11Colors::fromString(p.bgColorName);
		} else {
			p.bgColor = p.color;
		}

		return p;
	};

	// PLAIN_STYLE (pass 1)
	it++ = createStyleTableEntry(zeroPass1 ? &pass2PatternSrc[0] : &pass1PatternSrc[0]);

	// PLAIN_STYLE (pass 2)
	it++ = createStyleTableEntry(zeroPass2 ? &pass1PatternSrc[0] : &pass2PatternSrc[0]);

	// explicit styles (pass 1)
	for (size_t i = 1; i < pass1PatternSrc.size(); i++) {
		it++ = createStyleTableEntry(&pass1PatternSrc[i]);
	}

	// explicit styles (pass 2)
	for (size_t i = 1; i < pass2PatternSrc.size(); i++) {
		it++ = createStyleTableEntry(&pass2PatternSrc[i]);
	}

	// Create the style buffer
	auto styleBuf = std::make_unique<TextBuffer>();
	styleBuf->BufSetSyncXSelection(false);

	const int contextLines = patternSet->lineContext;
	const int contextChars = patternSet->charContext;

	// Collect all of the highlighting information in a single structure
	auto highlightData                        = std::make_unique<WindowHighlightData>();
	highlightData->pass1Patterns              = std::move(pass1Pats);
	highlightData->pass2Patterns              = std::move(pass2Pats);
	highlightData->parentStyles               = std::move(parentStyles);
	highlightData->styleTable                 = std::move(styleTable);
	highlightData->styleBuffer                = std::move(styleBuf);
	highlightData->contextRequirements.nLines = contextLines;
	highlightData->contextRequirements.nChars = contextChars;
	highlightData->patternSetForWindow        = patternSet;

	return highlightData;
}

/*
** Transform pattern sources into the compiled highlight information
** actually used by the code.  Output is a tree of HighlightData structures
** containing compiled regular expressions and style information.
*/
std::unique_ptr<HighlightData[]> DocumentWidget::compilePatterns(const std::vector<HighlightPattern> &patternSrc, Verbosity verbosity) {

	/* Allocate memory for the compiled patterns.  The list is terminated
	   by a record with style == 0. */
	auto compiledPats = std::make_unique<HighlightData[]>(patternSrc.size() + 1);

	compiledPats[patternSrc.size()].style = 0;

	// Build the tree of parse expressions
	for (size_t i = 0; i < patternSrc.size(); i++) {
		compiledPats[i].nSubPatterns = 0;
		compiledPats[i].nSubBranches = 0;
	}

	for (size_t i = 1; i < patternSrc.size(); i++) {
		if (patternSrc[i].subPatternOf.isNull()) {
			compiledPats[0].nSubPatterns++;
		} else {
			compiledPats[Highlight::indexOfNamedPattern(patternSrc, patternSrc[i].subPatternOf)].nSubPatterns++;
		}
	}

	for (size_t i = 0; i < patternSrc.size(); i++) {
		if (compiledPats[i].nSubPatterns != 0) {
			compiledPats[i].subPatterns = std::make_unique<HighlightData *[]>(compiledPats[i].nSubPatterns);
		}
	}

	for (size_t i = 0; i < patternSrc.size(); i++) {
		compiledPats[i].nSubPatterns = 0;
	}

	for (size_t i = 1; i < patternSrc.size(); i++) {
		if (patternSrc[i].subPatternOf.isNull()) {
			compiledPats[0].subPatterns[compiledPats[0].nSubPatterns++] = &compiledPats[i];
		} else {
			const size_t parentIndex = Highlight::indexOfNamedPattern(patternSrc, patternSrc[i].subPatternOf);

			compiledPats[parentIndex].subPatterns[compiledPats[parentIndex].nSubPatterns++] = &compiledPats[i];
		}
	}

	/* Process color-only sub patterns (no regular expressions to match,
	   just colors and fonts for sub-expressions of the parent pattern */
	for (size_t i = 0; i < patternSrc.size(); i++) {
		compiledPats[i].colorOnly      = (patternSrc[i].flags & COLOR_ONLY) != 0;
		compiledPats[i].userStyleIndex = Highlight::IndexOfNamedStyle(patternSrc[i].style);

		if (compiledPats[i].colorOnly && compiledPats[i].nSubPatterns != 0) {
			QMessageBox::warning(
				this,
				tr("Color-only Pattern"),
				tr("Color-only pattern \"%1\" may not have subpatterns").arg(patternSrc[i].name));
			return nullptr;
		}

		static const QRegularExpression re(QLatin1String("[0-9]+"));

		{
			if (!patternSrc[i].startRE.isNull()) {
				Input in(&patternSrc[i].startRE);
				Q_FOREVER {
					if (in.match(QLatin1Char('&'))) {
						compiledPats[i].startSubexprs.push_back(0);
					} else if (in.match(QLatin1Char('\\'))) {

						QString number;
						if (in.match(re, &number)) {
							compiledPats[i].startSubexprs.push_back(number.toUInt());
						} else {
							break;
						}
					} else {
						break;
					}
				}
			}
		}

		{
			if (!patternSrc[i].endRE.isNull()) {
				Input in(&patternSrc[i].endRE);
				Q_FOREVER {
					if (in.match(QLatin1Char('&'))) {
						compiledPats[i].endSubexprs.push_back(0);
					} else if (in.match(QLatin1Char('\\'))) {

						QString number;
						if (in.match(re, &number)) {
							compiledPats[i].endSubexprs.push_back(number.toUInt());
						} else {
							break;
						}
					} else {
						break;
					}
				}
			}
		}
	}

	// Compile regular expressions for all highlight patterns
	for (size_t i = 0; i < patternSrc.size(); i++) {

		if (patternSrc[i].startRE.isNull() || compiledPats[i].colorOnly) {
			compiledPats[i].startRE = nullptr;
		} else {
			compiledPats[i].startRE = compileRegexAndWarn(patternSrc[i].startRE);
			if (!compiledPats[i].startRE) {
				return nullptr;
			}
		}

		if (patternSrc[i].endRE.isNull() || compiledPats[i].colorOnly) {
			compiledPats[i].endRE = nullptr;
		} else {
			compiledPats[i].endRE = compileRegexAndWarn(patternSrc[i].endRE);
			if (!compiledPats[i].endRE) {
				return nullptr;
			}
		}

		if (patternSrc[i].errorRE.isNull()) {
			compiledPats[i].errorRE = nullptr;
		} else {
			compiledPats[i].errorRE = compileRegexAndWarn(patternSrc[i].errorRE);
			if (!compiledPats[i].errorRE) {
				return nullptr;
			}
		}
	}

	/* Construct and compile the great hairy pattern to match the OR of the
	   end pattern, the error pattern, and all of the start patterns of the
	   sub-patterns */
	for (size_t patternNum = 0; patternNum < patternSrc.size(); patternNum++) {
		if (patternSrc[patternNum].endRE.isNull() && patternSrc[patternNum].errorRE.isNull() && compiledPats[patternNum].nSubPatterns == 0) {
			compiledPats[patternNum].subPatternRE = nullptr;
			continue;
		}

		int length;
		length = (compiledPats[patternNum].colorOnly || patternSrc[patternNum].endRE.isNull()) ? 0 : patternSrc[patternNum].endRE.size() + 5;
		length += (compiledPats[patternNum].colorOnly || patternSrc[patternNum].errorRE.isNull()) ? 0 : patternSrc[patternNum].errorRE.size() + 5;

		for (size_t i = 0; i < compiledPats[patternNum].nSubPatterns; i++) {
			const size_t subPatIndex = compiledPats[patternNum].subPatterns[i] - &compiledPats[0];
			length += compiledPats[subPatIndex].colorOnly ? 0 : patternSrc[subPatIndex].startRE.size() + 5;
		}

		if (length == 0) {
			compiledPats[patternNum].subPatternRE = nullptr;
			continue;
		}

		std::string bigPattern;
		bigPattern.reserve(static_cast<size_t>(length));

		if (!patternSrc[patternNum].endRE.isNull()) {
			bigPattern += '(';
			bigPattern += '?';
			bigPattern += ':';
			bigPattern += patternSrc[patternNum].endRE.toStdString();
			bigPattern += ')';
			bigPattern += '|';
			compiledPats[patternNum].nSubBranches++;
		}

		if (!patternSrc[patternNum].errorRE.isNull()) {
			bigPattern += '(';
			bigPattern += '?';
			bigPattern += ':';
			bigPattern += patternSrc[patternNum].errorRE.toStdString();
			bigPattern += ')';
			bigPattern += '|';
			compiledPats[patternNum].nSubBranches++;
		}

		for (size_t i = 0; i < compiledPats[patternNum].nSubPatterns; i++) {
			const size_t subPatIndex = compiledPats[patternNum].subPatterns[i] - &compiledPats[0];

			if (compiledPats[subPatIndex].colorOnly) {
				continue;
			}

			bigPattern += '(';
			bigPattern += '?';
			bigPattern += ':';
			bigPattern += patternSrc[subPatIndex].startRE.toStdString();
			bigPattern += ')';
			bigPattern += '|';
			compiledPats[patternNum].nSubBranches++;
		}

		bigPattern.pop_back(); // remove last '|' character

		try {
			compiledPats[patternNum].subPatternRE = std::make_unique<Regex>(bigPattern, REDFLT_STANDARD);
		} catch (const RegexError &e) {
			qWarning("NEdit: Error compiling syntax highlight patterns:\n%s", e.what());
			if (verbosity == Verbosity::Verbose) {
				throw;
			}
			return nullptr;
		}
	}

	// Copy remaining parameters from pattern template to compiled tree
	for (size_t i = 0; i < patternSrc.size(); i++) {
		compiledPats[i].flags = patternSrc[i].flags;
	}

	return compiledPats;
}

/*
** compile a regular expression and present a user friendly dialog on failure.
*/
std::unique_ptr<Regex> DocumentWidget::compileRegexAndWarn(const QString &re) {

	try {
		return make_regex(re, REDFLT_STANDARD);
	} catch (const RegexError &e) {

		constexpr int MaxLength = 4096;

		/* If the regex is too long, truncate it and append ... */
		QString boundedRe = re;

		if (boundedRe.size() > MaxLength) {
			boundedRe.resize(MaxLength - 3);
			boundedRe.append(tr("..."));
		}

		QMessageBox::warning(
			this,
			tr("Error in Regex"),
			tr("Error in syntax highlighting regular expression:\n%1\n%2").arg(boundedRe, QString::fromLatin1(e.what())));
		return nullptr;
	}
}

/*
** Call this before closing a window, to clean up macro references to the
** window, stop any macro which might be running from it, free associated
** memory, and check that a macro is not attempting to close the window from
** which it is run.  If this is being called from a macro, and the window
** this routine is examining is the window from which the macro was run, this
** routine will return false, and the caller must NOT CLOSE THE WINDOW.
** Instead, empty it and make it Untitled, and let the macro completion
** process close the window when the macro is finished executing.
*/
bool DocumentWidget::macroWindowCloseActions() {
	const std::shared_ptr<MacroCommandData> cmdData = macroCmdData_;

	CommandRecorder *recorder = CommandRecorder::instance();

	if (recorder->isRecording() && recorder->macroRecordDocument() == this) {
		finishLearning();
	}

	/* If no macro is executing in the window, allow the close, but check
	   if macros executing in other windows have it as focus.  If so, set
	   their focus back to the window from which they were originally run */
	if (!cmdData) {
		for (DocumentWidget *document : DocumentWidget::allDocuments()) {

			const std::shared_ptr<MacroCommandData> mcd = document->macroCmdData_;

			if (document == MacroRunDocument() && MacroFocusDocument() == this) {
				SetMacroFocusDocument(MacroRunDocument());
			} else if (mcd && mcd->context->FocusDocument == this) {
				mcd->context->FocusDocument = mcd->context->RunDocument;
			}
		}

		return true;
	}

	/* If the macro currently running (and therefore calling us, because
	   execution must otherwise return to the main loop to execute any
	   commands), is running in this window, tell the caller not to close,
	   and schedule window close on completion of macro */
	if (MacroRunDocument() == this) {
		cmdData->closeOnCompletion = true;
		return false;
	}

	// Kill the macro command
	finishMacroCmdExecution();
	return true;
}

/*
** Cancel the macro command in progress (user cancellation via GUI)
*/
void DocumentWidget::abortMacroCommand() {
	if (!macroCmdData_) {
		return;
	}

	/* If there's both a macro and a shell command executing, the shell command
	   must have been called from the macro.  When called from a macro, shell
	   commands don't put up cancellation controls of their own, but rely
	   instead on the macro cancellation mechanism (here) */
	if (shellCmdData_) {
		abortShellCommand();
	}

	// Kill the macro command
	finishMacroCmdExecution();
}

/*
** Cancel Learn mode, or macro execution (they're bound to the same menu item)
*/
void DocumentWidget::cancelMacroOrLearn() {
	if (CommandRecorder::instance()->isRecording()) {
		cancelLearning();
	} else if (macroCmdData_) {
		abortMacroCommand();
	}
}

/*
** Execute the learn/replay sequence
*/
void DocumentWidget::replay() {

	QString replayMacro = CommandRecorder::instance()->replayMacro();

	// Verify that a replay macro exists and it's not empty and that
	// we're not already running a macro
	if (!replayMacro.isEmpty() && !macroCmdData_) {

		QString errMsg;
		int stoppedAt;

		Program *prog = compileMacro(replayMacro, &errMsg, &stoppedAt);
		if (!prog) {
			qWarning("NEdit: internal error, learn/replay macro syntax error: %s", qPrintable(errMsg));
			return;
		}

		runMacro(prog);
	}
}

void DocumentWidget::cancelLearning() {

	if (!CommandRecorder::instance()->isRecording()) {
		return;
	}

	DocumentWidget *document = CommandRecorder::instance()->macroRecordDocument();
	Q_ASSERT(document);

	for (MainWindow *window : MainWindow::allWindows()) {
		window->ui.action_Learn_Keystrokes->setEnabled(true);
	}

	if (document->isTopDocument()) {
		auto win = MainWindow::fromDocument(document);
		Q_ASSERT(win);
		win->ui.action_Finish_Learn->setEnabled(false);
		win->ui.action_Cancel_Learn->setEnabled(false);
	}

	document->clearModeMessage();
}

void DocumentWidget::finishLearning() {

	if (!CommandRecorder::instance()->isRecording()) {
		return;
	}

	DocumentWidget *document = CommandRecorder::instance()->macroRecordDocument();
	Q_ASSERT(document);

	CommandRecorder::instance()->stopRecording();

	for (MainWindow *window : MainWindow::allWindows()) {
		window->ui.action_Learn_Keystrokes->setEnabled(true);
		window->ui.action_Replay_Keystrokes->setEnabled(true);
	}

	if (document->isTopDocument()) {
		if (auto win = MainWindow::fromDocument(document)) {
			win->ui.action_Finish_Learn->setEnabled(false);
			win->ui.action_Cancel_Learn->setEnabled(false);
		}
	}

	document->clearModeMessage();
}

/*
** Executes macro string "macro" using the lastFocus pane in this document.
** Reports errors via a dialog, integrating the name "errInName" into the
** message to help identify the source of the error.
*/
void DocumentWidget::doMacro(const QString &macro, const QString &errInName) {

	/* Add a terminating newline (which command line users are likely to omit
	   since they are typically invoking a single routine) */
	QString qMacro = macro + QLatin1Char('\n');
	QString errMsg;

	// Parse the macro and report errors if it fails
	int stoppedAt;
	Program *const prog = compileMacro(qMacro, &errMsg, &stoppedAt);
	if (!prog) {
		Preferences::reportError(this, qMacro, stoppedAt, errInName, errMsg);
		return;
	}

	// run the executable program (prog is freed upon completion)
	runMacro(prog);
}

/*
**  Read the initial NEdit macro file if one exists.
*/
void DocumentWidget::readMacroInitFile() {

	static bool initFileLoaded = false;

	if (!initFileLoaded) {

		const QString autoloadName = Settings::autoLoadMacroFile();
		if (autoloadName.isNull()) {
			return;
		}

		readMacroFile(autoloadName, false);
		initFileLoaded = true;
	}
}

/*
** Parse and execute a macro string including macro definitions.  Report
** parsing errors in a dialog.
*/
bool DocumentWidget::readMacroString(const QString &string, const QString &errIn) {
	return readCheckMacroString(this, string, this, errIn, nullptr);
}

/**
 * @brief DocumentWidget::endSmartIndent
 */
void DocumentWidget::endSmartIndent() {

	if (!info_->smartIndentData) {
		return;
	}

	info_->smartIndentData = nullptr;
}

/**
 * @brief DocumentWidget::inSmartIndentMacros
 * @return
 */
bool DocumentWidget::inSmartIndentMacros() const {
	return info_->smartIndentData && (info_->smartIndentData->inModMacro || info_->smartIndentData->inNewLineMacro);
}

/**
 * @brief DocumentWidget::getAnySelection
 * @return
 */
QString DocumentWidget::getAnySelection() const {
	return getAnySelection(ErrorSound::Silent);
}

/**
 * @brief DocumentWidget::GetAnySelection
 * @param errorSound
 * @return
 */
QString DocumentWidget::getAnySelection(ErrorSound errorSound) const {

	if (QApplication::clipboard()->supportsSelection()) {
		const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
		if (mimeData->hasText()) {
			return mimeData->text();
		}
	}

	// If the selection is in the window's own buffer get it from there
	if (info_->buffer->primary.hasSelection()) {
		return QString::fromStdString(info_->buffer->BufGetSelectionText());
	}

	if (errorSound == ErrorSound::Beep) {
		QApplication::beep();
	}

	return QString();
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns nullptr when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching, and the regex engine can skip
** compiling a delimiter table when delimiters is null).
*/
QString DocumentWidget::getWindowDelimiters() const {
	if (languageMode_ == PLAIN_LANGUAGE_MODE) {
		return QString();
	} else {
		return Preferences::LanguageModes[languageMode_].delimiters;
	}
}

/**
 * @brief DocumentWidget::useTabs
 * @return
 */
bool DocumentWidget::useTabs() const {
	return info_->buffer->BufGetUseTabs();
}

/**
 * @brief DocumentWidget::setUseTabs
 * @param value
 */
void DocumentWidget::setUseTabs(bool value) {

	emit_event("set_use_tabs", QString::number(value));
	info_->buffer->BufSetUseTabs(value);
}

/**
 * @brief DocumentWidget::highlightSyntax
 * @return
 */
bool DocumentWidget::highlightSyntax() const {
	return highlightSyntax_;
}

/**
 * @brief DocumentWidget::setHighlightSyntax
 * @param value
 */
void DocumentWidget::setHighlightSyntax(bool value) {

	emit_event("set_highlight_syntax", QString::number(value));

	highlightSyntax_ = value;

	if (isTopDocument()) {
		if (auto win = MainWindow::fromDocument(this)) {
			no_signals(win->ui.action_Highlight_Syntax)->setChecked(value);
		}
	}

	if (highlightSyntax_) {
		startHighlighting(Verbosity::Verbose);
	} else {
		stopHighlighting();
	}
}

/**
 * @brief DocumentWidget::makeBackupCopy
 * @return
 */
bool DocumentWidget::makeBackupCopy() const {
	return info_->saveOldVersion;
}

/**
 * @brief DocumentWidget::setMakeBackupCopy
 * @param value
 */
void DocumentWidget::setMakeBackupCopy(bool value) {

	emit_event("set_make_backup_copy", value ? QLatin1String("1") : QLatin1String("0"));

	if (isTopDocument()) {
		if (auto win = MainWindow::fromDocument(this)) {
			no_signals(win->ui.action_Make_Backup_Copy)->setChecked(value);
		}
	}

	info_->saveOldVersion = value;
}

/**
 * @brief DocumentWidget::incrementalBackup
 * @return
 */
bool DocumentWidget::incrementalBackup() const {
	return info_->autoSave;
}

/**
 * @brief DocumentWidget::setIncrementalBackup
 * @param value
 */
void DocumentWidget::setIncrementalBackup(bool value) {

	emit_event("set_incremental_backup", QString::number(value));

	info_->autoSave = value;

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	no_signals(win->ui.action_Highlight_Syntax)->setChecked(value);
}

/**
 * @brief DocumentWidget::userLocked
 * @return
 */
bool DocumentWidget::userLocked() const {
	return info_->lockReasons.isUserLocked();
}

void DocumentWidget::setUserLocked(bool value) {
	emit_event("set_locked", QString::number(value));

	info_->lockReasons.setUserLocked(value);

	if (!isTopDocument()) {
		return;
	}

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return;
	}

	no_signals(win->ui.action_Read_Only)->setChecked(info_->lockReasons.isAnyLocked());
	Q_EMIT updateWindowTitle(this);
	Q_EMIT updateWindowReadOnly(this);
}

/**
 * @brief DocumentWidget::addMark
 * @param area
 * @param label
 */
void DocumentWidget::addMark(TextArea *area, QChar label) {

	/* look for a matching mark to re-use, or advance
	   nMarks to create a new one */
	label = label.toUpper();

	auto it = markTable_.find(label);
	if (it != markTable_.end()) {
		// store the cursor location and selection position in the table
		it->second.label     = label;
		it->second.cursorPos = area->cursorPos();
		it->second.sel       = info_->buffer->primary;
	} else {
		Bookmark bookmark;
		bookmark.label     = label;
		bookmark.cursorPos = area->cursorPos();
		bookmark.sel       = info_->buffer->primary;
		markTable_.emplace(label, bookmark);
	}
}

void DocumentWidget::selectNumberedLine(TextArea *area, int64_t lineNum) {
	int i;
	TextCursor lineStart = {};

	// count lines to find the start and end positions for the selection
	if (lineNum < 1) {
		lineNum = 1;
	}

	auto lineEnd = TextCursor(-1);

	for (i = 1; i <= lineNum && lineEnd < info_->buffer->length(); i++) {
		lineStart = lineEnd + 1;
		lineEnd   = info_->buffer->BufEndOfLine(lineStart);
	}

	// highlight the line
	if (i > lineNum) {
		// Line was found
		if (lineEnd < info_->buffer->length()) {
			info_->buffer->BufSelect(lineStart, lineEnd + 1);
		} else {
			// Don't select past the end of the buffer !
			info_->buffer->BufSelect(lineStart, info_->buffer->BufEndOfBuffer());
		}
	} else {
		/* Line was not found -> position the selection & cursor at the end
		   without making a real selection and beep */
		lineStart = info_->buffer->BufEndOfBuffer();
		info_->buffer->BufSelect(lineStart, lineStart);
		QApplication::beep();
	}

	makeSelectionVisible(area);
	area->TextSetCursorPos(lineStart);
}

void DocumentWidget::gotoMark(TextArea *area, QChar label, bool extendSel) {

	// look up the mark in the mark table
	label   = label.toUpper();
	auto it = markTable_.find(label);
	if (it == markTable_.end()) {
		QApplication::beep();
		return;
	}

	const Bookmark &bookmark = it->second;

	// reselect marked the selection, and move the cursor to the marked pos
	const TextBuffer::Selection &sel    = bookmark.sel;
	const TextBuffer::Selection &oldSel = info_->buffer->primary;

	TextCursor cursorPos = bookmark.cursorPos;
	if (extendSel) {

		const TextCursor oldStart = oldSel.hasSelection() ? oldSel.start() : area->cursorPos();
		const TextCursor oldEnd   = oldSel.hasSelection() ? oldSel.end() : area->cursorPos();
		const TextCursor newStart = sel.hasSelection() ? sel.start() : cursorPos;
		const TextCursor newEnd   = sel.hasSelection() ? sel.end() : cursorPos;

		info_->buffer->BufSelect(oldStart < newStart ? oldStart : newStart, oldEnd > newEnd ? oldEnd : newEnd);
	} else {
		if (sel.hasSelection()) {
			if (sel.isRectangular()) {
				info_->buffer->BufRectSelect(sel.start(), sel.end(), sel.rectStart(), sel.rectEnd());
			} else {
				info_->buffer->BufSelect(sel.start(), sel.end());
			}
		} else {
			info_->buffer->BufUnselect();
		}
	}

	/* Move the window into a pleasing position relative to the selection
	   or cursor.   MakeSelectionVisible is not great with multi-line
	   selections, and here we will sometimes give it one.  And to set the
	   cursor position without first using the less pleasing capability
	   of the widget itself for bringing the cursor in to view, you have to
	   first turn it off, set the position, then turn it back on. */
	area->setAutoShowInsertPos(false);
	area->TextSetCursorPos(cursorPos);
	makeSelectionVisible(area);
	area->setAutoShowInsertPos(true);
}

/**
 * @brief DocumentWidget::lockReasons
 * @return
 */
LockReasons DocumentWidget::lockReasons() const {
	return info_->lockReasons;
}

/*      Finds all matches and handles tag "collisions". Prompts user with a
		list of collided tags in the hash table and allows the user to select
		the correct one. */
int DocumentWidget::findAllMatches(TextArea *area, const QString &string) {

	int i;
	int pathMatch = 0;
	int samePath  = 0;
	int nMatches  = 0;

	// verify that the string is reasonable as a tag
	if (string.isEmpty()) {
		QApplication::beep();
		return -1;
	}

	Tags::tagName = string;

	QList<Tags::Tag> tags = Tags::lookupTag(string, Tags::searchMode);

	// First look up all of the matching tags
	for (const Tags::Tag &tag : tags) {

		QString fileToSearch = tag.file;
		QString searchString = tag.searchString;
		QString tagPath      = tag.path;
		size_t langMode      = tag.language;
		int64_t startPos     = tag.posInf;

		/*
		** Skip this tag if it has a language mode that doesn't match the
		** current language mode, but don't skip anything if the window is in
		** PLAIN_LANGUAGE_MODE.
		*/
		if (getLanguageMode() != PLAIN_LANGUAGE_MODE && Preferences::GetPrefSmartTags() && langMode != PLAIN_LANGUAGE_MODE && langMode != getLanguageMode()) {
			continue;
		}

		if (QFileInfo(fileToSearch).isAbsolute()) {
			Tags::tagFiles[nMatches] = fileToSearch;
		} else {
			Tags::tagFiles[nMatches] = QStringLiteral("%1%2").arg(tagPath, fileToSearch);
		}

		Tags::tagSearch[nMatches] = searchString;
		Tags::tagPosInf[nMatches] = startPos;

		PathInfo fi = parseFilename(Tags::tagFiles[nMatches]);

		// Is this match in the current file?  If so, use it!
		if (Preferences::GetPrefSmartTags() && info_->filename == fi.filename && info_->path == fi.pathname) {
			if (nMatches) {
				Tags::tagFiles[0]  = Tags::tagFiles[nMatches];
				Tags::tagSearch[0] = Tags::tagSearch[nMatches];
				Tags::tagPosInf[0] = Tags::tagPosInf[nMatches];
			}
			nMatches = 1;
			break;
		}

		// Is this match in the same dir. as the current file?
		if (info_->path == fi.pathname) {
			samePath++;
			pathMatch = nMatches;
		}

		if (++nMatches >= Tags::MaxDupTags) {
			QMessageBox::warning(this, tr("Tags"), tr("Too many duplicate tags, first %1 shown").arg(Tags::MaxDupTags));
			break;
		}
	}

	// Did we find any matches?
	if (!nMatches) {
		return 0;
	}

	// Only one of the matches is in the same dir. as this file.  Use it.
	if (Preferences::GetPrefSmartTags() && samePath == 1 && nMatches > 1) {
		Tags::tagFiles[0]  = Tags::tagFiles[pathMatch];
		Tags::tagSearch[0] = Tags::tagSearch[pathMatch];
		Tags::tagPosInf[0] = Tags::tagPosInf[pathMatch];
		nMatches           = 1;
	}

	//  If all of the tag entries are the same file, just use the first.
	if (Preferences::GetPrefSmartTags()) {
		for (i = 1; i < nMatches; i++) {
			if (Tags::tagFiles[i] != Tags::tagFiles[i - 1]) {
				break;
			}
		}

		if (i == nMatches) {
			nMatches = 1;
		}
	}

	if (nMatches > 1) {
		QStringList dupTagsList;

		for (i = 0; i < nMatches; i++) {

			QString temp;

			PathInfo fi = parseFilename(Tags::tagFiles[i]);

			if ((i < nMatches - 1 && (Tags::tagFiles[i] == Tags::tagFiles[i + 1])) || (i > 0 && (Tags::tagFiles[i] == Tags::tagFiles[i - 1]))) {

				if (!Tags::tagSearch[i].isEmpty() && (Tags::tagPosInf[i] != -1)) {
					// etags
					temp = QString::asprintf("%2d. %s%s %8lli %s", i + 1, qPrintable(fi.pathname), qPrintable(fi.filename), static_cast<long long>(Tags::tagPosInf[i]), qPrintable(Tags::tagSearch[i]));
				} else if (!Tags::tagSearch[i].isEmpty()) {
					// ctags search expr
					temp = QString::asprintf("%2d. %s%s          %s", i + 1, qPrintable(fi.pathname), qPrintable(fi.filename), qPrintable(Tags::tagSearch[i]));
				} else {
					// line number only
					temp = QString::asprintf("%2d. %s%s %8lli", i + 1, qPrintable(fi.pathname), qPrintable(fi.filename), static_cast<long long>(Tags::tagPosInf[i]));
				}
			} else {
				temp = QString::asprintf("%2d. %s%s", i + 1, qPrintable(fi.pathname), qPrintable(fi.filename));
			}

			dupTagsList.push_back(temp);
		}

		createSelectMenu(area, dupTagsList);
		return 1;
	}

	/*
	**  No need for a dialog list, there is only one tag matching --
	**  Go directly to the tag
	*/
	if (Tags::searchMode == Tags::SearchMode::TAG) {
		editTaggedLocation(area, 0);
	} else {
		Tags::showMatchingCalltip(this, area, 0);
	}

	return 1;
}

/**
 * @brief DocumentWidget::createSelectMenuEx
 * @param area
 * @param args
 *
 * Create a Menu for user to select from the collided tags
 */
void DocumentWidget::createSelectMenu(TextArea *area, const QStringList &args) {

	auto dialog = std::make_unique<DialogDuplicateTags>(this, area);
	dialog->setTag(Tags::tagName);
	for (int i = 0; i < args.size(); ++i) {
		dialog->addListItem(args[i], i);
	}
	dialog->exec();
}

/*
** Try to display a calltip
**  anchored:       If true, tip appears at position pos
**  lookup:         If true, text is considered a key to be searched for in the
**                  tip and/or tag database depending on search_type
**  search_type:    Either TIP or TIP_FROM_TAG
*/
int DocumentWidget::showTipString(const QString &text, bool anchored, int pos, bool lookup, Tags::SearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode) {
	if (search_type == Tags::SearchMode::TAG) {
		return 0;
	}

	// So we don't have to carry all of the calltip alignment info around
	Tags::globAnchored  = anchored;
	Tags::globPos       = pos;
	Tags::globHAlign    = hAlign;
	Tags::globVAlign    = vAlign;
	Tags::globAlignMode = alignMode;

	MainWindow *win = MainWindow::fromDocument(this);
	if (!win) {
		return 0;
	}

	// If this isn't a lookup request, just display it.
	if (!lookup) {
		return Tags::tagsShowCalltip(win->lastFocus(), text);
	} else {
		return findDefinitionHelperCommon(win->lastFocus(), text, search_type);
	}
}

/*  Open a new (or existing) editor window to the location specified in
	tagFiles[i], tagSearch[i], tagPosInf[i] */
void DocumentWidget::editTaggedLocation(TextArea *area, int i) {

	const PathInfo fi = parseFilename(Tags::tagFiles[i]);

	// open the file containing the definition
	DocumentWidget::editExistingFile(
		this,
		fi.filename,
		fi.pathname,
		0,
		QString(),
		/*iconic=*/false,
		QString(),
		Preferences::GetPrefOpenInTab(),
		/*bgOpen=*/false);

	DocumentWidget *documentToSearch = MainWindow::findWindowWithFile(fi);
	if (!documentToSearch) {
		QMessageBox::warning(
			this,
			tr("File not found"),
			tr("File %1 not found").arg(Tags::tagFiles[i]));
		return;
	}

	const int64_t tagLineNumber = Tags::tagPosInf[i];

	if (Tags::tagSearch[i].isEmpty()) {
		// if the search string is empty, select the numbered line
		selectNumberedLine(area, tagLineNumber);
		return;
	}

	int64_t startPos = tagLineNumber;
	int64_t endPos;

	// search for the tags file search string in the newly opened file
	if (!Tags::fakeRegExSearch(documentToSearch->buffer()->BufAsString(), Tags::tagSearch[i], &startPos, &endPos)) {
		QMessageBox::warning(
			this,
			tr("Tag Error"),
			tr("Definition for %1\nnot found in %2").arg(Tags::tagName, Tags::tagFiles[i]));
		return;
	}

	// select the matched string
	documentToSearch->buffer()->BufSelect(TextCursor(startPos), TextCursor(endPos));
	documentToSearch->raiseFocusDocumentWindow(true);

	/* Position it nicely in the window,
	   about 1/4 of the way down from the top */
	const int64_t lineNum = documentToSearch->buffer()->BufCountLines(TextCursor(0), TextCursor(startPos));

	QPointer<TextArea> tagArea = MainWindow::fromDocument(documentToSearch)->lastFocus();
	int rows                   = tagArea->getRows();
	tagArea->verticalScrollBar()->setValue(lineNum - (rows / 4));
	tagArea->horizontalScrollBar()->setValue(0);
	tagArea->TextSetCursorPos(TextCursor(endPos));
}

/**
 * @brief DocumentWidget::fileFormat
 * @return
 */
FileFormats DocumentWidget::fileFormat() const {
	return info_->fileFormat;
}

/**
 * @brief DocumentWidget::defaultFont
 * @return
 */
QFont DocumentWidget::defaultFont() const {
	return font_;
}

/**
 * @brief DocumentWidget::setPath
 * @param pathname
 */
void DocumentWidget::setPath(const QString &pathname) {
	info_->path = pathname;

	// do we have a "/" at the end? if not, add one
	if (!info_->path.isEmpty() && !info_->path.endsWith(QLatin1Char('/'))) {
		info_->path.append(QLatin1Char('/'));
	}
}

/**
 * @brief DocumentWidget::setPath
 * @param pathname
 */
void DocumentWidget::setPath(const QDir &pathname) {
	setPath(pathname.path());
}

/**
 * @brief DocumentWidget::device
 * @return
 */
dev_t DocumentWidget::device() const {
	return info_->statbuf.st_dev;
}

/**
 * @brief DocumentWidget::inode
 * @return
 */
ino_t DocumentWidget::inode() const {
	return info_->statbuf.st_ino;
}

/**
 * @brief DocumentWidget::buffer
 * @return
 */
TextBuffer *DocumentWidget::buffer() const {
	return info_->buffer.get();
}

/**
 * @brief DocumentWidget::filenameSet
 * @return
 */
bool DocumentWidget::filenameSet() const {
	return info_->filenameSet;
}

/**
 * @brief DocumentWidget::fileChanged
 * @return
 */
bool DocumentWidget::fileChanged() const {
	return info_->fileChanged;
}

/**
 * @brief DocumentWidget::showMatchingStyle
 * @return
 */
ShowMatchingStyle DocumentWidget::showMatchingStyle() const {
	return info_->showMatchingStyle;
}

/**
 * @brief DocumentWidget::autoIndentStyle
 * @return
 */
IndentStyle DocumentWidget::autoIndentStyle() const {
	return info_->indentStyle;
}

/**
 * @brief DocumentWidget::wrapMode
 * @return
 */
WrapStyle DocumentWidget::wrapMode() const {
	return info_->wrapMode;
}

/**
 * @brief DocumentWidget::setFileFormat
 * @param format
 */
void DocumentWidget::setFileFormat(FileFormats fileFormat) {
	info_->fileFormat = fileFormat;
}

/**
 * @brief dragEnterEvent
 * @param event
 */
void DocumentWidget::dragEnterEvent(QDragEnterEvent *event) {
	if (event->mimeData()->hasUrls()) {
		event->accept();
	}
}

/**
 * @brief dropEvent
 * @param event
 */
void DocumentWidget::dropEvent(QDropEvent *event) {
	auto urls = event->mimeData()->urls();
	for (auto url : urls) {
		if (url.isLocalFile()) {
			QString fileName = url.toLocalFile();
			open(fileName);
		} else {
			QApplication::beep();
		}
	}
}
