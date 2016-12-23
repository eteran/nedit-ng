
#include <QBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QDateTime>
#include <QtDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QRadioButton>
#include "SignalBlocker.h"
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "DialogReplace.h"
#include "TextArea.h"
#include "preferences.h"
#include "fileUtils.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "Color.h"
#include "highlight.h"
#include "LanguageMode.h"
#include "search.h"
#include "clearcase.h"
#include "dragEndCBStruct.h"
#include "smartIndentCBStruct.h"
#include "smartIndent.h"
#include "highlightData.h"
#include "MenuItem.h"
#include "Font.h"
#include "tags.h"
#include "HighlightData.h"
#include "WindowHighlightData.h"
#include "misc.h"
#include "file.h"
#include "UndoInfo.h"
#include "utils.h"
#include "interpret.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>

namespace {

struct SmartIndentData {
    Program *newlineMacro;
    int inNewLineMacro;
    Program *modMacro;
    int inModMacro;
};

/*
 * Number of bytes read at once by cmpWinAgainstFile
 */
const int PREFERRED_CMPBUF_LEN = 32768;

// TODO(eteran): use an enum for this
const int FORWARD = 1;
const int REVERSE = 2;

/* Maximum frequency in miliseconds of checking for external modifications.
   The periodic check is only performed on buffer modification, and the check
   interval is only to prevent checking on every keystroke in case of a file
   system which is slow to process stat requests (which I'm not sure exists) */
const int MOD_CHECK_INTERVAL = 3000;

void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *user) {
    if(auto w = static_cast<DocumentWidget *>(user)) {
        w->modifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText);
    }
}

void smartIndentCB(TextArea *area, smartIndentCBStruct *data, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->smartIndentCallback(area, data);
	}
}

void movedCB(TextArea *area, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->movedCallback(area);
	}
}

void dragStartCB(TextArea *area, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->dragStartCallback(area);
	}
}

void dragEndCB(TextArea *area, dragEndCBStruct *data, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->dragEndCallback(area, data);
	}
}



/*
** Update a position across buffer modifications specified by
** "modPos", "nDeleted", and "nInserted".
*/
void maintainPosition(int *position, int modPos, int nInserted, int nDeleted) {
    if (modPos > *position) {
        return;
    }

    if (modPos + nDeleted <= *position) {
        *position += nInserted - nDeleted;
    } else {
        *position = modPos;
    }
}

/*
** Update a selection across buffer modifications specified by
** "pos", "nDeleted", and "nInserted".
*/
void maintainSelection(TextSelection *sel, int pos, int nInserted, int nDeleted) {
    if (!sel->selected || pos > sel->end) {
        return;
    }

    maintainPosition(&sel->start, pos, nInserted, nDeleted);
    maintainPosition(&sel->end, pos, nInserted, nDeleted);
    if (sel->end <= sel->start) {
        sel->selected = false;
    }
}

/*
**
*/
UndoTypes determineUndoType(int nInserted, int nDeleted) {
    int textDeleted, textInserted;

    textDeleted = (nDeleted > 0);
    textInserted = (nInserted > 0);

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

/*
** Open an existing file specified by name and path.  Use the window inWindow
** unless inWindow is nullptr or points to a window which is already in use
** (displays a file other than Untitled, or is Untitled but modified).  Flags
** can be any of:
**
**	CREATE: 		If file is not found, (optionally) prompt the
**				user whether to create
**	SUPPRESS_CREATE_WARN	When creating a file, don't ask the user
**	PREF_READ_ONLY		Make the file read-only regardless
**
** If languageMode is passed as nullptr, it will be determined automatically
** from the file extension or file contents.
**
** If bgOpen is true, then the file will be open in background. This
** works in association with the SetLanguageMode() function that has
** the syntax highlighting deferred, in order to speed up the file-
** opening operation when multiple files are being opened in succession.
*/
DocumentWidget *EditExistingFileEx(DocumentWidget *inWindow, const QString &name, const QString &path, int flags, char *geometry, int iconic, const char *languageMode, int tabbed, int bgOpen) {

    // first look to see if file is already displayed in a window
    if(DocumentWidget *window = MainWindow::FindWindowWithFile(name, path)) {
        if (!bgOpen) {
            if (iconic) {
                window->RaiseDocument();
            } else {
                window->RaiseDocumentWindow();
            }
        }
        return window;
    }



    /* If an existing window isn't specified; or the window is already
       in use (not Untitled or Untitled and modified), or is currently
       busy running a macro; create the window */
    DocumentWidget *window = nullptr;
    if(!inWindow) {
        // TODO(eteran): implement geometry stuff
        auto win = new MainWindow();
        window = win->CreateDocument(name);
        if(iconic) {
            win->showMinimized();
        } else {
            win->showNormal();
        }
    } else if (inWindow->filenameSet_ || inWindow->fileChanged_ || inWindow->macroCmdData_) {
        if (tabbed) {
            if(auto win = inWindow->toWindow()) {
                window = win->CreateDocument(name);
            }
        } else {
            // TODO(eteran): implement geometry stuff
            Q_UNUSED(geometry);
            auto win = new MainWindow();
            window = win->CreateDocument(name);
            if(iconic) {
                win->showMinimized();
            } else {
                win->showNormal();
            }
        }
    } else {
        // open file in untitled document
        window            = inWindow;
        window->path_     = path;
        window->filename_ = name;

        if (!iconic && !bgOpen) {
            window->RaiseDocumentWindow();
        }
    }

    // Open the file
    if (!window->doOpen(name, path, flags)) {
#if 0
        /* The user may have destroyed the window instead of closing the
           warning dialog; don't close it twice */
        safeClose(window);
#endif
        return nullptr;
    }

    if(auto win = window->toWindow()) {
        win->forceShowLineNumbers();
    }

    // Decide what language mode to use, trigger language specific actions
    if(!languageMode) {
        window->DetermineLanguageMode(true);
    } else {
        window->SetLanguageMode(FindLanguageMode(languageMode), true);
    }

    if(auto win = window->toWindow()) {
        // update tab label and tooltip
        window->RefreshTabState();
        win->SortTabBar();
        win->ShowTabBar(win->GetShowTabBar());
    }


    if (!bgOpen) {
        window->RaiseDocument();
    }


    /* Bring the title bar and statistics line up to date, doOpen does
       not necessarily set the window title or read-only status */
    if(auto win = window->toWindow()) {
        win->UpdateWindowTitle(window);
        window->UpdateWindowReadOnly();
        window->UpdateStatsLine(nullptr);
    }

    // Add the name to the convenience menu of previously opened files
    QString fullname = QString(QLatin1String("%1%2")).arg(path, name);


    if (GetPrefAlwaysCheckRelTagsSpecs()) {
        AddRelTagsFile(GetPrefTagFile(), path.toLatin1().data(), TAG);
    }

    if(auto win = window->toWindow()) {
        win->AddToPrevOpenMenu(fullname);
    }

    return window;
}

}

//------------------------------------------------------------------------------
// Name: DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	ui.setupUi(this);

    ui.labelFileAndSize->setElideMode(Qt::ElideLeft);

	buffer_ = new TextBuffer();
    buffer_->BufAddModifyCB(SyntaxHighlightModifyCBEx, this);

	// create the text widget
	splitter_ = new QSplitter(Qt::Vertical, this);
	splitter_->setChildrenCollapsible(false);
	ui.verticalLayout->addWidget(splitter_);
	ui.verticalLayout->setContentsMargins(0, 0, 0, 0);

	// initialize the members
	multiFileReplSelected_ = false;
	multiFileBusy_         = false;
	writableWindows_       = nullptr;
	nWritableWindows_      = 0;
	fileChanged_           = false;
	fileMissing_           = true;
	fileMode_              = 0;
	fileUid_               = 0;
	fileGid_               = 0;
	filenameSet_           = false;
	fileFormat_            = UNIX_FILE_FORMAT;
	lastModTime_           = 0;
	filename_              = name;
	undo_                  = std::list<UndoInfo *>();
	redo_                  = std::list<UndoInfo *>();
	autoSaveCharCount_     = 0;
	autoSaveOpCount_       = 0;
	undoMemUsed_           = 0;
	
	lockReasons_.clear();
	
	indentStyle_           = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	autoSave_              = GetPrefAutoSave();
	saveOldVersion_        = GetPrefSaveOldVersion();
	wrapMode_              = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	overstrike_            = false;
    showMatchingStyle_     = static_cast<ShowMatchingStyle>(GetPrefShowMatching());
	matchSyntaxBased_      = GetPrefMatchSyntaxBased();
	highlightSyntax_       = GetPrefHighlightSyntax();
	backlightCharTypes_    = QString();
	backlightChars_        = GetPrefBacklightChars();
	
	if (backlightChars_) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && backlightChars_) {			
			backlightCharTypes_ = QLatin1String(cTypes);
		}
	}
	
	modeMessageDisplayed_  = false;
    modeMessage_           = QString();
	ignoreModify_          = false;
	windowMenuValid_       = false;

    flashTimer_            = new QTimer(this);
    contextMenu_           = nullptr;
	fileClosedAtom_        = None;
	wasSelected_           = false;
	fontName_              = GetPrefFontName();
	italicFontName_        = GetPrefItalicFontName();
	boldFontName_          = GetPrefBoldFontName();
	boldItalicFontName_    = GetPrefBoldItalicFontName();
	dialogColors_          = nullptr;
	fontList_              = GetPrefFontList();
	italicFontStruct_      = GetPrefItalicFont();
	boldFontStruct_        = GetPrefBoldFont();
	boldItalicFontStruct_  = GetPrefBoldItalicFont();
	dialogFonts_           = nullptr;
	nMarks_                = 0;
	markTimeoutID_         = 0;
	highlightData_         = nullptr;
	shellCmdData_          = nullptr;
	macroCmdData_          = nullptr;
	smartIndentData_       = nullptr;
	languageMode_          = PLAIN_LANGUAGE_MODE;
	iSearchHistIndex_      = 0;
	iSearchStartPos_       = -1;
	iSearchLastRegexCase_  = true;
	iSearchLastLiteralCase_= false;
	device_                = 0;
    inode_                 = 0;
	
    if(auto win = toWindow()) {
		ui.statusFrame->setVisible(win->showStats_);
	}

    flashTimer_->setInterval(1500);
    flashTimer_->setSingleShot(true);
    connect(flashTimer_, SIGNAL(timeout()), this, SLOT(flashTimerTimeout()));

#if 1
    {
        auto area = createTextArea(buffer_);
        splitter_->addWidget(area);
        area->setFocus();
    }
#if 0
    {
        auto area = createTextArea(buffer_);
        splitter_->addWidget(area);
        area->setFocus();
    }
#endif
#endif
}


//------------------------------------------------------------------------------
// Name: ~DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::~DocumentWidget() {
}

//------------------------------------------------------------------------------
// Name: createTextArea
//------------------------------------------------------------------------------
TextArea *DocumentWidget::createTextArea(TextBuffer *buffer) {

    int P_marginWidth  = 5;
    int P_marginHeight = 5;
    int lineNumCols    = 0;
    int marginWidth    = 0;
    int charWidth      = 0;
    int l = P_marginWidth + ((lineNumCols == 0) ? 0 : marginWidth + charWidth * lineNumCols);
    int h = P_marginHeight;

    auto area = new TextArea(this,
                             l,
                             h,
                             100,
                             100,
                             0,
                             0,
                             buffer,
                             toQFont(GetDefaultFontStruct(fontList_)),
                             Qt::white,
                             Qt::black,
                             Qt::white,
                             Qt::blue,
                             Qt::black, //QColor highlightFGPixel,
                             Qt::black, //QColor highlightBGPixel,
                             Qt::black, //QColor cursorFGPixel,
                             Qt::black  //QColor lineNumFGPixel,
                             );

    Pixel textFgPix   = AllocColor(GetPrefColorName(TEXT_FG_COLOR));
    Pixel textBgPix   = AllocColor(GetPrefColorName(TEXT_BG_COLOR));
    Pixel selectFgPix = AllocColor(GetPrefColorName(SELECT_FG_COLOR));
    Pixel selectBgPix = AllocColor(GetPrefColorName(SELECT_BG_COLOR));
    Pixel hiliteFgPix = AllocColor(GetPrefColorName(HILITE_FG_COLOR));
    Pixel hiliteBgPix = AllocColor(GetPrefColorName(HILITE_BG_COLOR));
    Pixel lineNoFgPix = AllocColor(GetPrefColorName(LINENO_FG_COLOR));
    Pixel cursorFgPix = AllocColor(GetPrefColorName(CURSOR_FG_COLOR));

    area->TextDSetColors(
        toQColor(textFgPix),
        toQColor(textBgPix),
        toQColor(selectFgPix),
        toQColor(selectBgPix),
        toQColor(hiliteFgPix),
        toQColor(hiliteBgPix),
        toQColor(lineNoFgPix),
        toQColor(cursorFgPix));

    // add focus, drag, cursor tracking, and smart indent callbacks
    area->addCursorMovementCallback(movedCB, this);
    area->addDragStartCallback(dragStartCB, this);
    area->addDragEndCallback(dragEndCB, this);
    area->addSmartIndentCallback(smartIndentCB, this);

    connect(area, SIGNAL(focusIn(QWidget*)), this, SLOT(onFocusIn(QWidget*)));
    connect(area, SIGNAL(focusOut(QWidget*)), this, SLOT(onFocusOut(QWidget*)));

    buffer_->BufAddModifyCB(modifiedCB, this);

    // Set the requested hardware tab distance and useTabs in the text buffer
    buffer_->BufSetTabDistance(GetPrefTabDist(PLAIN_LANGUAGE_MODE));
    buffer_->useTabs_ = GetPrefInsertTabs();

    return area;
}

