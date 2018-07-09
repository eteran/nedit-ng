
#include "DocumentWidget.h"
#include "CommandRecorder.h"
#include "DialogMoveDocument.h"
#include "DialogDuplicateTags.h"
#include "DialogOutput.h"
#include "DialogPrint.h"
#include "DialogReplace.h"
#include "DragEndEvent.h"
#include "EditFlags.h"
#include "Util/fileUtils.h"
#include "Font.h"
#include "FontType.h"
#include "HighlightData.h"
#include "Highlight.h"
#include "HighlightStyle.h"
#include "Util/Input.h"
#include "interpret.h"
#include "LanguageMode.h"
#include "parse.h"
#include "macro.h"
#include "MainWindow.h"
#include "PatternSet.h"
#include "preferences.h"
#include "Search.h"
#include "Settings.h"
#include "SignalBlocker.h"
#include "SmartIndentEntry.h"
#include "SmartIndentEvent.h"
#include "SmartIndent.h"
#include "Style.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "userCmds.h"
#include "Util/ClearCase.h"
#include "Util/utils.h"
#include "WindowHighlightData.h"
#include "WindowMenuEvent.h"
#include "X11Colors.h"

#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QRadioButton>
#include <QSplitter>
#include <QTemporaryFile>
#include <QTimer>
#include <QButtonGroup>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// NOTE(eteran): generally, this class reaches out to MainWindow FAR too much
// it would be better to create some fundamental signals that MainWindow could
// listen on and update itself as needed. This would reduce a lot fo the heavy
// coupling seen in this class :-/.

/* data attached to window during shell command execution with information for
 * controling and communicating with the process */
struct ShellCommandData {
    QTimer        bannerTimer;
    QByteArray    standardError;
    QByteArray    standardOutput;
    QProcess *    process;    
    TextArea *    area;    
    TextCursor    leftPos;
    TextCursor    rightPos;
    CommandSource source;
    int           flags;
    bool          bannerIsUp;    
};

DocumentWidget *DocumentWidget::LastCreated;

namespace {

// number of distinct chars the user can typebefore NEdit gens. new backup file
constexpr int AUTOSAVE_CHAR_LIMIT  = 80;

// number of distinct editing operations user can do before NEdit gens. new backup file
constexpr int AUTOSAVE_OP_LIMIT    = 8;

// Max # of ADDITIONAL text editing panes  that can be added to a window
constexpr int MAX_PANES  = 6;

// how long to wait (msec) before putting up Shell Command Executing... banner
constexpr int BANNER_WAIT_TIME = 6000;

// flags for issueCommand
enum {
    ACCUMULATE  	  = 1,
    ERROR_DIALOGS	  = 2,
    REPLACE_SELECTION = 4,
    RELOAD_FILE_AFTER = 8,
    OUTPUT_TO_DIALOG  = 16,
    OUTPUT_TO_STRING  = 32
};

struct CharMatchTable {
    char      c;
    char      match;
    Direction direction;
};

constexpr CharMatchTable MatchingChars[] = {
    {'{', '}',   Direction::Forward},
    {'}', '{',   Direction::Backward},
    {'(', ')',   Direction::Forward},
    {')', '(',   Direction::Backward},
    {'[', ']',   Direction::Forward},
    {']', '[',   Direction::Backward},
    {'<', '>',   Direction::Forward},
    {'>', '<',   Direction::Backward},
    {'/', '/',   Direction::Forward},
    {'"', '"',   Direction::Forward},
    {'\'', '\'', Direction::Forward},
    {'`', '`',   Direction::Forward},
    {'\\', '\\', Direction::Forward},
};

constexpr CharMatchTable FlashingChars[] = {
    {'{', '}',   Direction::Forward},
    {'}', '{',   Direction::Backward},
    {'(', ')',   Direction::Forward},
    {')', '(',   Direction::Backward},
    {'[', ']',   Direction::Forward},
    {']', '[',   Direction::Backward},
};

/*
 * Number of bytes read at once by cmpWinAgainstFile
 */
constexpr auto PREFERRED_CMPBUF_LEN = static_cast<int64_t>(0x8000);

/* Maximum frequency in miliseconds of checking for external modifications.
   The periodic check is only performed on buffer modification, and the check
   interval is only to prevent checking on every keystroke in case of a file
   system which is slow to process stat requests (which I'm not sure exists) */
constexpr int MOD_CHECK_INTERVAL = 3000;


void modifiedCB(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user) {
    if(auto document = static_cast<DocumentWidget *>(user)) {
        document->modifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText);
    }
}

void smartIndentCB(TextArea *area, SmartIndentEvent *data, void *user) {
    if(auto document = static_cast<DocumentWidget *>(user)) {
        document->smartIndentCallback(area, data);
	}
}

void movedCB(TextArea *area, void *user) {
    if(auto document = static_cast<DocumentWidget *>(user)) {
        document->movedCallback(area);
	}
}

void dragStartCB(TextArea *area, void *user) {
    if(auto document = static_cast<DocumentWidget *>(user)) {
        document->dragStartCallback(area);
	}
}

void dragEndCB(TextArea *area, const DragEndEvent *data, void *user) {
    if(auto document = static_cast<DocumentWidget *>(user)) {
        document->dragEndCallback(area, data);
	}
}

void handleUnparsedRegionCB(const TextArea *area, TextCursor pos, const void *user) {
    if(auto document = static_cast<const DocumentWidget *>(user)) {
        document->handleUnparsedRegionEx(area->getStyleBuffer(), pos);
    }
}

/*
** Buffer replacement wrapper routine to be used for inserting output from
** a command into the buffer, which takes into account that the buffer may
** have been shrunken by the user (eg, by Undo). If necessary, the starting
** and ending positions (part of the state of the command) are corrected.
*/
void safeBufReplace(TextBuffer *buf, TextCursor *start, TextCursor *end, view::string_view text) {

    const TextCursor last = buf->BufEndOfBuffer();

    *start = std::min(last, *start);
    *end   = std::min(last, *end);

    buf->BufReplaceEx(*start, *end, text);
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
    if (!sel.selected || pos > sel.end) {
        return;
    }

    maintainPosition(sel.start, pos, nInserted, nDeleted);
    maintainPosition(sel.end,   pos, nInserted, nDeleted);

    if (sel.end <= sel.start) {
        sel.selected = false;
    }
}

/**
 * @brief determineUndoType
 * @param nInserted
 * @param nDeleted
 * @return
 */
UndoTypes determineUndoType(int64_t nInserted, int64_t nDeleted) {

    const bool textDeleted  = (nDeleted  > 0);
    const bool textInserted = (nInserted > 0);

    if (textInserted && !textDeleted) {
        // Insert
        if (nInserted == 1) return ONE_CHAR_INSERT;
        else                return BLOCK_INSERT;
    } else if (textInserted && textDeleted) {
        // Replace
        if (nInserted == 1) return ONE_CHAR_REPLACE;
        else                return BLOCK_REPLACE;
    } else if (!textInserted && textDeleted) {
        // Delete
        if (nDeleted == 1)  return ONE_CHAR_DELETE;
        else                return BLOCK_DELETE;
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
    switch(how) {
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

}

/*
** Open an existing file specified by name and path.  Use the document
** inDocument unless inDocument is nullptr or points to a document which is
** already in use (displays a file other than Untitled, or is Untitled but
** modified). Flags can be any of:
**
**	CREATE: 		      If file is not found, (optionally) prompt the user
**                        whether to create
**	SUPPRESS_CREATE_WARN  When creating a file, don't ask the user
**	PREF_READ_ONLY		  Make the file read-only regardless
**
** If languageMode is passed as QString(), it will be determined automatically
** from the file extension or file contents.
**
** If bgOpen is true, then the file will be open in background. This
** works in association with the SetLanguageMode() function that has
** the syntax highlighting deferred, in order to speed up the file-
** opening operation when multiple files are being opened in succession.
*/
DocumentWidget *DocumentWidget::EditExistingFileEx(DocumentWidget *inDocument, const QString &name, const QString &path, int flags, const QString &geometry, bool iconic, const QString &languageMode, bool tabbed, bool bgOpen) {

    // first look to see if file is already displayed in a window
    if(DocumentWidget *document = MainWindow::FindWindowWithFile(name, path)) {
        if (!bgOpen) {
            if (iconic) {
                document->RaiseDocument();
            } else {
                document->RaiseDocumentWindow();
            }
        }
        return document;
    }

    // helper local function to reduce code duplication
    auto createInNewWindow = [&name, &geometry, iconic]() {
        auto win = new MainWindow();
        DocumentWidget *document = win->CreateDocument(name);
        if(iconic) {
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
	if(!inDocument) {
        document = createInNewWindow();
	} else if (inDocument->filenameSet_ || inDocument->fileChanged_ || inDocument->macroCmdData_) {
        if (tabbed) {
            if(auto win = MainWindow::fromDocument(inDocument)) {
                document = win->CreateDocument(name);
            } else {
				return nullptr;
			}
        } else {
            document = createInNewWindow();
        }
    } else {
        // open file in untitled document
		document            = inDocument;
        document->path_     = path;
        document->filename_ = name;

        if (!iconic && !bgOpen) {
            document->RaiseDocumentWindow();
        }
    }

    MainWindow *const win = MainWindow::fromDocument(document);
    if(!win) {
        return nullptr;
    }

    // Open the file
    if (!document->doOpen(name, path, flags)) {
        document->CloseDocument();
        return nullptr;
    }

    win->forceShowLineNumbers();

    // Decide what language mode to use, trigger language specific actions
    if(languageMode.isNull()) {
        document->DetermineLanguageMode(/*forceNewDefaults=*/true);
    } else {
        document->action_Set_Language_Mode(languageMode, /*forceNewDefaults=*/true);
    }

    // update tab label and tooltip
    document->RefreshTabState();
    win->SortTabBar();

    if (!bgOpen) {
        document->RaiseDocument();
    }

    /* Bring the title bar and statistics line up to date, doOpen does
       not necessarily set the window title or read-only status */
    win->UpdateWindowTitle(document);
    win->UpdateWindowReadOnly(document);

    document->UpdateStatsLine(nullptr);

    // Add the name to the convenience menu of previously opened files
    auto fullname = tr("%1%2").arg(path, name);

    if (Preferences::GetPrefAlwaysCheckRelTagsSpecs()) {
        Tags::AddRelTagsFileEx(Preferences::GetPrefTagFile(), path, Tags::SearchMode::TAG);
    }

    MainWindow::AddToPrevOpenMenu(fullname);
    return document;
}

/**
 * @brief DocumentWidget::DocumentWidget
 * @param name
 * @param parent
 * @param f
 */
DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	ui.setupUi(this);

    // track what the last created document was so that focus_document("last")
    // works correctly
    LastCreated = this;

    // Every document has a backing buffer
	buffer_ = new TextBuffer();
    buffer_->BufAddModifyCB(SyntaxHighlightModifyCBEx, this);

	// create the text widget
	splitter_ = new QSplitter(Qt::Vertical, this);
	splitter_->setChildrenCollapsible(false);
	ui.verticalLayout->addWidget(splitter_);

    // NOTE(eteran): I'm not sure why this is necessary to make things look right :-/
#ifdef Q_OS_MACOS
    ui.verticalLayout->setContentsMargins(0, 5, 0, 0);
#else
    ui.verticalLayout->setContentsMargins(0, 0, 0, 0);
#endif
	// initialize the members
	filename_              = name;
    indentStyle_           = Preferences::GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
    autoSave_              = Preferences::GetPrefAutoSave();
    saveOldVersion_        = Preferences::GetPrefSaveOldVersion();
    wrapMode_              = Preferences::GetPrefWrap(PLAIN_LANGUAGE_MODE);
    showMatchingStyle_     = Preferences::GetPrefShowMatching();
    matchSyntaxBased_      = Preferences::GetPrefMatchSyntaxBased();
    highlightSyntax_       = Preferences::GetPrefHighlightSyntax();
    backlightChars_        = Preferences::GetPrefBacklightChars();
	
	if (backlightChars_) {
        QString cTypes = Preferences::GetPrefBacklightCharTypes();
        if (!cTypes.isNull()) {
            backlightCharTypes_ = cTypes;
		}
	}
	
    flashTimer_            = new QTimer(this);
    fontName_              = Preferences::GetPrefFontName();
    italicFontName_        = Preferences::GetPrefItalicFontName();
    boldFontName_          = Preferences::GetPrefBoldFontName();
    boldItalicFontName_    = Preferences::GetPrefBoldItalicFontName();
    fontStruct_            = Preferences::GetPrefDefaultFont();
    italicFontStruct_      = Preferences::GetPrefItalicFont();
    boldFontStruct_        = Preferences::GetPrefBoldFont();
    boldItalicFontStruct_  = Preferences::GetPrefBoldItalicFont();
	languageMode_          = PLAIN_LANGUAGE_MODE;
    showStats_             = Preferences::GetPrefStatsLine();
	
    ShowStatsLine(showStats_);

    flashTimer_->setInterval(1500);
    flashTimer_->setSingleShot(true);

    connect(flashTimer_, &QTimer::timeout, this, [this]() {
        eraseFlashEx();
    });

    auto area = createTextArea(buffer_);

    buffer_->BufAddModifyCB(modifiedCB, this);

    // Set the requested hardware tab distance and useTabs in the text buffer
    buffer_->BufSetTabDistance(Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE), true);
    buffer_->BufSetUseTabs(Preferences::GetPrefInsertTabs());

    static int n = 0;
    area->setObjectName(tr("TextArea_%1").arg(n++));
    area->setBacklightCharTypes(backlightCharTypes_);
    splitter_->addWidget(area);
}

/**
 * @brief DocumentWidget::~DocumentWidget
 */
DocumentWidget::~DocumentWidget() noexcept {

    // NOTE(eteran): we probably don't need to be doing so much work here
    // a lot (almost all?) of this will get cleaned up as a side effect of
    // deletion anyway. But we'll save that refactor for another day

    // first delete all of the text area's so that they can properly
    // remove themselves from the buffer's callbacks
    const std::vector<TextArea *> textAreas = textPanes();
    qDeleteAll(textAreas);

    // And delete the rangeset table too for the same reasons
    rangesetTable_ = nullptr;

    // NOTE(eteran): there used to be some logic about syncronizing the multi-file
    //               replace dialog. It was complex and error prone. Simpler to
    //               just make the multi-file replace dialog modal and avoid the
    //               issue all together

    // Free syntax highlighting patterns, if any. w/o redisplaying
    FreeHighlightingDataEx();

    buffer_->BufRemoveModifyCB(modifiedCB, this);
    buffer_->BufRemoveModifyCB(SyntaxHighlightModifyCBEx, this);

    delete buffer_;
}

/**
 * @brief DocumentWidget::createTextArea
 * @param buffer
 * @return
 */
TextArea *DocumentWidget::createTextArea(TextBuffer *buffer) {

    auto area = new TextArea(this,
                             buffer,
                             Preferences::GetPrefDefaultFont());

    area->setCursorVPadding(Preferences::GetVerticalAutoScroll());
    area->setEmulateTabs(Preferences::GetPrefEmTabDist(PLAIN_LANGUAGE_MODE));

    const QColor textFgPix   = X11Colors::fromString(Preferences::GetPrefColorName(TEXT_FG_COLOR));
    const QColor textBgPix   = X11Colors::fromString(Preferences::GetPrefColorName(TEXT_BG_COLOR));
    const QColor selectFgPix = X11Colors::fromString(Preferences::GetPrefColorName(SELECT_FG_COLOR));
    const QColor selectBgPix = X11Colors::fromString(Preferences::GetPrefColorName(SELECT_BG_COLOR));
    const QColor hiliteFgPix = X11Colors::fromString(Preferences::GetPrefColorName(HILITE_FG_COLOR));
    const QColor hiliteBgPix = X11Colors::fromString(Preferences::GetPrefColorName(HILITE_BG_COLOR));
    const QColor lineNoFgPix = X11Colors::fromString(Preferences::GetPrefColorName(LINENO_FG_COLOR));
    const QColor cursorFgPix = X11Colors::fromString(Preferences::GetPrefColorName(CURSOR_FG_COLOR));

    area->TextDSetColors(
        textFgPix,
        textBgPix,
        selectFgPix,
        selectBgPix,
        hiliteFgPix,
        hiliteBgPix,
        lineNoFgPix,
        cursorFgPix);

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

    connect(area, &TextArea::customContextMenuRequested, this, [this](const QPoint &pos) {
        if(contextMenu_) {
            contextMenu_->exec(pos);
        }
    });

    return area;
}

/*
** Change the window appearance and the window data structure to show
** that the file it contains has been modified
*/
void DocumentWidget::SetWindowModified(bool modified) {
    if(auto win = MainWindow::fromDocument(this)) {

		if (!fileChanged_ && modified) {
			win->ui.action_Close->setEnabled(true);
			fileChanged_ = true;
		} else if (fileChanged_ && !modified) {
			fileChanged_ = false;
		}

        win->UpdateWindowTitle(this);
        RefreshTabState();
	}
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void DocumentWidget::RefreshTabState() {

    if(auto w = MainWindow::fromDocument(this)) {
        QTabWidget *tabWidget = w->tabWidget();
        int index = tabWidget->indexOf(this);

        QString labelString;

        static const auto saveIcon = QIcon::fromTheme(QLatin1String("document-save"));
        if(!saveIcon.isNull()) {
            tabWidget->setTabIcon(index, fileChanged_ ? saveIcon : QIcon());
            labelString = filename_;
        } else {
            /* Set tab label to document's filename. Position of
            "*" (modified) will change per label alignment setting */
            QStyle *const style = tabWidget->tabBar()->style();
            const int alignment = style->styleHint(QStyle::SH_TabBar_Alignment);

            if (alignment != Qt::AlignRight) {
                labelString = tr("%1%2").arg(fileChanged_ ? tr("*") : QString(), filename_);
            } else {
                labelString = tr("%2%1").arg(fileChanged_ ? tr("*") : QString(), filename_);
            }
        }

        QString tipString;
        if (Preferences::GetPrefShowPathInWindowsMenu() && filenameSet_) {
            tipString = tr("%1 - %2").arg(labelString, path_);
        } else {
            tipString = labelString;
        }

        tabWidget->setTabText(index, labelString);
        tabWidget->setTabToolTip(index, tipString);
    }
}

/*
** Apply language mode matching criteria and set languageMode_ to
** the appropriate mode for the current file, trigger language mode
** specific actions (turn on/off highlighting), and update the language
** mode menu item.  If forceNewDefaults is true, re-establish default
** settings for language-specific preferences regardless of whether
** they were previously set by the user.
*/
void DocumentWidget::DetermineLanguageMode(bool forceNewDefaults) {
	SetLanguageMode(matchLanguageMode(), forceNewDefaults);
}

/**
 * @brief DocumentWidget::GetLanguageMode
 * @return
 */
size_t DocumentWidget::GetLanguageMode() const {
    return languageMode_;
}

/*
** Set the language mode for the window, update the menu and trigger language
** mode specific actions (turn on/off highlighting).  If forceNewDefaults is
** true, re-establish default settings for language-specific preferences
** regardless of whether they were previously set by the user.
*/
void DocumentWidget::SetLanguageMode(size_t mode, bool forceNewDefaults) {

    // Do mode-specific actions
    reapplyLanguageMode(mode, forceNewDefaults);

	// Select the correct language mode in the sub-menu
    if (IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            QList<QAction *> languages = win->ui.action_Language_Mode->menu()->actions();

            auto it = std::find_if(languages.begin(), languages.end(), [mode](QAction *action) {
                return action->data().value<qulonglong>() == mode;
            });

            if(it != languages.end()) {
                (*it)->setChecked(true);
            }
        }
	}
}

/**
 * @brief DocumentWidget::action_Set_Language_Mode
 * @param languageMode
 * @param forceNewDefaults
 */
void DocumentWidget::action_Set_Language_Mode(const QString &languageMode, bool forceNewDefaults) {
    emit_event("set_language_mode", languageMode);
    SetLanguageMode(Preferences::FindLanguageMode(languageMode), forceNewDefaults);
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
    const std::string first200 = buffer_->BufGetRangeEx(TextCursor(), TextCursor(200));
    if(first200.empty()) {
        return PLAIN_LANGUAGE_MODE;
    }

    for (size_t i = 0; i < Preferences::LanguageModes.size(); i++) {
        if (!Preferences::LanguageModes[i].recognitionExpr.isNull()) {

            Search::Result searchResult;

            const bool result = Search::SearchString(
                        first200,
                        Preferences::LanguageModes[i].recognitionExpr,
                        Direction::Forward,
                        SearchType::Regex,
                        WrapMode::NoWrap,
                        0,
                        &searchResult,
                        QString());

            if (result) {
                return i;
			}
		}
	}

	/* Look at file extension ("@@/" starts a ClearCase version extended path,
	   which gets appended after the file extension, and therefore must be
	   stripped off to recognize the extension to make ClearCase users happy) */
    int fileNameLen = filename_.size();

    const int versionExtendedPathIndex = ClearCase::GetVersionExtendedPathIndex(filename_);
	if (versionExtendedPathIndex != -1) {
		fileNameLen = versionExtendedPathIndex;
	}

    const QStringRef file = filename_.midRef(0, fileNameLen);

    for (size_t i = 0; i < Preferences::LanguageModes.size(); i++) {
        Q_FOREACH(const QString &ext, Preferences::LanguageModes[i].extensions) {
            if(file.endsWith(ext)) {
                return i;
            }
		}
	}

	// no appropriate mode was found
	return PLAIN_LANGUAGE_MODE;
}

/*
** Update the optional statistics line.
*/
void DocumentWidget::UpdateStatsLine(TextArea *area) {

    if (!IsTopDocument()) {
        return;
    }

    /* This routine is called for each character typed, so its performance
       affects overall editor perfomance.  Only update if the line is on. */
    if (!showStats_) {
        return;
    }

    if(auto win = MainWindow::fromDocument(this)) {
        if(!area) {
            area = win->lastFocus_;
            if(!area) {
                area = firstPane();
            }
		}

		// Compose the string to display. If line # isn't available, leave it off
        const TextCursor pos = area->TextGetCursorPos();

		QString format;
		switch(fileFormat_) {
        case FileFormats::Dos:
			format = tr(" DOS");
			break;
        case FileFormats::Mac:
			format = tr(" Mac");
			break;
		default:
			format = QString();
			break;
		}

		QString string;
		QString slinecol;
        int64_t line;
        int64_t colNum;
        const int64_t length = buffer_->BufGetLength();

		if (!area->TextDPosToLineAndCol(pos, &line, &colNum)) {
            string   = tr("%1%2%3 %4 bytes").arg(path_, filename_, format).arg(length);
			slinecol = tr("L: ---  C: ---");
		} else {
			slinecol = tr("L: %1  C: %2").arg(line).arg(colNum);
			if (win->showLineNumbers_) {
                string = tr("%1%2%3 byte %4 of %5").arg(path_, filename_, format).arg(to_integer(pos)).arg(length);
			} else {
                string = tr("%1%2%3 %4 bytes").arg(path_, filename_, format).arg(length);
			}
		}

		// Update the line/column number
		ui.labelStats->setText(slinecol);

		// Don't clobber the line if there's a special message being displayed
        if(!modeMessageDisplayed()) {
			ui.labelFileAndSize->setText(string);
		}
	}
}

/**
 * @brief DocumentWidget::movedCallback
 * @param area
 */
void DocumentWidget::movedCallback(TextArea *area) {

	if (ignoreModify_) {
		return;
	}

	// update line and column nubers in statistics line
	UpdateStatsLine(area);

	// Check the character before the cursor for matchable characters
    FlashMatchingEx(area);

	// Check for changes to read-only status and/or file modifications
    CheckForChangesToFileEx();

    if(QTimer *const blinkTimer = area->cursorBlinkTimer()) {
        if(!blinkTimer->isActive()) {
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
	Q_UNUSED(area);
	// don't record all of the intermediate drag steps for undo
	ignoreModify_ = true;
}

/*
** Keep the marks in the windows book-mark table up to date across
** changes to the underlying buffer
*/
void DocumentWidget::UpdateMarkTable(TextCursor pos, int64_t nInserted, int64_t nDeleted) {

    for (size_t i = 0; i < nMarks_; ++i) {
        maintainSelection(markTable_[i].sel,       pos, nInserted, nDeleted);
        maintainPosition (markTable_[i].cursorPos, pos, nInserted, nDeleted);
    }
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

    Q_UNUSED(nRestyled);

    const bool selected = buffer_->primary.selected;

    // update the table of bookmarks
    if (!ignoreModify_) {
        UpdateMarkTable(pos, nInserted, nDeleted);
    }

    if(auto win = MainWindow::fromDocument(this)) {
        // Check and dim/undim selection related menu items
        if ((win->wasSelected_ && !selected) || (!win->wasSelected_ && selected)) {
            win->wasSelected_ = selected;

            /* do not refresh shell-level items (window, menu-bar etc)
               when motifying non-top document */
            if (IsTopDocument()) {
                win->selectionChanged(selected);

                DimSelectionDepUserMenuItems(selected);

                if(auto dialog = win->dialogReplace_) {
                    dialog->UpdateReplaceActionButtons();
                }
            }
        }

        /* When the program needs to make a change to a text area without without
           recording it for undo or marking file as changed it sets ignoreModify */
        if (ignoreModify_ || (nDeleted == 0 && nInserted == 0)) {
            return;
        }

        // Make sure line number display is sufficient for new data
        win->updateLineNumDisp();

        /* Save information for undoing this operation (this call also counts
           characters and editing operations for triggering autosave */
        SaveUndoInformation(pos, nInserted, nDeleted, deletedText);

        // Trigger automatic backup if operation or character limits reached
        if (autoSave_ && (autoSaveCharCount_ > AUTOSAVE_CHAR_LIMIT || autoSaveOpCount_ > AUTOSAVE_OP_LIMIT)) {
            WriteBackupFile();
            autoSaveCharCount_ = 0;
            autoSaveOpCount_   = 0;
        }

        // Indicate that the window has now been modified
        SetWindowModified(true);

        // Update # of bytes, and line and col statistics
        UpdateStatsLine(nullptr);

        // Check if external changes have been made to file and warn user
        CheckForChangesToFileEx();
    }
}

/**
 * @brief DocumentWidget::dragEndCallback
 * @param area
 * @param data
 */
void DocumentWidget::dragEndCallback(TextArea *area, const DragEndEvent *data) {

    // NOTE(eteran): so, it's a minor shame here that we don't use the area
    // variable. The **buffer** is what reports the modifications, and that is
    // lower level than a TextArea. So it doesn't pass that along to the other
    // versions of the modified callback. The code will eventually call
    // "UpdateStatsLine(nullptr)", which will force it to fall back on the
    // "lastFocus_" variable. Which **probably** points to the correct widget.
    Q_UNUSED(area);

	// restore recording of undo information
	ignoreModify_ = false;

	// Do nothing if drag operation was canceled
    if (data->nCharsInserted == 0) {
		return;
    }

    /* Save information for undoing this operation not saved while undo
     * recording was off */
    modifiedCallback(data->startPos, data->nCharsInserted, data->nCharsDeleted, 0, data->deletedText);
}

/**
 * @brief DocumentWidget::smartIndentCallback
 * @param area
 * @param data
 */
void DocumentWidget::smartIndentCallback(TextArea *area, SmartIndentEvent *data) {

    Q_UNUSED(area);

	if (!smartIndentData_) {
		return;
	}

	switch(data->reason) {
	case CHAR_TYPED:
        executeModMacroEx(data);
		break;
	case NEWLINE_INDENT_NEEDED:
        executeNewlineMacroEx(data);
        break;
	}
}

/*
** raise the document and its shell window and optionally focus.
*/
void DocumentWidget::RaiseFocusDocumentWindow(bool focus) {
    RaiseDocument();
    if(auto win = MainWindow::fromDocument(this)) {

        if(!win->isMaximized()) {
            win->showNormal();
        }

        if(focus) {
            win->raise();
            win->activateWindow();            
        }
    }
}

/**
 * raise the document and its shell window and focus depending on pref.
 *
 * @brief DocumentWidget::RaiseDocumentWindow
 */
void DocumentWidget::RaiseDocumentWindow() {
    RaiseFocusDocumentWindow(Preferences::GetPrefFocusOnRaise());
}

/**
 * @brief DocumentWidget::documentRaised
 */
void DocumentWidget::documentRaised() {
    // Turn on syntax highlight that might have been deferred.
    if (highlightSyntax_ && !highlightData_) {
        StartHighlightingEx(/*warn=*/false);
    }

    /* now refresh this state/info. RefreshWindowStates()
       has a lot of work to do, so we update the screen first so
       the document appears to switch swiftly. */
    RefreshWindowStates();

    /* Make sure that the "In Selection" button tracks the presence of a
       selection and that the this inherits the proper search scope. */
    if(auto win = MainWindow::fromDocument(this)) {
        if(auto dialog = win->dialogReplace_) {
            dialog->UpdateReplaceActionButtons();
        }
    }
}

void DocumentWidget::RaiseDocument() {
    if(auto win = MainWindow::fromDocument(this)) {
        // NOTE(eteran): indirectly triggers a call to documentRaised()
        win->tabWidget()->setCurrentWidget(this);
    }
}

/*
** Change the language mode to the one indexed by "mode", reseting word
** delimiters, syntax highlighting and other mode specific parameters
*/
void DocumentWidget::reapplyLanguageMode(size_t mode, bool forceDefaults) {

    const bool topDocument = IsTopDocument();

    if(auto win = MainWindow::fromDocument(this)) {
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
            Tags::DeleteTagsFileEx(Preferences::LanguageModes[oldMode].defTipsFile, Tags::SearchMode::TIP, false);
        }

        // Set delimiters for all text widgets
        QString delimiters;
        if (mode == PLAIN_LANGUAGE_MODE || Preferences::LanguageModes[mode].delimiters.isNull()) {
            delimiters = Preferences::GetPrefDelimiters();
        } else {
            delimiters = Preferences::LanguageModes[mode].delimiters;
        }

        const std::vector<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            area->setWordDelimiters(delimiters.toStdString());
        }

        /* Decide on desired values for language-specific parameters.  If a
           parameter was set to its default value, set it to the new default,
           otherwise, leave it alone */
        const bool wrapModeIsDef = (wrapMode_ == Preferences::GetPrefWrap(oldMode));
        const bool tabDistIsDef  = (buffer_->BufGetTabDistance() == Preferences::GetPrefTabDist(oldMode));

        const int oldEmTabDist = textAreas[0]->getEmulateTabs();
        const QString oldlanguageModeName = Preferences::LanguageModeName(oldMode);

        const bool emTabDistIsDef     = oldEmTabDist == Preferences::GetPrefEmTabDist(oldMode);
        const bool indentStyleIsDef   = indentStyle_ == Preferences::GetPrefAutoIndent(oldMode)   || (Preferences::GetPrefAutoIndent(oldMode) == IndentStyle::Smart && indentStyle_ == IndentStyle::Auto && !SmartIndent::SmartIndentMacrosAvailable(Preferences::LanguageModeName(oldMode)));
        const bool highlightIsDef     = highlightSyntax_ == Preferences::GetPrefHighlightSyntax() || (Preferences::GetPrefHighlightSyntax() && Highlight::FindPatternSet(!oldlanguageModeName.isNull() ? oldlanguageModeName : QLatin1String("")) == nullptr);
        const WrapStyle wrapMode      = wrapModeIsDef                                || forceDefaults ? Preferences::GetPrefWrap(mode)        : wrapMode_;
        const int tabDist             = tabDistIsDef                                 || forceDefaults ? Preferences::GetPrefTabDist(mode)     : buffer_->BufGetTabDistance();
        const int emTabDist           = emTabDistIsDef                               || forceDefaults ? Preferences::GetPrefEmTabDist(mode)   : oldEmTabDist;
        IndentStyle indentStyle       = indentStyleIsDef                             || forceDefaults ? Preferences::GetPrefAutoIndent(mode)  : indentStyle_;
        bool highlight                = highlightIsDef                               || forceDefaults ? Preferences::GetPrefHighlightSyntax() : highlightSyntax_;

        /* Dim/undim smart-indent and highlighting menu items depending on
           whether patterns/macros are available */
        QString languageModeName   = Preferences::LanguageModeName(mode);
        bool haveHighlightPatterns = Highlight::FindPatternSet(languageModeName);
        bool haveSmartIndentMacros = SmartIndent::SmartIndentMacrosAvailable(Preferences::LanguageModeName(mode));

        if (topDocument) {
            win->ui.action_Highlight_Syntax->setEnabled(haveHighlightPatterns);
            win->ui.action_Indent_Smart    ->setEnabled(haveSmartIndentMacros);
        }

        // Turn off requested options which are not available
        highlight = haveHighlightPatterns && highlight;
        if (indentStyle == IndentStyle::Smart && !haveSmartIndentMacros) {
            indentStyle = IndentStyle::Auto;
        }

        // Change highlighting
        highlightSyntax_ = highlight;

        no_signals(win->ui.action_Highlight_Syntax)->setChecked(highlight);

        StopHighlightingEx();

        // we defer highlighting to RaiseDocument() if doc is hidden
        if (topDocument && highlight) {
            StartHighlightingEx(/*warn=*/false);
        }

        // Force a change of smart indent macros (SetAutoIndent will re-start)
        if (indentStyle_ == IndentStyle::Smart) {
            EndSmartIndent();
            indentStyle_ = IndentStyle::Auto;
        }

        // set requested wrap, indent, and tabs
        SetAutoWrap(wrapMode);
        SetAutoIndent(indentStyle);
        SetTabDist(tabDist);
        SetEmTabDist(emTabDist);

        // Load calltips files for new mode
        if (mode != PLAIN_LANGUAGE_MODE && !Preferences::LanguageModes[mode].defTipsFile.isNull()) {
            Tags::AddTagsFileEx(Preferences::LanguageModes[mode].defTipsFile, Tags::SearchMode::TIP);
        }

        // Add/remove language specific menu items
        win->UpdateUserMenus(this);
    }
}

/**
 * @brief DocumentWidget::SetTabDist
 * @param tabDist
 */
void DocumentWidget::SetTabDist(int tabDist) {

    emit_event("set_tab_dist", QString::number(tabDist));

    if (buffer_->BufGetTabDist() != tabDist) {
        TextCursor saveCursorPositions[MAX_PANES + 1];
        int64_t saveVScrollPositions[MAX_PANES + 1];
        int saveHScrollPositions[MAX_PANES + 1];

        ignoreModify_ = true;

        const std::vector<TextArea *> textAreas = textPanes();
        const size_t paneCount = textAreas.size();
		
        for(size_t paneIndex = 0; paneIndex < paneCount; ++paneIndex) {
            TextArea *area = textAreas[paneIndex];

            area->TextDGetScroll(&saveVScrollPositions[paneIndex], &saveHScrollPositions[paneIndex]);
            saveCursorPositions[paneIndex] = area->TextGetCursorPos();
            area->setModifyingTabDist(1);
        }

        buffer_->BufSetTabDistance(tabDist, true);

        for(size_t paneIndex = 0; paneIndex < paneCount; ++paneIndex) {
            TextArea *area = textAreas[paneIndex];

            area->setModifyingTabDist(0);
            area->TextSetCursorPos(saveCursorPositions[paneIndex]);
            area->TextDSetScroll(saveVScrollPositions[paneIndex], saveHScrollPositions[paneIndex]);
        }

        ignoreModify_ = false;
    }
}

/**
 * @brief DocumentWidget::SetEmTabDist
 * @param emTabDist
 */
void DocumentWidget::SetEmTabDist(int emTabDist) {

    emit_event("set_em_tab_dist", QString::number(emTabDist));

    for(TextArea *area : textPanes()) {
        area->setEmulateTabs(emTabDist);
    }
}

/*
** Set autoindent state to one of  NO_AUTO_INDENT, AUTO_INDENT, or SMART_INDENT.
*/
void DocumentWidget::SetAutoIndent(IndentStyle state) {

    const bool autoIndent  = (state == IndentStyle::Auto);
    const bool smartIndent = (state == IndentStyle::Smart);

    if (indentStyle_ == IndentStyle::Smart && !smartIndent) {
        EndSmartIndent();
    } else if (smartIndent && indentStyle_ != IndentStyle::Smart) {
        BeginSmartIndentEx(/*warn=*/true);
    }

    indentStyle_ = state;

    for(TextArea *area : textPanes()) {
        area->setAutoIndent(autoIndent);
        area->setSmartIndent(smartIndent);
    }

    if (IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Indent_Smart)->setChecked(smartIndent);
            no_signals(win->ui.action_Indent_On)->setChecked(autoIndent);
            no_signals(win->ui.action_Indent_Off)->setChecked(state == IndentStyle::None);
        }
    }
}

