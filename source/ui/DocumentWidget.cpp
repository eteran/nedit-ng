
#include <QBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QDateTime>
#include <QtDebug>
#include <QMessageBox>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>


namespace {

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


}

//------------------------------------------------------------------------------
// Name: DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	ui.setupUi(this);

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
	showMatchingStyle_     = GetPrefShowMatching();
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
	modeMessage_           = nullptr;
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
    }
#if 0
    {
        auto area = createTextArea(buffer_);
        splitter_->addWidget(area);
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
#if 0
            WriteBackupFile(window);
#endif
            autoSaveCharCount_ = 0;
            autoSaveOpCount_ = 0;
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
#if 0
		executeNewlineMacro(window, data);
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

void DocumentWidget::RaiseDocument() {
    if(auto win = toWindow()) {
        win->ui.tabWidget->setCurrentWidget(this);
#if 0
    if (WindowList.empty()) {
        return;
    }

    Document *win;

    Document *lastwin = MarkActiveDocument();
    if (lastwin != this && lastwin->IsValidWindow()) {
        lastwin->MarkLastDocument();
    }

    // document already on top?
    XtVaGetValues(mainWin_, XmNuserData, &win, nullptr);
    if (win == this) {
        return;
    }

    // set the document as top document
    XtVaSetValues(mainWin_, XmNuserData, this, nullptr);

    // show the new top document
    XtVaSetValues(mainWin_, XmNworkWindow, splitPane_, nullptr);
    XtManageChild(splitPane_);
    XRaiseWindow(TheDisplay, XtWindow(splitPane_));

    /* Turn on syntax highlight that might have been deferred.
       NB: this must be done after setting the document as
           XmNworkWindow and managed, else the parent shell
       this may shrink on some this-managers such as
       metacity, due to changes made in UpdateWMSizeHints().*/
    if (highlightSyntax_ && highlightData_ == nullptr)
        StartHighlighting(this, false);

    // put away the bg menu tearoffs of last active document
    hideTearOffs(win->bgMenuPane_);

    // restore the bg menu tearoffs of active document
    redisplayTearOffs(bgMenuPane_);

    // set tab as active
    XmLFolderSetActiveTab(tabBar_, getTabPosition(tab_), false);

    /* set keyboard focus. Must be done before unmanaging previous
       top document, else lastFocus will be reset to textArea */
    XmProcessTraversal(lastFocus_, XmTRAVERSE_CURRENT);

    /* we only manage the top document, else the next time a document
       is raised again, it's textpane might not resize properly.
       Also, somehow (bug?) XtUnmanageChild() doesn't hide the
       splitPane, which obscure lower part of the statsform when
       we toggle its components, so we need to put the document at
       the back */
    XLowerWindow(TheDisplay, XtWindow(win->splitPane_));
    XtUnmanageChild(win->splitPane_);
    win->RefreshTabState();

    /* now refresh this state/info. RefreshWindowStates()
       has a lot of work to do, so we update the screen first so
       the document appears to switch swiftly. */
    XmUpdateDisplay(splitPane_);
    RefreshWindowStates();
    RefreshTabState();

    // put away the bg menu tearoffs of last active document
    hideTearOffs(win->bgMenuPane_);

    // restore the bg menu tearoffs of active document
    redisplayTearOffs(bgMenuPane_);

    /* Make sure that the "In Selection" button tracks the presence of a
       selection and that the this inherits the proper search scope. */
    if(auto dialog = getDialogReplace()) {
        dialog->UpdateReplaceActionButtons();
    }

    UpdateWMSizeHints();
#endif
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

        win->ui.action_Highlight_Syntax->blockSignals(true);
        win->ui.action_Highlight_Syntax->setChecked(highlight);
        win->ui.action_Highlight_Syntax->blockSignals(false);

        StopHighlighting();

        // we defer highlighting to RaiseDocument() if doc is hidden
        if (IsTopDocument() && highlight) {
            StartHighlightingEx(this, false);
        }

        // Force a change of smart indent macros (SetAutoIndent will re-start)
        if (indentStyle_ == SMART_INDENT) {
#if 0
            EndSmartIndentEx(this);
#endif
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
#if 0
        EndSmartIndentEx(this);
#endif
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
            win->ui.action_Indent_Smart->blockSignals(true);
            win->ui.action_Indent_Smart->setChecked(smartIndent);
            win->ui.action_Indent_Smart->blockSignals(false);

            win->ui.action_Indent_On->blockSignals(true);
            win->ui.action_Indent_On->setChecked(autoIndent);
            win->ui.action_Indent_On->blockSignals(false);

            win->ui.action_Indent_Off->blockSignals(true);
            win->ui.action_Indent_Off->setChecked(state == NO_AUTO_INDENT);
            win->ui.action_Indent_Off->blockSignals(false);
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
            win->ui.action_Wrap_Auto_Newline->blockSignals(true);
            win->ui.action_Wrap_Auto_Newline->setChecked(autoWrap);
            win->ui.action_Wrap_Auto_Newline->blockSignals(false);

            win->ui.action_Wrap_Continuous->blockSignals(true);
            win->ui.action_Wrap_Continuous->setChecked(contWrap);
            win->ui.action_Wrap_Continuous->blockSignals(false);

            win->ui.action_Wrap_None->blockSignals(true);
            win->ui.action_Wrap_None->setChecked(state == NO_WRAP);
            win->ui.action_Wrap_None->blockSignals(false);
        }
    }
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
    #if 0
                if(save) {
                    SaveWindow(window);
                }
    #endif
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

        win->ui.action_Read_Only->blockSignals(true);
        win->ui.action_Read_Only->setChecked(state);
        win->ui.action_Read_Only->blockSignals(false);

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
#if 0
        if (!doOpen(window, name.toLatin1().data(), path.toLatin1().data(), openFlags)) {
            /* This is a bit sketchy.  The only error in doOpen that irreperably
                    damages the window is "too much binary data".  It should be
                    pretty rare to be reverting something that was fine only to find
                    that now it has too much binary data. */
            if (!window->fileMissing_)
                safeClose(window);
            else {
                // Treat it like an externally modified file
                window->lastModTime_ = 0;
                window->fileMissing_ = false;
            }
            return;
        }
        forceShowLineNumbers(window);
    #endif
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