//------------------------------------------------------------------------------
// Name: onFocusChanged
//------------------------------------------------------------------------------
void DocumentWidget::onFocusIn(QWidget *now) {
	if(auto w = qobject_cast<TextArea *>(now)) {
		if(auto window = qobject_cast<MainWindow *>(w->window())) {

			// record which window pane last had the keyboard focus
			window->lastFocus_ = w;

			// update line number statistic to reflect current focus pane
			UpdateStatsLine(w);

			// finish off the current incremental search
			EndISearch();

			// Check for changes to read-only status and/or file modifications
            CheckForChangesToFile();
		}
	}
}

void DocumentWidget::onFocusOut(QWidget *was) {
	Q_UNUSED(was);
}

/*
** Incremental searching is anchored at the position where the cursor
** was when the user began typing the search string.  Call this routine
** to forget about this original anchor, and if the search bar is not
** permanently up, pop it down.
*/
void DocumentWidget::EndISearch() {

	/* Note: Please maintain this such that it can be freely peppered in
	   mainline code, without callers having to worry about performance
	   or visual glitches.  */

	// Forget the starting position used for the current run of searches
	iSearchStartPos_ = -1;

	// Mark the end of incremental search history overwriting
    saveSearchHistory("", nullptr, SEARCH_LITERAL, false);

	// Pop down the search line (if it's not pegged up in Preferences)
    if(auto win = toWindow()) {
        win->TempShowISearch(false);
    }
}

MainWindow *DocumentWidget::toWindow() const {
    return qobject_cast<MainWindow *>(window());
}