/*
** Select auto-wrap mode, one of None, Newline, or Continuous
*/
void DocumentWidget::SetAutoWrap(WrapStyle state) {

    emit_event("set_wrap_text", to_string(state));

    const bool autoWrap = (state == WrapStyle::Newline);
    const bool contWrap = (state == WrapStyle::Continuous);

    for(TextArea *area : textPanes()) {
        area->setAutoWrap(autoWrap);
        area->setContinuousWrap(contWrap);
    }

    wrapMode_ = state;

    if (IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Wrap_Auto_Newline)->setChecked(autoWrap);
            no_signals(win->ui.action_Wrap_Continuous)->setChecked(contWrap);
            no_signals(win->ui.action_Wrap_None)->setChecked(state == WrapStyle::None);
        }
    }
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

    for(int i = 0; i < count; ++i) {
        if(auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
            list.push_back(area);
        }
    }

    return list;
}

/**
 * @brief DocumentWidget::IsTopDocument
 * @return
 */
bool DocumentWidget::IsTopDocument() const {
    if(auto window = MainWindow::fromDocument(this)) {
        return window->currentDocument() == this;
    }

    return false;
}

/**
 * @brief DocumentWidget::getWindowsMenuEntry
 * @return
 */
QString DocumentWidget::getWindowsMenuEntry() const {

    auto fullTitle = tr("%1%2").arg(filename_, fileChanged_ ? tr("*") : QString());

    if (Preferences::GetPrefShowPathInWindowsMenu() && filenameSet_) {
        fullTitle.append(tr(" - "));
        fullTitle.append(path_);
    }

    return fullTitle;
}

/*
** Turn off syntax highlighting and free style buffer, compiled patterns, and
** related data.
*/
void DocumentWidget::StopHighlightingEx() {
    if (!highlightData_) {
        return;
    }

    // Free and remove the highlight data from the window
    highlightData_ = nullptr;

    /* Remove and detach style buffer and style table from all text
       display(s) of window, and redisplay without highlighting */
    for(TextArea *area : textPanes()) {
        area->RemoveWidgetHighlightEx();
    }
}

/**
 * @brief DocumentWidget::firstPane
 * @return
 */
TextArea *DocumentWidget::firstPane() const {

    for(int i = 0; i < splitter_->count(); ++i) {
        if(auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
            return area;
        }
    }

    return nullptr;
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns nullptr when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching.
*/
QString DocumentWidget::GetWindowDelimiters() const {
    if (languageMode_ == PLAIN_LANGUAGE_MODE)
        return QString();
    else
        return Preferences::LanguageModes[languageMode_].delimiters;
}

/*
** Dim/undim user programmable menu items which depend on there being
** a selection in their associated window.
*/
void DocumentWidget::DimSelectionDepUserMenuItems(bool enabled) {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = MainWindow::fromDocument(this)) {
        dimSelDepItemsInMenu(win->ui.menu_Shell, ShellMenuData, enabled);
        dimSelDepItemsInMenu(win->ui.menu_Macro, MacroMenuData, enabled);
        dimSelDepItemsInMenu(contextMenu_,       BGMenuData,    enabled);

    }
}

void DocumentWidget::dimSelDepItemsInMenu(QMenu *menuPane, const gsl::span<MenuData> &menuList, bool enabled) {

    if(menuPane) {
        const QList<QAction *> actions = menuPane->actions();
        for(QAction *action : actions) {
            if(QMenu *subMenu = action->menu()) {
                dimSelDepItemsInMenu(subMenu, menuList, enabled);
            } else {
                int index = action->data().toInt();
                if (index < 0 || index >= menuList.size()) {
                    return;
                }

                if (menuList[index].item.input == FROM_SELECTION) {
                    action->setEnabled(enabled);
                }
            }
        }
    }
}