/*
** Change the window appearance and the window data structure to show
** that the file it contains has been modified
*/
void DocumentWidget::SetWindowModified(bool modified) {
    if(auto win = toWindow()) {
		if (!fileChanged_ && modified) {
			win->ui.action_Close->setEnabled(true);
			fileChanged_ = true;
			win->UpdateWindowTitle(this);
			RefreshTabState();
		} else if (fileChanged_ && !modified) {
			fileChanged_ = false;
			win->UpdateWindowTitle(this);
			RefreshTabState();
		}
	}
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void DocumentWidget::RefreshTabState() {

    if(auto w = toWindow()) {
		TabWidget *tabWidget = w->ui.tabWidget;
		int index = tabWidget->indexOf(this);

		QString labelString;

		/* Set tab label to document's filename. Position of
		   "*" (modified) will change per label alignment setting */
		QStyle *style = tabWidget->getTabBar()->style();
		int alignment = style->styleHint(QStyle::SH_TabBar_Alignment);

		if (alignment != Qt::AlignRight) {
			labelString = tr("%1%2").arg(fileChanged_ ? tr("*") : tr("")).arg(filename_);
		} else {
			labelString = tr("%1%2").arg(filename_).arg(fileChanged_ ? tr("*") : tr(""));
		}

        // NOTE(eteran): original code made the tab with focus show in bold
        // a) There is not *easy* way to do this in Qt (can be done with style sheets)
        // b) I say, by default let Qt style the tabs as it likes, user can always override with theming

		tabWidget->setTabText(index, labelString);

		QString tipString;
		if (GetPrefShowPathInWindowsMenu() && filenameSet_) {
			tipString = tr("%1  - %2").arg(labelString, path_);
		} else {
			tipString = labelString;
		}

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

/*
** Set the language mode for the window, update the menu and trigger language
** mode specific actions (turn on/off highlighting).  If forceNewDefaults is
** true, re-establish default settings for language-specific preferences
** regardless of whether they were previously set by the user.
*/
void DocumentWidget::SetLanguageMode(int mode, bool forceNewDefaults) {

    // Do mode-specific actions
    reapplyLanguageMode(mode, forceNewDefaults);

	// Select the correct language mode in the sub-menu
    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            QList<QAction *> languages = win->ui.action_Language_Mode->menu()->actions();
            for(QAction *action : languages) {
                if(action->data().value<int>() == mode) {
                    action->setChecked(true);
                    break;
                }
            }
        }
	}
}

/*
** Find and return the name of the appropriate languange mode for
** the file in "window".  Returns a pointer to a string, which will
** remain valid until a change is made to the language modes list.
*/
int DocumentWidget::matchLanguageMode() {

	int fileNameLen;
	int beginPos;
	int endPos;

	/*... look for an explicit mode statement first */

	// Do a regular expression search on for recognition pattern
	std::string first200 = buffer_->BufGetRangeEx(0, 200);
	for (int i = 0; i < NLanguageModes; i++) {
		if (!LanguageModes[i]->recognitionExpr.isNull()) {
            if (SearchString(first200, LanguageModes[i]->recognitionExpr.toLatin1().data(), SEARCH_FORWARD, SEARCH_REGEX, false, 0, &beginPos, &endPos, nullptr, nullptr, nullptr)) {
				return i;
			}
		}
	}

	/* Look at file extension ("@@/" starts a ClearCase version extended path,
	   which gets appended after the file extension, and therefore must be
	   stripped off to recognize the extension to make ClearCase users happy) */
	fileNameLen = filename_.size();

	int versionExtendedPathIndex = GetClearCaseVersionExtendedPathIndex(filename_);
	if (versionExtendedPathIndex != -1) {
		fileNameLen = versionExtendedPathIndex;
	}

	for (int i = 0; i < NLanguageModes; i++) {
        for(const QString &ext : LanguageModes[i]->extensions) {
			// TODO(eteran): we could use QFileInfo to get the extension to make this code a little simpler
			int extLen = ext.size();
			int start = fileNameLen - extLen;

			if (start >= 0 && strncmp(&filename_.toLatin1().data()[start], ext.toLatin1().data(), extLen) == 0) {
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

    if(auto win = toWindow()) {

        /* This routine is called for each character typed, so its performance
           affects overall editor perfomance.  Only update if the line is on. */
        if (!win->showStats_) {
            return;
        }

        if(!area) {
            area = win->lastFocus_;
            if(!area) {
                area = qobject_cast<TextArea *>(splitter_->widget(0));
            }
		}

		// Compose the string to display. If line # isn't available, leave it off
		int pos = area->TextGetCursorPos();

		QString format;
		switch(fileFormat_) {
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
		if (!area->TextDPosToLineAndCol(pos, &line, &colNum)) {
			string   = tr("%1%2%3 %4 bytes").arg(path_, filename_, format).arg(buffer_->BufGetLength());
			slinecol = tr("L: ---  C: ---");
		} else {
			slinecol = tr("L: %1  C: %2").arg(line).arg(colNum);
			if (win->showLineNumbers_) {
				string = tr("%1%2%3 byte %4 of %5").arg(path_, filename_, format).arg(pos).arg(buffer_->BufGetLength());
			} else {
				string = tr("%1%2%3 %4 bytes").arg(path_, filename_, format).arg(buffer_->BufGetLength());
			}
		}

		// Update the line/column number
		ui.labelStats->setText(slinecol);

		// Don't clobber the line if there's a special message being displayed
		if(!modeMessageDisplayed_) {
			ui.labelFileAndSize->setText(string);
		}
	}
}

void DocumentWidget::movedCallback(TextArea *area) {

	if (ignoreModify_) {
		return;
	}

	// update line and column nubers in statistics line
	UpdateStatsLine(area);

	// Check the character before the cursor for matchable characters
    FlashMatchingEx(this, area);


	// Check for changes to read-only status and/or file modifications
    CheckForChangesToFile();

	/*  This callback is not only called for focussed panes, but for newly
		created panes as well. So make sure that the cursor is left alone
		for unfocussed panes.
		TextWidget have no state per se about focus, so we use the related
		ID for the blink procedure.  */
    if(QTimer *const blinkTimer = area->cursorBlinkTimer()) {
        if(!blinkTimer->isActive()) {
            //  Start blinking the caret again.
            blinkTimer->start();
        }
    }
}

void DocumentWidget::dragStartCallback(TextArea *area) {
	Q_UNUSED(area);
	// don't record all of the intermediate drag steps for undo
	ignoreModify_ = true;
}

/*
** Keep the marks in the windows book-mark table up to date across
** changes to the underlying buffer
*/
void DocumentWidget::UpdateMarkTable(int pos, int nInserted, int nDeleted) {

    for (int i = 0; i < nMarks_; i++) {
        maintainSelection(&markTable_[i].sel,       pos, nInserted, nDeleted);
        maintainPosition (&markTable_[i].cursorPos, pos, nInserted, nDeleted);
    }
}


/*
**
*/
void DocumentWidget::modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText) {

    Q_UNUSED(nRestyled);

    bool selected = buffer_->primary_.selected;

    // update the table of bookmarks
    if (!ignoreModify_) {
        UpdateMarkTable(pos, nInserted, nDeleted);
    }

    if(auto win = toWindow()) {
        // Check and dim/undim selection related menu items
        if ((wasSelected_ && !selected) || (!wasSelected_ && selected)) {
            wasSelected_ = selected;

            /* do not refresh shell-level items (window, menu-bar etc)
               when motifying non-top document */
            if (IsTopDocument()) {
                win->ui.action_Print_Selection->setEnabled(selected);
                win->ui.action_Cut->setEnabled(selected);
                win->ui.action_Copy->setEnabled(selected);
                win->ui.action_Delete->setEnabled(selected);

                /* Note we don't change the selection for items like
                   "Open Selected" and "Find Selected".  That's because
                   it works on selections in external applications.
                   Desensitizing it if there's no NEdit selection
                   disables this feature. */

                win->ui.action_Filter_Selection->setEnabled(selected);

                DimSelectionDepUserMenuItems(selected);

                if(auto dialog = win->getDialogReplace()) {
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
        CheckForChangesToFile();

    }
}

void DocumentWidget::dragEndCallback(TextArea *area, dragEndCBStruct *data) {

    // NOTE(eteran): so, it's a minor shame here that we don't use the area
    // variable. The **buffer** is what reports the modifications, and that is
    // lower level than a TextArea. So it doesn't pass that along to the other
    // versions of the modified callback. The code will eventually call
    // "UpdateStatsLine(nullptr)", which will force it to fall back on the
    // "lastFocus_" variable. Which **probably** points to the correct widget.
    // TODO(eteran): improve this somehow!
    Q_UNUSED(area);

	// restore recording of undo information
	ignoreModify_ = false;

	// Do nothing if drag operation was canceled
    if (data->nCharsInserted == 0) {
		return;
    }

	/* Save information for undoing this operation not saved while
	   undo recording was off */
    modifiedCallback(data->startPos, data->nCharsInserted, data->nCharsDeleted, 0, data->deletedText);
}

void DocumentWidget::smartIndentCallback(TextArea *area, smartIndentCBStruct *data) {

    Q_UNUSED(area);

	if (!smartIndentData_) {
		return;
	}

	switch(data->reason) {
	case CHAR_TYPED:
#if 0
		executeModMacro(window, data);
#endif
		break;
	case NEWLINE_INDENT_NEEDED:
#if 1
        executeNewlineMacroEx(data);
#endif
        break;
	}
}

/*
** raise the document and its shell window and optionally focus.
*/
void DocumentWidget::RaiseFocusDocumentWindow(bool focus) {
    RaiseDocument();
    if(auto win = toWindow()) {
        if(!win->isMaximized()) {
            win->showNormal();
        }
    }

    if(focus) {
        setFocus();
    }
}

/*
** raise the document and its shell window and focus depending on pref.
*/
void DocumentWidget::RaiseDocumentWindow() {
    RaiseDocument();
    if(auto win = toWindow()) {
        if(!win->isMaximized()) {
            win->showNormal();
        }
    }

    if(GetPrefFocusOnRaise()) {
        setFocus();
    }
}


void DocumentWidget::documentRaised() {
    /* Turn on syntax highlight that might have been deferred.
       NB: this must be done after setting the document as
           XmNworkWindow and managed, else the parent shell
       this may shrink on some this-managers such as
       metacity, due to changes made in UpdateWMSizeHints().*/
    if (highlightSyntax_ && highlightData_ == nullptr) {
        StartHighlightingEx(this, false);
    }

    RefreshTabState();

    /* now refresh this state/info. RefreshWindowStates()
       has a lot of work to do, so we update the screen first so
       the document appears to switch swiftly. */
    RefreshWindowStates();

    RefreshTabState();


    /* Make sure that the "In Selection" button tracks the presence of a
       selection and that the this inherits the proper search scope. */
    if(auto win = toWindow()) {
        if(auto dialog = win->getDialogReplace()) {
            dialog->UpdateReplaceActionButtons();
        }
    }

#if 0
    UpdateWMSizeHints();
#endif
}

void DocumentWidget::RaiseDocument() {
    if(auto win = toWindow()) {

#if 0
        if (WindowList.empty()) {
            return;
        }
#endif

#if 0
        Document *win;
        Document *lastwin = MarkActiveDocument();
        if (lastwin != this && lastwin->IsValidWindow()) {
            lastwin->MarkLastDocument();
        }
#endif

        // document already on top?
        if(win->ui.tabWidget->currentWidget() == this) {
            return;
        }

        // set the document as top document
        // show the new top document
        // NOTE(eteran): indirectly triggers a call to documentRaised()
        win->ui.tabWidget->setCurrentWidget(this);
    }
}

/*
** Change the language mode to the one indexed by "mode", reseting word
** delimiters, syntax highlighting and other mode specific parameters
*/
void DocumentWidget::reapplyLanguageMode(int mode, bool forceDefaults) {

    if(auto win = toWindow()) {
        const int oldMode = languageMode_;

        /* If the mode is the same, and changes aren't being forced (as might
           happen with Save As...), don't mess with already correct settings */
        if (languageMode_ == mode && !forceDefaults) {
            return;
        }

        // Change the mode name stored in the window
        languageMode_ = mode;

        // Decref oldMode's default calltips file if needed
        if (oldMode != PLAIN_LANGUAGE_MODE && !LanguageModes[oldMode]->defTipsFile.isNull()) {
            DeleteTagsFileEx(LanguageModes[oldMode]->defTipsFile, TIP, False);
        }

        // Set delimiters for all text widgets
        QString delimiters;
        if (mode == PLAIN_LANGUAGE_MODE || LanguageModes[mode]->delimiters.isNull()) {
            delimiters = GetPrefDelimiters();
        } else {
            delimiters = LanguageModes[mode]->delimiters;
        }

        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            area->setWordDelimiters(delimiters);
        }

        /* Decide on desired values for language-specific parameters.  If a
           parameter was set to its default value, set it to the new default,
           otherwise, leave it alone */
        bool wrapModeIsDef = wrapMode_ == GetPrefWrap(oldMode);
        bool tabDistIsDef = buffer_->BufGetTabDistance() == GetPrefTabDist(oldMode);

        int oldEmTabDist = textAreas[0]->getEmulateTabs();
        QString oldlanguageModeName = LanguageModeName(oldMode);

        bool emTabDistIsDef   = oldEmTabDist == GetPrefEmTabDist(oldMode);
        bool indentStyleIsDef = indentStyle_ == GetPrefAutoIndent(oldMode)   || (GetPrefAutoIndent(oldMode) == SMART_INDENT && indentStyle_ == AUTO_INDENT && !SmartIndentMacrosAvailable(LanguageModeName(oldMode).toLatin1().data()));
        bool highlightIsDef   = highlightSyntax_ == GetPrefHighlightSyntax() || (GetPrefHighlightSyntax() && FindPatternSet(!oldlanguageModeName.isNull() ? oldlanguageModeName : QLatin1String("")) == nullptr);
        int wrapMode          = wrapModeIsDef                                || forceDefaults ? GetPrefWrap(mode)        : wrapMode_;
        int tabDist           = tabDistIsDef                                 || forceDefaults ? GetPrefTabDist(mode)     : buffer_->BufGetTabDistance();
        int emTabDist         = emTabDistIsDef                               || forceDefaults ? GetPrefEmTabDist(mode)   : oldEmTabDist;
        int indentStyle       = indentStyleIsDef                             || forceDefaults ? GetPrefAutoIndent(mode)  : indentStyle_;
        int highlight         = highlightIsDef                               || forceDefaults ? GetPrefHighlightSyntax() : highlightSyntax_;

        /* Dim/undim smart-indent and highlighting menu items depending on
           whether patterns/macros are available */
        QString languageModeName = LanguageModeName(mode);
        bool haveHighlightPatterns = FindPatternSet(!languageModeName.isNull() ? languageModeName : QLatin1String("")) != nullptr;
        bool haveSmartIndentMacros = SmartIndentMacrosAvailable(LanguageModeName(mode).toLatin1().data());

        if (IsTopDocument()) {
            win->ui.action_Highlight_Syntax->setEnabled(haveHighlightPatterns);
            win->ui.action_Indent_Smart    ->setEnabled(haveSmartIndentMacros);
        }

        // Turn off requested options which are not available
        highlight = haveHighlightPatterns && highlight;
        if (indentStyle == SMART_INDENT && !haveSmartIndentMacros) {
            indentStyle = AUTO_INDENT;
        }


        // Change highlighting
        highlightSyntax_ = highlight;

        no_signals(win->ui.action_Highlight_Syntax)->setChecked(highlight);

        StopHighlighting();

        // we defer highlighting to RaiseDocument() if doc is hidden
        if (IsTopDocument() && highlight) {
            StartHighlightingEx(this, false);
        }

        // Force a change of smart indent macros (SetAutoIndent will re-start)
        if (indentStyle_ == SMART_INDENT) {
            EndSmartIndentEx(this);
            indentStyle_ = AUTO_INDENT;
        }


        // set requested wrap, indent, and tabs
        SetAutoWrap(wrapMode);
        SetAutoIndent(indentStyle);
        SetTabDist(tabDist);
        SetEmTabDist(emTabDist);


        // Load calltips files for new mode
        if (mode != PLAIN_LANGUAGE_MODE && !LanguageModes[mode]->defTipsFile.isNull()) {
            AddTagsFileEx(LanguageModes[mode]->defTipsFile, TIP);
        }

        // Add/remove language specific menu items
        UpdateUserMenus();
    }
}

/*
** Create either the variable Shell menu, Macro menu or Background menu
** items of "window" (driven by value of "menuType")
*/
QMenu *DocumentWidget::createUserMenu(const QVector<MenuData> &data) {

    auto rootMenu = new QMenu(this);
    for(int i = 0; i < data.size(); ++i) {
        const MenuData &menuData = data[i];

        bool found = menuData.info->umiNbrOfLanguageModes == 0;
        for(int language = 0; language < menuData.info->umiNbrOfLanguageModes; ++language) {
            if(menuData.info->umiLanguageMode[language] == languageMode_) {
                found = true;
            }
        }

        if(!found) {
            continue;
        }

        QMenu *parentMenu = rootMenu;
        QString name = QLatin1String(menuData.info->umiName);

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
                    parentMenu->addAction(toWindow()->ui.action_Cut);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("copy_clipboard()")) {
                    parentMenu->addAction(toWindow()->ui.action_Copy);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("paste_clipboard()")) {
                    parentMenu->addAction(toWindow()->ui.action_Paste);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("undo()")) {
                    parentMenu->addAction(toWindow()->ui.action_Undo);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("redo()")) {
                    parentMenu->addAction(toWindow()->ui.action_Redo);
                } else {
                    QAction *action = parentMenu->addAction(name);
                    action->setData(i);
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

/*
** Update the Shell, Macro, and Window Background menus of window
** "window" from the currently loaded command descriptions.
*/
void DocumentWidget::UpdateUserMenus() {

    if (!IsTopDocument()) {
        return;
    }

    // TODO(eteran): the old code used to only do this if the language mode changed
    //               we should probably restore that behavior
    if(auto win = toWindow()) {
        /* update user menus, which are shared over all documents, only
           if language mode was changed */
        auto shellMenu = createUserMenu(ShellMenuData);
        win->ui.menu_Shell->clear();
        win->ui.menu_Shell->addAction(win->ui.action_Execute_Command);
        win->ui.menu_Shell->addAction(win->ui.action_Execute_Command_Line);
        win->ui.menu_Shell->addAction(win->ui.action_Filter_Selection);
        win->ui.menu_Shell->addAction(win->ui.action_Cancel_Shell_Command);
        win->ui.menu_Shell->addSeparator();
        win->ui.menu_Shell->addActions(shellMenu->actions());

        auto macroMenu = createUserMenu(MacroMenuData);
        win->ui.menu_Macro->clear();
        win->ui.menu_Macro->addAction(win->ui.action_Learn_Keystrokes);
        win->ui.menu_Macro->addAction(win->ui.action_Finish_Learn);
        win->ui.menu_Macro->addAction(win->ui.action_Cancel_Learn);
        win->ui.menu_Macro->addAction(win->ui.action_Replay_Keystrokes);
        win->ui.menu_Macro->addAction(win->ui.action_Repeat);
        win->ui.menu_Macro->addSeparator();
        win->ui.menu_Macro->addActions(macroMenu->actions());

        /* update background menu, which is owned by a single document, only
           if language mode was changed */
        contextMenu_ = createUserMenu(BGMenuData);
        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            area->setContextMenu(contextMenu_);
        }
    }
}

/*
**
*/
void DocumentWidget::SetTabDist(int tabDist) {
    if (buffer_->tabDist_ != tabDist) {
        int saveCursorPositions[MAX_PANES + 1];
        int saveVScrollPositions[MAX_PANES + 1];
        int saveHScrollPositions[MAX_PANES + 1];

        ignoreModify_ = true;

        const QList<TextArea *> textAreas = textPanes();
        for(int paneIndex = 0; paneIndex < textAreas.size(); ++paneIndex) {
            TextArea *area = textAreas[paneIndex];

            area->TextDGetScroll(&saveVScrollPositions[paneIndex], &saveHScrollPositions[paneIndex]);
            saveCursorPositions[paneIndex] = area->TextGetCursorPos();
            area->setModifyingTabDist(1);
        }

        buffer_->BufSetTabDistance(tabDist);

        for(int paneIndex = 0; paneIndex < textAreas.size(); ++paneIndex) {
            TextArea *area = textAreas[paneIndex];

            area->setModifyingTabDist(0);
            area->TextSetCursorPos(saveCursorPositions[paneIndex]);
            area->TextDSetScroll(saveVScrollPositions[paneIndex], saveHScrollPositions[paneIndex]);
        }

        ignoreModify_ = false;
    }
}

/*
**
*/
void DocumentWidget::SetEmTabDist(int emTabDist) {

    const QList<TextArea *> textAreas = textPanes();
    for(TextArea *area : textAreas) {
        area->setEmulateTabs(emTabDist);
    }
}

/*
** Set autoindent state to one of  NO_AUTO_INDENT, AUTO_INDENT, or SMART_INDENT.
*/
// TODO(eteran): make state an enum
void DocumentWidget::SetAutoIndent(int state) {
    bool autoIndent  = (state == AUTO_INDENT);
    bool smartIndent = (state == SMART_INDENT);

    if (indentStyle_ == SMART_INDENT && !smartIndent) {
        EndSmartIndentEx(this);
    } else if (smartIndent && indentStyle_ != SMART_INDENT) {
#if 0
        BeginSmartIndentEx(this, true);
#endif
    }

    indentStyle_ = state;

    const QList<TextArea *> textAreas = textPanes();
    for(TextArea *area : textAreas) {
        area->setAutoIndent(smartIndent);
        area->setSmartIndent(autoIndent);
    }

    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            no_signals(win->ui.action_Indent_Smart)->setChecked(smartIndent);
            no_signals(win->ui.action_Indent_On)->setChecked(autoIndent);
            no_signals(win->ui.action_Indent_Off)->setChecked(state == NO_AUTO_INDENT);
        }
    }
}

/*
** Select auto-wrap mode, one of NO_WRAP, NEWLINE_WRAP, or CONTINUOUS_WRAP
*/
void DocumentWidget::SetAutoWrap(int state) {
    const bool autoWrap = (state == NEWLINE_WRAP);
    const bool contWrap = (state == CONTINUOUS_WRAP);

    const QList<TextArea *> textAreas = textPanes();
    for(TextArea *area : textAreas) {
        area->setAutoWrap(autoWrap);
        area->setContinuousWrap(contWrap);
    }

    wrapMode_ = state;

    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            no_signals(win->ui.action_Wrap_Auto_Newline)->setChecked(autoWrap);
            no_signals(win->ui.action_Wrap_Continuous)->setChecked(contWrap);
            no_signals(win->ui.action_Wrap_None)->setChecked(state == NO_WRAP);
        }
    }
}

int DocumentWidget::textPanesCount() const {
    return splitter_->count();
}

QList<TextArea *> DocumentWidget::textPanes() const {

    QList<TextArea *> list;
    list.reserve(splitter_->count());

    for(int i = 0; i < splitter_->count(); ++i) {
        if(auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
            list.push_back(area);
        }
    }

    return list;
}

bool DocumentWidget::IsTopDocument() const {
    if(auto win = toWindow()) {
        return win->ui.tabWidget->currentWidget() == this;
    }

    return false;
}

/*
** Update the Window menu of a single window to reflect the current state of
** all NEdit windows as determined by the global WindowList.
*/
void DocumentWidget::updateWindowMenu() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = toWindow()) {

        // Make a sorted list of windows
        QList<DocumentWidget *> documents = win->allDocuments();

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

        win->ui.menu_Windows->clear();
        win->ui.menu_Windows->addAction(win->ui.action_Split_Pane);
        win->ui.menu_Windows->addAction(win->ui.action_Close_Pane);
        win->ui.menu_Windows->addSeparator();
        win->ui.menu_Windows->addAction(win->ui.action_Detach_Tab);
        win->ui.menu_Windows->addAction(win->ui.action_Move_Tab_To);
        win->ui.menu_Windows->addSeparator();

        for(DocumentWidget *document : documents) {
            QString title = document->getWindowsMenuEntry();
            QAction *action = win->ui.menu_Windows->addAction(title, win, SLOT(raiseCB()));
            action->setData(reinterpret_cast<qulonglong>(document));
        }
    }
}

QString DocumentWidget::getWindowsMenuEntry() {

    QString fullTitle = tr("%1%2").arg(filename_).arg(fileChanged_ ? tr("*") : tr(""));

    if (GetPrefShowPathInWindowsMenu() && filenameSet_) {
        fullTitle.append(tr(" - "));
        fullTitle.append(path_);
    }

    return fullTitle;
}

void DocumentWidget::setLanguageMode(const QString &mode) {

    // TODO(eteran): this is inefficient, we started with the mode number
    //               converted it to a string, and now we look up the number
    //               again to pass to this function. We should just pass the
    //               number and skip the round trip
    SetLanguageMode(FindLanguageMode(mode.toLatin1().data()), false);
}


/*
** Turn off syntax highlighting and free style buffer, compiled patterns, and
** related data.
*/
void DocumentWidget::StopHighlighting() {
    if (!highlightData_) {
        return;
    }

    if(TextArea *area = firstPane()) {

        /* Get the line height being used by the highlight fonts in the window,
           to be used after highlighting is turned off to resize the window
           back to the line height of the primary font */
        int oldFontHeight = area->getFontHeight();

        // Free and remove the highlight data from the window
        freeHighlightData(static_cast<WindowHighlightData *>(highlightData_));
        highlightData_ = nullptr;

        /* Remove and detach style buffer and style table from all text
           display(s) of window, and redisplay without highlighting */
        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            RemoveWidgetHighlightEx(area);
        }

        Q_UNUSED(oldFontHeight);
    #if 0
        /* Re-size the window to fit the primary font properly & tell the window
           manager about the potential line-height change as well */
        updateWindowHeight(window, oldFontHeight);
        window->UpdateWMSizeHints();
        window->UpdateMinPaneHeights();
    #endif
    }
}