/*
** SaveUndoInformation stores away the changes made to the text buffer.  As a
** side effect, it also increments the autoSave operation and character counts
** since it needs to do the classification anyhow.
**
** Note: This routine must be kept efficient.  It is called for every
**       character typed.
*/
void DocumentWidget::SaveUndoInformation(TextCursor pos, int64_t nInserted, int64_t nDeleted, view::string_view deletedText) {

    const int isUndo = (!undo_.empty() && undo_.front().inUndo);
    const int isRedo = (!redo_.empty() && redo_.front().inUndo);

    /* redo operations become invalid once the user begins typing or does
       other editing.  If this is not a redo or undo operation and a redo
       list still exists, clear it and dim the redo menu item */
    if (!(isUndo || isRedo) && !redo_.empty()) {
        ClearRedoList();
    }

    /* figure out what kind of editing operation this is, and recall
       what the last one was */
    const UndoTypes newType = determineUndoType(nInserted, nDeleted);
    if (newType == UNDO_NOOP) {
        return;
    }

    UndoInfo *const currentUndo = undo_.empty() ? nullptr : &undo_.front();

    const UndoTypes oldType = (!currentUndo || isUndo) ? UNDO_NOOP : currentUndo->type;

    /*
    ** Check for continuations of single character operations.  These are
    ** accumulated so a whole insertion or deletion can be undone, rather
    ** than just the last character that the user typed.  If the this
    ** is currently in an unmodified state, don't accumulate operations
    ** across the save, so the user can undo back to the unmodified state.
    */
    if (fileChanged_) {

        // normal sequential character insertion
        if (((oldType == ONE_CHAR_INSERT || oldType == ONE_CHAR_REPLACE) && newType == ONE_CHAR_INSERT) && (pos == currentUndo->endPos)) {
            currentUndo->endPos++;
            autoSaveCharCount_++;
            return;
        }

        // overstrike mode replacement
        if ((oldType == ONE_CHAR_REPLACE && newType == ONE_CHAR_REPLACE) && (pos == currentUndo->endPos)) {
            appendDeletedText(deletedText, nDeleted, Direction::Forward);
            currentUndo->endPos++;
            autoSaveCharCount_++;
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
            currentUndo->startPos--;
            currentUndo->endPos--;
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
    autoSaveOpCount_++;

    /* if the this is currently unmodified, remove the previous
       restoresToSaved marker, and set it on this record */
    if (!fileChanged_) {
        undo.restoresToSaved = true;

        for(UndoInfo &u : undo_) {
            u.restoresToSaved = false;
        }

        for(UndoInfo &u : redo_) {
            u.restoresToSaved = false;
        }
    }

    /* Add the new record to the undo list  unless SaveUndoInfo is
       saving information generated by an Undo operation itself, in
       which case, add the new record to the redo list. */
    if (isUndo) {
        addRedoItem(std::move(undo));
    } else {
        addUndoItem(std::move(undo));
    }
}

/*
** ClearUndoList, ClearRedoList
**
** Functions for clearing all of the information off of the undo or redo
** lists and adjusting the edit menu accordingly
*/
void DocumentWidget::ClearUndoList() {
	while (!undo_.empty()) {
		removeUndoItem();
	}
}

void DocumentWidget::ClearRedoList() {
    while (!redo_.empty()) {
        removeRedoItem();
    }
}

/*
** Add deleted text to the beginning or end
** of the text saved for undoing the last operation.  This routine is intended
** for continuing of a string of one character deletes or replaces, but will
** work with more than one character.
*/
void DocumentWidget::appendDeletedText(view::string_view deletedText, int64_t deletedLen, Direction direction) {
    UndoInfo &undo = undo_.front();

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

    undo_.emplace_front(std::move(undo));

    // Trim the list if it exceeds any of the limits
    if (undo_.size() > UNDO_OP_LIMIT) {
        trimUndoList(UNDO_OP_TRIMTO);
    }

    if(auto window = MainWindow::fromDocument(this)) {
        window->undoAvailable(!undo_.empty());
    }
}

/*
** Add an item (already allocated by the caller) to the this's redo list.
*/
void DocumentWidget::addRedoItem(UndoInfo &&redo) {

    redo_.emplace_front(std::move(redo));

    if(auto window = MainWindow::fromDocument(this)) {
        window->redoAvailable(!redo_.empty());
    }
}

/*
** Pop the current undo record from the undo list
*/
void DocumentWidget::removeUndoItem() {

    if (undo_.empty()) {
        return;
    }

    undo_.pop_front();

    if(auto window = MainWindow::fromDocument(this)) {
        window->undoAvailable(!undo_.empty());
    }
}

/*
** Pop the current redo record from the redo list
*/
void DocumentWidget::removeRedoItem() {

    if (redo_.empty()) {
        return;
    }

    redo_.pop_front();

    if(auto window = MainWindow::fromDocument(this)) {
        window->redoAvailable(!redo_.empty());
    }
}


/*
** Trim records off of the END of the undo list to reduce it to length
** maxLength
*/
void DocumentWidget::trimUndoList(size_t maxLength) {

    if (undo_.empty()) {
        return;
    }

    auto it = undo_.begin();
    size_t i = 1;

    // Find last item on the list to leave intact
    while(it != undo_.end() && i < maxLength) {
        ++it;
        ++i;
    }

    // Trim off all subsequent entries
    undo_.erase(it, undo_.end());
}

void DocumentWidget::Undo() {

    if(auto win = MainWindow::fromDocument(this)) {

        if (undo_.empty()) {
            return;
        }

        UndoInfo &undo = undo_.front();

        /* BufReplaceEx will eventually call SaveUndoInformation.  This is mostly
           good because it makes accumulating redo operations easier, however
           SaveUndoInformation needs to know that it is being called in the context
           of an undo.  The inUndo field in the undo record indicates that this
           record is in the process of being undone. */
        undo.inUndo = true;

        // use the saved undo information to reverse changes
        buffer_->BufReplaceEx(undo.startPos, undo.endPos, undo.oldText);

        const auto restoredTextLength = static_cast<int64_t>(undo.oldText.size());
        if (!buffer_->primary.selected || Preferences::GetPrefUndoModifiesSelection()) {
            /* position the cursor in the focus pane after the changed text
               to show the user where the undo was done */
            if(QPointer<TextArea> area = win->lastFocus_) {
                area->TextSetCursorPos(undo.startPos + restoredTextLength);
            }
        }

        if (Preferences::GetPrefUndoModifiesSelection()) {
            if (restoredTextLength > 0) {
                buffer_->BufSelect(undo.startPos, undo.startPos + restoredTextLength);
            } else {
                buffer_->BufUnselect();
            }
        }

        if(QPointer<TextArea> area = win->lastFocus_) {
            MakeSelectionVisible(area);
        }

        /* restore the file's unmodified status if the file was unmodified
           when the change being undone was originally made.  Also, remove
           the backup file, since the text in the buffer is now identical to
           the original file */
        if (undo.restoresToSaved) {
            SetWindowModified(false);
            RemoveBackupFile();
        }

        // free the undo record and remove it from the chain
        removeUndoItem();
    }
}

void DocumentWidget::Redo() {

    if(auto win = MainWindow::fromDocument(this)) {

        if (redo_.empty()) {
            return;
        }

        UndoInfo &redo = redo_.front();

        /* BufReplaceEx will eventually call SaveUndoInformation.  To indicate
           to SaveUndoInformation that this is the context of a redo operation,
           we set the inUndo indicator in the redo record */
        redo.inUndo = true;

        // use the saved redo information to reverse changes
        buffer_->BufReplaceEx(redo.startPos, redo.endPos, redo.oldText);

        const auto restoredTextLength = static_cast<int64_t>(redo.oldText.size());
        if (!buffer_->primary.selected || Preferences::GetPrefUndoModifiesSelection()) {
            /* position the cursor in the focus pane after the changed text
               to show the user where the undo was done */
            if(QPointer<TextArea> area = win->lastFocus_) {
                area->TextSetCursorPos(redo.startPos + restoredTextLength);
            }
        }

        if (Preferences::GetPrefUndoModifiesSelection()) {

            if (restoredTextLength > 0) {
                buffer_->BufSelect(redo.startPos, redo.startPos + restoredTextLength);
            } else {
                buffer_->BufUnselect();
            }
        }

        if(QPointer<TextArea> area = win->lastFocus_) {
            MakeSelectionVisible(win->lastFocus_);
        }

        /* restore the file's unmodified status if the file was unmodified
           when the change being redone was originally made. Also, remove
           the backup file, since the text in the buffer is now identical to
           the original file */
        if (redo.restoresToSaved) {
            SetWindowModified(/*modified=*/false);
            RemoveBackupFile();
        }

        // remove the redo record from the chain and free it
        removeRedoItem();
    }
}

/*
** Check the read-only or locked status of the window and beep and return
** false if the window should not be written in.
*/
bool DocumentWidget::CheckReadOnly() const {
    if (lockReasons_.isAnyLocked()) {
        QApplication::beep();
        return true;
    }
    return false;
}

/*
** If the selection (or cursor position if there's no selection) is not
** fully shown, scroll to bring it in to view.  Note that as written,
** this won't work well with multi-line selections.  Modest re-write
** of the horizontal scrolling part would be quite easy to make it work
** well with rectangular selections.
*/
void DocumentWidget::MakeSelectionVisible(TextArea *area) {

	bool isRect;
    int horizOffset;
    TextCursor left;
    int64_t rectEnd;
    int64_t rectStart;
    TextCursor right;
    int64_t topLineNum;
    int leftX;
    int rightX;
    int y;

    const TextCursor topChar  = area->TextFirstVisiblePos();
    const TextCursor lastChar = area->TextLastVisiblePos();

    // find out where the selection is
    if (!buffer_->BufGetSelectionPos(&left, &right, &isRect, &rectStart, &rectEnd)) {
		left = right = area->TextGetCursorPos();
        isRect = false;
    }

    /* Check vertical positioning unless the selection is already shown or
       already covers the display.  If the end of the selection is below
       bottom, scroll it in to view until the end selection is scrollOffset
       lines from the bottom of the display or the start of the selection
       scrollOffset lines from the top.  Calculate a pleasing distance from the
       top or bottom of the window, to scroll the selection to (if scrolling is
       necessary), around 1/3 of the height of the window */
    if (!((left >= topChar && right <= lastChar) || (left <= topChar && right >= lastChar))) {

        const int rows = area->getRows();
        const int scrollOffset = rows / 3;

		area->TextDGetScroll(&topLineNum, &horizOffset);
        if (right > lastChar) {
            // End of sel. is below bottom of screen
            const int64_t leftLineNum   = topLineNum + area->TextDCountLines(topChar, left, false);
            const int64_t targetLineNum = topLineNum + scrollOffset;

            if (leftLineNum >= targetLineNum) {
                // Start of sel. is not between top & target
                int64_t linesToScroll = area->TextDCountLines(lastChar, right, false) + scrollOffset;
                if (leftLineNum - linesToScroll < targetLineNum) {
                    linesToScroll = leftLineNum - targetLineNum;
                }

                // Scroll start of selection to the target line
				area->TextDSetScroll(topLineNum + linesToScroll, horizOffset);
            }
        } else if (left < topChar) {
            // Start of sel. is above top of screen
            const int64_t lastLineNum   = topLineNum + rows;
            const int64_t rightLineNum  = lastLineNum - area->TextDCountLines(right, lastChar, false);
            const int64_t targetLineNum = lastLineNum - scrollOffset;

            if (rightLineNum <= targetLineNum) {
                // End of sel. is not between bottom & target
                int64_t linesToScroll = area->TextDCountLines(left, topChar, false) + scrollOffset;
                if (rightLineNum + linesToScroll > targetLineNum) {
                    linesToScroll = targetLineNum - rightLineNum;
                }

                // Scroll end of selection to the target line
				area->TextDSetScroll(topLineNum - linesToScroll, horizOffset);
            }
        }
    }

    /* If either end of the selection off screen horizontally, try to bring it
       in view, by making sure both end-points are visible.  Using only end
       points of a multi-line selection is not a great idea, and disaster for
       rectangular selections, so this part of the routine should be re-written
       if it is to be used much with either.  Note also that this is a second
       scrolling operation, causing the display to jump twice.  It's done after
       vertical scrolling to take advantage of TextDPosToXY which requires it's
       reqested position to be vertically on screen) */
	if (area->TextDPositionToXY(left, &leftX, &y) && area->TextDPositionToXY(right, &rightX, &y) && leftX <= rightX) {
		area->TextDGetScroll(&topLineNum, &horizOffset);

        const int margin   = area->getMarginWidth();
        const int width    = area->width();
        const int numWidth = area->getLineNumWidth();
        const int numLeft  = area->getLineNumLeft();

        if (leftX < margin + numLeft + numWidth) {
            horizOffset -= margin + numLeft + numWidth - leftX;
        } else if (rightX > width - margin) {
            horizOffset += rightX - (width - margin);
        }

		area->TextDSetScroll(topLineNum, horizOffset);
    }

    // make sure that the statistics line is up to date
	UpdateStatsLine(area);
}

/*
** Remove the backup file associated with this window
*/
void DocumentWidget::RemoveBackupFile() const {

    // Don't delete backup files when backups aren't activated.
    if (!autoSave_) {
        return;
    }

	QFile::remove(backupFileNameEx());
}

/*
** Generate the name of the backup file for this window from the filename
** and path in the window data structure & write into name
*/
QString DocumentWidget::backupFileNameEx() const {

    if (filenameSet_) {
        return tr("%1~%2").arg(path_, filename_);
    } else {
        return PrependHomeEx(tr("~%1").arg(filename_));
    }
}

/*
** Check if the file in the window was changed by an external source.
** and put up a warning dialog if it has.
*/
void DocumentWidget::CheckForChangesToFileEx() {

    // TODO(eteran): 2.0, this concept can probably be reworked in terms of QFileSystemWatcher
    static QPointer<DocumentWidget> lastCheckWindow;
    static qint64 lastCheckTime = 0;

    if (!filenameSet_) {
        return;
    }

    // If last check was very recent, don't impact performance
    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    if (this == lastCheckWindow && (timestamp - lastCheckTime) < MOD_CHECK_INTERVAL) {
        return;
    }

    lastCheckWindow = this;
    lastCheckTime   = timestamp;

    bool silent = false;

    auto win = MainWindow::fromDocument(this);

    /* Update the status, but don't pop up a dialog if we're called from a
     * place where the window might be iconic (e.g., from the replace dialog)
     * or on another desktop.
     */
    if (!IsTopDocument()) {
        silent = true;
    } else if(!win->isVisible()) {
        silent = true;
    }

    if(win) {
        // Get the file mode and modification time
        QString fullname = FullPath();

        struct stat statbuf;
        if (::stat(fullname.toUtf8().data(), &statbuf) != 0) {

            // Return if we've already warned the user or we can't warn him now
            if (fileMissing_ || silent) {
                return;
            }

            /* Can't stat the file --
             * maybe it's been deleted. The filename is now invalid
             */
            fileMissing_ = true;
            lastModTime_ = 1;
            dev_         = 0;
            ino_         = 0;

            /* Warn the user, if they like to be warned (Maybe this should be its
                own preference setting: GetPrefWarnFileDeleted()) */
            if (Preferences::GetPrefWarnFileMods()) {
                bool save = false;

                //  Set title, message body and button to match stat()'s error.
                switch (errno) {
                case ENOENT:
                    {
                        // A component of the path file_name does not exist.
                        int resp = QMessageBox::critical(
                                    this,
                                    tr("File not Found"),
                                    tr("File '%1' (or directory in its path) no longer exists.\n"
                                       "Another program may have deleted or moved it.").arg(filename_),
                                    QMessageBox::Save | QMessageBox::Cancel);
                        save = (resp == QMessageBox::Save);
                    }
                    break;
                case EACCES:
                    {
                        // Search permission denied for a path component. We add one to the response because Re-Save wouldn't really make sense here.
                        int resp = QMessageBox::critical(
                                    this,
                                    tr("Permission Denied"),
                                    tr("You no longer have access to file '%1'.\n"
                                       "Another program may have changed the permissions of one of its parent directories.").arg(filename_),
                                    QMessageBox::Save | QMessageBox::Cancel);
                        save = (resp == QMessageBox::Save);
                    }
                    break;
                default:
                    {
                        // Everything else. This hints at an internal error (eg. ENOTDIR) or at some bad state at the host.
                        int resp = QMessageBox::critical(
                                    this,
                                    tr("File not Accessible"),
                                    tr("Error while checking the status of file '%1':\n"
                                       "    '%2'\n"
                                       "Please make sure that no data is lost before closing this window.").arg(filename_, ErrorString(errno)),
                                    QMessageBox::Save | QMessageBox::Cancel);
                        save = (resp == QMessageBox::Save);
                    }
                    break;
                }

                if(save) {
                    SaveWindow();
                }
            }

            // A missing or (re-)saved file can't be read-only.
            // NOTE: A document without a file can be locked though.
            // Make sure that the window was not destroyed behind our back!
            lockReasons_.setPermLocked(false);
            win->UpdateWindowTitle(this);
            win->UpdateWindowReadOnly(this);
            return;
        }

        /* Check that the file's read-only status is still correct (but
           only if the file can still be opened successfully in read mode) */
        if (mode_ != statbuf.st_mode || uid_ != statbuf.st_uid || gid_ != statbuf.st_gid) {

            mode_ = statbuf.st_mode;
            uid_  = statbuf.st_uid;
            gid_  = statbuf.st_gid;

            FILE *fp = ::fopen(fullname.toUtf8().data(), "r");
            if (fp) {
                ::fclose(fp);

                const bool readOnly = ::access(fullname.toUtf8().data(), W_OK) != 0;

                if (lockReasons_.isPermLocked() != readOnly) {
                    lockReasons_.setPermLocked(readOnly);
                    win->UpdateWindowTitle(this);
                    win->UpdateWindowReadOnly(this);
                }
            }
        }

        /* Warn the user if the file has been modified, unless checking is
         * turned off or the user has already been warned. */
        if (!silent && ((lastModTime_ != 0 && lastModTime_ != statbuf.st_mtime) || fileMissing_)) {

            lastModTime_ = 0; // Inhibit further warnings
            fileMissing_ = false;
            if (!Preferences::GetPrefWarnFileMods()) {
                return;
            }

            if (Preferences::GetPrefWarnRealFileMods() && !cmpWinAgainstFile(fullname)) {
                // Contents hasn't changed. Update the modification time.
                lastModTime_ = statbuf.st_mtime;
                return;
            }

            QMessageBox messageBox(this);
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setWindowTitle(tr("File modified externally"));
            QPushButton *buttonReload = messageBox.addButton(tr("Reload"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);

            Q_UNUSED(buttonCancel);

            if (fileChanged_) {
                messageBox.setText(tr("%1 has been modified by another program.  Reload?\n\nWARNING: Reloading will discard changes made in this editing session!").arg(filename_));
            } else {
                messageBox.setText(tr("%1 has been modified by another program.  Reload?").arg(filename_));
            }

            messageBox.exec();
            if(messageBox.clickedButton() == buttonReload) {
                RevertToSaved();
            }
        }
    }
}

/**
 * @brief DocumentWidget::FullPath
 * @return
 */
QString DocumentWidget::FullPath() const {
    return tr("%1%2").arg(path_, filename_);
}

/**
 * @brief DocumentWidget::FileName
 * @return
 */
QString DocumentWidget::FileName() const {
    return filename_;
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
bool DocumentWidget::cmpWinAgainstFile(const QString &fileName) const {

    char pendingCR = '\0';

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        return true;
    }

    int64_t fileLen = file.size();

    // For UNIX/macOS files, we can do a quick check to see if the on disk file
    // has a different length, but for DOS files, it's not that simple...
    switch(fileFormat_) {
    case FileFormats::Unix:
    case FileFormats::Mac:
        if (fileLen != buffer_->BufGetLength()) {
            return true;
        }
        break;
    case FileFormats::Dos:
        // However, if a DOS file is smaller on disk, it's certainly different
        if (fileLen < buffer_->BufGetLength()) {
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
    MainWindow::AllWindowsBusyEx(tr("Comparing externally modified %1 ...").arg(filename_));

    // make sure that we unbusy the windows when we're done
    auto _ = gsl::finally([]() {
        MainWindow::AllWindowsUnbusyEx();
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
        nRead   += offset;

        // check for on-disk file format changes, but only for the first chunk
        if (bufPos == 0 && fileFormat_ != FormatOfFileEx(view::string_view(fileString, static_cast<size_t>(nRead)))) {
            MainWindow::AllWindowsUnbusyEx();
            return true;
        }

        switch(fileFormat_) {
        case FileFormats::Mac:
            ConvertFromMacFileString(fileString, nRead);
            break;
        case FileFormats::Dos:
            ConvertFromDosFileString(fileString, &nRead, &pendingCR);
            break;
        case FileFormats::Unix:
            break;
        }

        if (int rv = buffer_->BufCmpEx(bufPos, fileString, nRead)) {
            return rv;
        }

        bufPos += nRead;
        restLen = std::min(fileLen - filePos, PREFERRED_CMPBUF_LEN);
    }


    if (pendingCR) {
        if (int rv = buffer_->BufCmpEx(bufPos, pendingCR)) {
            return rv;
        }
        bufPos += 1;
    }

    if (bufPos != buffer_->BufGetLength()) {
        return true;
    }

    return false;
}

void DocumentWidget::RevertToSaved() {

    if(auto win = MainWindow::fromDocument(this)) {
        TextCursor insertPositions[MAX_PANES];
        int64_t topLines[MAX_PANES];
        int horizOffsets[MAX_PANES];
        int openFlags = 0;

        // Can't revert untitled windows
        if (!filenameSet_) {
            QMessageBox::warning(
                        this,
                        tr("Error"),
                        tr("Window '%1' was never saved, can't re-read").arg(filename_));
            return;
        }

        const std::vector<TextArea *> textAreas = textPanes();
        const size_t panesCount = textAreas.size();

        // save insert & scroll positions of all of the panes to restore later
        for (size_t i = 0; i < panesCount; i++) {
            TextArea *area = textAreas[i];
            insertPositions[i] = area->TextGetCursorPos();
            area->TextDGetScroll(&topLines[i], &horizOffsets[i]);
        }

        // re-read the file, update the window title if new file is different
        QString name = filename_;
        QString path = path_;

        RemoveBackupFile();
        ClearUndoList();
		openFlags |= lockReasons_.isUserLocked() ? EditFlags::PREF_READ_ONLY : 0;

        if (!doOpen(name, path, openFlags)) {
            /* This is a bit sketchy.  The only error in doOpen that irreperably
               damages the window is "too much binary data".  It should be
               pretty rare to be reverting something that was fine only to find
               that now it has too much binary data. */
            if (!fileMissing_) {
                CloseDocument();
            } else {
                // Treat it like an externally modified file
                lastModTime_ = 0;
                fileMissing_ = false;
            }
            return;
        }

        win->forceShowLineNumbers();
        win->UpdateWindowTitle(this);
        win->UpdateWindowReadOnly(this);

        // restore the insert and scroll positions of each pane
        for (size_t i = 0; i < panesCount; i++) {
            TextArea *area = textAreas[i];
            area->TextSetCursorPos(insertPositions[i]);
            area->TextDSetScroll(topLines[i], horizOffsets[i]);
        }
    }
}

/*
** Create a backup file for the current window.  The name for the backup file
** is generated using the name and path stored in the window and adding a
** tilde (~) on UNIX and underscore (_) on VMS to the beginning of the name.
*/
bool DocumentWidget::WriteBackupFile() {
    FILE *fp;

    // Generate a name for the autoSave file
    QString name = backupFileNameEx();

    // remove the old backup file. Well, this might fail - we'll notice later however.
	QFile::remove(name);

    /* open the file, set more restrictive permissions (using default
       permissions was somewhat of a security hole, because permissions were
       independent of those of the original file being edited */
    int fd = ::open(name.toUtf8().data(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd < 0 || (fp = fdopen(fd, "w")) == nullptr) {

        QMessageBox::warning(
                    this,
                    tr("Error writing Backup"),
                    tr("Unable to save backup for %1:\n%2\nAutomatic backup is now off").arg(filename_, ErrorString(errno)));

        autoSave_ = false;

        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Incremental_Backup)->setChecked(false);
        }
        return false;
    }

    // get the text buffer contents
    std::string fileString = buffer_->BufGetAllEx();

    // add a terminating newline if the file doesn't already have one
    // TODO(eteran): should this respect Preferences::GetPrefAppendLF() ?
    if (!fileString.empty() && fileString.back() != '\n') {
        fileString.append("\n");
    }

    auto _ = gsl::finally([fp] { ::fclose(fp); });

    // write out the file
	::fwrite(fileString.data(), 1, fileString.size(), fp);
    if (::ferror(fp)) {
        QMessageBox::critical(
                    this,
                    tr("Error saving Backup"),
                    tr("Error while saving backup for %1:\n%2\nAutomatic backup is now off").arg(filename_, ErrorString(errno)));

		QFile::remove(name);
        autoSave_ = false;
        return false;
    }

    return true;
}

bool DocumentWidget::SaveWindow() {

    // Try to ensure our information is up-to-date
    CheckForChangesToFileEx();

    /* Return success if the file is normal & unchanged or is a
        read-only file. */
    if ((!fileChanged_ && !fileMissing_ && lastModTime_ > 0) || lockReasons_.isAnyLockedIgnoringPerm()) {
        return true;
    }

    // Prompt for a filename if this is an Untitled window
    if (!filenameSet_) {
        return SaveWindowAs(QString(), false);
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
                              "or Revert to Saved to revert to the modified version.").arg(filename_));

        QPushButton *buttonContinue = messageBox.addButton(tr("Continue"), QMessageBox::AcceptRole);
        QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
        Q_UNUSED(buttonCancel);

        messageBox.exec();
        if(messageBox.clickedButton() != buttonContinue) {
            // Cancel and mark file as externally modified
            lastModTime_ = 0;
            fileMissing_ = false;
            return false;
        }
    }

    if (writeBckVersion()) {
        return false;
    }

    const bool stat = doSave();
    if (stat) {
        RemoveBackupFile();
    }

    return stat;
}

bool DocumentWidget::doSave() {
    struct stat statbuf;

    QString fullname = FullPath();

    /*  Check for root and warn him if he wants to write to a file with
        none of the write bits set.  */
    if ((getuid() == 0) && (::stat(fullname.toUtf8().data(), &statbuf) == 0) && !(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
        int result = QMessageBox::warning(
                    this,
                    tr("Writing Read-only File"),
                    tr("File '%1' is marked as read-only.\nDo you want to save anyway?").arg(filename_),
                    QMessageBox::Save | QMessageBox::Cancel);

        if (result != QMessageBox::Save) {
            return true;
        }
    }

    /* add a terminating newline if the file doesn't already have one for
       Unix utilities which get confused otherwise
       NOTE: this must be done _before_ we create/open the file, because the
             (potential) buffer modification can trigger a check for file
             changes. If the file is created for the first time, it has
             zero size on disk, and the check would falsely conclude that the
             file has changed on disk, and would pop up a warning dialog */
    if (!buffer_->BufIsEmpty() && buffer_->BufGetCharacter(TextCursor(buffer_->BufGetLength() - 1)) != '\n' && Preferences::GetPrefAppendLF()) {
        buffer_->BufAppendEx('\n');
    }

    // open the file
    FILE *fp = ::fopen(fullname.toUtf8().data(), "wb");
    if(!fp) {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Error saving File"));
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setText(tr("Unable to save %1:\n%2\n\nSave as a new file?").arg(filename_, ErrorString(errno)));

        QPushButton *buttonSaveAs = messageBox.addButton(tr("Save As..."), QMessageBox::AcceptRole);
        QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
        Q_UNUSED(buttonCancel);

        messageBox.exec();
        if(messageBox.clickedButton() == buttonSaveAs) {
            return SaveWindowAs(QString(), /*addWrap=*/false);
        }

        return false;
    }

    auto _ = gsl::finally([fp] { ::fclose(fp); });

    // get the text buffer contents and its length
    std::string fileString = buffer_->BufGetAllEx();

    // If the file is to be saved in DOS or Macintosh format, reconvert
    switch(fileFormat_) {
    case FileFormats::Dos:
        ConvertToDosFileStringEx(fileString);
        break;
    case FileFormats::Mac:
        ConvertToMacFileStringEx(fileString);
        break;
    default:
        break;
    }

    // write to the file
    ::fwrite(fileString.data(), 1, fileString.size(), fp);

    if (::ferror(fp)) {
        QMessageBox::critical(this, tr("Error saving File"), tr("%1 not saved:\n%2").arg(filename_, ErrorString(errno)));
		QFile::remove(fullname);
        return false;
    }

    // success, file was written
    SetWindowModified(false);

    // update the modification time
    if (::stat(fullname.toUtf8().data(), &statbuf) == 0) {
        lastModTime_ = statbuf.st_mtime;
        fileMissing_ = false;
        dev_         = statbuf.st_dev;
        ino_         = statbuf.st_ino;
    } else {
        // This needs to produce an error message -- the file can't be accessed!
        lastModTime_ = 0;
        fileMissing_ = true;
        dev_         = 0;
        ino_         = 0;
    }

    return true;
}

/**
 * @brief DocumentWidget::SaveWindowAs
 * @param newName
 * @param addWrap
 * @return
 */
bool DocumentWidget::SaveWindowAs(const QString &newName, bool addWrap) {

    // NOTE(eteran): this seems a bit redundant to other code...
    if(auto win = MainWindow::fromDocument(this)) {

        QString fullname;

        if(newName.isNull()) {
            QFileDialog dialog(this, tr("Save File As"));
            dialog.setFileMode(QFileDialog::AnyFile);
            dialog.setAcceptMode(QFileDialog::AcceptSave);
            dialog.setDirectory(path_);
            dialog.setOptions(QFileDialog::DontUseNativeDialog);

            if(auto layout = qobject_cast<QGridLayout*>(dialog.layout())) {
                if(layout->rowCount() == 4 && layout->columnCount() == 3) {
                    auto boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);

                    auto unixCheck = new QRadioButton(tr("&Unix"));
                    auto dosCheck  = new QRadioButton(tr("D&OS"));
                    auto macCheck  = new QRadioButton(tr("&Macintosh"));

                    switch(fileFormat_) {
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
                    if(addWrap) {
                        wrapCheck->setChecked(true);
                    }

                    connect(wrapCheck, &QCheckBox::toggled, this, [wrapCheck, this](bool checked) {
                        if(checked) {
                            int ret = QMessageBox::information(
                                        this,
                                        tr("Add Wrap"),
                                        tr("This operation adds permanent line breaks to match the automatic wrapping done by the Continuous Wrap mode Preferences Option.\n\n"
                                           "*** This Option is Irreversable ***\n\n"
                                           "Once newlines are inserted, continuous wrapping will no longer work automatically on these lines"),
                                        QMessageBox::Ok | QMessageBox::Cancel);

                            if(ret != QMessageBox::Ok) {
                                wrapCheck->setChecked(false);
                            }
                        }
                    });


                    if (wrapMode_ == WrapStyle::Continuous) {
                        layout->addWidget(wrapCheck, row, 1, 1, 1);
                    }

                    if(dialog.exec()) {
                        if(dosCheck->isChecked()) {
                            fileFormat_ = FileFormats::Dos;
                        } else if(macCheck->isChecked()) {
                            fileFormat_ = FileFormats::Mac;
                        } else if(unixCheck->isChecked()) {
                            fileFormat_ = FileFormats::Unix;
                        }

                        addWrap = wrapCheck->isChecked();
						QStringList selectedFiles = dialog.selectedFiles();
                        fullname = selectedFiles[0];
                    } else {
                        return false;
                    }

                }
            }
        } else {
            fullname = newName;
        }

        // Add newlines if requested
        if (addWrap) {
            addWrapNewlines();
        }

        QString filename;
        QString pathname;

        if (!ParseFilenameEx(fullname, &filename, &pathname) != 0) {
            return false;
        }

        // If the requested file is this file, just save it and return
        if (filename_ == filename && path_ == pathname) {
            if (writeBckVersion()) {
                return false;
            }

            return doSave();
        }

        // If the file is open in another window, make user close it.
        if (DocumentWidget *otherWindow = MainWindow::FindWindowWithFile(filename, pathname)) {

            QMessageBox messageBox(this);
            messageBox.setWindowTitle(tr("File open"));
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setText(tr("%1 is open in another NEdit window").arg(filename));
            QPushButton *buttonCloseOther = messageBox.addButton(tr("Close Other Window"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel     = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonCloseOther);

            messageBox.exec();
            if(messageBox.clickedButton() == buttonCancel) {
                return false;
            }

            /*
             * after doing the dialog, check again whether the window still
             * exists in case the user somehow closed the window
             */
            if (otherWindow == MainWindow::FindWindowWithFile(filename, pathname)) {
                if (!otherWindow->CloseFileAndWindow(CloseMode::Prompt)) {
                    return false;
                }
            }
        }

        // Change the name of the file and save it under the new name
        RemoveBackupFile();
        filename_ = filename;
        path_     = pathname;
        mode_     = 0;
        uid_      = 0;
        gid_      = 0;

        lockReasons_.clear();
        const int retVal = doSave();
        win->UpdateWindowReadOnly(this);
        RefreshTabState();

        // Add the name to the convenience menu of previously opened files
        MainWindow::AddToPrevOpenMenu(fullname);

        /*  If name has changed, language mode may have changed as well, unless
            it's an Untitled window for which the user already set a language
            mode; it's probably the right one.  */
        if (languageMode_ == PLAIN_LANGUAGE_MODE || filenameSet_) {
            DetermineLanguageMode(false);
        }
        filenameSet_ = true;

        // Update the stats line and window title with the new filename
        win->UpdateWindowTitle(this);
        UpdateStatsLine(nullptr);

        win->SortTabBar();
        return retVal;
    }

    return false;
}

/*
** Change a window created in NEdit's continuous wrap mode to the more
** conventional Unix format of embedded newlines.  Indicate to the user
** by turning off Continuous Wrap mode.
*/
void DocumentWidget::addWrapNewlines() {

    TextCursor insertPositions[MAX_PANES];
    int64_t topLines[MAX_PANES];
    int horizOffset;

    const std::vector<TextArea *> textAreas = textPanes();
    const size_t paneCount = textAreas.size();

    // save the insert and scroll positions of each pane
    for(size_t i = 0; i < paneCount; ++i) {
        TextArea *area = textAreas[i];
        insertPositions[i] = area->TextGetCursorPos();
        area->TextDGetScroll(&topLines[i], &horizOffset);
    }

    // Modify the buffer to add wrapping
    TextArea *area = textAreas[0];
    std::string fileString = area->TextGetWrappedEx(buffer_->BufStartOfBuffer(), buffer_->BufEndOfBuffer());

    buffer_->BufSetAllEx(fileString);

    // restore the insert and scroll positions of each pane
    for(size_t i = 0; i < paneCount; ++i) {
        TextArea *area = textAreas[i];
        area->TextSetCursorPos(insertPositions[i]);
        area->TextDSetScroll(topLines[i], 0);
    }

    /* Show the user that something has happened by turning off
       Continuous Wrap mode */
    if(auto win = MainWindow::fromDocument(this)) {
        no_signals(win->ui.action_Wrap_Continuous)->setChecked(false);
    }
}

/*
** If saveOldVersion is on, copies the existing version of the file to
** <filename>.bck in anticipation of a new version being saved.  Returns
** true if backup fails and user requests that the new file not be written.
*/
bool DocumentWidget::writeBckVersion() {

	// TODO(eteran): 2.0, this is essentially just a QFile::copy("filename", "filename.bck");
    // with error reporting

    // Do only if version backups are turned on
    if (!saveOldVersion_) {
        return false;
    }

    // Get the full name of the file
    QString fullname = FullPath();

    // Generate name for old version
    auto bckname = tr("%1.bck").arg(fullname);

    // Delete the old backup file
    // Errors are ignored; we'll notice them later.
	QFile::remove(bckname);

    /* open the file being edited.  If there are problems with the
     * old file, don't bother the user, just skip the backup */
    int in_fd = ::open(fullname.toUtf8().data(), O_RDONLY);
    if (in_fd < 0) {
        return false;
    }

    auto _1 = gsl::finally([in_fd]() {
        ::close(in_fd);
    });

    /* Get permissions of the file.
     * We preserve the normal permissions but not ownership, extended
     * attributes, et cetera. */
    struct stat statbuf;
    if (::fstat(in_fd, &statbuf) != 0) {
        return false;
    }

    // open the destination file exclusive and with restrictive permissions.
    int out_fd = ::open(bckname.toUtf8().data(), O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (out_fd < 0) {
        return backupError(tr("Error open backup file"), bckname);
    }

    auto _2 = gsl::finally([out_fd]() {
        ::close(out_fd);
    });

    // Set permissions on new file
    if (::fchmod(out_fd, statbuf.st_mode) != 0) {
		QFile::remove(bckname);
        return backupError(tr("fchmod() failed"), bckname);
    }

    // Allocate I/O buffer
    constexpr size_t IO_BUFFER_SIZE = (1024 * 1024);
    std::vector<char> io_buffer(IO_BUFFER_SIZE);

    // copy loop
    for (;;) {
        ssize_t bytes_read = ::read(in_fd, io_buffer.data(), io_buffer.size());

        if (bytes_read < 0) {
			QFile::remove(bckname);
            return backupError(tr("read() error"), filename_);
        }

        if (bytes_read == 0) {
            break; // EOF
        }

        // write to the file
        ssize_t bytes_written = ::write(out_fd, io_buffer.data(), static_cast<size_t>(bytes_read));
        if (bytes_written != bytes_read) {
			QFile::remove(bckname);
            return backupError(ErrorString(errno), bckname);
        }
    }

    return false;
}

/*
** Error processing for writeBckVersion, gives the user option to cancel
** the subsequent save, or continue and optionally turn off versioning
*/
bool DocumentWidget::backupError(const QString &errorMessage, const QString &file) {

    QMessageBox messageBox(this);
    messageBox.setWindowTitle(tr("Error writing Backup"));
    messageBox.setIcon(QMessageBox::Critical);
    messageBox.setText(tr("Couldn't write .bck (last version) file.\n%1: %2").arg(file, errorMessage));

    QPushButton *buttonCancelSave = messageBox.addButton(tr("Cancel Save"),      QMessageBox::RejectRole);
    QPushButton *buttonTurnOff    = messageBox.addButton(tr("Turn off Backups"), QMessageBox::AcceptRole);
    QPushButton *buttonContinue   = messageBox.addButton(tr("Continue"),         QMessageBox::AcceptRole);

    Q_UNUSED(buttonContinue);

    messageBox.exec();
    if(messageBox.clickedButton() == buttonCancelSave) {
        return true;
    }

    if(messageBox.clickedButton() == buttonTurnOff) {
        saveOldVersion_ = false;

        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Make_Backup_Copy)->setChecked(false);
        }
    }

    return false;
}

/*
** Return true if the file displayed in window has been modified externally
** to nedit.  This should return false if the file has been deleted or is
** unavailable.
*/
bool DocumentWidget::fileWasModifiedExternally() const {

    if (!filenameSet_) {
        return false;
    }

    QString fullname = FullPath();

    struct stat statbuf;
    if (::stat(fullname.toLocal8Bit().data(), &statbuf) != 0) {
        return false;
    }

    if (lastModTime_ == statbuf.st_mtime) {
        return false;
    }

    if (Preferences::GetPrefWarnRealFileMods() && !cmpWinAgainstFile(fullname)) {
        return false;
    }

    return true;
}

bool DocumentWidget::CloseFileAndWindow(CloseMode preResponse) {

    // Make sure that the window is not in iconified state
    if (fileChanged_) {
        RaiseDocumentWindow();
    }

    /* If the window is a normal & unmodified file or an empty new file,
       or if the user wants to ignore external modifications then
       just close it.  Otherwise ask for confirmation first. */
    if (!fileChanged_ &&
            /* Normal File */
            ((!fileMissing_ && lastModTime_ > 0) ||
             /* New File */
             (fileMissing_ && lastModTime_ == 0) ||
             /* File deleted/modified externally, ignored by user. */
             !Preferences::GetPrefWarnFileMods())) {

        CloseDocument();
        // up-to-date windows don't have outstanding backup files to close
    } else {

        int response = QMessageBox::Yes;
        switch(preResponse) {
        case CloseMode::Prompt:
            response = QMessageBox::warning(
                        this,
                        tr("Save File"),
                        tr("Save %1 before closing?").arg(filename_),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            break;
        case CloseMode::Save:
            response = QMessageBox::Yes;
            break;
        case CloseMode::NoSave:
            response = QMessageBox::No;
            break;
        }

        switch(response) {
        case QMessageBox::Yes:
            // Save
            if (SaveWindow()) {
                CloseDocument();
            } else {
                return false;
            }
            break;
        case QMessageBox::No:
            // Don't Save
            RemoveBackupFile();
            CloseDocument();
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
void DocumentWidget::CloseDocument() {

    // Free smart indent macro programs
    EndSmartIndent();

    /* Clean up macro references to the doomed window.  If a macro is
       executing, stop it.  If macro is calling this (closing its own
       window), leave the window alive until the macro completes */
    const bool keepWindow = !MacroWindowCloseActionsEx();

    // Kill shell sub-process
    AbortShellCommandEx();

    // Unload the default tips files for this language mode if necessary
    UnloadLanguageModeTipsFileEx();

    /* if this is the last window, or must be kept alive temporarily because
       it's running the macro calling us, don't close it, make it Untitled */
    const size_t windowCount   = MainWindow::allWindows().size();
    const size_t documentCount = DocumentWidget::allDocuments().size();

    Q_EMIT documentClosed();

    auto win = MainWindow::fromDocument(this);
    if(!win) {
        return;
    }

    if (keepWindow || (windowCount == 1 && documentCount == 1)) {

        QString name = MainWindow::UniqueUntitledNameEx();
        lockReasons_.clear();

        mode_         = 0;
        uid_          = 0;
        gid_          = 0;
        lastModTime_  = 0;
        dev_          = 0;
        ino_          = 0;
        nMarks_       = 0;
        filename_     = name;
        path_         = QString();

        // clear the buffer, but ignore changes
        ignoreModify_ = true;
        buffer_->BufSetAllEx("");
        ignoreModify_ = false;

        filenameSet_  = false;
        fileChanged_  = false;
        fileMissing_  = true;
        fileFormat_   = FileFormats::Unix;

        StopHighlightingEx();
        EndSmartIndent();
        win->UpdateWindowTitle(this);
        win->UpdateWindowReadOnly(this);
        win->ui.action_Close->setEnabled(false);
        win->ui.action_Read_Only->setEnabled(true);
        win->ui.action_Read_Only->setChecked(false);
        ClearUndoList();
        ClearRedoList();
        UpdateStatsLine(nullptr);
        DetermineLanguageMode(true);
        RefreshTabState();
        win->updateLineNumDisp();
        return;
    }

    // deallocate the document data structure
    // NOTE(eteran): we re-parent the document so that it is no longer in
    // MainWindow::openDocuments() and DocumentWidget::allDocuments()
    this->setParent(nullptr);
    this->deleteLater();

    // update window menus
    MainWindow::UpdateWindowMenus();

    // Close of window running a macro may have been disabled.
    // NOTE(eteran): this may be redundant...
    MainWindow::CheckCloseDimEx();

    // if we deleted the last tab, then we can close the window too
    if(win->TabCount() == 0) {
        win->deleteLater();
        win->setVisible(false);
    }

    // The number of open windows may have changed...
    const std::vector<MainWindow *> windows = MainWindow::allWindows();
    const bool enabled = windows.size() > 1;
    for(MainWindow *window : windows) {
        window->ui.action_Move_Tab_To->setEnabled(enabled);
    }
}

DocumentWidget *DocumentWidget::fromArea(TextArea *area) {

    if(!area) {
        return nullptr;
    }

    QObject *p = area->parent();
    while(p) {
        if(auto document = qobject_cast<DocumentWidget *>(p)) {
            return document;
        }

        p = p->parent();
    }

    return nullptr;
}

/**
 * @brief DocumentWidget::open
 * @param fullpath
 */
void DocumentWidget::open(const QString &fullpath) {

    QString filename;
    QString pathname;

    if (!ParseFilenameEx(fullpath, &filename, &pathname) != 0) {
        qWarning("NEdit: invalid file name for open action: %s", qPrintable(fullpath));
        return;
    }

    DocumentWidget::EditExistingFileEx(
                this,
                filename,
                pathname,
                0,
                QString(),
                false,
                QString(),
                Preferences::GetPrefOpenInTab(),
                false);

    MainWindow::CheckCloseDimEx();
}

/**
 * @brief DocumentWidget::doOpen
 * @param name
 * @param path
 * @param flags
 * @return
 */
bool DocumentWidget::doOpen(const QString &name, const QString &path, int flags) {

    auto window = MainWindow::fromDocument(this);
    if(!window) {
        return false;
    }

    // initialize lock reasons
    lockReasons_.clear();

    // Update the window data structure
    filename_    = name;
    path_        = path;
    filenameSet_ = true;
    fileMissing_ = true;

    FILE *fp = nullptr;

    // Get the full name of the file
    const QString fullname = FullPath();

    // Open the file
    /* The only advantage of this is if you use clearcase,
       which messes up the mtime of files opened with r+,
       even if they're never actually written.
       To avoid requiring special builds for clearcase users,
       this is now the default.
    */
    {
        if ((fp = ::fopen(fullname.toUtf8().data(), "r"))) {

            if (::access(fullname.toUtf8().data(), W_OK) != 0) {
                lockReasons_.setPermLocked(true);
            }

		} else if (flags & EditFlags::CREATE && errno == ENOENT) {
            // Give option to create (or to exit if this is the only window)
			if (!(flags & EditFlags::SUPPRESS_CREATE_WARN)) {

                QMessageBox msgbox(this);
                QAbstractButton  *exitButton;

                // ask user for next action if file not found
                std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
                int resp;

                if (documents.size() == 1 && documents.front() == this) {

                    msgbox.setIcon(QMessageBox::Warning);
                    msgbox.setWindowTitle(tr("New File"));
                    msgbox.setText(tr("Can't open %1:\n%2").arg(fullname, ErrorString(errno)));
                    msgbox.addButton(tr("New File"), QMessageBox::AcceptRole);
                    msgbox.addButton(QMessageBox::Cancel);
                    exitButton = msgbox.addButton(tr("Exit NEdit"), QMessageBox::RejectRole);
                    resp = msgbox.exec();

                } else {

                    msgbox.setIcon(QMessageBox::Warning);
                    msgbox.setWindowTitle(tr("New File"));
                    msgbox.setText(tr("Can't open %1:\n%2").arg(fullname, ErrorString(errno)));
                    msgbox.addButton(tr("New File"), QMessageBox::AcceptRole);
                    msgbox.addButton(QMessageBox::Cancel);
                    exitButton = nullptr;
                    resp = msgbox.exec();
                }

                if (resp == QMessageBox::Cancel) {
                    return false;
                } else if (msgbox.clickedButton() == exitButton) {
                    QApplication::quit();
                }
            }

            // Test if new file can be created
            int fd = ::creat(fullname.toUtf8().data(), 0666);
            if (fd == -1) {
                QMessageBox::critical(this, tr("Error creating File"), tr("Can't create %1:\n%2").arg(fullname, ErrorString(errno)));
                return false;
            } else {
                ::close(fd);
				QFile::remove(fullname);
            }

            SetWindowModified(false);
			if ((flags & EditFlags::PREF_READ_ONLY) != 0) {
                lockReasons_.setUserLocked(true);
            }

            window->UpdateWindowReadOnly(this);
            return true;
        } else {
            // A true error
            QMessageBox::critical(this, tr("Error opening File"), tr("Could not open %1%2:\n%3").arg(path, name, ErrorString(errno)));
            return false;
        }
    }

    auto _ = gsl::finally([fp] { ::fclose(fp); });

    /* Get the length of the file, the protection mode, and the time of the
       last modification to the file */
	struct stat statbuf;
    if (::fstat(fileno(fp), &statbuf) != 0) {
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error opening File"), tr("Error opening %1").arg(name));
        filenameSet_ = true;
        return false;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error opening File"), tr("Can't open directory %1").arg(name));
        filenameSet_ = true;
        return false;
    }

#ifdef S_ISBLK
    if (S_ISBLK(statbuf.st_mode)) {
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error opening File"), tr("Can't open block device %1").arg(name));
        filenameSet_ = true;
        return false;
    }
#endif

    const int64_t fileLength = statbuf.st_size;

    // Allocate space for the whole contents of the file (unfortunately)
    try {
        std::vector<char> fileString(static_cast<size_t>(fileLength));

        // Read the file into fileString and terminate with a null
        size_t readLen = ::fread(fileString.data(), 1, static_cast<size_t>(fileLength), fp);
        if (::ferror(fp)) {
            filenameSet_ = false; // Temp. prevent check for changes.
            QMessageBox::critical(this, tr("Error while opening File"), tr("Error reading %1:\n%2").arg(name, ErrorString(errno)));
            filenameSet_ = true;
            return false;
        }

        /* Any errors that happen after this point leave the window in a
           "broken" state, and thus RevertToSaved will abandon the window if
           window->fileMissing_ is false and doOpen fails. */
        mode_        = statbuf.st_mode;
        uid_         = statbuf.st_uid;
        gid_         = statbuf.st_gid;
        lastModTime_ = statbuf.st_mtime;
        dev_         = statbuf.st_dev;
        ino_         = statbuf.st_ino;
        fileMissing_ = false;

        assert(readLen <= fileString.size());

        // Detect and convert DOS and Macintosh format files
        if (Preferences::GetPrefForceOSConversion()) {
            switch (FormatOfFileEx(view::string_view(fileString.data(), readLen))) {
            case FileFormats::Dos:
                ConvertFromDosFileString(fileString.data(), &readLen, nullptr);
                break;
            case FileFormats::Mac:
                ConvertFromMacFileString(fileString.data(), readLen);
                break;
            case FileFormats::Unix:
                break;
            }
        }

        auto contents = view::string_view(fileString.data(), readLen);

        // Display the file contents in the text widget
        ignoreModify_ = true;
        buffer_->BufSetAllEx(contents);
        ignoreModify_ = false;

        // Set window title and file changed flag
        if ((flags & EditFlags::PREF_READ_ONLY) != 0) {
            lockReasons_.setUserLocked(true);
        }

        if (lockReasons_.isPermLocked()) {
            fileChanged_ = false;
            window->UpdateWindowTitle(this);
        } else {
            SetWindowModified(false);
            if (lockReasons_.isAnyLocked()) {
                window->UpdateWindowTitle(this);
            }
        }

        window->UpdateWindowReadOnly(this);
        return true;
    } catch(const std::bad_alloc &) {
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error while opening File"), tr("File is too large to edit"));
        filenameSet_ = true;
        return false;
    }
}

/*
** refresh window state for this document
*/
void DocumentWidget::RefreshWindowStates() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = MainWindow::fromDocument(this)) {

        if (modeMessageDisplayed()) {
            ui.labelFileAndSize->setText(modeMessage_);
        } else {
            UpdateStatsLine(nullptr);
        }

        win->UpdateWindowReadOnly(this);
        win->UpdateWindowTitle(this);

        // show/hide statsline as needed
        if (modeMessageDisplayed() && !ui.statusFrame->isVisible()) {
            // turn on statline to display mode message
            ShowStatsLine(true);
        } else if (showStats_ && !ui.statusFrame->isVisible()) {
            // turn on statsline since it is enabled
            ShowStatsLine(true);
        } else if (!showStats_ && !modeMessageDisplayed() && ui.statusFrame->isVisible()) {
            // turn off statsline since there's nothing to show
            ShowStatsLine(false);
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
}

/*
** Refresh the various settings/state of the shell window per the
** settings of the top document.
*/
void DocumentWidget::refreshMenuBar() {
    if(auto win = MainWindow::fromDocument(this)) {
        RefreshMenuToggleStates();

        // Add/remove language specific menu items
        win->UpdateUserMenus(this);

        // refresh selection-sensitive menus
        DimSelectionDepUserMenuItems(win->wasSelected_);
    }
}

/*
** Refresh the menu entries per the settings of the
** top document.
*/
void DocumentWidget::RefreshMenuToggleStates() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = MainWindow::fromDocument(this)) {
        // File menu
        win->ui.action_Print_Selection->setEnabled(win->wasSelected_);

        // Edit menu
        win->ui.action_Undo->setEnabled(!undo_.empty());
        win->ui.action_Redo->setEnabled(!redo_.empty());
        win->ui.action_Cut->setEnabled(win->wasSelected_);
        win->ui.action_Copy->setEnabled(win->wasSelected_);
        win->ui.action_Delete->setEnabled(win->wasSelected_);

        // Preferences menu
        no_signals(win->ui.action_Statistics_Line)->setChecked(showStats_);
        no_signals(win->ui.action_Incremental_Search_Line)->setChecked(win->showISearchLine_);
        no_signals(win->ui.action_Show_Line_Numbers)->setChecked(win->showLineNumbers_);
        no_signals(win->ui.action_Highlight_Syntax)->setChecked(highlightSyntax_);        
        no_signals(win->ui.action_Apply_Backlighting)->setChecked(backlightChars_);
        no_signals(win->ui.action_Make_Backup_Copy)->setChecked(saveOldVersion_);
        no_signals(win->ui.action_Incremental_Backup)->setChecked(autoSave_);
        no_signals(win->ui.action_Overtype)->setChecked(overstrike_);
        no_signals(win->ui.action_Matching_Syntax)->setChecked(matchSyntaxBased_);
        no_signals(win->ui.action_Read_Only)->setChecked(lockReasons_.isUserLocked());

        win->ui.action_Indent_Smart->setEnabled(SmartIndent::SmartIndentMacrosAvailable(Preferences::LanguageModeName(languageMode_)));
        win->ui.action_Highlight_Syntax->setEnabled(languageMode_ != PLAIN_LANGUAGE_MODE);

        SetAutoIndent(indentStyle_);
        SetAutoWrap(wrapMode_);
        SetShowMatching(showMatchingStyle_);
        SetLanguageMode(languageMode_, /*forceNewDefaults=*/false);

        // Windows Menu
        win->ui.action_Split_Pane->setEnabled(textPanesCount() < MAX_PANES);
        win->ui.action_Close_Pane->setEnabled(textPanesCount() > 1);

        std::vector<MainWindow *> windows = MainWindow::allWindows();
        win->ui.action_Move_Tab_To->setEnabled(windows.size() > 1);
    }
}

/*
** Run the newline macro with information from the smart-indent callback
** structure passed by the widget
*/
void DocumentWidget::executeNewlineMacroEx(SmartIndentEvent *cbInfo) {

    if(const std::unique_ptr<SmartIndentData> &winData = smartIndentData_) {

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
        std::array<DataValue, 1> args;
        args[0] = make_value(to_integer(cbInfo->pos));

        ++(winData->inNewLineMacro);

        std::shared_ptr<MacroContext> continuation;
        int stat = ExecuteMacroEx(this, winData->newlineMacro, args, &result, continuation, &errMsg);

        // Don't allow preemption or time limit.  Must get return value
        while (stat == MACRO_TIME_LIMIT) {
            stat = ContinueMacroEx(continuation, &result, &errMsg);
        }

        --(winData->inNewLineMacro);

        // Process errors in macro execution
        if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
            QMessageBox::critical(
                        this,
                        tr("Smart Indent"),
                        tr("Error in smart indent macro:\n%1").arg(stat == MACRO_ERROR ? errMsg : tr("dialogs and shell commands not permitted")));
            EndSmartIndent();
            return;
        }

        // Validate and return the result
        if (!is_integer(result) || to_integer(result) < -1 || to_integer(result) > 1000) {
            QMessageBox::critical(this, tr("Smart Indent"), tr("Smart indent macros must return integer indent distance"));
            EndSmartIndent();
            return;
        }

        cbInfo->indentRequest = to_integer(result);
    }
}

/*
** Set showMatching state to one of None, Delimeter or Range.
** Update the menu to reflect the change of state.
*/
void DocumentWidget::SetShowMatching(ShowMatchingStyle state) {

    emit_event("set_show_matching", to_string(state));

    showMatchingStyle_ = state;
    if (IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            switch(state) {
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
    }
}

/*
** Run the modification macro with information from the smart-indent callback
** structure passed by the widget
*/
void DocumentWidget::executeModMacroEx(SmartIndentEvent *cbInfo) {

    if(const std::unique_ptr<SmartIndentData> &winData = smartIndentData_) {

        DataValue result;
        QString errMsg;

        /* Check for inappropriate calls and prevent re-entering if the macro
           makes a buffer modification */
        if (winData->modMacro == nullptr || winData->inModMacro > 0) {
            return;
        }

        /* Call modification macro with the position of the modification,
           and the character(s) inserted.  Don't allow
           preemption or time limit.  Execution must not overlap or re-enter */
        std::array<DataValue, 2> args;
        args[0] = make_value(to_integer(cbInfo->pos));
        args[1] = make_value(cbInfo->charsTyped);

        ++(winData->inModMacro);

        std::shared_ptr<MacroContext> continuation;
        int stat = ExecuteMacroEx(this, winData->modMacro, args, &result, continuation, &errMsg);

        while (stat == MACRO_TIME_LIMIT) {
            stat = ContinueMacroEx(continuation, &result, &errMsg);
        }

        --(winData->inModMacro);

        // Process errors in macro execution
        if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
            QMessageBox::critical(
                        this,
                        tr("Smart Indent"),
                        tr("Error in smart indent modification macro:\n%1").arg((stat == MACRO_ERROR) ? errMsg : tr("dialogs and shell commands not permitted")));

            EndSmartIndent();
            return;
        }
    }
}

/**
 * @brief DocumentWidget::macroBannerTimeoutProc
 */
void DocumentWidget::macroBannerTimeoutProc() {

    auto window = MainWindow::fromDocument(this);
    if(!window) {
        return;
    }

    macroCmdData_->bannerIsUp = true;

    // Extract accelerator text from menu PushButtons
    QString cCancel = window->ui.action_Cancel_Learn->shortcut().toString();

    // Create message
    if (cCancel.isEmpty()) {
        SetModeMessageEx(tr("Macro Command in Progress"));
    } else {
        SetModeMessageEx(tr("Macro Command in Progress -- Press %1 to Cancel").arg(cCancel));
    }
}

/**
 * @brief DocumentWidget::shellBannerTimeoutProc
 */
void DocumentWidget::shellBannerTimeoutProc() {

    auto window = MainWindow::fromDocument(this);
    if(!window) {
        return;
    }

    shellCmdData_->bannerIsUp = true;

    // Extract accelerator text from menu PushButtons
    QString cCancel = window->ui.action_Cancel_Shell_Command->shortcut().toString();

    // Create message
    if (cCancel.isEmpty()) {
        SetModeMessageEx(tr("Shell Command in Progress"));
    } else {
        SetModeMessageEx(tr("Shell Command in Progress -- Press %1 to Cancel").arg(cCancel));
    }
}

void DocumentWidget::actionClose(CloseMode mode) {

    CloseFileAndWindow(mode);
    MainWindow::CheckCloseDimEx();
}

bool DocumentWidget::includeFile(const QString &name) {

    if (CheckReadOnly()) {
        return false;
    }

    QFile file(name);
    if(!file.open(QFile::ReadOnly)) {
        QMessageBox::critical(this, tr("Error opening File"), file.errorString());
        return false;
    }

    uchar *memory = file.map(0, file.size());
    if (!memory) {
        QMessageBox::critical(this, tr("Error opening File"), file.errorString());
        return false;
    }

    auto fileString = std::string{reinterpret_cast<char *>(memory), static_cast<size_t>(file.size())};

    file.unmap(memory);

    // Detect and convert DOS and Macintosh format files
    switch (FormatOfFileEx(fileString)) {
    case FileFormats::Dos:
        ConvertFromDosFileStringEx(&fileString, nullptr);
        break;
    case FileFormats::Mac:
        ConvertFromMacFileStringEx(&fileString);
        break;
    case FileFormats::Unix:
        //  Default is Unix, no conversion necessary.
        break;
    }

    /* insert the contents of the file in the selection or at the insert
       position in the window if no selection exists */
    if (buffer_->primary.selected) {
        buffer_->BufReplaceSelectedEx(fileString);
    } else {
        if(auto win = MainWindow::fromDocument(this)) {
            buffer_->BufInsertEx(win->lastFocus()->TextGetCursorPos(), fileString);
        }
    }

    return true;
}

void DocumentWidget::GotoMatchingCharacter(TextArea *area) {

    TextCursor selStart;
    TextCursor selEnd;

    /* get the character to match and its position from the selection, or
       the character before the insert point if nothing is selected.
       Give up if too many characters are selected */
    if (!buffer_->GetSimpleSelection(&selStart, &selEnd)) {

        selEnd = area->TextGetCursorPos();

        if (overstrike_) {
            selEnd += 1;
        }

        if(selEnd == 0) {
            QApplication::beep();
            return;
        }

        selStart = selEnd - 1;
    }

    if ((selEnd - selStart) != 1) {
        QApplication::beep();
        return;
    }

    // Search for it in the buffer
    boost::optional<TextCursor> matchPos = findMatchingCharEx(buffer_->BufGetCharacter(selStart), GetHighlightInfoEx(selStart), selStart, buffer_->BufStartOfBuffer(), buffer_->BufEndOfBuffer());
    if (!matchPos) {
        QApplication::beep();
        return;
    }

    /* temporarily shut off autoShowInsertPos before setting the cursor
       position so MakeSelectionVisible gets a chance to place the cursor
       string at a pleasing position on the screen (otherwise, the cursor would
       be automatically scrolled on screen and MakeSelectionVisible would do
       nothing) */
    area->setAutoShowInsertPos(false);
    area->TextSetCursorPos(*matchPos + 1);
    MakeSelectionVisible(area);
    area->setAutoShowInsertPos(true);
}

boost::optional<TextCursor> DocumentWidget::findMatchingCharEx(char toMatch, Style styleToMatch, TextCursor charPos, TextCursor startLimit, TextCursor endLimit) {

    Style style;
    bool matchSyntaxBased = matchSyntaxBased_;

    // If we don't match syntax based, fake a matching style.
    if (!matchSyntaxBased) {
        style = styleToMatch;
    }

    // Look up the matching character and match direction
    auto matchIt = std::find_if(std::begin(MatchingChars), std::end(MatchingChars), [toMatch](const CharMatchTable &entry) {
        return entry.c == toMatch;
    });

    if(matchIt == std::end(MatchingChars)) {
        return boost::none;
    }

    const char matchChar      = matchIt->match;
    const Direction direction = matchIt->direction;

    // find it in the buffer

    int nestDepth = 1;

    switch(direction) {
    case Direction::Forward:
    {
        const TextCursor beginPos = charPos + 1;

        for (TextCursor pos = beginPos; pos < endLimit; pos++) {
            const char ch = buffer_->BufGetCharacter(pos);
            if (ch == matchChar) {
                if (matchSyntaxBased) {
                    style = GetHighlightInfoEx(pos);
                }

                if (style == styleToMatch) {
                    --nestDepth;
                    if (nestDepth == 0) {
                        return pos;
                    }
                }
            } else if (ch == toMatch) {
                if (matchSyntaxBased) {
                    style = GetHighlightInfoEx(pos);
                }

                if (style == styleToMatch) {
                    ++nestDepth;
                }
            }
        }
        break;
    }
    case Direction::Backward:
        if(charPos != startLimit) {
            const TextCursor beginPos = charPos - 1;

            for (TextCursor pos = beginPos; pos >= startLimit; pos--) {
                const char ch = buffer_->BufGetCharacter(pos);
                if (ch == matchChar) {
                    if (matchSyntaxBased) {
                        style = GetHighlightInfoEx(pos);
                    }

                    if (style == styleToMatch) {
                        --nestDepth;
                        if (nestDepth == 0) {
                            return pos;
                        }
                    }
                } else if (ch == toMatch) {
                    if (matchSyntaxBased) {
                        style = GetHighlightInfoEx(pos);
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

void DocumentWidget::SelectToMatchingCharacter(TextArea *area) {

    TextCursor selStart;
    TextCursor selEnd;

    /* get the character to match and its position from the selection, or
       the character before the insert point if nothing is selected.
       Give up if too many characters are selected */
    if (!buffer_->GetSimpleSelection(&selStart, &selEnd)) {

        selEnd = area->TextGetCursorPos();
        if (overstrike_) {
            selEnd += 1;
        }

        if(selEnd == 0) {
            QApplication::beep();
            return;
        }

        selStart = selEnd - 1;
    }

    if ((selEnd - selStart) != 1) {
        QApplication::beep();
        return;
    }

    // Search for it in the buffer
    boost::optional<TextCursor> matchPos = findMatchingCharEx(buffer_->BufGetCharacter(selStart), GetHighlightInfoEx(selStart), selStart, buffer_->BufStartOfBuffer(), buffer_->BufEndOfBuffer());
    if (!matchPos) {
        QApplication::beep();
        return;
    }

    const TextCursor startPos = (*matchPos > selStart) ? selStart : *matchPos;
    const TextCursor endPos   = (*matchPos > selStart) ? *matchPos : selStart;

    /* temporarily shut off autoShowInsertPos before setting the cursor
       position so MakeSelectionVisible gets a chance to place the cursor
       string at a pleasing position on the screen (otherwise, the cursor would
       be automatically scrolled on screen and MakeSelectionVisible would do
       nothing) */
    area->setAutoShowInsertPos(false);
    buffer_->BufSelect(startPos, endPos + 1);
    MakeSelectionVisible(area);
    area->setAutoShowInsertPos(true);
}

/*
** See findDefHelper
*/
void DocumentWidget::FindDefCalltip(TextArea *area, const QString &tipName) {

	// Reset calltip parameters to reasonable defaults
    Tags::globAnchored  = false;
    Tags::globPos       = -1;
    Tags::globHAlign    = TipHAlignMode::Left;
    Tags::globVAlign    = TipVAlignMode::Below;
    Tags::globAlignMode = TipAlignMode::Sloppy;

    findDefinitionHelper(area, tipName, Tags::SearchMode::TIP);

}

/*
** Lookup the definition for the current primary selection the currently
** loaded tags file and bring up the file and line that the tags file
** indicates.
*/
void DocumentWidget::findDefinitionHelper(TextArea *area, const QString &arg, Tags::SearchMode search_type) {
	if (!arg.isNull()) {
		findDef(area, arg, search_type);
	} else {
        Tags::searchMode = search_type;

        QString selected = GetAnySelectionEx(/*beep_on_error=*/false);
        if(selected.isEmpty()) {
			return;
		}

        findDef(area, selected, search_type);
	}
}

/*
** This code path is followed if the request came from either
** FindDefinition or FindDefCalltip.  This should probably be refactored.
*/
int DocumentWidget::findDef(TextArea *area, const QString &value, Tags::SearchMode search_type) {
	int status = 0;

    Tags::searchMode = search_type;

    // make sure that the whole value is ascii
    auto p = std::find_if(value.begin(), value.end(), [](QChar ch) {
        return !safe_ctype<isascii>(ch.toLatin1());
    });

    if (p == value.end()) {

        // See if we can find the tip/tag
        status = findAllMatchesEx(area, value);

        // If we didn't find a requested calltip, see if we can use a tag
        if (status == 0 && search_type == Tags::SearchMode::TIP && !Tags::TagsFileList.empty()) {
            Tags::searchMode = Tags::SearchMode::TIP_FROM_TAG;
            status = findAllMatchesEx(area, value);
        }

        if (status == 0) {
            // Didn't find any matches
            if (Tags::searchMode == Tags::SearchMode::TIP_FROM_TAG || Tags::searchMode == Tags::SearchMode::TIP) {
                Tags::tagsShowCalltipEx(area, tr("No match for \"%1\" in calltips or tags.").arg(Tags::tagName));
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

/**
 * @brief DocumentWidget::FindDefinition
 * @param area
 * @param tagName
 */
void DocumentWidget::FindDefinition(TextArea *area, const QString &tagName) {
    findDefinitionHelper(area, tagName, Tags::SearchMode::TAG);
}

/**
 * @brief DocumentWidget::execAP
 * @param area
 * @param command
 */
void DocumentWidget::execAP(TextArea *area, const QString &command) {

    if (CheckReadOnly()) {
        return;
    }

    ExecShellCommandEx(area, command, CommandSource::User);
}

/*
** Execute shell command "command", depositing the result at the current
** insert position or in the current selection if the window has a
** selection.
*/
void DocumentWidget::ExecShellCommandEx(TextArea *area, const QString &command, CommandSource source) {
    if(auto win = MainWindow::fromDocument(this)) {
        int flags = 0;

        // Can't do two shell commands at once in the same window
        if (shellCmdData_) {
            QApplication::beep();
            return;
        }

        // get the selection or the insert position
        const TextCursor pos = area->TextGetCursorPos();

        TextCursor left;
        TextCursor right;
        if (buffer_->GetSimpleSelection(&left, &right)) {
            flags = ACCUMULATE | REPLACE_SELECTION;
        } else {
            left  = pos;
            right = pos;
        }

        /* Substitute the current file name for % and the current line number
           for # in the shell command */
        QString fullName = FullPath();

        int64_t line;
        int64_t column;
        area->TextDPosToLineAndCol(pos, &line, &column);

        QString substitutedCommand = command;
        substitutedCommand.replace(QLatin1Char('%'), fullName);
		substitutedCommand.replace(QLatin1Char('#'), QString::number(line));

        if(substitutedCommand.isNull()) {
            QMessageBox::critical(this, tr("Shell Command"), tr("Shell command is too long due to\n"
                                                               "filename substitutions with '%%' or\n"
                                                               "line number substitutions with '#'"));
            return;
        }

        issueCommandEx(
                    win,
                    area,
                    substitutedCommand,
                    QString(),
                    flags,
                    left,
                    right,
                    source);

    }
}

/**
 * @brief DocumentWidget::PrintWindow
 * @param area
 * @param selectedOnly
 */
void DocumentWidget::PrintWindow(TextArea *area, bool selectedOnly) {

    std::string fileString;

    /* get the contents of the text buffer from the text area widget.  Add
       wrapping newlines if necessary to make it match the displayed text */
    if (selectedOnly) {

        const TextBuffer::Selection *sel = &buffer_->primary;

        if (!sel->selected) {
            QApplication::beep();
            return;
        }
        if (sel->rectangular) {
            fileString = buffer_->BufGetSelectionTextEx();
        } else {
            fileString = area->TextGetWrappedEx(sel->start, sel->end);
        }
    } else {
        fileString = area->TextGetWrappedEx(buffer_->BufStartOfBuffer(), buffer_->BufEndOfBuffer());
    }

    // add a terminating newline if the file doesn't already have one
    if (!fileString.empty() && fileString.back() != '\n') {
        fileString.push_back('\n');
    }

    // Print the string
    PrintStringEx(fileString, filename_);
}

/**
 * Print a string
 *
 * @brief DocumentWidget::PrintStringEx
 * @param string
 * @param jobname
 */
void DocumentWidget::PrintStringEx(const std::string &string, const QString &jobname) {

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

	if(splitter_->count() >= MAX_PANES) {
		return;
	}

    auto area = createTextArea(buffer_);

    if(auto activeArea = qobject_cast<TextArea *>(splitter_->widget(0))) {
        area->setLineNumCols(activeArea->getLineNumCols());
        area->setBacklightCharTypes(backlightCharTypes_);
        area->setFont(fontStruct_);
    }

    AttachHighlightToWidgetEx(area);

    splitter_->addWidget(area);
    area->setFocus();
}

/*
** Close the window pane that last had the keyboard focus.
*/
void DocumentWidget::closePane() {

    if(splitter_->count() > 1) {

        TextArea *lastFocus = MainWindow::fromDocument(this)->lastFocus();

        for(int i = 0; i < splitter_->count(); ++i) {
            if(auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
                if(area == lastFocus) {
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
void DocumentWidget::BeginSmartIndentEx(bool warn) {

    static bool initialized = false;

    // Find the window's language mode.  If none is set, warn the user
    QString modeName = Preferences::LanguageModeName(languageMode_);
    if(modeName.isNull()) {
        if (warn) {
            QMessageBox::warning(
                        this,
                        tr("Smart Indent"),
                        tr("No language-specific mode has been set for this file.\n\nTo use smart indent in this window, please select a language from the Preferences -> Language Modes menu."));
        }
        return;
    }

    // Look up the appropriate smart-indent macros for the language
    const SmartIndentEntry *indentMacros = SmartIndent::findIndentSpec(modeName);
    if(!indentMacros) {
        if (warn) {
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
    ReadMacroInitFileEx();

    /* Compile and run the common and language-specific initialization macros
       (Note that when these return, the immediate commands in the file have not
       necessarily been executed yet.  They are only SCHEDULED for execution) */
    if (!initialized) {
        if (!ReadMacroStringEx(SmartIndent::CommonMacros, tr("smart indent common initialization macros"))) {
            return;
        }

        initialized = true;
    }

    if (!indentMacros->initMacro.isNull()) {
        if (!ReadMacroStringEx(indentMacros->initMacro, tr("smart indent initialization macro"))) {
            return;
        }
    }

    // Compile the newline and modify macros and attach them to the window
    int stoppedAt;
    QString errMsg;

    auto winData = std::make_unique<SmartIndentData>();
    winData->newlineMacro = ParseMacro(indentMacros->newlineMacro, &errMsg, &stoppedAt);

    if (!winData->newlineMacro) {
        Preferences::reportError(this, indentMacros->newlineMacro, stoppedAt, tr("newline macro"), errMsg);
        return;
    }

    if (indentMacros->modMacro.isNull()) {
        winData->modMacro = nullptr;
    } else {
        winData->modMacro = ParseMacro(indentMacros->modMacro, &errMsg, &stoppedAt);
        if (!winData->modMacro) {

            delete winData->newlineMacro;
            Preferences::reportError(this, indentMacros->modMacro, stoppedAt, tr("smart indent modify macro"), errMsg);
            return;
        }
    }

    smartIndentData_ = std::move(winData);
}

/*
** present dialog for selecting a target window to move this document
** into. Do nothing if there is only one shell window opened.
*/
void DocumentWidget::moveDocument(MainWindow *fromWindow) {

    // all windows
    std::vector<MainWindow *> allWindows = MainWindow::allWindows();

    // except for the source window
    allWindows.erase(std::remove_if(allWindows.begin(), allWindows.end(), [fromWindow](MainWindow *window) {
        return window == fromWindow;
    }), allWindows.end());

    // stop here if there's no other window to move to
    if (allWindows.empty()) {
        return;
    }

    auto dialog = std::make_unique<DialogMoveDocument>(this);

    // load them into the dialog
    for(MainWindow *win : allWindows) {
        dialog->addItem(win);
    }

    // reset the dialog and display it
    dialog->resetSelection();
    dialog->setLabel(filename_);
    dialog->setMultipleDocuments(fromWindow->TabCount() > 1);
    int r = dialog->exec();

    if(r == QDialog::Accepted) {

        int selection = dialog->selectionIndex();

        // get the this to move document into
        MainWindow *targetWin = allWindows[static_cast<size_t>(selection)];

        // move top document
        if (dialog->moveAllSelected()) {
            // move all documents
            for(DocumentWidget *document : fromWindow->openDocuments()) {

                targetWin->tabWidget()->addTab(document, document->filename_);
                RaiseFocusDocumentWindow(true);
                targetWin->show();
            }
        } else {
            targetWin->tabWidget()->addTab(this, filename_);
            RaiseFocusDocumentWindow(true);
            targetWin->show();
        }

        // if we just emptied the window, then delete it
        if(fromWindow->TabCount() == 0) {
            fromWindow->deleteLater();
        }
    }
}

/*
** Turn on and off the display of the statistics line
*/
void DocumentWidget::ShowStatsLine(bool state) {

    /* In continuous wrap mode, text widgets must be told to keep track of
       the top line number in absolute (non-wrapped) lines, because it can
       be a costly calculation, and is only needed for displaying line
       numbers, either in the stats line, or along the left margin */
    for(TextArea *area : textPanes()) {
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
    for(TextArea *area : textPanes()) {
        area->setWrapMargin(margin);
    }
}


/*
** Set the fonts for "window" from a font name, and updates the display.
** Also updates window->fontList_ which is used for statistics line.
*/
void DocumentWidget::action_Set_Fonts(const QString &fontName, const QString &italicName, const QString &boldName, const QString &boldItalicName) {

    emit_event("set_fonts", fontName, italicName, boldName, boldItalicName);

    // Check which fonts have changed
    bool primaryChanged = fontName != fontName_;

    bool highlightChanged = false;
    if (italicName != italicFontName_) {
        highlightChanged = true;
    }

    if (boldName != boldFontName_) {
        highlightChanged = true;
    }

    if (boldItalicName != boldItalicFontName_) {
        highlightChanged = true;
    }

    if (!primaryChanged && !highlightChanged)
        return;

    if (primaryChanged) {
        fontName_   = fontName;
        fontStruct_ = Font::fromString(fontName_);
    }

    if (highlightChanged) {
        italicFontName_       = italicName;
        boldFontName_         = boldName;
        boldItalicFontName_   = boldItalicName;
		
        italicFontStruct_     = Font::fromString(italicName);
        boldFontStruct_       = Font::fromString(boldName);
        boldItalicFontStruct_ = Font::fromString(boldItalicName);
    }

    // Change the primary font in all the widgets
    if (primaryChanged) {
        for(TextArea *area : textPanes()) {
            area->setFont(fontStruct_);
        }
    }

    /* Change the highlight fonts, even if they didn't change, because
       primary font is read through the style table for syntax highlighting */
    if (highlightData_) {
        UpdateHighlightStylesEx();
    }
}

/*
** Set the backlight character class string
*/
void DocumentWidget::SetBacklightChars(const QString &applyBacklightTypes) {

    backlightChars_     = !applyBacklightTypes.isNull();
    backlightCharTypes_ = applyBacklightTypes;

    for(TextArea *area : textPanes()) {
        area->setBacklightCharTypes(backlightCharTypes_);
    }
}

/**
 * @brief DocumentWidget::SetShowStatisticsLine
 * @param value
 */
void DocumentWidget::SetShowStatisticsLine(bool value) {

    emit_event("set_statistics_line", value ? QLatin1String("1") : QLatin1String("0"));

    // stats line is a shell-level item, so we toggle the button state
    // regardless of it's 'topness'
    if(auto win = MainWindow::fromDocument(this)) {
        no_signals(win->ui.action_Statistics_Line)->setChecked(value);
    }

    showStats_ = value;
}

/**
 * @brief DocumentWidget::GetShowStatisticsLine
 * @return
 */
bool DocumentWidget::GetShowStatisticsLine() const {
    return showStats_;
}

/**
 * @brief DocumentWidget::GetMatchSyntaxBased
 * @return
 */
bool DocumentWidget::GetMatchSyntaxBased() const {
    return matchSyntaxBased_;
}

/**
 * @brief DocumentWidget::SetMatchSyntaxBased
 * @param value
 */
void DocumentWidget::SetMatchSyntaxBased(bool value) {

    emit_event("set_match_syntax_based", value ? QLatin1String("1") : QLatin1String("0"));

    if(IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Matching_Syntax)->setChecked(value);
        }
    }

    matchSyntaxBased_ = value;
}

/**
 * @brief DocumentWidget::GetOverstrike
 * @return
 */
bool DocumentWidget::GetOverstrike() const {
    return overstrike_;
}

/*
** Set insert/overstrike mode
*/
void DocumentWidget::SetOverstrike(bool overstrike) {

    emit_event("set_overtype_mode", overstrike ? QLatin1String("1") : QLatin1String("0"));

    if(IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Overtype)->setChecked(overstrike);
        }
    }

    for(TextArea *area : textPanes()) {
        area->setOverstrike(overstrike);
    }

    overstrike_ = overstrike;
}

/**
 * @brief DocumentWidget::gotoAP
 * @param area
 * @param lineNum
 * @param column
 */
void DocumentWidget::gotoAP(TextArea *area, int64_t lineNum, int64_t column) {

    TextCursor position;

	// User specified column, but not line number
    if (lineNum == -1) {
		position = area->TextGetCursorPos();

        int64_t curCol;
        if (!area->TextDPosToLineAndCol(position, &lineNum, &curCol)) {
			return;
		}
	} else if (column == -1) {
		// User didn't specify a column
        SelectNumberedLineEx(area, lineNum);
		return;
	}

    position = area->TextDLineAndColToPos(lineNum, column);
	if (position == -1) {
		return;
	}

	area->TextSetCursorPos(position);
}

/**
 * @brief DocumentWidget::SetColors
 * @param textFg
 * @param textBg
 * @param selectFg
 * @param selectBg
 * @param hiliteFg
 * @param hiliteBg
 * @param lineNoFg
 * @param cursorFg
 */
void DocumentWidget::SetColors(const QString &textFg, const QString &textBg, const QString &selectFg, const QString &selectBg, const QString &hiliteFg, const QString &hiliteBg, const QString &lineNoFg, const QString &cursorFg) {

    QColor textFgPix   = X11Colors::fromString(textFg);
    QColor textBgPix   = X11Colors::fromString(textBg);
    QColor selectFgPix = X11Colors::fromString(selectFg);
    QColor selectBgPix = X11Colors::fromString(selectBg);
    QColor hiliteFgPix = X11Colors::fromString(hiliteFg);
    QColor hiliteBgPix = X11Colors::fromString(hiliteBg);
    QColor lineNoFgPix = X11Colors::fromString(lineNoFg);
    QColor cursorFgPix = X11Colors::fromString(cursorFg);


    for(TextArea *area : textPanes()) {
        area->TextDSetColors(textFgPix,
                             textBgPix,
                             selectFgPix,
                             selectBgPix,
                             hiliteFgPix,
                             hiliteBgPix,
                             lineNoFgPix,
                             cursorFgPix);
    }

    // Redo any syntax highlighting
    if (highlightData_) {
        UpdateHighlightStylesEx();
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
void DocumentWidget::SetModeMessageEx(const QString &message) {

    if(message.isNull()) {
        return;
    }

    /* this document may be hidden (not on top) or later made hidden,
       so we save a copy of the mode message, so we can restore the
       statsline when the document is raised to top again */
    modeMessage_ = message;

    ui.labelFileAndSize->setText(message);

    // Don't invoke the stats line again, if stats line is already displayed.
    if (!showStats_) {
        ShowStatsLine(true);
    }
}

/*
** Clear special statistics line message set in SetModeMessage, returns
** the statistics line to its original state as set in window->showStats_
*/
void DocumentWidget::ClearModeMessage() {

    if (!modeMessageDisplayed()) {
        return;
    }

    modeMessage_ = QString();

    /*
     * Remove the stats line only if indicated by it's window state.
     */
    if (!showStats_) {
        ShowStatsLine(false);
    }

    UpdateStatsLine(nullptr);
}

// Decref the default calltips file(s) for this window
void DocumentWidget::UnloadLanguageModeTipsFileEx() {

    const size_t mode = languageMode_;
    if (mode != PLAIN_LANGUAGE_MODE && !Preferences::LanguageModes[mode].defTipsFile.isNull()) {
        Tags::DeleteTagsFileEx(Preferences::LanguageModes[mode].defTipsFile, Tags::SearchMode::TIP, false);
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
**   ACCUMULATE     	Causes output from the command to be saved up until
**  	    	    	the command completes.
**   ERROR_DIALOGS  	Presents stderr output separately in popup a dialog,
**  	    	    	and also reports failed exit status as a popup dialog
**  	    	    	including the command output.
**   REPLACE_SELECTION  Causes output to replace the selection in textW.
**   RELOAD_FILE_AFTER  Causes the file to be completely reloaded after the
**  	    	    	command completes.
**   OUTPUT_TO_DIALOG   Send output to a pop-up dialog instead of textW
**   OUTPUT_TO_STRING   Output to a macro-language string instead of a text
**  	    	    	widget or dialog.
**
** REPLACE_SELECTION, ERROR_DIALOGS, and OUTPUT_TO_STRING can only be used
** along with ACCUMULATE (these operations can't be done incrementally).
*/
void DocumentWidget::issueCommandEx(MainWindow *window, TextArea *area, const QString &command, const QString &input, int flags, TextCursor replaceLeft, TextCursor replaceRight, CommandSource source) {

    // verify consistency of input parameters
    if ((flags & ERROR_DIALOGS || flags & REPLACE_SELECTION || flags & OUTPUT_TO_STRING) && !(flags & ACCUMULATE)) {
        return;
    }

    DocumentWidget *document = this;

    /* a shell command called from a macro must be executed in the same
       window as the macro, regardless of where the output is directed,
       so the user can cancel them as a unit */
    if (source == CommandSource::Macro) {
        document = MacroRunDocumentEx();
    }

    // put up a watch cursor over the waiting window
    if (source == CommandSource::User) {
        setCursor(Qt::WaitCursor);
    }

    // enable the cancel menu item
    if (source == CommandSource::User) {
        window->ui.action_Cancel_Shell_Command->setEnabled(true);
    }

    // create the process and connect the output streams to the readyRead events
    auto process = new QProcess(this);

    connect(process,
            static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            document,
            &DocumentWidget::processFinished);

    // support for merged output if we are not using ERROR_DIALOGS
    if (flags & ERROR_DIALOGS) {
        connect(process, &QProcess::readyReadStandardError, this, [this]() {
            QByteArray data = shellCmdData_->process->readAllStandardError();
            shellCmdData_->standardError.append(data);
        });

        connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
            QByteArray data = shellCmdData_->process->readAllStandardOutput();
            shellCmdData_->standardOutput.append(data);
        });
    } else {
        process->setProcessChannelMode(QProcess::MergedChannels);

        connect(process, &QProcess::readyRead, this, [this]() {
            QByteArray data = shellCmdData_->process->readAll();
            shellCmdData_->standardOutput.append(data);
        });
    }

    // start it off!
    QStringList args;
    args << QLatin1String("-c");
    args << command;
    process->start(Preferences::GetPrefShell(), args);

    // if there's nothing to write to the process' stdin, close it now, otherwise
    // write it to the process
    if(!input.isEmpty()) {
        process->write(input.toLocal8Bit());
    }

    process->closeWriteChannel();

    /* Create a data structure for passing process information around
       amongst the callback routines which will process i/o and completion */
    auto cmdData = std::make_unique<ShellCommandData>();
    cmdData->process     = process;
    cmdData->flags       = flags;
    cmdData->area        = area;
    cmdData->bannerIsUp  = false;
    cmdData->source      = source;
    cmdData->leftPos     = replaceLeft;
    cmdData->rightPos    = replaceRight;

    document->shellCmdData_ = std::move(cmdData);

    // Set up timer proc for putting up banner when process takes too long
    if (source == CommandSource::User) {
        connect(&document->shellCmdData_->bannerTimer, &QTimer::timeout, document, &DocumentWidget::shellBannerTimeoutProc);
        document->shellCmdData_->bannerTimer.setSingleShot(true);
        document->shellCmdData_->bannerTimer.start(BANNER_WAIT_TIME);
    }

    /* If this was called from a macro, preempt the macro until shell
       command completes */
    if (source == CommandSource::Macro) {
        PreemptMacro();
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

    auto window = MainWindow::fromDocument(this);
    if(!window) {
        return;
    }

    const std::unique_ptr<ShellCommandData> &cmdData = shellCmdData_;    
    bool fromMacro = (cmdData->source == CommandSource::Macro);

    // Cancel pending timeouts
    cmdData->bannerTimer.stop();

    // Clean up waiting-for-shell-command-to-complete mode
    if (cmdData->source == CommandSource::User) {
        setCursor(Qt::ArrowCursor);
        window->ui.action_Cancel_Shell_Command->setEnabled(false);
        if (cmdData->bannerIsUp) {
            ClearModeMessage();
        }
    }

    // when this function ends, do some cleanup
    auto _ = gsl::finally([this, &cmdData, fromMacro] {
        delete cmdData->process;
        shellCmdData_ = nullptr;

        if (fromMacro) {
            ResumeMacroExecutionEx();
        }
    });

    QString errText;
    QString outText;

    // If the process was killed or became inaccessable, give up
    if (exitStatus != QProcess::NormalExit) {
        return;
    }

    // if we have terminated the process, let's be 100% sure we've gotten all input
    cmdData->process->waitForReadyRead();

    /* Assemble the output from the process' stderr and stdout streams into
       strings */
    if (cmdData->flags & ERROR_DIALOGS) {
        // make sure we got the rest and convert it to a string
        QByteArray dataErr = cmdData->process->readAllStandardError();
        cmdData->standardError.append(dataErr);
        errText = QString::fromLocal8Bit(cmdData->standardError);

        QByteArray dataOut = cmdData->process->readAllStandardOutput();
        cmdData->standardOutput.append(dataOut);
        outText = QString::fromLocal8Bit(cmdData->standardOutput);
    } else {

        QByteArray data = cmdData->process->readAll();
        cmdData->standardOutput.append(data);
        outText = QString::fromLocal8Bit(cmdData->standardOutput);
    }

    static const QRegularExpression trailingNewlines(QLatin1String("\\n+$"));

    /* Present error and stderr-information dialogs.  If a command returned
       error output, or if the process' exit status indicated failure,
       present the information to the user. */
    if (cmdData->flags & ERROR_DIALOGS) {
        bool cancel = false;
        // NOTE(eteran): assumes UNIX return code style!
        bool failure = exitCode != 0;
        bool errorReport = !errText.isEmpty();

        static constexpr int DF_MAX_MSG_LENGTH = 4096;

        if (failure && errorReport) {
            errText.remove(trailingNewlines);
            errText.truncate(DF_MAX_MSG_LENGTH);

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
            outText.truncate(DF_MAX_MSG_LENGTH);

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
            errText.truncate(DF_MAX_MSG_LENGTH);

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
        if (cmdData->flags & OUTPUT_TO_DIALOG) {
            outText.remove(trailingNewlines);

            if (!outText.isEmpty()) {
                auto dialog = new DialogOutput(this);
                dialog->setText(outText);
                dialog->show();
            }
        } else if (cmdData->flags & OUTPUT_TO_STRING) {
            ReturnShellCommandOutputEx(this, outText, exitCode);
        } else {

            std::string output_string = outText.toStdString();

            auto area = cmdData->area;
            TextBuffer *buf = area->TextGetBuffer();

            if (cmdData->flags & REPLACE_SELECTION) {
                TextCursor reselectStart = buf->primary.rectangular ? TextCursor(-1) : buf->primary.start;
                buf->BufReplaceSelectedEx(output_string);

                area->TextSetCursorPos(buf->BufCursorPosHint());

                if (reselectStart != -1) {
                    buf->BufSelect(reselectStart, reselectStart + static_cast<int64_t>(output_string.size()));
                }
            } else {
                safeBufReplace(buf, &cmdData->leftPos, &cmdData->rightPos, output_string);
                area->TextSetCursorPos(cmdData->leftPos + outText.size());
            }
        }

        // If the command requires the file to be reloaded afterward, reload it
        if (cmdData->flags & RELOAD_FILE_AFTER) {
            RevertToSaved();
        }
    }
}

/*
** Cancel the shell command in progress
*/
void DocumentWidget::AbortShellCommandEx() {
    if(const std::unique_ptr<ShellCommandData> &cmdData = shellCmdData_) {
        if(QProcess *process = cmdData->process) {
            process->kill();
        }
    }
}

/*
** Execute the line of text where the the insertion cursor is positioned
** as a shell command.
*/
void DocumentWidget::ExecCursorLineEx(TextArea *area, CommandSource source) {

    auto window = MainWindow::fromDocument(this);
    if(!window) {
        return;
    }

    TextCursor left;
    TextCursor right;
    TextCursor insertPos;
    int64_t line;
    int64_t column;

    // Can't do two shell commands at once in the same window
    if (shellCmdData_) {
        QApplication::beep();
        return;
    }

    // get all of the text on the line with the insert position
    TextCursor pos = area->TextGetCursorPos();

    if (!buffer_->GetSimpleSelection(&left, &right)) {
        left  = pos;
        right = pos;
        left  = buffer_->BufStartOfLine(left);
        right = buffer_->BufEndOfLine(right);
        insertPos = right;
    } else {
        insertPos = buffer_->BufEndOfLine(right);
    }

    std::string cmdText = buffer_->BufGetRangeEx(left, right);

    // insert a newline after the entire line
    buffer_->BufInsertEx(insertPos, '\n');

    /* Substitute the current file name for % and the current line number
       for # in the shell command */
    auto fullName = tr("%1%2").arg(path_, filename_);
    area->TextDPosToLineAndCol(pos, &line, &column);

    auto substitutedCommand = QString::fromStdString(cmdText);
    substitutedCommand.replace(QLatin1Char('%'), fullName);
	substitutedCommand.replace(QLatin1Char('#'), QString::number(line));

    if(substitutedCommand.isNull()) {
        QMessageBox::critical(
                    this,
                    tr("Shell Command"),
                    tr("Shell command is too long due to\n"
                       "filename substitutions with '%%' or\n"
                       "line number substitutions with '#'"));
        return;
    }

    // issue the command
    issueCommandEx(
                window,
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

    if (CheckReadOnly()) {
        return;
    }

    auto window = MainWindow::fromDocument(this);
    if(!window) {
        return;
    }

    // Can't do two shell commands at once in the same window
    if (shellCmdData_) {
        QApplication::beep();
        return;
    }

    /* Get the selection and the range in character positions that it
       occupies.  Beep and return if no selection */
    const std::string text = buffer_->BufGetSelectionTextEx();
    if (text.empty()) {
        QApplication::beep();
        return;
    }

    const TextCursor left  = buffer_->primary.start;
    const TextCursor right = buffer_->primary.end;

    // Issue the command and collect its output
    issueCommandEx(
                window,
                window->lastFocus_,
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
void DocumentWidget::DoShellMenuCmd(MainWindow *inWindow, TextArea *area, const QString &command, InSrcs input, OutDests output, bool outputReplacesInput, bool saveFirst, bool loadAfter, CommandSource source) {

    int flags = 0;
    TextCursor left  = {};
    TextCursor right = {};
    int64_t line;
    int64_t column;

    // Can't do two shell commands at once in the same window
    if (shellCmdData_) {
        QApplication::beep();
        return;
    }

    /* Substitute the current file name for % and the current line number
       for # in the shell command */
    auto fullName = tr("%1%2").arg(path_, filename_);
    TextCursor pos = area->TextGetCursorPos();
    area->TextDPosToLineAndCol(pos, &line, &column);

    QString substitutedCommand = command;
    substitutedCommand.replace(QLatin1Char('%'), fullName);
	substitutedCommand.replace(QLatin1Char('#'), QString::number(line));

    // NOTE(eteran): probably not possible for this to be true anymore...
    if(substitutedCommand.isNull()) {
        QMessageBox::critical(this,
                              tr("Shell Command"),
                              tr("Shell command is too long due to filename substitutions with '%%' or line number substitutions with '#'"));
        return;
    }

    /* Get the command input as a text string.  If there is input, errors
      shouldn't be mixed in with output, so set flags to ERROR_DIALOGS */
    std::string text;
    switch(input) {
    case FROM_SELECTION:
        text = buffer_->BufGetSelectionTextEx();
        if (text.empty()) {
            QApplication::beep();
            return;
        }
        flags |= ACCUMULATE | ERROR_DIALOGS;
        break;
    case FROM_WINDOW:
        text = buffer_->BufGetAllEx();
        flags |= ACCUMULATE | ERROR_DIALOGS;
        break;
    case FROM_EITHER:
        text = buffer_->BufGetSelectionTextEx();
        if (text.empty()) {
            text = buffer_->BufGetAllEx();
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
    switch(output) {
    case TO_DIALOG:
        flags |= OUTPUT_TO_DIALOG;
        left  = TextCursor();
        right = TextCursor();
        break;
    case TO_NEW_WINDOW:
        if(DocumentWidget *document = MainWindow::EditNewFileEx(
                    Preferences::GetPrefOpenInTab() ? inWindow : nullptr,
                    QString(),
                    false,
                    QString(),
                    path_)) {

            inWindow  = MainWindow::fromDocument(document);
            outWidget = document->firstPane();
            left      = TextCursor();
            right     = TextCursor();
            MainWindow::CheckCloseDimEx();
        }
        break;
    case TO_SAME_WINDOW:
        outWidget = area;
        if (outputReplacesInput && input != FROM_NONE) {
            if (input == FROM_WINDOW) {
                left  = TextCursor();
                right = buffer_->BufEndOfBuffer();
            } else if (input == FROM_SELECTION) {
                buffer_->GetSimpleSelection(&left, &right);
                flags |= ACCUMULATE | REPLACE_SELECTION;
            } else if (input == FROM_EITHER) {
                if (buffer_->GetSimpleSelection(&left, &right)) {
                    flags |= ACCUMULATE | REPLACE_SELECTION;
                } else {
                    left  = TextCursor();
                    right = buffer_->BufEndOfBuffer();
                }
            }
        } else {
            if (buffer_->GetSimpleSelection(&left, &right)) {
                flags |= ACCUMULATE | REPLACE_SELECTION;
            } else {
                left = right = area->TextGetCursorPos();
            }
        }
        break;
    }

    // If the command requires the file be saved first, save it
    if (saveFirst) {
        if (!SaveWindow()) {
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
    issueCommandEx(
                inWindow,
                outWidget,
                substitutedCommand,
                QString::fromStdString(text),
                flags,
                left,
                right,
                source);
}

/**
 * @brief DocumentWidget::WidgetToPaneIndex
 * @param area
 * @return
 */
int DocumentWidget::WidgetToPaneIndex(TextArea *area) const {
    return splitter_->indexOf(area);
}

/*
** Execute shell command "command", on input string "input", depositing the
** in a macro string (via a call back to ReturnShellCommandOutput).
*/
void DocumentWidget::ShellCmdToMacroStringEx(const QString &command, const QString &input) {

    // fork the command and begin processing input/output
    issueCommandEx(
                MainWindow::fromDocument(this),
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
void DocumentWidget::SetAutoScroll(int margin) {

    for(TextArea *area : textPanes()) {
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
 * ("how == REPEAT_TO_END).
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
    Program *const prog = ParseMacro(loopedCmd, &errMsg, &stoppedAt);
    if(!prog) {
        qWarning("NEdit: internal error, repeat macro syntax wrong: %s", qPrintable(errMsg));
        return;
    }

    // run the executable program
    runMacroEx(prog);
}

/**
 * @brief DocumentWidget::allDocuments
 * @return
 */
std::vector<DocumentWidget *> DocumentWidget::allDocuments() {
    std::vector<MainWindow *> windows = MainWindow::allWindows();
    std::vector<DocumentWidget *> documents;

    for(MainWindow *window : windows) {

        std::vector<DocumentWidget *> openDocuments = window->openDocuments();

        documents.reserve(documents.size() + openDocuments.size());
        documents.insert(documents.end(), openDocuments.begin(), openDocuments.end());
    }

    return documents;
}

/**
 * @brief DocumentWidget::BeginLearnEx
 */
void DocumentWidget::BeginLearnEx() {

	// If we're already in learn mode, return
    if(CommandRecorder::instance()->isRecording()) {
		return;
	}

    MainWindow *thisWindow = MainWindow::fromDocument(this);
	if(!thisWindow) {
		return;
	}

	// dim the inappropriate menus and items, and undim finish and cancel
    for(MainWindow *window : MainWindow::allWindows()) {
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
            SetModeMessageEx(tr("Learn Mode -- Use menu to finish or cancel"));
		} else {
            SetModeMessageEx(tr("Learn Mode -- Use menu to finish, press %1 to cancel").arg(cCancel));
		}
	} else {
		if (cCancel.isEmpty()) {
            SetModeMessageEx(tr("Learn Mode -- Press %1 to finish, use menu to cancel").arg(cFinish));
		} else {
            SetModeMessageEx(tr("Learn Mode -- Press %1 to finish, %2 to cancel").arg(cFinish, cCancel));
		}
	}
}

/*
** Read an NEdit macro file.  Extends the syntax of the macro parser with
** define keyword, and allows intermixing of defines with immediate actions.
*/
bool DocumentWidget::ReadMacroFileEx(const QString &fileName, bool warnNotExist) {

	/* read-in macro file and force a terminating \n, to prevent syntax
	** errors with statements on the last line
	*/
    QString fileString = ReadAnyTextFileEx(fileName, /*forceNL=*/true);
	if (fileString.isNull()) {
        if (warnNotExist) {
            QMessageBox::critical(this,
                                  tr("Read Macro"),
                                  tr("Error reading macro file %1: %2").arg(fileName, ErrorString(errno)));
		}
		return false;
	}


	// Parse fileString
	return readCheckMacroStringEx(this, fileString, this, fileName, nullptr);
}

/*
** Run a pre-compiled macro, changing the interface state to reflect that
** a macro is running, and handling preemption, resumption, and cancellation.
** frees prog when macro execution is complete;
*/
void DocumentWidget::runMacroEx(Program *prog) {

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
    if(auto win = MainWindow::fromDocument(this)) {
        win->ui.action_Cancel_Learn->setText(tr("Cancel Macro"));
        win->ui.action_Cancel_Learn->setEnabled(true);
    }

    /* Create a data structure for passing macro execution information around
       amongst the callback routines which will process i/o and completion */
    auto cmdData = std::make_unique<MacroCommandData>();
    cmdData->bannerIsUp         = false;
    cmdData->closeOnCompletion  = false;
    cmdData->program            = prog;
    cmdData->context            = nullptr;

    macroCmdData_ = std::move(cmdData);

    // Set up timer proc for putting up banner when macro takes too long
    QObject::connect(&macroCmdData_->bannerTimer, &QTimer::timeout, this, &DocumentWidget::macroBannerTimeoutProc);
    macroCmdData_->bannerTimer.setSingleShot(true);
    macroCmdData_->bannerTimer.start(BANNER_WAIT_TIME);

    // Begin macro execution
    DataValue result;
    QString errMsg;
    const int stat = ExecuteMacroEx(this, prog, {}, &result, macroCmdData_->context, &errMsg);

    switch(stat) {
    case MACRO_ERROR:
        finishMacroCmdExecutionEx();
        QMessageBox::critical(this, tr("Macro Error"), tr("Error executing macro: %1").arg(errMsg));
        break;
    case MACRO_DONE:
        finishMacroCmdExecutionEx();
        break;
    case MACRO_TIME_LIMIT:
        ResumeMacroExecutionEx();
        break;
    case MACRO_PREEMPT:
        break;
    }
}

/*
** Clean up after the execution of a macro command: free memory, and restore
** the user interface state.
*/
void DocumentWidget::finishMacroCmdExecutionEx() {

    const bool closeOnCompletion = macroCmdData_->closeOnCompletion;

    // Cancel pending timeout and work proc
    macroCmdData_->bannerTimer.stop();
    macroCmdData_->continuationTimer.stop();

    // Clean up waiting-for-macro-command-to-complete mode
    setCursor(Qt::ArrowCursor);

    // enable the cancel menu item
    if(auto win = MainWindow::fromDocument(this)) {
        win->ui.action_Cancel_Learn->setText(tr("Cancel Learn"));
        win->ui.action_Cancel_Learn->setEnabled(false);
    }

    if (macroCmdData_->bannerIsUp) {
        ClearModeMessage();
    }

    // Free execution information
    delete macroCmdData_->program;
    macroCmdData_ = nullptr;

    /* If macro closed its own window, window was made empty and untitled,
       but close was deferred until completion.  This is completion, so if
       the window is still empty, do the close */
    if (closeOnCompletion && !filenameSet_ && !fileChanged_) {
        CloseDocument();
    }
}

/**
 * @brief DocumentWidget::continueWorkProcEx
 * @return
 */
DocumentWidget::MacroContinuationCode DocumentWidget::continueWorkProcEx() {


    // on the last loop, it may have been set to nullptr!
    if(!macroCmdData_) {
        return MacroContinuationCode::Stop;
    }

    QString errMsg;
    DataValue result;
    const int stat = ContinueMacroEx(macroCmdData_->context, &result, &errMsg);

    switch(stat) {
    case MACRO_ERROR:
        finishMacroCmdExecutionEx();
        QMessageBox::critical(this, tr("Macro Error"), tr("Error executing macro: %1").arg(errMsg));
        return MacroContinuationCode::Stop;
    case MACRO_DONE:
        finishMacroCmdExecutionEx();
        return MacroContinuationCode::Stop;
    case MACRO_PREEMPT:
        return MacroContinuationCode::Stop;
    case MACRO_TIME_LIMIT:
        // Macro exceeded time slice, re-schedule it
        return MacroContinuationCode::Continue;
    default:
        return MacroContinuationCode::Stop;
    }
}

/*
** Continue with macro execution after preemption.  Called by the routines
** whose actions cause preemption when they have completed their lengthy tasks.
** Re-establishes macro execution work proc.
*/
void DocumentWidget::ResumeMacroExecutionEx() {

    if(const std::shared_ptr<MacroCommandData> &cmdData = macroCmdData_) {

        // create a background task that will run so long as the function returns false
        connect(&cmdData->continuationTimer, &QTimer::timeout, this, [cmdData, this]() {
            if(continueWorkProcEx() == MacroContinuationCode::Stop) {
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
void DocumentWidget::FlashMatchingEx(TextArea *area) {

    // if a marker is already drawn, erase it and cancel the timeout
    if (flashTimer_->isActive()) {
        eraseFlashEx();
        flashTimer_->stop();
    }

    // no flashing required
    if (showMatchingStyle_ == ShowMatchingStyle::None) {
        return;
    }

    // don't flash matching characters if there's a selection
    if (buffer_->primary.selected) {
        return;
    }

    // get the character to match and the position to start from
    const TextCursor currentPos = area->TextGetCursorPos();
    if(currentPos == 0) {
        return;
    }

    const TextCursor pos = currentPos - 1;

    const char ch = buffer_->BufGetCharacter(pos);

    Style style = GetHighlightInfoEx(pos);


    // is the character one we want to flash?
    auto matchIt = std::find_if(std::begin(FlashingChars), std::end(FlashingChars), [ch](const CharMatchTable &entry) {
        return entry.c == ch;
    });

    if(matchIt == std::end(FlashingChars)) {
        return;
    }

    /* constrain the search to visible text only when in single-pane mode
       AND using delimiter flashing (otherwise search the whole buffer) */
    bool constrain = (textPanes().empty() && (showMatchingStyle_ == ShowMatchingStyle::Delimiter));

    TextCursor startPos;
    TextCursor endPos;
    TextCursor searchPos;

    if (matchIt->direction == Direction::Backward) {
        startPos  = constrain ? area->TextFirstVisiblePos() : buffer_->BufStartOfBuffer();
        endPos    = pos;
        searchPos = endPos;
    } else {
        startPos  = pos;
        endPos    = constrain ? area->TextLastVisiblePos() : buffer_->BufEndOfBuffer();
        searchPos = startPos;
    }

    // do the search
    boost::optional<TextCursor> matchPos = findMatchingCharEx(ch, style, searchPos, startPos, endPos);
    if (!matchPos) {
        return;
    }

    if (showMatchingStyle_ == ShowMatchingStyle::Delimiter) {
        // Highlight either the matching character ...
        buffer_->BufHighlight(*matchPos, *matchPos + 1);
    } else {
        // ... or the whole range.
        if (matchIt->direction == Direction::Backward) {
            buffer_->BufHighlight(*matchPos, pos + 1);
        } else {
            buffer_->BufHighlight(*matchPos + 1, pos);
        }
    }

    flashTimer_->start();
}

/*
** Erase the marker drawn on a matching parenthesis bracket or brace
** character.
*/
void DocumentWidget::eraseFlashEx() {
    buffer_->BufUnhighlight();
}

/*
** Find the pattern set matching the window's current language mode, or
** tell the user if it can't be done (if warn is true) and return nullptr.
*/
PatternSet *DocumentWidget::findPatternsForWindowEx(bool warn) {

    // Find the window's language mode.  If none is set, warn user
    QString modeName = Preferences::LanguageModeName(languageMode_);
    if(modeName.isNull()) {
        if (warn) {
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
    if(!patterns) {
        if (warn) {
            QMessageBox::warning(this,
                                 tr("Language Mode"),
                                 tr("Syntax highlighting is not available in language\n"
                                    "mode %1.\n\n"
                                    "You can create new syntax highlight patterns in the\n"
                                    "Preferences -> Default Settings -> Syntax Highlighting\n"
                                    "dialog, or choose a different language mode from:\n"
                                    "Preferences -> Language Mode.").arg(modeName));
            return nullptr;
        }
    }

    return patterns;
}

/*
** Free highlighting data from a window destined for destruction, without
** redisplaying.
*/
void DocumentWidget::FreeHighlightingDataEx() {

    if (!highlightData_) {
        return;
    }

    highlightData_ = nullptr;

    /* The text display may make a last desperate attempt to access highlight
       information when it is destroyed, which would be a disaster. */
    for(TextArea *area : textPanes()) {
        area->setStyleBuffer(nullptr);
    }
}

/*
** Change highlight fonts and/or styles in a highlighted window, without
** re-parsing.
*/
void DocumentWidget::UpdateHighlightStylesEx() {

    std::unique_ptr<WindowHighlightData> &oldHighlightData = highlightData_;

    // Do nothing if window not highlighted
    if (!oldHighlightData) {
        return;
    }

    // Find the pattern set for the window's current language mode
    PatternSet *patterns = findPatternsForWindowEx(/*warn=*/false);
    if(!patterns) {
        StopHighlightingEx();
        return;
    }

    // Build new patterns
    std::unique_ptr<WindowHighlightData> newHighlightData = createHighlightDataEx(patterns);
    if(!newHighlightData) {
        StopHighlightingEx();
        return;
    }

    /* Update highlight pattern data in the window data structure, but
       preserve all of the effort that went in to parsing the buffer
       by swapping it with the empty one in highlightData (which is then
       freed in freeHighlightData) */
    newHighlightData->styleBuffer = oldHighlightData->styleBuffer;

    highlightData_ = std::move(newHighlightData);

    /* Attach new highlight information to text widgets in each pane
       (and redraw) */
    for(TextArea *area : textPanes()) {
        AttachHighlightToWidgetEx(area);
    }
}

/*
** Find the HighlightPattern structure with a given name in the window.
*/
HighlightPattern *DocumentWidget::FindPatternOfWindowEx(const QString &name) const {

    if(const std::unique_ptr<WindowHighlightData> &hData = highlightData_) {
        if (PatternSet *set = hData->patternSetForWindow) {

            for(HighlightPattern &pattern : set->patterns) {
                if (pattern.name == name) {
                    return &pattern;
                }
            }
        }
    }
    return nullptr;
}

QColor DocumentWidget::GetHighlightBGColorOfCodeEx(size_t hCode) const {
    StyleTableEntry *entry = styleTableEntryOfCodeEx(hCode);

    if (entry && !entry->bgColorName.isNull()) {
        return entry->bgColor;
    } else {
        // pick up background color of the (first) text widget of the window
        return firstPane()->getBackgroundPixel();
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
Style DocumentWidget::GetHighlightInfoEx(TextCursor pos) {

    HighlightData *pattern = nullptr;
    const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_;
    if (!highlightData) {
        return Style();
    }

    // Be careful with signed/unsigned conversions. NO conversion here!
    int style = highlightData->styleBuffer->BufGetCharacter(pos);

    // Beware of unparsed regions.
    if (style == UNFINISHED_STYLE) {
        handleUnparsedRegionEx(highlightData->styleBuffer, pos);
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
int64_t DocumentWidget::StyleLengthOfCodeFromPosEx(TextCursor pos) {

    const TextCursor oldPos = pos;

    if(const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {
        if (const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer) {

            auto hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
            if (!hCode) {
                return 0;
            }

            if (hCode == UNFINISHED_STYLE) {
                // encountered "unfinished" style, trigger parsing
                handleUnparsedRegionEx(highlightData->styleBuffer, pos);
                hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
            }

            StyleTableEntry *entry = styleTableEntryOfCodeEx(hCode);
            if(!entry) {
                return 0;
            }

            QString checkStyleName = entry->styleName;

            while (hCode == UNFINISHED_STYLE || ((entry = styleTableEntryOfCodeEx(hCode)) && entry->styleName == checkStyleName)) {
                if (hCode == UNFINISHED_STYLE) {
                    // encountered "unfinished" style, trigger parsing, then loop
                    handleUnparsedRegionEx(highlightData->styleBuffer, pos);
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
QString DocumentWidget::HighlightNameOfCodeEx(size_t hCode) const {
    if(StyleTableEntry *entry = styleTableEntryOfCodeEx(hCode)) {
        return entry->highlightName;
    }

    return QString();
}

QString DocumentWidget::HighlightStyleOfCodeEx(size_t hCode) const {
    if(StyleTableEntry *entry = styleTableEntryOfCodeEx(hCode)) {
        return entry->styleName;
    }

    return QString();
}

QColor DocumentWidget::HighlightColorValueOfCodeEx(size_t hCode) const {
    if (StyleTableEntry *entry = styleTableEntryOfCodeEx(hCode)) {
        return entry->color;
    }

    // pick up foreground color of the (first) text widget of the window
    return firstPane()->getForegroundPixel();
}

/*
** Returns a pointer to the entry in the style table for the entry of code
** hCode (if any).
*/
StyleTableEntry *DocumentWidget::styleTableEntryOfCodeEx(size_t hCode) const {
    const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_;

    hCode -= UNFINISHED_STYLE; // get the correct index value
    if (!highlightData || hCode >= highlightData->styleTable.size()) {
        return nullptr;
    }

    return &highlightData->styleTable[hCode];
}


/*
** Picks up the entry in the style buffer for the position (if any). Rather
** like styleOfPos() in TextDisplay.c. Returns the style code or zero.
*/
size_t DocumentWidget::HighlightCodeOfPosEx(TextCursor pos) {

    size_t hCode = 0;
    if(const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {

        if (const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer) {

            hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
            if (hCode == UNFINISHED_STYLE) {
                // encountered "unfinished" style, trigger parsing
                handleUnparsedRegionEx(highlightData->styleBuffer, pos);
                hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
            }
        }
    }

    return hCode;
}

/*
** Returns the length over which a particular highlight code applies, starting
** at pos. If the initial code value *checkCode is zero, the highlight code of
** pos is used.
*/
/* YOO: This is called from only one other function, which uses a constant
    for checkCode and never evaluates it after the call. */
int64_t DocumentWidget::HighlightLengthOfCodeFromPosEx(TextCursor pos, size_t *checkCode) {

    const TextCursor oldPos = pos;

    if(const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {

        if (const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer) {

            auto hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
            if (!hCode) {
                return 0;
            }

            if (hCode == UNFINISHED_STYLE) {
                // encountered "unfinished" style, trigger parsing
                handleUnparsedRegionEx(highlightData->styleBuffer, pos);
                hCode = static_cast<uint8_t>(styleBuf->BufGetCharacter(pos));
            }

            if (*checkCode == 0) {
                *checkCode = hCode;
            }

            while (hCode == *checkCode || hCode == UNFINISHED_STYLE) {
                if (hCode == UNFINISHED_STYLE) {
                    // encountered "unfinished" style, trigger parsing, then loop
                    handleUnparsedRegionEx(highlightData->styleBuffer, pos);
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
void DocumentWidget::handleUnparsedRegionEx(const std::shared_ptr<TextBuffer> &styleBuf, TextCursor pos) const {

    TextBuffer *buf = buffer_;
    const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_;

    const ReparseContext &context = highlightData->contextRequirements;
    HighlightData *pass2Patterns  = highlightData->pass2Patterns;

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

    for (TextCursor p = beginParse; p >= beginSafety; p--) {
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

    for (TextCursor p = pos; p < endSafety; p++) {
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
    /* printf("callback pass2 parsing from %d thru %d w/ safety from %d thru %d\n",
            beginParse, endParse, beginSafety, endSafety); */

    std::string str             = buf->BufGetRangeEx(beginSafety, endSafety);
    const char *string          = &str[0];
    const char *const match_to  = string + str.size();

    std::string styleStr  = styleBuf->BufGetRangeEx(beginSafety, endSafety);
    char *styleString     = &styleStr[0];
    char *stylePtr        = &styleStr[0];

    // Parse it with pass 2 patterns
    int prevChar = Highlight::getPrevChar(buf, beginSafety);

    Highlight::parseString(
        pass2Patterns,
        string,
        string + str.size(),
        string,
        stylePtr,
        endParse - beginSafety,
        &prevChar,
        GetWindowDelimiters(),
        string,
        match_to);

    /* Update the style buffer the new style information, but only between
       beginParse and endParse.  Skip the safety region */
    auto view = view::string_view(&styleString[beginParse - beginSafety], static_cast<size_t>(endParse - beginParse));
    styleBuf->BufReplaceEx(beginParse, endParse, view);
}

/*
** Turn on syntax highlighting.  If "warn" is true, warn the user when it
** can't be done, otherwise, just return.
*/
void DocumentWidget::StartHighlightingEx(bool warn) {

    int prevChar = -1;

    /* Find the pattern set matching the window's current
       language mode, tell the user if it can't be done */
    PatternSet *patterns = findPatternsForWindowEx(warn);
    if(!patterns) {
        return;
    }

    // Compile the patterns
    std::unique_ptr<WindowHighlightData> highlightData = createHighlightDataEx(patterns);
    if(!highlightData) {
        return;
    }

    // Prepare for a long delay, refresh display and put up a watch cursor
    const QCursor prevCursor = cursor();
    setCursor(Qt::WaitCursor);

    const int64_t bufLength = buffer_->BufGetLength();

    /* Parse the buffer with pass 1 patterns.  If there are none, initialize
       the style buffer to all UNFINISHED_STYLE to trigger parsing later */
    std::vector<char> styleString(static_cast<size_t>(bufLength) + 1);
    char *const styleBegin = &styleString[0];
    char *stylePtr = styleBegin;

    if (!highlightData->pass1Patterns) {
        for (int i = 0; i < bufLength; ++i) {
            *stylePtr++ = UNFINISHED_STYLE;
        }
    } else {

        view::string_view bufString = buffer_->BufAsStringEx();
        const char *stringPtr       = bufString.data();
        const char *const match_to  = bufString.data() + bufString.size();

        Highlight::parseString(
            highlightData->pass1Patterns,
            bufString.data(),
            bufString.data() + bufString.size(),
            stringPtr,
            stylePtr,
            bufLength,
            &prevChar,
            GetWindowDelimiters(),
            stringPtr,
            match_to);
    }

    highlightData->styleBuffer->BufSetAllEx(view::string_view(styleBegin, static_cast<size_t>(stylePtr - styleBegin)));

    // install highlight pattern data in the window data structure
    highlightData_ = std::move(highlightData);

    // Attach highlight information to text widgets in each pane
    for(TextArea *area : textPanes()) {
        AttachHighlightToWidgetEx(area);
    }

    setCursor(prevCursor);
}

/*
** Attach style information from a window's highlight data to a
** text widget and redisplay.
*/
void DocumentWidget::AttachHighlightToWidgetEx(TextArea *area) {
    if(const std::unique_ptr<WindowHighlightData> &highlightData = highlightData_) {
        area->TextDAttachHighlightData(
                    highlightData->styleBuffer,
                    highlightData->styleTable,
                    UNFINISHED_STYLE,
                    handleUnparsedRegionCB,
                    this);
    }
}

/*
** Create complete syntax highlighting information from "patternSrc", using
** highlighting fonts from "window", includes pattern compilation.  If errors
** are encountered, warns user with a dialog and returns nullptr.  To free the
** allocated components of the returned data structure, use freeHighlightData.
*/
std::unique_ptr<WindowHighlightData> DocumentWidget::createHighlightDataEx(PatternSet *patSet) {

    std::vector<HighlightPattern> &patterns = patSet->patterns;

    int contextLines = patSet->lineContext;
    int contextChars = patSet->charContext;

    HighlightData *pass1Pats;
    HighlightData *pass2Pats;

    // The highlighting code can't handle empty pattern sets, quietly say no
    if (patterns.empty()) {
        return nullptr;
    }

    // Check that the styles and parent pattern names actually exist
    if (!Highlight::NamedStyleExists(QLatin1String("Plain"))) {
        QMessageBox::warning(this, tr("Highlight Style"), tr("Highlight style \"Plain\" is missing"));
        return nullptr;
    }

    for(const HighlightPattern &pattern : patterns) {
        if (!pattern.subPatternOf.isNull() && Highlight::indexOfNamedPattern(patterns, pattern.subPatternOf) == -1) {
            QMessageBox::warning(
                        this,
                        tr("Parent Pattern"),
                        tr("Parent field \"%1\" in pattern \"%2\"\ndoes not match any highlight patterns in this set").arg(pattern.subPatternOf, pattern.name));
            return nullptr;
        }
    }

    for(const HighlightPattern &pattern : patterns) {
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
    int i = 0;
    for(HighlightPattern &pattern : patterns) {

        if (!pattern.subPatternOf.isNull()) {
            const int parentindex = Highlight::findTopLevelParentIndex(patterns, i);
            if (parentindex == -1) {
                QMessageBox::warning(
                            this,
                            tr("Parent Pattern"),
                            tr("Pattern \"%1\" does not have valid parent").arg(pattern.name));
                return nullptr;
            }

            if (patterns[static_cast<size_t>(parentindex)].flags & DEFER_PARSING) {
                pattern.flags |= DEFER_PARSING;
            } else {
                pattern.flags &= ~DEFER_PARSING;
            }
        }

        ++i;
    }

    /* Sort patterns into those to be used in pass 1 parsing, and those to
       be used in pass 2, and add default pattern (0) to each list */
    int nPass1Patterns = 1;
    int nPass2Patterns = 1;
    for(const HighlightPattern &pattern : patterns) {
        if (pattern.flags & DEFER_PARSING) {
            nPass2Patterns++;
        } else {
            nPass1Patterns++;
        }
    }

    auto pass1PatternSrc = new HighlightPattern[static_cast<size_t>(nPass1Patterns)];
    auto pass2PatternSrc = new HighlightPattern[static_cast<size_t>(nPass2Patterns)];

    HighlightPattern *p1Ptr = pass1PatternSrc;
    HighlightPattern *p2Ptr = pass2PatternSrc;

    *p1Ptr++ = HighlightPattern(QLatin1String("Plain"));
    *p2Ptr++ = HighlightPattern(QLatin1String("Plain"));

    for(const HighlightPattern &pattern : patterns) {
        if (pattern.flags & DEFER_PARSING) {
            *p2Ptr++ = pattern;
        } else {
            *p1Ptr++ = pattern;
        }
    }

    /* If a particular pass is empty except for the default pattern, don't
       bother compiling it or setting up styles */
    if (nPass1Patterns == 1) {
        nPass1Patterns = 0;
    }

    if (nPass2Patterns == 1) {
        nPass2Patterns = 0;
    }

    // Compile patterns
    if (nPass1Patterns == 0) {
        pass1Pats = nullptr;
    } else {
        pass1Pats = compilePatternsEx({pass1PatternSrc, nPass1Patterns});
        if (!pass1Pats) {
            return nullptr;
        }
    }

    if (nPass2Patterns == 0) {
        pass2Pats = nullptr;
    } else {
        pass2Pats = compilePatternsEx({pass2PatternSrc, nPass2Patterns});
        if (!pass2Pats) {
            delete [] pass1Pats;
            return nullptr;
        }
    }

    /* Set pattern styles.  If there are pass 2 patterns, pass 1 pattern
       0 should have a default style of UNFINISHED_STYLE.  With no pass 2
       patterns, unstyled areas of pass 1 patterns should be PLAIN_STYLE
       to avoid triggering re-parsing every time they are encountered */
    const bool noPass1 = (nPass1Patterns == 0);
    const bool noPass2 = (nPass2Patterns == 0);

    if (noPass2) {
        Q_ASSERT(pass1Pats);
        pass1Pats[0].style = PLAIN_STYLE;
    } else if (noPass1) {
        Q_ASSERT(pass2Pats);
        pass2Pats[0].style = PLAIN_STYLE;
    } else {
        Q_ASSERT(pass1Pats);
        Q_ASSERT(pass2Pats);
        pass1Pats[0].style = UNFINISHED_STYLE;
        pass2Pats[0].style = PLAIN_STYLE;
    }

    for (int i = 1; i < nPass1Patterns; i++) {
        pass1Pats[i].style = gsl::narrow<uint8_t>(PLAIN_STYLE + i);
    }

    for (int i = 1; i < nPass2Patterns; i++) {
        pass2Pats[i].style = gsl::narrow<uint8_t>(PLAIN_STYLE + (noPass1 ? 0 : nPass1Patterns - 1) + i);
    }

    // Create table for finding parent styles
    std::vector<uint8_t> parentStyles;
    parentStyles.reserve(static_cast<size_t>(nPass1Patterns + nPass2Patterns + 2));

    auto parentStylesPtr = std::back_inserter(parentStyles);

    *parentStylesPtr++ = '\0';
    *parentStylesPtr++ = '\0';

    for (int i = 1; i < nPass1Patterns; i++) {
        if(pass1PatternSrc[i].subPatternOf.isNull()) {
            *parentStylesPtr++ = PLAIN_STYLE;
        } else {
            *parentStylesPtr++ = pass1Pats[Highlight::indexOfNamedPattern({pass1PatternSrc, nPass1Patterns}, pass1PatternSrc[i].subPatternOf)].style;
        }
    }

    for (int i = 1; i < nPass2Patterns; i++) {
        if(pass2PatternSrc[i].subPatternOf.isNull()) {
            *parentStylesPtr++ = PLAIN_STYLE;
        } else {
            *parentStylesPtr++ = pass2Pats[Highlight::indexOfNamedPattern({pass2PatternSrc, nPass2Patterns}, pass2PatternSrc[i].subPatternOf)].style;
        }
    }

    // Set up table for mapping colors and fonts to syntax
    std::vector<StyleTableEntry> styleTable;
    styleTable.reserve(static_cast<size_t>(nPass1Patterns + nPass2Patterns));

    auto it = std::back_inserter(styleTable);

    auto setStyleTableEntry = [this](HighlightPattern *pat) {

        StyleTableEntry p;

        p.underline     = false;
        p.highlightName = pat->name;
        p.styleName     = pat->style;
        p.font          = FontOfNamedStyleEx(pat->style);
        p.colorName     = Highlight::FgColorOfNamedStyleEx     (pat->style);
        p.bgColorName   = Highlight::BgColorOfNamedStyleEx   (pat->style);
        p.isBold        = Highlight::FontOfNamedStyleIsBold  (pat->style);
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
    it++ = setStyleTableEntry(noPass1 ? &pass2PatternSrc[0] : &pass1PatternSrc[0]);

    // PLAIN_STYLE (pass 2)
    it++ = setStyleTableEntry(noPass2 ? &pass1PatternSrc[0] : &pass2PatternSrc[0]);

    // explicit styles (pass 1)
    for (int i = 1; i < nPass1Patterns; i++) {
        it++ = setStyleTableEntry(&pass1PatternSrc[i]);
    }

    // explicit styles (pass 2)
    for (int i = 1; i < nPass2Patterns; i++) {
        it++ = setStyleTableEntry(&pass2PatternSrc[i]);
    }

    // Free the temporary sorted pattern source list
    delete[] pass1PatternSrc;
    delete[] pass2PatternSrc;

    // Create the style buffer
    auto styleBuf = std::make_shared<TextBuffer>();
    styleBuf->BufSetSyncXSelection(false);

    // Collect all of the highlighting information in a single structure
    auto highlightData = std::make_unique<WindowHighlightData>();
    highlightData->pass1Patterns              = pass1Pats;
    highlightData->pass2Patterns              = pass2Pats;
    highlightData->parentStyles               = std::move(parentStyles);
    highlightData->styleTable                 = std::move(styleTable);
    highlightData->styleBuffer                = styleBuf;
    highlightData->contextRequirements.nLines = contextLines;
    highlightData->contextRequirements.nChars = contextChars;
    highlightData->patternSetForWindow        = patSet;

    return highlightData;
}

/*
** Transform pattern sources into the compiled highlight information
** actually used by the code.  Output is a tree of HighlightData structures
** containing compiled regular expressions and style information.
*/
HighlightData *DocumentWidget::compilePatternsEx(const gsl::span<HighlightPattern> &patternSrc) {

    int parentIndex;

    /* Allocate memory for the compiled patterns.  The list is terminated
       by a record with style == 0. */
    auto compiledPats = new HighlightData[static_cast<size_t>(patternSrc.size() + 1)];

    compiledPats[patternSrc.size()].style = 0;

    // Build the tree of parse expressions
    for (int i = 0; i < patternSrc.size(); i++) {
        compiledPats[i].nSubPatterns = 0;
        compiledPats[i].nSubBranches = 0;
    }

    for (int i = 1; i < patternSrc.size(); i++) {
        if (patternSrc[i].subPatternOf.isNull()) {
            compiledPats[0].nSubPatterns++;
        } else {
            compiledPats[Highlight::indexOfNamedPattern(patternSrc, patternSrc[i].subPatternOf)].nSubPatterns++;
        }
    }

    for (int i = 0; i < patternSrc.size(); i++) {
        compiledPats[i].subPatterns = (compiledPats[i].nSubPatterns == 0) ?
                    nullptr :
                    new HighlightData *[static_cast<size_t>(compiledPats[i].nSubPatterns)];
    }

    for (int i = 0; i < patternSrc.size(); i++) {
        compiledPats[i].nSubPatterns = 0;
    }

    for (int i = 1; i < patternSrc.size(); i++) {
        if (patternSrc[i].subPatternOf.isNull()) {
            compiledPats[0].subPatterns[compiledPats[0].nSubPatterns++] = &compiledPats[i];
        } else {
            parentIndex = Highlight::indexOfNamedPattern(patternSrc, patternSrc[i].subPatternOf);
            compiledPats[parentIndex].subPatterns[compiledPats[parentIndex].nSubPatterns++] = &compiledPats[i];
        }
    }

    /* Process color-only sub patterns (no regular expressions to match,
       just colors and fonts for sub-expressions of the parent pattern */
    for (int i = 0; i < patternSrc.size(); i++) {
        compiledPats[i].colorOnly      = (patternSrc[i].flags & COLOR_ONLY) != 0;
        compiledPats[i].userStyleIndex = Highlight::IndexOfNamedStyle(patternSrc[i].style);

        if (compiledPats[i].colorOnly && compiledPats[i].nSubPatterns != 0) {
            QMessageBox::warning(
                        this,
                        tr("Color-only Pattern"),
                        tr("Color-only pattern \"%1\" may not have subpatterns").arg(patternSrc[i].name));
            delete [] compiledPats;
            return nullptr;
        }

        static const QRegularExpression re(QLatin1String("[0-9]+"));

        {
            if (!patternSrc[i].startRE.isNull()) {
                Input in(&patternSrc[i].startRE);
                Q_FOREVER {
                    if(in.match(QLatin1Char('&'))) {
                        compiledPats[i].startSubexprs.push_back(0);
                    } else if(in.match(QLatin1Char('\\'))) {

                        QString number;
                        if(in.match(re, &number)) {
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
                    if(in.match(QLatin1Char('&'))) {
                        compiledPats[i].endSubexprs.push_back(0);
                    } else if(in.match(QLatin1Char('\\'))) {

                        QString number;
                        if(in.match(re, &number)) {
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
    for (int i = 0; i < patternSrc.size(); i++) {

        if (patternSrc[i].startRE.isNull() || compiledPats[i].colorOnly) {
            compiledPats[i].startRE = nullptr;
        } else {
            compiledPats[i].startRE = compileREAndWarnEx(patternSrc[i].startRE);
            if (!compiledPats[i].startRE) {
                delete [] compiledPats;
                return nullptr;
            }
        }

        if (patternSrc[i].endRE.isNull() || compiledPats[i].colorOnly) {
            compiledPats[i].endRE = nullptr;
        } else {
            compiledPats[i].endRE = compileREAndWarnEx(patternSrc[i].endRE);
            if (!compiledPats[i].endRE) {
                delete [] compiledPats;
                return nullptr;
            }
        }

        if (patternSrc[i].errorRE.isNull()) {
            compiledPats[i].errorRE = nullptr;
        } else {
            compiledPats[i].errorRE = compileREAndWarnEx(patternSrc[i].errorRE);
            if (!compiledPats[i].errorRE) {
                delete [] compiledPats;
                return nullptr;
            }
        }
    }

    /* Construct and compile the great hairy pattern to match the OR of the
       end pattern, the error pattern, and all of the start patterns of the
       sub-patterns */
    for (int patternNum = 0; patternNum < patternSrc.size(); patternNum++) {
        if (patternSrc[patternNum].endRE.isNull() && patternSrc[patternNum].errorRE.isNull() && compiledPats[patternNum].nSubPatterns == 0) {
            compiledPats[patternNum].subPatternRE = nullptr;
            continue;
        }

        int length;
        length  = (compiledPats[patternNum].colorOnly || patternSrc[patternNum].endRE.isNull())   ? 0 : patternSrc[patternNum].endRE.size()   + 5;
        length += (compiledPats[patternNum].colorOnly || patternSrc[patternNum].errorRE.isNull()) ? 0 : patternSrc[patternNum].errorRE.size() + 5;

        for (int i = 0; i < compiledPats[patternNum].nSubPatterns; i++) {
            long subPatIndex = compiledPats[patternNum].subPatterns[i] - compiledPats;
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

        for (int i = 0; i < compiledPats[patternNum].nSubPatterns; i++) {
            long subPatIndex = compiledPats[patternNum].subPatterns[i] - compiledPats;

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
            compiledPats[patternNum].subPatternRE = std::make_shared<Regex>(bigPattern, REDFLT_STANDARD);
        } catch(const RegexError &e) {
            qWarning("NEdit: Error compiling syntax highlight patterns:\n%s", e.what());
            delete [] compiledPats;
            return nullptr;
        }
    }

    // Copy remaining parameters from pattern template to compiled tree
    for (int i = 0; i < patternSrc.size(); i++) {
        compiledPats[i].flags = patternSrc[i].flags;
    }

    return compiledPats;
}

/*
** compile a regular expression and present a user friendly dialog on failure.
*/
std::shared_ptr<Regex> DocumentWidget::compileREAndWarnEx(const QString &re) {

    try {
        return std::make_shared<Regex>(re.toStdString(), REDFLT_STANDARD);
    } catch(const RegexError &e) {

        constexpr int MaxLength = 4096;

        /* Prevent buffer overflow. If the re is too long, truncate it and append ... */
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
bool DocumentWidget::MacroWindowCloseActionsEx() {
    const std::shared_ptr<MacroCommandData> &cmdData = macroCmdData_;

    CommandRecorder *recorder = CommandRecorder::instance();

    if (recorder->isRecording() && recorder->macroRecordWindow() == this) {
        FinishLearning();
    }

    /* If no macro is executing in the window, allow the close, but check
       if macros executing in other windows have it as focus.  If so, set
       their focus back to the window from which they were originally run */
    if(!cmdData) {
        for(DocumentWidget *document : DocumentWidget::allDocuments()) {

            const std::shared_ptr<MacroCommandData> &mcd = document->macroCmdData_;

            if (document == MacroRunDocumentEx() && MacroFocusDocument() == this) {
                SetMacroFocusDocument(MacroRunDocumentEx());
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
    if (MacroRunDocumentEx() == this) {
        cmdData->closeOnCompletion = true;
        return false;
    }

    // Kill the macro command
    finishMacroCmdExecutionEx();
    return true;
}

/*
** Cancel the macro command in progress (user cancellation via GUI)
*/
void DocumentWidget::AbortMacroCommandEx() {
    if (!macroCmdData_)
        return;

    /* If there's both a macro and a shell command executing, the shell command
       must have been called from the macro.  When called from a macro, shell
       commands don't put up cancellation controls of their own, but rely
       instead on the macro cancellation mechanism (here) */
    if (shellCmdData_) {
        AbortShellCommandEx();
    }

    // Kill the macro command
    finishMacroCmdExecutionEx();
}

/*
** Cancel Learn mode, or macro execution (they're bound to the same menu item)
*/
void DocumentWidget::CancelMacroOrLearnEx() {
    if(CommandRecorder::instance()->isRecording()) {
        cancelLearnEx();
    } else if (macroCmdData_) {
        AbortMacroCommandEx();
    }
}

/*
** Execute the learn/replay sequence stored in "window"
*/
void DocumentWidget::ReplayEx() {

    QString replayMacro = CommandRecorder::instance()->replayMacro;

    // Verify that a replay macro exists and it's not empty and that
    // we're not already running a macro
    if (!replayMacro.isEmpty() && !macroCmdData_) {

        QString errMsg;
        int stoppedAt;

        Program *prog = ParseMacro(replayMacro, &errMsg, &stoppedAt);
        if(!prog) {
            qWarning("NEdit: internal error, learn/replay macro syntax error: %s", qPrintable(errMsg));
            return;
        }

        runMacroEx(prog);
    }
}

void DocumentWidget::cancelLearnEx() {

    if(!CommandRecorder::instance()->isRecording()) {
        return;
    }

    DocumentWidget *document = CommandRecorder::instance()->macroRecordWindow();
    Q_ASSERT(document);

    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Learn_Keystrokes->setEnabled(true);
    }

    if (document->IsTopDocument()) {
        auto window = MainWindow::fromDocument(document);
        window->ui.action_Finish_Learn->setEnabled(false);
        window->ui.action_Cancel_Learn->setEnabled(false);
    }

    document->ClearModeMessage();
}

void DocumentWidget::FinishLearning() {

    if(!CommandRecorder::instance()->isRecording()) {
        return;
    }

    DocumentWidget *document = CommandRecorder::instance()->macroRecordWindow();
    Q_ASSERT(document);

    CommandRecorder::instance()->stopRecording();

    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Learn_Keystrokes->setEnabled(true);
    }

    if (document->IsTopDocument()) {
        if(auto window = MainWindow::fromDocument(document)) {
            window->ui.action_Finish_Learn->setEnabled(false);
            window->ui.action_Cancel_Learn->setEnabled(false);
        }
    }

    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Replay_Keystrokes->setEnabled(true);
    }

    document->ClearModeMessage();
}

/*
** Executes macro string "macro" using the lastFocus pane in "window".
** Reports errors via a dialog posted over "window", integrating the name
** "errInName" into the message to help identify the source of the error.
*/
void DocumentWidget::DoMacro(const QString &macro, const QString &errInName) {

    /* Add a terminating newline (which command line users are likely to omit
       since they are typically invoking a single routine) */
    QString qMacro = macro + QLatin1Char('\n');
    QString errMsg;

    // Parse the macro and report errors if it fails
    int stoppedAt;
    Program *const prog = ParseMacro(qMacro, &errMsg, &stoppedAt);
    if(!prog) {
        Preferences::reportError(this, qMacro, stoppedAt, errInName, errMsg);
        return;
    }

    // run the executable program (prog is freed upon completion)
    runMacroEx(prog);
}

/*
**  Read the initial NEdit macro file if one exists.
*/
void DocumentWidget::ReadMacroInitFileEx() {

    const QString autoloadName = Settings::autoLoadMacroFile();
    if(autoloadName.isNull()) {
        return;
    }

    static bool initFileLoaded = false;

    if (!initFileLoaded) {
        ReadMacroFileEx(autoloadName, false);
        initFileLoaded = true;
    }
}

/*
** Parse and execute a macro string including macro definitions.  Report
** parsing errors in a dialog posted over window->shell_.
*/
int DocumentWidget::ReadMacroStringEx(const QString &string, const QString &errIn) {
    return readCheckMacroStringEx(this, string, this, errIn, nullptr);
}

/**
 * @brief DocumentWidget::EndSmartIndentEx
 */
void DocumentWidget::EndSmartIndent() {
    const std::unique_ptr<SmartIndentData> &winData = smartIndentData_;

    if(!winData) {
        return;
    }

    // Free programs and allocated data
    if (winData->modMacro) {
        delete winData->modMacro;
    }

    delete winData->newlineMacro;

    smartIndentData_ = nullptr;
}

/**
 * @brief DocumentWidget::InSmartIndentMacrosEx
 * @return
 */
bool DocumentWidget::InSmartIndentMacrosEx() const {
    const std::unique_ptr<SmartIndentData> &winData = smartIndentData_;
    return winData && (winData->inModMacro || winData->inNewLineMacro);
}

/**
 * @brief DocumentWidget::GetAnySelectionEx
 * @return
 */
QString DocumentWidget::GetAnySelectionEx(bool beep_on_error) {

    // If the selection is in the window's own buffer get it from there
    if (buffer_->primary.selected) {
        return QString::fromStdString(buffer_->BufGetSelectionTextEx());
    }

    if(QApplication::clipboard()->supportsSelection()) {
        const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
        if(mimeData->hasText()) {
            return mimeData->text();
        }
    }

    if(beep_on_error) {
        QApplication::beep();
    }

    return QString();
}

/*
** Find the font (font struct) associated with a named style.
** This routine must only be called with a valid styleName (call
** NamedStyleExists to find out whether styleName is valid).
*/
QFont DocumentWidget::FontOfNamedStyleEx(const QString &styleName) const {

    const size_t styleNo = Highlight::IndexOfNamedStyle(styleName);

    if (styleNo == STYLE_NOT_FOUND) {
        return Preferences::GetPrefDefaultFont();
    } else {

        const int fontNum = Highlight::HighlightStyles[styleNo].font;

        switch(fontNum) {
        case BOLD_FONT:
            return boldFontStruct_;
        case ITALIC_FONT:
            return italicFontStruct_;
        case BOLD_FONT | ITALIC_FONT:
            return boldItalicFontStruct_;
        case PLAIN_FONT:
        default:
            return fontStruct_;
        }
    }
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns nullptr when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching, and ExecRE can skip compiling a
** delimiter table when delimiters is nullptr).
*/
QString DocumentWidget::GetWindowDelimitersEx() const {
    if (languageMode_ == PLAIN_LANGUAGE_MODE) {
        return QString();
    } else {
        return Preferences::LanguageModes[languageMode_].delimiters;
    }
}

/**
 * @brief DocumentWidget::GetUseTabs
 * @return
 */
bool DocumentWidget::GetUseTabs() const {
    return buffer_->BufGetUseTabs();
}

/**
 * @brief DocumentWidget::SetUseTabs
 * @param value
 */
void DocumentWidget::SetUseTabs(bool value) {

    emit_event("set_use_tabs", QString::number(value));
    buffer_->BufSetUseTabs(value);
}

/**
 * @brief DocumentWidget::GetHighlightSyntax
 * @return
 */
bool DocumentWidget::GetHighlightSyntax() const {
    return highlightSyntax_;
}

/**
 * @brief DocumentWidget::SetHighlightSyntax
 * @param value
 */
void DocumentWidget::SetHighlightSyntax(bool value) {

    emit_event("set_highlight_syntax", QString::number(value));

    highlightSyntax_ = value;

    if(IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Highlight_Syntax)->setChecked(value);
        }
    }

    if (highlightSyntax_) {
        StartHighlightingEx(/*warn=*/true);
    } else {
        StopHighlightingEx();
    }
}

bool DocumentWidget::GetMakeBackupCopy() const {
    return saveOldVersion_;
}

void DocumentWidget::SetMakeBackupCopy(bool value) {

    emit_event("set_make_backup_copy", value ? QLatin1String("1") : QLatin1String("0"));

    if (IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Make_Backup_Copy)->setChecked(value);
        }
    }

    saveOldVersion_ = value;
}


bool DocumentWidget::GetIncrementalBackup() const {
    return autoSave_;
}

void DocumentWidget::SetIncrementalBackup(bool value) {

    emit_event("set_incremental_backup", QString::number(value));

    autoSave_ = value;

    if(IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Highlight_Syntax)->setChecked(value);
        }
    }
}

bool DocumentWidget::GetUserLocked() const {
    return lockReasons_.isUserLocked();
}

void DocumentWidget::SetUserLocked(bool value) {
    emit_event("set_locked", QString::number(value));

    lockReasons_.setUserLocked(value);

    if(IsTopDocument()) {
        if(auto win = MainWindow::fromDocument(this)) {
            no_signals(win->ui.action_Read_Only)->setChecked(lockReasons_.isAnyLocked());
            win->UpdateWindowTitle(this);
            win->UpdateWindowReadOnly(this);
        }
    }
}

void DocumentWidget::AddMarkEx(TextArea *area, QChar label) {


    /* look for a matching mark to re-use, or advance
       nMarks to create a new one */
    label = label.toUpper();

    size_t index;
    for (index = 0; index < nMarks_; index++) {
        if (markTable_[index].label == label) {
            break;
        }
    }

    if (index >= MAX_MARKS) {
        qWarning("NEdit: no more marks allowed"); // shouldn't happen
        return;
    }

    if (index == nMarks_) {
        nMarks_++;
    }

    // store the cursor location and selection position in the table
    markTable_[index].label     = label;
    markTable_[index].sel       = buffer_->primary;
    markTable_[index].cursorPos = area->TextGetCursorPos();
}

void DocumentWidget::SelectNumberedLineEx(TextArea *area, int64_t lineNum) {
    int i;
    TextCursor lineStart = {};

    // count lines to find the start and end positions for the selection
    if (lineNum < 1) {
        lineNum = 1;
    }

    TextCursor lineEnd = TextCursor(-1);

    for (i = 1; i <= lineNum && lineEnd < buffer_->BufGetLength(); i++) {
        lineStart = lineEnd + 1;
        lineEnd = buffer_->BufEndOfLine(lineStart);
    }

    // highlight the line
    if (i > lineNum) {
        // Line was found
        if (lineEnd < buffer_->BufGetLength()) {
            buffer_->BufSelect(lineStart, lineEnd + 1);
        } else {
            // Don't select past the end of the buffer !
            buffer_->BufSelect(lineStart, buffer_->BufEndOfBuffer());
        }
    } else {
        /* Line was not found -> position the selection & cursor at the end
           without making a real selection and beep */
        lineStart = buffer_->BufEndOfBuffer();
        buffer_->BufSelect(lineStart, lineStart);
        QApplication::beep();
    }

    MakeSelectionVisible(area);
    area->TextSetCursorPos(lineStart);
}

void DocumentWidget::gotoMark(TextArea *area, QChar label, bool extendSel) {

    size_t index;

    // look up the mark in the mark table
    label = label.toUpper();
    for (index = 0; index < nMarks_; index++) {
        if (markTable_[index].label == label)
            break;
    }

    if (index == nMarks_) {
        QApplication::beep();
        return;
    }

    // reselect marked the selection, and move the cursor to the marked pos
    const TextBuffer::Selection &sel    = markTable_[index].sel;
    const TextBuffer::Selection &oldSel = buffer_->primary;

    TextCursor cursorPos = markTable_[index].cursorPos;
    if (extendSel) {

        const TextCursor oldStart = oldSel.selected ? oldSel.start : area->TextGetCursorPos();
        const TextCursor oldEnd   = oldSel.selected ? oldSel.end   : area->TextGetCursorPos();
        const TextCursor newStart = sel.selected    ? sel.start    : cursorPos;
        const TextCursor newEnd   = sel.selected    ? sel.end      : cursorPos;

        buffer_->BufSelect(oldStart < newStart ? oldStart : newStart, oldEnd > newEnd ? oldEnd : newEnd);
    } else {
        if (sel.selected) {
            if (sel.rectangular) {
                buffer_->BufRectSelect(sel.start, sel.end, sel.rectStart, sel.rectEnd);
            } else {
                buffer_->BufSelect(sel.start, sel.end);
            }
        } else {
            buffer_->BufUnselect();
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
    MakeSelectionVisible(area);
    area->setAutoShowInsertPos(true);
}

/**
 * @brief DocumentWidget::lockReasons
 * @return
 */
LockReasons DocumentWidget::lockReasons() const {
    return lockReasons_;
}

/*      Finds all matches and handles tag "collisions". Prompts user with a
        list of collided tags in the hash table and allows the user to select
        the correct one. */
int DocumentWidget::findAllMatchesEx(TextArea *area, const QString &string) {

    QString filename;
    QString pathname;

    int i;
    int pathMatch = 0;
    int samePath = 0;
    int nMatches = 0;

    // verify that the string is reasonable as a tag
    if(string.isEmpty()) {
        QApplication::beep();
        return -1;
    }

    Tags::tagName = string;

    QList<Tags::Tag> tags = Tags::LookupTag(string, Tags::searchMode);

    // First look up all of the matching tags
    for(const Tags::Tag &tag : tags) {

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
        if (GetLanguageMode() != PLAIN_LANGUAGE_MODE && Preferences::GetPrefSmartTags() && langMode != PLAIN_LANGUAGE_MODE && langMode != GetLanguageMode()) {
            continue;
        }

        if (QFileInfo(fileToSearch).isAbsolute()) {
            Tags::tagFiles[nMatches] = fileToSearch;
        } else {
            Tags::tagFiles[nMatches] = tr("%1%2").arg(tagPath, fileToSearch);
        }

        Tags::tagSearch[nMatches] = searchString;
        Tags::tagPosInf[nMatches] = startPos;

        // NOTE(eteran): no error checking...
        ParseFilenameEx(Tags::tagFiles[nMatches], &filename, &pathname);

        // Is this match in the current file?  If so, use it!
        if (Preferences::GetPrefSmartTags() && filename_ == filename && path_ == pathname) {
            if (nMatches) {
                Tags::tagFiles[0]  = Tags::tagFiles[nMatches];
                Tags::tagSearch[0] = Tags::tagSearch[nMatches];
                Tags::tagPosInf[0] = Tags::tagPosInf[nMatches];
            }
            nMatches = 1;
            break;
        }

        // Is this match in the same dir. as the current file?
        if (path_ == pathname) {
            samePath++;
            pathMatch = nMatches;
        }

        if (++nMatches >= Tags::MAXDUPTAGS) {
            QMessageBox::warning(this, tr("Tags"), tr("Too many duplicate tags, first %1 shown").arg(Tags::MAXDUPTAGS));
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
        nMatches = 1;
    }

    //  If all of the tag entries are the same file, just use the first.
    if (Preferences::GetPrefSmartTags()) {
        for (i = 1; i < nMatches; i++) {
            if(Tags::tagFiles[i] != Tags::tagFiles[i - 1]) {
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

            // NOTE(eteran): no error checking...
            ParseFilenameEx(Tags::tagFiles[i], &filename, &pathname);
            if ((i < nMatches - 1 && (Tags::tagFiles[i] == Tags::tagFiles[i + 1])) || (i > 0 && (Tags::tagFiles[i] == Tags::tagFiles[i - 1]))) {

                if (!Tags::tagSearch[i].isEmpty() && (Tags::tagPosInf[i] != -1)) {
                    // etags
                    temp = QString::asprintf("%2d. %s%s %8li %s", i + 1, qPrintable(pathname), qPrintable(filename), Tags::tagPosInf[i], qPrintable(Tags::tagSearch[i]));
                } else if (!Tags::tagSearch[i].isEmpty()) {
                    // ctags search expr
                    temp = QString::asprintf("%2d. %s%s          %s", i + 1, qPrintable(pathname), qPrintable(filename), qPrintable(Tags::tagSearch[i]));
                } else {
                    // line number only
                    temp = QString::asprintf("%2d. %s%s %8li", i + 1, qPrintable(pathname), qPrintable(filename), Tags::tagPosInf[i]);
                }
            } else {
                temp = QString::asprintf("%2d. %s%s", i + 1, qPrintable(pathname), qPrintable(filename));
            }

            dupTagsList.push_back(temp);
        }

        createSelectMenuEx(area, dupTagsList);
        return 1;
    }

    /*
    **  No need for a dialog list, there is only one tag matching --
    **  Go directly to the tag
    */
    if (Tags::searchMode == Tags::SearchMode::TAG) {
        editTaggedLocationEx(area, 0);
    } else {
        Tags::showMatchingCalltipEx(this, area, 0);
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
void DocumentWidget::createSelectMenuEx(TextArea *area, const QStringList &args) {

    auto dialog = std::make_unique<DialogDuplicateTags>(this, area);
    dialog->setTag(Tags::tagName);
    for(int i = 0; i < args.size(); ++i) {
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
int DocumentWidget::ShowTipStringEx(const QString &text, bool anchored, int pos, bool lookup, Tags::SearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignMode alignMode) {
    if (search_type == Tags::SearchMode::TAG) {
        return 0;
    }

    // So we don't have to carry all of the calltip alignment info around
    Tags::globAnchored  = anchored;
    Tags::globPos       = pos;
    Tags::globHAlign    = hAlign;
    Tags::globVAlign    = vAlign;
    Tags::globAlignMode = alignMode;

    // If this isn't a lookup request, just display it.

    if(auto window = MainWindow::fromDocument(this)) {
        if (!lookup) {
            return Tags::tagsShowCalltipEx(window->lastFocus(), text);
        } else {
            return findDef(window->lastFocus(), text, search_type);
        }
    }

    return 0;
}

/*  Open a new (or existing) editor window to the location specified in
    tagFiles[i], tagSearch[i], tagPosInf[i] */
void DocumentWidget::editTaggedLocationEx(TextArea *area, int i) {

    QString filename;
    QString pathname;

    // NOTE(eteran): no error checking...
    ParseFilenameEx(Tags::tagFiles[i], &filename, &pathname);

    // open the file containing the definition
    DocumentWidget::EditExistingFileEx(
                this,
                filename,
                pathname,
                0,
                QString(),
                false,
                QString(),
                Preferences::GetPrefOpenInTab(),
                false);

    DocumentWidget *documentToSearch = MainWindow::FindWindowWithFile(filename, pathname);
    if(!documentToSearch) {
        QMessageBox::warning(
                    this,
                    tr("File not found"),
                    tr("File %1 not found").arg(Tags::tagFiles[i]));
        return;
    }

    const int64_t tagLineNumber = Tags::tagPosInf[i];

    if (Tags::tagSearch[i].isEmpty()) {
        // if the search string is empty, select the numbered line
        SelectNumberedLineEx(area, tagLineNumber);
        return;
    }

    int64_t startPos;
    int64_t endPos;

    // search for the tags file search string in the newly opened file
    if (!Tags::fakeRegExSearchEx(documentToSearch->buffer_->BufAsStringEx(), Tags::tagSearch[i], &startPos, &endPos)) {
        QMessageBox::warning(
                    this,
                    tr("Tag Error"),
                    tr("Definition for %1\nnot found in %2").arg(Tags::tagName, Tags::tagFiles[i]));
        return;
    }

    // select the matched string
    documentToSearch->buffer_->BufSelect(TextCursor(startPos), TextCursor(endPos));
    documentToSearch->RaiseFocusDocumentWindow(true);

    /* Position it nicely in the window,
       about 1/4 of the way down from the top */
    const int64_t lineNum = documentToSearch->buffer_->BufCountLines(TextCursor(), TextCursor(startPos));

    int rows = area->getRows();

    area->TextDSetScroll(lineNum - rows / 4, 0);
    area->TextSetCursorPos(TextCursor(endPos));
}

/**
 * @brief DocumentWidget::defaultFont
 * @return
 */
QFont DocumentWidget::defaultFont() const {
    return fontStruct_;
}