TextArea *DocumentWidget::firstPane() const {
    const QList<TextArea *> textAreas = textPanes();
    if(!textAreas.isEmpty()) {
        return textAreas[0];
    }

    return nullptr;
}

/*
** Free allocated memory associated with highlight data, including compiled
** regular expressions, style buffer and style table.  Note: be sure to
** nullptr out the widget references to the objects in this structure before
** calling this.  Because of the slow, multi-phase destruction of
** widgets, this data can be referenced even AFTER destroying the widget.
*/
void DocumentWidget::freeHighlightData(WindowHighlightData *hd) {

    if(hd) {
        freePatterns(hd->pass1Patterns);
        freePatterns(hd->pass2Patterns);
        delete hd->styleBuffer;
        delete[] hd->styleTable;
        delete hd;
    }
}

/*
** Free a pattern list and all of its allocated components
*/
void DocumentWidget::freePatterns(HighlightData *patterns) {

    if(patterns) {
        for (int i = 0; patterns[i].style != 0; i++) {
            delete patterns[i].startRE;
            delete patterns[i].endRE;
            delete patterns[i].errorRE;
            delete patterns[i].subPatternRE;
        }

        for (int i = 0; patterns[i].style != 0; i++) {
            delete [] patterns[i].subPatterns;
        }

        delete [] patterns;
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
QString DocumentWidget::GetWindowDelimiters() const {
    if (languageMode_ == PLAIN_LANGUAGE_MODE)
        return QString();
    else
        return LanguageModes[languageMode_]->delimiters;
}

void DocumentWidget::flashTimerTimeout() {
    eraseFlashEx(this);
}

/*
** Dim/undim user programmable menu items which depend on there being
** a selection in their associated window.
*/
void DocumentWidget::DimSelectionDepUserMenuItems(bool sensitive) {
    if(auto win = toWindow()) {
        if (!IsTopDocument()) {
            return;
        }

        dimSelDepItemsInMenu(win->ui.menu_Shell, ShellMenuData, sensitive);
        dimSelDepItemsInMenu(win->ui.menu_Macro, MacroMenuData, sensitive);
        dimSelDepItemsInMenu(contextMenu_,       BGMenuData,    sensitive);

    }
}

void DocumentWidget::dimSelDepItemsInMenu(QMenu *menuPane, const QVector<MenuData> &menuList, bool sensitive) {

    if(menuPane) {
        const QList<QAction *> actions = menuPane->actions();
        for(QAction *action : actions) {
            if(QMenu *subMenu = action->menu()) {
                dimSelDepItemsInMenu(subMenu, menuList, sensitive);
            } else {
                int index = action->data().value<int>();
                if (index < 0 || index >= menuList.size()) {
                    return;
                }

                if (menuList[index].item->input == FROM_SELECTION) {
                    action->setEnabled(sensitive);
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
void DocumentWidget::SaveUndoInformation(int pos, int nInserted, int nDeleted, view::string_view deletedText) {

    const int isUndo = (!undo_.empty() && undo_.front()->inUndo);
    const int isRedo = (!redo_.empty() && redo_.front()->inUndo);

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

    UndoInfo *const currentUndo = undo_.empty() ? nullptr : undo_.front();

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
            appendDeletedText(deletedText, nDeleted, FORWARD);
            currentUndo->endPos++;
            autoSaveCharCount_++;
            return;
        }

        // forward delete
        if ((oldType == ONE_CHAR_DELETE && newType == ONE_CHAR_DELETE) && (pos == currentUndo->startPos)) {
            appendDeletedText(deletedText, nDeleted, FORWARD);
            return;
        }

        // reverse delete
        if ((oldType == ONE_CHAR_DELETE && newType == ONE_CHAR_DELETE) && (pos == currentUndo->startPos - 1)) {
            appendDeletedText(deletedText, nDeleted, REVERSE);
            currentUndo->startPos--;
            currentUndo->endPos--;
            return;
        }
    }

    /*
    ** The user has started a new operation, create a new undo record
    ** and save the new undo data.
    */
    auto undo = new UndoInfo(newType, pos, pos + nInserted);

    // if text was deleted, save it
    if (nDeleted > 0) {
        undo->oldLen = nDeleted + 1; // +1 is for null at end
        undo->oldText = deletedText.to_string();
    }

    // increment the operation count for the autosave feature
    autoSaveOpCount_++;

    /* if the this is currently unmodified, remove the previous
       restoresToSaved marker, and set it on this record */
    if (!fileChanged_) {
        undo->restoresToSaved = true;

        for(UndoInfo *u : undo_) {
            u->restoresToSaved = false;
        }

        for(UndoInfo *u : redo_) {
            u->restoresToSaved = false;
        }
    }

    /* Add the new record to the undo list  unless SaveUndoInfo is
       saving information generated by an Undo operation itself, in
       which case, add the new record to the redo list. */
    if (isUndo) {
        addRedoItem(undo);
    } else {
        addUndoItem(undo);
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
void DocumentWidget::appendDeletedText(view::string_view deletedText, int deletedLen, int direction) {
    UndoInfo *undo = undo_.front();

    // re-allocate, adding space for the new character(s)
    std::string comboText;
    comboText.reserve(undo->oldLen + deletedLen);

    // copy the new character and the already deleted text to the new memory
    if (direction == FORWARD) {
        comboText.append(undo->oldText);
        comboText.append(deletedText.begin(), deletedText.end());
    } else {
        comboText.append(deletedText.begin(), deletedText.end());
        comboText.append(undo->oldText);
    }

    // keep track of the additional memory now used by the undo list
    undoMemUsed_++;

    // free the old saved text and attach the new
    undo->oldText = comboText;
    undo->oldLen += deletedLen;
}

/*
** Add an undo record (already allocated by the caller) to the this's undo
** list if the item pushes the undo operation or character counts past the
** limits, trim the undo list to an acceptable length.
*/
void DocumentWidget::addUndoItem(UndoInfo *undo) {

    // Make the undo menu item sensitive now that there's something to undo
    if (undo_.empty()) {
        if(auto win = toWindow()) {
            win->ui.action_Undo->setEnabled(true);
        }
    }

    // Add the item to the beginning of the list
    undo_.push_front(undo);

    // Increment the operation and memory counts
    undoMemUsed_ += undo->oldLen;

    // Trim the list if it exceeds any of the limits
    if (undo_.size() > UNDO_OP_LIMIT) {
        trimUndoList(UNDO_OP_TRIMTO);
    }

    if (undoMemUsed_ > UNDO_WORRY_LIMIT) {
        trimUndoList(UNDO_WORRY_TRIMTO);
    }

    if (undoMemUsed_ > UNDO_PURGE_LIMIT) {
        trimUndoList(UNDO_PURGE_TRIMTO);
    }
}

/*
** Add an item (already allocated by the caller) to the this's redo list.
*/
void DocumentWidget::addRedoItem(UndoInfo *redo) {
    // Make the redo menu item sensitive now that there's something to redo
    if (redo_.empty()) {
        if(auto win = toWindow()) {
            win->ui.action_Redo->setEnabled(true);
        }
    }

    // Add the item to the beginning of the list
    redo_.push_front(redo);
}

/*
** Pop (remove and free) the current (front) undo record from the undo list
*/
void DocumentWidget::removeUndoItem() {

    if (undo_.empty()) {
        return;
    }

    UndoInfo *undo = undo_.front();


    // Decrement the operation and memory counts
    undoMemUsed_ -= undo->oldLen;

    // Remove and free the item
    undo_.pop_front();
    delete undo;

    // if there are no more undo records left, dim the Undo menu item
    if (undo_.empty()) {
        if(auto win = toWindow()) {
            win->ui.action_Undo->setEnabled(false);
        }
    }
}

/*
** Pop (remove and free) the current (front) redo record from the redo list
*/
void DocumentWidget::removeRedoItem() {
    UndoInfo *redo = redo_.front();

    // Remove and free the item
    redo_.pop_front();
    delete redo;

    // if there are no more redo records left, dim the Redo menu item
    if (redo_.empty()) {
        if(auto win = toWindow()) {
            win->ui.action_Redo->setEnabled(false);
        }
    }
}


/*
** Trim records off of the END of the undo list to reduce it to length
** maxLength
*/
void DocumentWidget::trimUndoList(int maxLength) {

    if (undo_.empty()) {
        return;
    }

    auto it = undo_.begin();
    int i   = 1;

    // Find last item on the list to leave intact
    while(it != undo_.end() && i < maxLength) {
        ++it;
        ++i;
    }

    // Trim off all subsequent entries
    while(it != undo_.end()) {
        UndoInfo *u = *it;

        undoMemUsed_ -= u->oldLen;
        delete u;

        it = undo_.erase(it);
    }
}

void DocumentWidget::Undo() {

    if(auto win = toWindow()) {
        int restoredTextLength;

        // return if nothing to undo
        if (undo_.empty()) {
            return;
        }

        UndoInfo *undo = undo_.front();

        /* BufReplaceEx will eventually call SaveUndoInformation.  This is mostly
           good because it makes accumulating redo operations easier, however
           SaveUndoInformation needs to know that it is being called in the context
           of an undo.  The inUndo field in the undo record indicates that this
           record is in the process of being undone. */
        undo->inUndo = true;

        // use the saved undo information to reverse changes
        buffer_->BufReplaceEx(undo->startPos, undo->endPos, undo->oldText);

        restoredTextLength = undo->oldText.size();
        if (!buffer_->primary_.selected || GetPrefUndoModifiesSelection()) {
            /* position the cursor in the focus pane after the changed text
               to show the user where the undo was done */
            auto area = win->lastFocus_;
            area->TextSetCursorPos(undo->startPos + restoredTextLength);
        }

        if (GetPrefUndoModifiesSelection()) {
            if (restoredTextLength > 0) {
                buffer_->BufSelect(undo->startPos, undo->startPos + restoredTextLength);
            } else {
                buffer_->BufUnselect();
            }
        }
        MakeSelectionVisible(win->lastFocus_);

        /* restore the file's unmodified status if the file was unmodified
           when the change being undone was originally made.  Also, remove
           the backup file, since the text in the buffer is now identical to
           the original file */
        if (undo->restoresToSaved) {
            SetWindowModified(false);
            RemoveBackupFile();
        }

        // free the undo record and remove it from the chain
        removeUndoItem();
    }
}

void DocumentWidget::Redo() {

    if(auto win = toWindow()) {
        int restoredTextLength;

        // return if nothing to redo
        if (redo_.empty()) {
            return;
        }

        UndoInfo *redo = redo_.front();

        /* BufReplaceEx will eventually call SaveUndoInformation.  To indicate
           to SaveUndoInformation that this is the context of a redo operation,
           we set the inUndo indicator in the redo record */
        redo->inUndo = true;

        // use the saved redo information to reverse changes
        buffer_->BufReplaceEx(redo->startPos, redo->endPos, redo->oldText);

        restoredTextLength = redo->oldText.size();
        if (!buffer_->primary_.selected || GetPrefUndoModifiesSelection()) {
            /* position the cursor in the focus pane after the changed text
               to show the user where the undo was done */
            auto area = win->lastFocus_;
            area->TextSetCursorPos(redo->startPos + restoredTextLength);
        }
        if (GetPrefUndoModifiesSelection()) {

            if (restoredTextLength > 0) {
                buffer_->BufSelect(redo->startPos, redo->startPos + restoredTextLength);
            } else {
                buffer_->BufUnselect();
            }
        }
        MakeSelectionVisible(win->lastFocus_);

        /* restore the file's unmodified status if the file was unmodified
           when the change being redone was originally made. Also, remove
           the backup file, since the text in the buffer is now identical to
           the original file */
        if (redo->restoresToSaved) {
            SetWindowModified(false);
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

    int width;
    bool isRect;
    int horizOffset;
    int lastLineNum;
    int left;
    int leftLineNum;
    int leftX;
    int linesToScroll;
    int margin;
    int rectEnd;
    int rectStart;
    int right;
    int rightLineNum;
    int rightX;
    int rows;
    int scrollOffset;
    int targetLineNum;
    int topLineNum;
    int y;

    int topChar  = area->TextFirstVisiblePos();
    int lastChar = area->TextLastVisiblePos();

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

        rows = area->getRows();

        scrollOffset = rows / 3;
        area->TextDGetScroll(&topLineNum, &horizOffset);
        if (right > lastChar) {
            // End of sel. is below bottom of screen
            leftLineNum = topLineNum + area->TextDCountLines(topChar, left, false);
            targetLineNum = topLineNum + scrollOffset;
            if (leftLineNum >= targetLineNum) {
                // Start of sel. is not between top & target
                linesToScroll = area->TextDCountLines(lastChar, right, false) + scrollOffset;
                if (leftLineNum - linesToScroll < targetLineNum)
                    linesToScroll = leftLineNum - targetLineNum;
                // Scroll start of selection to the target line
                area->TextDSetScroll(topLineNum + linesToScroll, horizOffset);
            }
        } else if (left < topChar) {
            // Start of sel. is above top of screen
            lastLineNum = topLineNum + rows;
            rightLineNum = lastLineNum - area->TextDCountLines(right, lastChar, false);
            targetLineNum = lastLineNum - scrollOffset;
            if (rightLineNum <= targetLineNum) {
                // End of sel. is not between bottom & target
                linesToScroll = area->TextDCountLines(left, topChar, false) + scrollOffset;
                if (rightLineNum + linesToScroll > targetLineNum)
                    linesToScroll = targetLineNum - rightLineNum;
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

        margin = area->getMarginWidth();
        width  = area->width();

        if (leftX < margin + area->getLineNumLeft() + area->getLineNumWidth())
            horizOffset -= margin + area->getLineNumLeft() + area->getLineNumWidth() - leftX;
        else if (rightX > width - margin)
            horizOffset += rightX - (width - margin);

        area->TextDSetScroll(topLineNum, horizOffset);
    }

    // make sure that the statistics line is up to date
    UpdateStatsLine(area);
}



/*
** Remove the backup file associated with this window
*/
void DocumentWidget::RemoveBackupFile() {

    // Don't delete backup files when backups aren't activated.
    if (autoSave_ == false)
        return;

    QString name = backupFileNameEx();
    ::remove(name.toLatin1().data());
}

/*
** Generate the name of the backup file for this window from the filename
** and path in the window data structure & write into name
*/
QString DocumentWidget::backupFileNameEx() {

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
void DocumentWidget::CheckForChangesToFile() {

    static DocumentWidget *lastCheckWindow = nullptr;
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


    /* Update the status, but don't pop up a dialog if we're called
       from a place where the window might be iconic (e.g., from the
       replace dialog) or on another desktop.

       This works, but I bet it costs a round-trip to the server.
       Might be better to capture MapNotify/Unmap events instead.

       For tabs that are not on top, we don't want the dialog either,
       and we don't even need to contact the server to find out. By
       performing this check first, we avoid a server round-trip for
       most files in practice. */
    if (!IsTopDocument()) {
        silent = true;
    } else {
        MainWindow *win = toWindow();
        if(!win->isVisible()) {
            silent = true;
        }
    }

    if(auto win = toWindow()) {
        // Get the file mode and modification time
        QString fullname = FullPath();

        struct stat statbuf;
        if (::stat(fullname.toLatin1().data(), &statbuf) != 0) {

            // Return if we've already warned the user or we can't warn him now
            if (fileMissing_ || silent) {
                return;
            }

            /* Can't stat the file -- maybe it's been deleted.
               The filename is now invalid */
            fileMissing_ = true;
            lastModTime_ = 1;
            device_      = 0;
            inode_       = 0;

            /* Warn the user, if they like to be warned (Maybe this should be its
                own preference setting: GetPrefWarnFileDeleted()) */
            if (GetPrefWarnFileMods()) {
                bool save = false;

                //  Set title, message body and button to match stat()'s error.
                switch (errno) {
                case ENOENT:
                    {
                        // A component of the path file_name does not exist.
                        int resp = QMessageBox::critical(this, tr("File not Found"), tr("File '%1' (or directory in its path)\nno longer exists.\nAnother program may have deleted or moved it.").arg(filename_), QMessageBox::Save | QMessageBox::Cancel);
                        save = (resp == QMessageBox::Save);
                    }
                    break;
                case EACCES:
                    {
                        // Search permission denied for a path component. We add one to the response because Re-Save wouldn't really make sense here.
                        int resp = QMessageBox::critical(this, tr("Permission Denied"), tr("You no longer have access to file '%1'.\nAnother program may have changed the permissions of one of its parent directories.").arg(filename_), QMessageBox::Save | QMessageBox::Cancel);
                        save = (resp == QMessageBox::Save);
                    }
                    break;
                default:
                    {
                        // Everything else. This hints at an internal error (eg. ENOTDIR) or at some bad state at the host.
                        int resp = QMessageBox::critical(this, tr("File not Accessible"), tr("Error while checking the status of file '%1':\n    '%2'\nPlease make sure that no data is lost before closing this window.").arg(filename_).arg(QLatin1String(strerror(errno))), QMessageBox::Save | QMessageBox::Cancel);
                        save = (resp == QMessageBox::Save);
                    }
                    break;
                }

                if(save) {
                    SaveWindow();
                }
            }

            // A missing or (re-)saved file can't be read-only.
            //  TODO: A document without a file can be locked though.
            // Make sure that the window was not destroyed behind our back!
            lockReasons_.setPermLocked(false);
            win->UpdateWindowTitle(this);
            UpdateWindowReadOnly();

            return;
        }


        /* Check that the file's read-only status is still correct (but
           only if the file can still be opened successfully in read mode) */
        if (fileMode_ != statbuf.st_mode || fileUid_ != statbuf.st_uid || fileGid_ != statbuf.st_gid) {

            fileMode_ = statbuf.st_mode;
            fileUid_  = statbuf.st_uid;
            fileGid_  = statbuf.st_gid;

            FILE *fp;
            if ((fp = fopen(fullname.toLatin1().data(), "r"))) {
                fclose(fp);

                bool readOnly = access(fullname.toLatin1().data(), W_OK) != 0;

                if (lockReasons_.isPermLocked() != readOnly) {
                    lockReasons_.setPermLocked(readOnly);
                    win->UpdateWindowTitle(this);
                    UpdateWindowReadOnly();
                }
            }
        }


        /* Warn the user if the file has been modified, unless checking is
           turned off or the user has already been warned.  Popping up a dialog
           from a focus callback (which is how this routine is usually called)
           seems to catch Motif off guard, and if the timing is just right, the
           dialog can be left with a still active pointer grab from a Motif menu
           which is still in the process of popping down.  The workaround, below,
           of calling XUngrabPointer is inelegant but seems to fix the problem. */
        if (!silent && ((lastModTime_ != 0 && lastModTime_ != statbuf.st_mtime) || fileMissing_)) {

            lastModTime_ = 0; // Inhibit further warnings
            fileMissing_ = false;
            if (!GetPrefWarnFileMods()) {
                return;
            }

            if (GetPrefWarnRealFileMods() && !cmpWinAgainstFile(fullname)) {
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

QString DocumentWidget::FullPath() const {
    return QString(QLatin1String("%1%2")).arg(path_, filename_);
}

/*
** Update the read-only state of the text area(s) in the window, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void DocumentWidget::UpdateWindowReadOnly() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = toWindow()) {
        bool state = lockReasons_.isAnyLocked();

        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            area->setReadOnly(state);
        }

        no_signals(win->ui.action_Read_Only)->setChecked(state);

        win->ui.action_Read_Only->setEnabled(!lockReasons_.isAnyLockedIgnoringUser());
    }
}

/*
 * Check if the contens of the TextBuffer *buf is equal
 * the contens of the file named fileName. The format of
 * the file (UNIX/DOS/MAC) is handled properly.
 *
 * Return values
 *   0: no difference found
 *  !0: difference found or could not compare contents.
 */
int DocumentWidget::cmpWinAgainstFile(const QString &fileName) {

    char fileString[PREFERRED_CMPBUF_LEN + 2];
    struct stat statbuf;
    int rv;
    int offset;
    char pendingCR         = 0;
    FileFormats fileFormat = fileFormat_;
    TextBuffer *buf        = buffer_;

    FILE *fp = fopen(fileName.toLatin1().data(), "r");
    if (!fp) {
        return 1;
    }

    if (fstat(fileno(fp), &statbuf) != 0) {
        fclose(fp);
        return 1;
    }

    int fileLen = statbuf.st_size;
    // For DOS files, we can't simply check the length
    if (fileFormat != DOS_FILE_FORMAT) {
        if (fileLen != buf->BufGetLength()) {
            fclose(fp);
            return 1;
        }
    } else {
        // If a DOS file is smaller on disk, it's certainly different
        if (fileLen < buf->BufGetLength()) {
            fclose(fp);
            return 1;
        }
    }

    /* For large files, the comparison can take a while. If it takes too long,
       the user should be given a clue about what is happening. */
    char message[MAXPATHLEN + 50];
    snprintf(message, sizeof(message), "Comparing externally modified %s ...", filename_.toLatin1().data());

    int restLen = std::min<int>(PREFERRED_CMPBUF_LEN, fileLen);
    int bufPos  = 0;
    int filePos = 0;

    while (restLen > 0) {
#if 0
        AllWindowsBusy(message);
#endif
        if (pendingCR) {
            fileString[0] = pendingCR;
            offset = 1;
        } else {
            offset = 0;
        }

        int nRead = fread(fileString + offset, sizeof(char), restLen, fp);
        if (nRead != restLen) {
            fclose(fp);
#if 0
            AllWindowsUnbusy();
#endif
            return 1;
        }
        filePos += nRead;

        nRead += offset;

        // check for on-disk file format changes, but only for the first hunk
        if (bufPos == 0 && fileFormat != FormatOfFileEx(view::string_view(fileString, nRead))) {
            fclose(fp);
#if 0
            AllWindowsUnbusy();
#endif
            return 1;
        }

        if (fileFormat == MAC_FILE_FORMAT) {
            ConvertFromMacFileString(fileString, nRead);
        } else if (fileFormat == DOS_FILE_FORMAT) {
            ConvertFromDosFileString(fileString, &nRead, &pendingCR);
        }

        // Beware of 0 chars !
        buf->BufSubstituteNullChars(fileString, nRead);
        rv = buf->BufCmpEx(bufPos, nRead, fileString);
        if (rv) {
            fclose(fp);
#if 0
            AllWindowsUnbusy();
#endif
            return rv;
        }
        bufPos += nRead;
        restLen = std::min<int>(fileLen - filePos, PREFERRED_CMPBUF_LEN);
    }

#if 0
    AllWindowsUnbusy();
#endif
    fclose(fp);
    if (pendingCR) {
        rv = buf->BufCmpEx(bufPos, 1, &pendingCR);
        if (rv) {
            return rv;
        }
        bufPos += 1;
    }

    if (bufPos != buf->BufGetLength()) {
        return 1;
    }
    return 0;
}

void DocumentWidget::RevertToSaved() {

    if(auto win = toWindow()) {
        int insertPositions[MAX_PANES];
        int topLines[MAX_PANES];
        int horizOffsets[MAX_PANES];
        int openFlags = 0;

        // Can't revert untitled windows
        if (!filenameSet_) {
            QMessageBox::warning(this, tr("Error"), tr("Window '%1' was never saved, can't re-read").arg(filename_));
            return;
        }

        const QList<TextArea *> textAreas = textPanes();
        const int panesCount = textAreas.size();

        // save insert & scroll positions of all of the panes to restore later
        for (int i = 0; i < panesCount; i++) {
            TextArea *area = textAreas[i];
            insertPositions[i] = area->TextGetCursorPos();
            area->TextDGetScroll(&topLines[i], &horizOffsets[i]);
        }

        // re-read the file, update the window title if new file is different
        QString name = filename_;
        QString path = path_;

        RemoveBackupFile();
        ClearUndoList();
        openFlags |= lockReasons_.isUserLocked() ? PREF_READ_ONLY : 0;

        if (!doOpen(name, path, openFlags)) {
            /* This is a bit sketchy.  The only error in doOpen that irreperably
                    damages the window is "too much binary data".  It should be
                    pretty rare to be reverting something that was fine only to find
                    that now it has too much binary data. */
            if (!fileMissing_) {
#if 0
                safeClose(window);
#endif
            } else {
                // Treat it like an externally modified file
                lastModTime_ = 0;
                fileMissing_ = false;
            }
            return;
        }


        win->forceShowLineNumbers();
        win->UpdateWindowTitle(this);
        UpdateWindowReadOnly();

        // restore the insert and scroll positions of each pane
        for (int i = 0; i < panesCount; i++) {
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
int DocumentWidget::WriteBackupFile() {
    FILE *fp;

    // Generate a name for the autoSave file
    QString name = backupFileNameEx();

    // remove the old backup file. Well, this might fail - we'll notice later however.
    ::remove(name.toLatin1().data());

    /* open the file, set more restrictive permissions (using default
        permissions was somewhat of a security hole, because permissions were
        independent of those of the original file being edited */
    int fd = ::open(name.toLatin1().data(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd < 0 || (fp = fdopen(fd, "w")) == nullptr) {

        QMessageBox::warning(this, tr("Error writing Backup"), tr("Unable to save backup for %1:\n%2\nAutomatic backup is now off").arg(filename_).arg(QLatin1String(strerror(errno))));
        autoSave_ = false;

        if(auto win = toWindow()) {
            no_signals(win->ui.action_Incremental_Backup)->setChecked(false);
        }
        return false;
    }

    // get the text buffer contents and its length
    std::string fileString = buffer_->BufGetAllEx();

    // If null characters are substituted for, put them back
    buffer_->BufUnsubstituteNullCharsEx(fileString);

    // add a terminating newline if the file doesn't already have one
    if (!fileString.empty() && fileString.back() != '\n') {
        fileString.append("\n"); // null terminator no longer needed
    }

    // write out the file
    ::fwrite(fileString.data(), sizeof(char), fileString.size(), fp);
    if (::ferror(fp)) {
        QMessageBox::critical(this, tr("Error saving Backup"), tr("Error while saving backup for %1:\n%2\nAutomatic backup is now off").arg(filename_).arg(QLatin1String(strerror(errno))));
        ::fclose(fp);
        ::remove(name.toLatin1().data());
        autoSave_ = false;
        return false;
    }

    // close the backup file
    if (fclose(fp) != 0) {
        return false;
    }

    return true;
}

int DocumentWidget::SaveWindow() {

    // Try to ensure our information is up-to-date
    CheckForChangesToFile();

    /* Return success if the file is normal & unchanged or is a
        read-only file. */
    if ((!fileChanged_ && !fileMissing_ && lastModTime_ > 0) || lockReasons_.isAnyLockedIgnoringPerm()) {
        return true;
    }

    // Prompt for a filename if this is an Untitled window
    if (!filenameSet_) {
        return SaveWindowAs(nullptr, false);
    }


    // Check for external modifications and warn the user
    if (GetPrefWarnFileMods() && fileWasModifiedExternally()) {

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
        Q_UNUSED(buttonContinue);

        messageBox.exec();
        if(messageBox.clickedButton() == buttonCancel) {
            // Cancel and mark file as externally modified
            lastModTime_ = 0;
            fileMissing_ = false;
            return false;
        }
    }

    if (writeBckVersion()) {
        return false;
    }

    bool stat = doSave();
    if (stat) {
        RemoveBackupFile();
    }

    return stat;
}

bool DocumentWidget::doSave() {
    struct stat statbuf;
    FILE *fp;

    // Get the full name of the file

    QString fullname = FullPath();

    /*  Check for root and warn him if he wants to write to a file with
        none of the write bits set.  */
    if ((getuid() == 0) && (stat(fullname.toLatin1().data(), &statbuf) == 0) && !(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {

        int result = QMessageBox::warning(this, tr("Writing Read-only File"), tr("File '%1' is marked as read-only.\nDo you want to save anyway?").arg(filename_), QMessageBox::Save | QMessageBox::Cancel);
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
    // TODO(eteran): this seems to be broken for an empty buffer
    //               it is going to read the buffer at index -1 :-/
    if (buffer_->BufGetCharacter(buffer_->BufGetLength() - 1) != '\n' && !buffer_->BufIsEmpty() && GetPrefAppendLF()) {
        buffer_->BufAppendEx("\n");
    }

    // open the file
    fp = ::fopen(fullname.toLatin1().data(), "wb");
    if(!fp) {

        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Error saving File"));
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setText(tr("Unable to save %1:\n%1\n\nSave as a new file?").arg(filename_).arg(QLatin1String(strerror(errno))));

        QPushButton *buttonSaveAs = messageBox.addButton(tr("Save As..."), QMessageBox::AcceptRole);
        QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
        Q_UNUSED(buttonCancel);

        messageBox.exec();
        if(messageBox.clickedButton() == buttonSaveAs) {
            return SaveWindowAs(nullptr, 0);
        }

        return false;
    }

    // get the text buffer contents and its length
    std::string fileString = buffer_->BufGetAllEx();

    // If null characters are substituted for, put them back
    buffer_->BufUnsubstituteNullCharsEx(fileString);

    // If the file is to be saved in DOS or Macintosh format, reconvert
    if (fileFormat_ == DOS_FILE_FORMAT) {
        if (!ConvertToDosFileStringEx(fileString)) {
            QMessageBox::critical(this, tr("Out of Memory"), tr("Out of memory!  Try\nsaving in Unix format"));

            // NOTE(eteran): fixes resource leak error
            fclose(fp);
            return false;
        }
    } else if (fileFormat_ == MAC_FILE_FORMAT) {
        ConvertToMacFileStringEx(fileString);
    }

    // write to the file
    fwrite(fileString.data(), sizeof(char), fileString.size(), fp);


    if (ferror(fp)) {
        QMessageBox::critical(this, tr("Error saving File"), tr("%2 not saved:\n%2").arg(filename_).arg(QLatin1String(strerror(errno))));
        fclose(fp);
        remove(fullname.toLatin1().data());
        return false;
    }

    // close the file
    if (fclose(fp) != 0) {
        QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("Error closing File"), QString(QLatin1String("Error closing file:\n%1")).arg(QLatin1String(strerror(errno))));
        return false;
    }

    // success, file was written
    SetWindowModified(false);

    // update the modification time
    if (::stat(fullname.toLatin1().data(), &statbuf) == 0) {
        lastModTime_ = statbuf.st_mtime;
        fileMissing_ = false;
        device_      = statbuf.st_dev;
        inode_       = statbuf.st_ino;
    } else {
        // This needs to produce an error message -- the file can't be accessed!
        lastModTime_ = 0;
        fileMissing_ = true;
        device_      = 0;
        inode_       = 0;
    }

    return true;
}

int DocumentWidget::SaveWindowAs(const char *newName, bool addWrap) {

    if(auto win = toWindow()) {

        int  retVal;
        char fullname[MAXPATHLEN];
        char filename[MAXPATHLEN];
        char pathname[MAXPATHLEN];

        if(!newName) {
            QFileDialog dialog(this, tr("Save File As"));
            dialog.setFileMode(QFileDialog::AnyFile);
            dialog.setAcceptMode(QFileDialog::AcceptSave);
            dialog.setDirectory((!path_.isEmpty()) ? path_ : QString());
            dialog.setOptions(QFileDialog::DontUseNativeDialog);

            if(QGridLayout* const layout = qobject_cast<QGridLayout*>(dialog.layout())) {
                if(layout->rowCount() == 4 && layout->columnCount() == 3) {
                    auto boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);

                    auto unixCheck = new QRadioButton(tr("&Unix"));
                    auto dosCheck  = new QRadioButton(tr("D&OS"));
                    auto macCheck  = new QRadioButton(tr("&Macintosh"));

                    switch(fileFormat_) {
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
                    if(addWrap) {
                        wrapCheck->setChecked(true);
                    }
    #if 0
                    // TODO(eteran): implement this once this is hoisted into a QObject
                    //               since Qt4 doesn't support lambda based connections
                    connect(wrapCheck, &QCheckBox::toggled, [&wrapCheck](bool checked) {
                        if(checked) {
                            int ret = QMessageBox::information(this, tr("Add Wrap"),
                                tr("This operation adds permanent line breaks to\n"
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

                    if (wrapMode_ == CONTINUOUS_WRAP) {
                        layout->addWidget(wrapCheck, row, 1, 1, 1);
                    }

                    if(dialog.exec()) {
                        if(dosCheck->isChecked()) {
                            fileFormat_ = DOS_FILE_FORMAT;
                        } else if(macCheck->isChecked()) {
                            fileFormat_ = MAC_FILE_FORMAT;
                        } else if(unixCheck->isChecked()) {
                            fileFormat_ = UNIX_FILE_FORMAT;
                        }

                        addWrap = wrapCheck->isChecked();
                        strcpy(fullname, dialog.selectedFiles()[0].toLocal8Bit().data());
                    } else {
                        return false;
                    }

                }
            }
        } else {
            strcpy(fullname, newName);
        }

        // Add newlines if requested
        if (addWrap) {
            addWrapNewlines();
        }

        if (ParseFilename(fullname, filename, pathname) != 0) {
            return false;
        }

        // If the requested file is this file, just save it and return
        if (filename_ == QLatin1String(filename) && path_ == QLatin1String(pathname)) {
            if (writeBckVersion()) {
                return false;
            }

            return doSave();
        }

        /* If the file is open in another window, make user close it.  Note that
           it is possible for user to close the window by hand while the dialog
           is still up, because the dialog is not application modal, so after
           doing the dialog, check again whether the window still exists. */

        DocumentWidget *otherWindow = MainWindow::FindWindowWithFile(QLatin1String(filename), QLatin1String(pathname));
        if (otherWindow) {

            QMessageBox messageBox(this);
            messageBox.setWindowTitle(tr("File open"));
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setText(tr("%1 is open in another NEdit window").arg(QLatin1String(filename)));
            QPushButton *buttonCloseOther = messageBox.addButton(tr("Close Other Window"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel     = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonCloseOther);

            messageBox.exec();
            if(messageBox.clickedButton() == buttonCancel) {
                return false;
            }

            if (otherWindow == MainWindow::FindWindowWithFile(QLatin1String(filename), QLatin1String(pathname))) {
#if 0
                if (!CloseFileAndWindow(otherWindow, PROMPT_SBC_DIALOG_RESPONSE)) {
                    return false;
                }
#endif
            }
        }


        // Destroy the file closed property for the original file
#if 0
        DeleteFileClosedProperty(window);
#endif

        // Change the name of the file and save it under the new name
        RemoveBackupFile();
        filename_ = QLatin1String(filename);
        path_     = QLatin1String(pathname);
        fileMode_ = 0;
        fileUid_  = 0;
        fileGid_  = 0;

        lockReasons_.clear();
        retVal = doSave();
        UpdateWindowReadOnly();
        RefreshTabState();

        // Add the name to the convenience menu of previously opened files
#if 0
        AddToPrevOpenMenu(fullname);
#endif

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
    return 0;

}

/*
** Change a window created in NEdit's continuous wrap mode to the more
** conventional Unix format of embedded newlines.  Indicate to the user
** by turning off Continuous Wrap mode.
*/
void DocumentWidget::addWrapNewlines() {

    int insertPositions[MAX_PANES];
    int topLines[MAX_PANES];
    int horizOffset;

    const QList<TextArea *> textAreas = textPanes();

    // save the insert and scroll positions of each pane
    for(int i = 0; i < textAreas.size(); ++i) {
        TextArea *area = textAreas[i];
        insertPositions[i] = area->TextGetCursorPos();
        area->TextDGetScroll(&topLines[i], &horizOffset);
    }

    // Modify the buffer to add wrapping
    TextArea *area = textAreas[0];
    std::string fileString = area->TextGetWrappedEx(0, buffer_->BufGetLength());

    buffer_->BufSetAllEx(fileString);

    // restore the insert and scroll positions of each pane
    for(int i = 0; i < textAreas.size(); ++i) {
        TextArea *area = textAreas[i];
        area->TextSetCursorPos(insertPositions[i]);
        area->TextDSetScroll(topLines[i], 0);
    }

    /* Show the user that something has happened by turning off
       Continuous Wrap mode */
    if(auto win = toWindow()) {
        no_signals(win->ui.action_Wrap_Continuous)->setChecked(false);
    }
}

/*
** If saveOldVersion is on, copies the existing version of the file to
** <filename>.bck in anticipation of a new version being saved.  Returns
** true if backup fails and user requests that the new file not be written.
*/
bool DocumentWidget::writeBckVersion() {

    char bckname[MAXPATHLEN];
    struct stat statbuf;

    static const size_t IO_BUFFER_SIZE = (1024 * 1024);

    // Do only if version backups are turned on
    if (!saveOldVersion_) {
        return false;
    }

    // Get the full name of the file
    QString fullname = FullPath();

    // Generate name for old version
    if (fullname.size() >= MAXPATHLEN) {
        return bckError(tr("file name too long"), filename_);
    }
    snprintf(bckname, sizeof(bckname), "%s.bck", fullname.toLatin1().data());

    // Delete the old backup file
    // Errors are ignored; we'll notice them later.
    ::remove(bckname);

    /* open the file being edited.  If there are problems with the
       old file, don't bother the user, just skip the backup */
    int in_fd = ::open(fullname.toLatin1().data(), O_RDONLY);
    if (in_fd < 0) {
        return false;
    }

    /* Get permissions of the file.
       We preserve the normal permissions but not ownership, extended
       attributes, et cetera. */
    if (::fstat(in_fd, &statbuf) != 0) {
        return false;
    }

    // open the destination file exclusive and with restrictive permissions.
    int out_fd = ::open(bckname, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (out_fd < 0) {
        return bckError(tr("Error open backup file"), QLatin1String(bckname));
    }

    // Set permissions on new file
    if (::fchmod(out_fd, statbuf.st_mode) != 0) {
        ::close(in_fd);
        ::close(out_fd);
        ::remove(bckname);
        return bckError(tr("fchmod() failed"), QLatin1String(bckname));
    }

    // Allocate I/O buffer
    auto io_buffer = new char[IO_BUFFER_SIZE];

    // copy loop
    for (;;) {
        ssize_t bytes_read;
        ssize_t bytes_written;
        bytes_read = ::read(in_fd, io_buffer, IO_BUFFER_SIZE);

        if (bytes_read < 0) {
            ::close(in_fd);
            ::close(out_fd);
            ::remove(bckname);
            delete [] io_buffer;
            return bckError(tr("read() error"), filename_);
        }

        if (bytes_read == 0) {
            break; // EOF
        }

        // write to the file
        bytes_written = write(out_fd, io_buffer, (size_t)bytes_read);
        if (bytes_written != bytes_read) {
            ::close(in_fd);
            ::close(out_fd);
            ::remove(bckname);
            delete [] io_buffer;
            return bckError(QLatin1String(strerror(errno)), QLatin1String(bckname));
        }
    }

    // close the input and output files
    ::close(in_fd);
    ::close(out_fd);

    delete [] io_buffer;

    return false;
}

/*
** Error processing for writeBckVersion, gives the user option to cancel
** the subsequent save, or continue and optionally turn off versioning
*/
bool DocumentWidget::bckError(const QString &errString, const QString &file) {

    QMessageBox messageBox(this);
    messageBox.setWindowTitle(tr("Error writing Backup"));
    messageBox.setIcon(QMessageBox::Critical);
    messageBox.setText(tr("Couldn't write .bck (last version) file.\n%1: %2").arg(file).arg(errString));

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

        if(auto win = toWindow()) {
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
int DocumentWidget::fileWasModifiedExternally() {
    struct stat statbuf;

    if (!filenameSet_) {
        return false;
    }

    QString fullname = FullPath();

    if (::stat(fullname.toLatin1().data(), &statbuf) != 0) {
        return false;
    }

    if (lastModTime_ == statbuf.st_mtime) {
        return false;
    }

    if (GetPrefWarnRealFileMods() && !cmpWinAgainstFile(fullname)) {
        return false;
    }

    return true;
}

int DocumentWidget::CloseFileAndWindow(int preResponse) {
    int response, stat;

    // Make sure that the window is not in iconified state
    if (fileChanged_) {
        RaiseDocumentWindow();
    }

    /* If the window is a normal & unmodified file or an empty new file,
       or if the user wants to ignore external modifications then
       just close it.  Otherwise ask for confirmation first. */
    if (!fileChanged_ &&
        // Normal File
        ((!fileMissing_ && lastModTime_ > 0) ||
         // New File
         (fileMissing_ && lastModTime_ == 0) ||
         // File deleted/modified externally, ignored by user.
         !GetPrefWarnFileMods())) {
        CloseWindow();
        // up-to-date windows don't have outstanding backup files to close
    } else {
        if (preResponse == PROMPT_SBC_DIALOG_RESPONSE) {

            int resp = QMessageBox::warning(this, tr("Save File"), tr("Save %1 before closing?").arg(filename_), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

            // TODO(eteran): factor out the need for this mapping...
            switch(resp) {
            case QMessageBox::Yes:
                response = 1;
                break;
            case QMessageBox::No:
                response = 2;
                break;
            case QMessageBox::Cancel:
            default:
                response = 3;
                break;
            }

        } else {
            response = preResponse;
        }

        switch(response) {
        case YES_SBC_DIALOG_RESPONSE:
            // Save
            stat = SaveWindow();
            if (stat) {
                CloseWindow();
            } else {
                return false;
            }
            break;

        case NO_SBC_DIALOG_RESPONSE:
            // Don't Save
            RemoveBackupFile();
            CloseWindow();
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
void DocumentWidget::CloseWindow() {


    int state;
    DocumentWidget *win;
    DocumentWidget *topBuf = nullptr;
    DocumentWidget *nextBuf = nullptr;

    Q_UNUSED(state);
    Q_UNUSED(win);
    Q_UNUSED(topBuf);
    Q_UNUSED(nextBuf);

    // Free smart indent macro programs
    EndSmartIndentEx(this);

#if 0
    /* Clean up macro references to the doomed window.  If a macro is
       executing, stop it.  If macro is calling this (closing its own
       window), leave the window alive until the macro completes */
    int keepWindow = !MacroWindowCloseActions(this);

    // Kill shell sub-process and free related memory
    AbortShellCommand(this);

    // Unload the default tips files for this language mode if necessary
    UnloadLanguageModeTipsFile(this);

    /* If a window is closed while it is on the multi-file replace dialog
       list of any other window (or even the same one), we must update those
       lists or we end up with dangling references. Normally, there can
       be only one of those dialogs at the same time (application modal),
       but LessTif doesn't even (always) honor application modalness, so
       there can be more than one dialog. */
    RemoveFromMultiReplaceDialog(this);

    // Destroy the file closed property for this file
    DeleteFileClosedProperty(this);

    /* Remove any possibly pending callback which might fire after the
       widget is gone. */
    cancelTimeOut(&flashTimeoutID_);
    cancelTimeOut(&markTimeoutID_);

    /* if this is the last window, or must be kept alive temporarily because
       it's running the macro calling us, don't close it, make it Untitled */
    if (keepWindow || (WindowList.size() == 1 && this == WindowList.front())) {
        filename_ = QLatin1String("");

        QString name = UniqueUntitledName();
        lockReasons_.clear();

        fileMode_     = 0;
        fileUid_      = 0;
        fileGid_      = 0;
        filename_     = name;
        path_         = QLatin1String("");
        ignoreModify_ = true;

        buffer_->BufSetAllEx("");

        ignoreModify_ = false;
        nMarks_       = 0;
        filenameSet_  = false;
        fileMissing_  = true;
        fileChanged_  = false;
        fileFormat_   = UNIX_FILE_FORMAT;
        lastModTime_  = 0;
        device_       = 0;
        inode_        = 0;

        StopHighlighting(this);
        EndSmartIndent(this);
        UpdateWindowTitle();
        UpdateWindowReadOnly();
        XtSetSensitive(closeItem_, false);
        XtSetSensitive(readOnlyItem_, true);
        XmToggleButtonSetState(readOnlyItem_, false, false);
        ClearUndoList();
        ClearRedoList();
        XmTextSetStringEx(statsLine_, ""); // resets scroll pos of stats line from long file names
        UpdateStatsLine();
        DetermineLanguageMode(this, true);
        RefreshTabState();
        updateLineNumDisp();
        return;
    }
#endif
    // Free syntax highlighting patterns, if any. w/o redisplaying
    FreeHighlightingDataEx(this);

    /* remove the buffer modification callbacks so the buffer will be
       deallocated when the last text widget is destroyed */
    buffer_->BufRemoveModifyCB(modifiedCB, this);
    buffer_->BufRemoveModifyCB(SyntaxHighlightModifyCB, this);


    // free the undo and redo lists
    ClearUndoList();
    ClearRedoList();
#if 0
    // close the document/window
    if (TabCount() > 1) {
        if (MacroRunWindow() && MacroRunWindow() != this && MacroRunWindow()->shell_ == shell_) {
            nextBuf = MacroRunWindow();
            if(nextBuf) {
                nextBuf->RaiseDocument();
            }
        } else if (IsTopDocument()) {
            // need to find a successor before closing a top document
            nextBuf = getNextTabWindow(1, 0, 0);
            if(nextBuf) {
                nextBuf->RaiseDocument();
            }
        } else {
            topBuf = GetTopDocument(shell_);
        }
    }

    // remove the window from the global window list, update window menus
    removeFromWindowList();
    InvalidateWindowMenus();
    CheckCloseDim(); // Close of window running a macro may have been disabled.

    // remove the tab of the closing document from tab bar
    XtDestroyWidget(tab_);

    // refresh tab bar after closing a document
    if (nextBuf) {
        nextBuf->ShowWindowTabBar();
        nextBuf->updateLineNumDisp();
    } else if (topBuf) {
        topBuf->ShowWindowTabBar();
        topBuf->updateLineNumDisp();
    }

    // dim/undim Detach_Tab menu items
    win = nextBuf ? nextBuf : topBuf;
    if (win) {
        state = win->TabCount() > 1;
        XtSetSensitive(win->detachDocumentItem_, state);
        XtSetSensitive(win->contextDetachDocumentItem_, state);
    }

    // dim/undim Attach_Tab menu items
    state = WindowList.front()->TabCount() < WindowCount();

    for(Document *win: WindowList) {
        if (win->IsTopDocument()) {
            XtSetSensitive(win->moveDocumentItem_, state);
            XtSetSensitive(win->contextMoveDocumentItem_, state);
        }
    }

    // free background menu cache for document
    FreeUserBGMenuCache(&userBGMenuCache_);

    // destroy the document's pane, or the window
    if (nextBuf || topBuf) {
        deleteDocument();
    } else {
        // free user menu cache for window
        FreeUserMenuCache(userMenuCache_);

        // remove and deallocate all of the widgets associated with window
        CloseAllPopupsFor(shell_);
        XtDestroyWidget(shell_);
    }


#if 1
    // TODO(eteran): why did I need to add this?!?
    //               looking above, RaiseDocument (which triggers the update)
    //               is called before removeFromWindowList, so I'm not 100% sure
    //               it ever worked without this...
    if(auto dialog = getDialogReplace()) {
        dialog->UpdateReplaceActionButtons();
    }
#endif
#endif
    // deallocate the window data structure
    delete this;
}

DocumentWidget *DocumentWidget::documentFrom(TextArea *area) {

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

void DocumentWidget::open(const char *fullpath) {

    char filename[MAXPATHLEN];
    char pathname[MAXPATHLEN];

    if (ParseFilename(fullpath, filename, pathname) != 0 || strlen(filename) + strlen(pathname) > MAXPATHLEN - 1) {
        fprintf(stderr, "nedit: invalid file name for open action: %s\n", fullpath);
        return;
    }

    EditExistingFileEx(this, QLatin1String(filename), QLatin1String(pathname), 0, nullptr, false, nullptr, GetPrefOpenInTab(), false);

    if(auto win = toWindow()) {
        win->CheckCloseDim();
    }
}

int DocumentWidget::doOpen(const QString &name, const QString &path, int flags) {

    // initialize lock reasons
    lockReasons_.clear();

    // Update the window data structure
    filename_    = name;
    path_        = path;
    filenameSet_ = true;
    fileMissing_ = true;

    struct stat statbuf;
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
        if ((fp = ::fopen(fullname.toLatin1().data(), "r"))) {
            if (::access(fullname.toLatin1().data(), W_OK) != 0) {
                lockReasons_.setPermLocked(true);
            }

        } else if (flags & CREATE && errno == ENOENT) {
            // Give option to create (or to exit if this is the only window)
            if (!(flags & SUPPRESS_CREATE_WARN)) {
#if 0
                /* on Solaris 2.6, and possibly other OSes, dialog won't
                   show if parent window is iconized. */
                RaiseShellWindow(window->shell_, false);
#endif
                QMessageBox msgbox(this);
                QAbstractButton  *exitButton;

                // ask user for next action if file not found

                QList<DocumentWidget *> documents = MainWindow::allDocuments();
                int resp;
                if (this == documents.front() && documents.size() == 1) {

                    msgbox.setIcon(QMessageBox::Warning);
                    msgbox.setWindowTitle(tr("New File"));
                    msgbox.setText(tr("Can't open %1:\n%2").arg(fullname, QLatin1String(strerror(errno))));
                    msgbox.addButton(tr("New File"), QMessageBox::AcceptRole);
                    msgbox.addButton(QMessageBox::Cancel);
                    exitButton = msgbox.addButton(tr("Exit NEdit"), QMessageBox::RejectRole);
                    resp = msgbox.exec();

                } else {

                    msgbox.setIcon(QMessageBox::Warning);
                    msgbox.setWindowTitle(tr("New File"));
                    msgbox.setText(tr("Can't open %1:\n%2").arg(fullname, QLatin1String(strerror(errno))));
                    msgbox.addButton(tr("New File"), QMessageBox::AcceptRole);
                    msgbox.addButton(QMessageBox::Cancel);
                    exitButton = nullptr;
                    resp = msgbox.exec();
                }

                if (resp == QMessageBox::Cancel) {
                    return false;
                } else if (msgbox.clickedButton() == exitButton) {
                    exit(EXIT_SUCCESS);
                }
            }

            // Test if new file can be created
            int fd = ::creat(fullname.toLatin1().data(), 0666);
            if (fd == -1) {
                QMessageBox::critical(this, tr("Error creating File"), tr("Can't create %1:\n%2").arg(fullname, QLatin1String(strerror(errno))));
                return false;
            } else {
                ::close(fd);
                ::remove(fullname.toLatin1().data());
            }

            SetWindowModified(false);
            if ((flags & PREF_READ_ONLY) != 0) {
                lockReasons_.setUserLocked(true);
            }
            UpdateWindowReadOnly();
            return true;
        } else {
            // A true error
            QMessageBox::critical(this, tr("Error opening File"), tr("Could not open %1%2:\n%3").arg(path, name, QLatin1String(strerror(errno))));
            return false;
        }
    }

    /* Get the length of the file, the protection mode, and the time of the
       last modification to the file */
    if (::fstat(fileno(fp), &statbuf) != 0) {
        ::fclose(fp);
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error opening File"), tr("Error opening %1").arg(name));
        filenameSet_ = true;
        return false;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        ::fclose(fp);
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error opening File"), tr("Can't open directory %1").arg(name));
        filenameSet_ = true;
        return false;
    }

#ifdef S_ISBLK
    if (S_ISBLK(statbuf.st_mode)) {
        ::fclose(fp);
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error opening File"), tr("Can't open block device %1").arg(name));
        filenameSet_ = true;
        return false;
    }
#endif

    int fileLen = statbuf.st_size;

    // Allocate space for the whole contents of the file (unfortunately)
    auto fileString = new char[fileLen + 1]; // +1 = space for null
    if(!fileString) {
        ::fclose(fp);
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error while opening File"), tr("File is too large to edit"));
        filenameSet_ = true;
        return false;
    }

    // Read the file into fileString and terminate with a null
    int readLen = ::fread(fileString, sizeof(char), fileLen, fp);
    if (ferror(fp)) {
        ::fclose(fp);
        filenameSet_ = false; // Temp. prevent check for changes.
        QMessageBox::critical(this, tr("Error while opening File"), tr("Error reading %1:\n%2").arg(name, QLatin1String(strerror(errno))));
        filenameSet_ = true;
        delete [] fileString;
        return false;
    }
    fileString[readLen] = '\0';

    // Close the file
    if (::fclose(fp) != 0) {
        // unlikely error
        QMessageBox::warning(this, tr("Error while opening File"), tr("Unable to close file"));
        // we read it successfully, so continue
    }

    /* Any errors that happen after this point leave the window in a
        "broken" state, and thus RevertToSaved will abandon the window if
        window->fileMissing_ is false and doOpen fails. */
    fileMode_    = statbuf.st_mode;
    fileUid_     = statbuf.st_uid;
    fileGid_     = statbuf.st_gid;
    lastModTime_ = statbuf.st_mtime;
    device_      = statbuf.st_dev;
    inode_       = statbuf.st_ino;
    fileMissing_ = false;

    // Detect and convert DOS and Macintosh format files
    if (GetPrefForceOSConversion()) {
        fileFormat_ = FormatOfFileEx(view::string_view(fileString, readLen));
        if (fileFormat_ == DOS_FILE_FORMAT) {
            ConvertFromDosFileString(fileString, &readLen, nullptr);
        } else if (fileFormat_ == MAC_FILE_FORMAT) {
            ConvertFromMacFileString(fileString, readLen);
        }
    }

    // Display the file contents in the text widget
    ignoreModify_ = true;
    buffer_->BufSetAllEx(view::string_view(fileString, readLen));
    ignoreModify_ = false;

    /* Check that the length that the buffer thinks it has is the same
       as what we gave it.  If not, there were probably nuls in the file.
       Substitute them with another character.  If that is impossible, warn
       the user, make the file read-only, and force a substitution */
    if (buffer_->BufGetLength() != readLen) {
        if (!buffer_->BufSubstituteNullChars(fileString, readLen)) {

            QMessageBox msgbox(this);
            msgbox.setIcon(QMessageBox::Critical);
            msgbox.setWindowTitle(tr("Error while opening File"));
            msgbox.setText(tr("Too much binary data in file.  You may view\nit, but not modify or re-save its contents."));
            msgbox.addButton(tr("View"), QMessageBox::AcceptRole);
            msgbox.addButton(QMessageBox::Cancel);
            int resp = msgbox.exec();

            if (resp == QMessageBox::Cancel) {
                return false;
            }

            lockReasons_.setTMBDLocked(true);
            for (char *c = fileString; c < &fileString[readLen]; c++) {
                if (*c == '\0') {
                    *c = (char)0xfe;
                }
            }
            buffer_->nullSubsChar_ = (char)0xfe;
        }
        ignoreModify_ = true;
        buffer_->BufSetAllEx(fileString);
        ignoreModify_ = false;
    }

    // Release the memory that holds fileString
    delete [] fileString;

    // Set window title and file changed flag
    if ((flags & PREF_READ_ONLY) != 0) {
        lockReasons_.setUserLocked(true);
    }

    if (lockReasons_.isPermLocked()) {
        fileChanged_ = false;
       if(auto win = toWindow()) {
            win->UpdateWindowTitle(this);
       }
    } else {
        SetWindowModified(false);
        if (lockReasons_.isAnyLocked()) {
            if(auto win = toWindow()) {
                 win->UpdateWindowTitle(this);
            }
        }
    }
    UpdateWindowReadOnly();

    return true;
}



/*
** refresh window state for this document
*/
void DocumentWidget::RefreshWindowStates() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = toWindow()) {


        if (modeMessageDisplayed_) {
            ui.labelFileAndSize->setText(modeMessage_);
        } else {
            UpdateStatsLine(nullptr);
        }

        UpdateWindowReadOnly();
        win->UpdateWindowTitle(this);
    #if 0
        // show/hide statsline as needed
        if (modeMessageDisplayed_ && !XtIsManaged(statsLineForm_)) {
            // turn on statline to display mode message
            showStatistics(true);
        } else if (showStats_ && !XtIsManaged(statsLineForm_)) {
            // turn on statsline since it is enabled
            showStatistics(true);
        } else if (!showStats_ && !modeMessageDisplayed_ && XtIsManaged(statsLineForm_)) {
            // turn off statsline since there's nothing to show
            showStatistics(false);
        }

        // signal if macro/shell is running
        if (shellCmdData_ || macroCmdData_)
            BeginWait(shell_);
        else
            EndWait(shell_);

        XmUpdateDisplay(statsLine_);
#endif
        refreshMenuBar();

        win->updateLineNumDisp();

    }
}

/*
** Refresh the various settings/state of the shell window per the
** settings of the top document.
*/
void DocumentWidget::refreshMenuBar() {
    RefreshMenuToggleStates();

    // Add/remove language specific menu items
    UpdateUserMenus();

    // refresh selection-sensitive menus
    DimSelectionDepUserMenuItems(wasSelected_);
}

/*
** Refresh the menu entries per the settings of the
** top document.
*/
void DocumentWidget::RefreshMenuToggleStates() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = toWindow()) {
        // File menu
        win->ui.action_Print_Selection->setEnabled(wasSelected_);

        // Edit menu
        win->ui.action_Undo->setEnabled(!undo_.empty());
        win->ui.action_Redo->setEnabled(!redo_.empty());
        win->ui.action_Cut->setEnabled(wasSelected_);
        win->ui.action_Copy->setEnabled(wasSelected_);
        win->ui.action_Delete->setEnabled(wasSelected_);

        // Preferences menu
        no_signals(win->ui.action_Statistics_Line)->setChecked(win->showStats_);
        no_signals(win->ui.action_Incremental_Search_Line)->setChecked(win->showISearchLine_);
        no_signals(win->ui.action_Show_Line_Numbers)->setChecked(win->showLineNumbers_);
        no_signals(win->ui.action_Highlight_Syntax)->setChecked(highlightSyntax_);
        no_signals(win->ui.action_Highlight_Syntax)->setEnabled(languageMode_ != PLAIN_LANGUAGE_MODE);
        no_signals(win->ui.action_Apply_Backlighting)->setChecked(backlightChars_);
        no_signals(win->ui.action_Make_Backup_Copy)->setChecked(saveOldVersion_);
        no_signals(win->ui.action_Incremental_Backup)->setChecked(autoSave_);
        no_signals(win->ui.action_Overtype)->setChecked(overstrike_);
        no_signals(win->ui.action_Matching_Syntax)->setChecked(matchSyntaxBased_);
        no_signals(win->ui.action_Read_Only)->setChecked(lockReasons_.isUserLocked());
        no_signals(win->ui.action_Indent_Smart)->setEnabled(SmartIndentMacrosAvailable(LanguageModeName(languageMode_).toLatin1().data()));

        SetAutoIndent(indentStyle_);
        SetAutoWrap(wrapMode_);
        SetShowMatching(showMatchingStyle_);
        SetLanguageMode(languageMode_, false);

        // Windows Menu
        no_signals(win->ui.action_Split_Pane)->setEnabled(textPanesCount() < MAX_PANES);
        no_signals(win->ui.action_Close_Pane)->setEnabled(textPanesCount() > 0);
        no_signals(win->ui.action_Detach_Tab)->setEnabled(win->ui.tabWidget->count() > 1);

        QList<MainWindow *> windows = MainWindow::allWindows();
        no_signals(win->ui.action_Move_Tab_To)->setEnabled(windows.size() > 1);
    }
}

/*
** Run the newline macro with information from the smart-indent callback
** structure passed by the widget
*/
void DocumentWidget::executeNewlineMacroEx(smartIndentCBStruct *cbInfo) {
    auto winData = static_cast<SmartIndentData *>(smartIndentData_);
    // posValue probably shouldn't be static due to re-entrance issues <slobasso>
    static DataValue posValue = INIT_DATA_VALUE;
    DataValue result;
    RestartData *continuation;
    const char *errMsg;
    int stat;

    /* Beware of recursion: the newline macro may insert a string which
       triggers the newline macro to be called again and so on. Newline
       macros shouldn't insert strings, but nedit must not crash either if
       they do. */
    if (winData->inNewLineMacro) {
        return;
    }

    // Call newline macro with the position at which to add newline/indent
    posValue.val.n = cbInfo->pos;
    ++(winData->inNewLineMacro);
#if 0
    stat = ExecuteMacro(window, winData->newlineMacro, 1, &posValue, &result, &continuation, &errMsg);
#endif

    // Don't allow preemption or time limit.  Must get return value
    while (stat == MACRO_TIME_LIMIT) {
        stat = ContinueMacro(continuation, &result, &errMsg);
    }

    --(winData->inNewLineMacro);
    /* Collect Garbage.  Note that the mod macro does not collect garbage,
       (because collecting per-line is more efficient than per-character)
       but GC now depends on the newline macro being mandatory */
#if 0
    SafeGC();
#endif

    // Process errors in macro execution
    if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
        QMessageBox::critical(this, tr("Smart Indent"), tr("Error in smart indent macro:\n%1").arg(QLatin1String(stat == MACRO_ERROR ? errMsg : "dialogs and shell commands not permitted")));
        EndSmartIndentEx(this);
        return;
    }

    // Validate and return the result
    if (result.tag != INT_TAG || result.val.n < -1 || result.val.n > 1000) {
        QMessageBox::critical(this, tr("Smart Indent"), tr("Smart indent macros must return\ninteger indent distance"));
        EndSmartIndentEx(this);
        return;
    }

    cbInfo->indentRequest = result.val.n;
}

/*
** Set showMatching state to one of NO_FLASH, FLASH_DELIMIT or FLASH_RANGE.
** Update the menu to reflect the change of state.
*/
void DocumentWidget::SetShowMatching(ShowMatchingStyle state) {
    showMatchingStyle_ = state;
    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            no_signals(win->ui.action_Matching_Off)->setChecked(state == NO_FLASH);
            no_signals(win->ui.action_Matching_Delimiter)->setChecked(state == FLASH_DELIMIT);
            no_signals(win->ui.action_Matching_Range)->setChecked(state == FLASH_RANGE);
        }
    }
}
