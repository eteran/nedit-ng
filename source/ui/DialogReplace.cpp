
#include <QApplication>
#include <QKeyEvent>
#include <QtDebug>
#include <QMessageBox>

#include "DialogReplace.h"
#include "DialogMultiReplace.h"
#include "Document.h"
#include "search.h" // for the search type enum
#include "server.h"
#include "preferences.h"
#include "MotifHelper.h"
#include "regularExp.h"

namespace {

// TODO(eteran): deprecate this in favor or a member function
void getSelectionCB(Widget w, SelectionInfo *selectionInfo, Atom *selection, Atom *type, char *value, int *length, int *format) {

	(void)w;
	(void)selection;

	Document *window = selectionInfo->window;
	(void)window;

	// return an empty string if we can't get the selection data 
	if (*type == XT_CONVERT_FAIL || *type != XA_STRING || value == nullptr || *length == 0) {
		XtFree(value);
		selectionInfo->selection = nullptr;
		selectionInfo->done = 1;
		return;
	}
	// return an empty string if the data is not of the correct format. 
	if (*format != 8) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Invalid Format"), QLatin1String("NEdit can't handle non 8-bit text"));
		XtFree(value);
		selectionInfo->selection = nullptr;
		selectionInfo->done = 1;
		return;
	}
	selectionInfo->selection = XtMalloc(*length + 1);
	memcpy(selectionInfo->selection, value, *length);
	selectionInfo->selection[*length] = 0;
	XtFree(value);
	selectionInfo->done = 1;
}

}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
DialogReplace::DialogReplace(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window) {
	
	ui.setupUi(this);
	
	lastRegexCase_   = true;
	lastLiteralCase_ = false;
	
#if REPLACE_SCOPE
	replaceScope_ = REPL_SCOPE_WIN;
#endif
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
DialogReplace::~DialogReplace() {
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
	ui.textFind->setFocus();
	
	// TODO(eteran): reset state on show
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::keyPressEvent(QKeyEvent *event) {

	if(ui.textFind->hasFocus()) {
		int index = window_->fHistIndex_;

		// only process up and down arrow keys 
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			return;
		}

		// increment or decrement the index depending on which arrow was pressed 
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return 
		if (index != 0 && historyIndex(index) == -1) {
			QApplication::beep();
			return;
		}

		// determine the strings and button settings to use 
		QString searchStr;
		int searchType;
		if (index == 0) {
			searchStr  = QLatin1String("");
			searchType = GetPrefSearch();
		} else {
			searchStr  = QLatin1String(SearchHistory[historyIndex(index)]);
			searchType = SearchTypeHistory[historyIndex(index)];
		}

		// Set the buttons and fields with the selected search type 
		initToggleButtons(searchType);

		ui.textFind->setText(searchStr);

		// Set the state of the Find ... button 
		fUpdateActionButtons();

		window_->fHistIndex_ = index;
	}

	if(ui.textReplace->hasFocus()) {
		int index = window_->rHistIndex_;

		// only process up and down arrow keys 
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			return;
		}

		// increment or decrement the index depending on which arrow was pressed 
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return 
		if (index != 0 && historyIndex(index) == -1) {
			QApplication::beep();
			return;
		}

		// change only the replace field information 
		if (index == 0) {
			ui.textReplace->setText(QString());
		} else {
			ui.textReplace->setText(QLatin1String(ReplaceHistory[historyIndex(index)]));
		}

		window_->rHistIndex_ = index;
	}
	
	QDialog::keyPressEvent(event);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkBackward_toggled(bool checked) {
	Q_UNUSED(checked);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkKeep_toggled(bool checked) {
	if (checked) {
		setWindowTitle(tr("Find/Replace (in %1)").arg(QString::fromStdString(window_->filename_)));
	} else {
		setWindowTitle(tr("Find/Replace"));
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_textFind_textChanged(const QString &text) {
	Q_UNUSED(text);
	UpdateReplaceActionButtons();
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonFind_clicked() {
	
	char searchString[SEARCHMAX];
	char replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
	if (!getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string	
	resetReplaceTabGroup(window_);

	// Find the text and mark it 

	windowNotToClose = window_;

	const char *params[4];
	params[0] = searchString;
	params[1] = directionArg(direction);
	params[2] = searchTypeArg(searchType);
	params[3] = searchWrapArg(GetPrefSearchWraps());
	XtCallActionProc(window_->lastFocus_, (String) "find", nullptr /*callData->event*/, (char **)params, 4);
	
	windowNotToClose = nullptr;

	/* Doctor the search history generated by the action to include the
	   replace string (if any), so the replace string can be used on
	   subsequent replaces, even though no actual replacement was done. */
	if (historyIndex(1) != -1 && !strcmp(SearchHistory[historyIndex(1)], searchString)) {
		XtFree(ReplaceHistory[historyIndex(1)]);
		ReplaceHistory[historyIndex(1)] = XtNewStringEx(replaceString);
	}

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonReplace_clicked() {
	// TODO(eteran): implement this
#if REPLACE_SCOPE && 0
	switch (replaceScope_) {
	case REPL_SCOPE_WIN:
		replaceAllCB(w, window, callData);
		break;
	case REPL_SCOPE_SEL:
		rInSelCB(w, window, callData);
		break;
	case REPL_SCOPE_MULTI:
		replaceMultiFileCB(w, window, callData);
		break;
	}
#else
	char searchString[SEARCHMAX];
	char replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
	if (!getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	resetReplaceTabGroup(window_);

	// Find the text and replace it 

	
	windowNotToClose = window_;

	const char *params[5];
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = directionArg(direction);
	params[3] = searchTypeArg(searchType);
	params[4] = searchWrapArg(GetPrefSearchWraps());
	XtCallActionProc(window_->lastFocus_, (String) "replace", nullptr /* callData->event */, (char **)params, 5);
		
	windowNotToClose = nullptr;

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
#endif
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonReplaceFind_clicked() {

	char searchString[SEARCHMAX + 1];
	char replaceString[SEARCHMAX + 1];
	SearchDirection direction;
	int searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
	if (!getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	resetReplaceTabGroup(window_);

	// Find the text and replace it 
	windowNotToClose = window_;
	
	const char *params[4];
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = directionArg(direction);
	params[3] = searchTypeArg(searchType);
	XtCallActionProc(window_->lastFocus_, (String) "replace_find", nullptr /* callData->event*/, (char **)params, 4);
	
	windowNotToClose = nullptr;

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}

}

#ifdef REPLACE_SCOPE

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_radioWindow_toggled(bool checked) {
	if (checked) {
		replaceScope_ = REPL_SCOPE_WIN;
		UpdateReplaceActionButtons();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_radioSelection_toggled(bool checked) {
	if (checked) {
		replaceScope_ = REPL_SCOPE_SEL;
		UpdateReplaceActionButtons();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_radioMulti_toggled(bool checked) {

	if (checked) {
		replaceScope_ = REPL_SCOPE_MULTI;
		UpdateReplaceActionButtons();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonAll_clicked() {
	
	// TODO(eteran): implement this
#if 0
	switch (replaceScope_) {
	case REPL_SCOPE_WIN:
		break;
	case REPL_SCOPE_SEL:
		break;
	case REPL_SCOPE_MULTI:
		replaceMultiFileCB(w, window, callData);
		break;
	}
#endif
}

#else

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonWindow_clicked() {


	char searchString[SEARCHMAX], replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
	if (!getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string	
	resetReplaceTabGroup(window_);

	// do replacement 
	windowNotToClose = window_;
	
	const char *params[3];
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = searchTypeArg(searchType);	
	XtCallActionProc(window_->lastFocus_, (String) "replace_all", nullptr /*callData->event*/, (char **)params, 3);
	windowNotToClose = nullptr;

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonSelection_clicked() {

	char searchString[SEARCHMAX];
	char replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
	if (!getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	resetReplaceTabGroup(window_);

	// do replacement 
	windowNotToClose = window_;
	
	const char *params[3];
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = searchTypeArg(searchType);	
	XtCallActionProc(window_->lastFocus_, "replace_in_selection", nullptr /*callData->event*/, (char **)params, 3);
	
	windowNotToClose = nullptr;

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonMulti_clicked() {

	char searchString[SEARCHMAX];
	char replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;

	// Validate and fetch the find and replace strings from the dialog 
	if (!getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;

	// Don't let the user select files when no replacement can be made 
	if (*searchString == '\0') {
		// Set the initial focus of the dialog back to the search string 
		resetReplaceTabGroup(window_);
		// pop down the replace dialog 
		
		if(!keepDialog()) {
			unmanageReplaceDialogs(window_);
		}
		return;
	}

	// Create the dialog if it doesn't already exist 
	if (!dialogMultiReplace_) {
		dialogMultiReplace_ = new DialogMultiReplace(window_, this, this);
	}

	// Raising the window doesn't make sense. It is modal, so we can't get here unless it is unmanaged
	// Prepare a list of writable windows 
	collectWritableWindows();

	// Initialize/update the list of files. 
	uploadFileListItems(false);

	// Display the dialog 
	// TODO(eteran): center on pointer
	dialogMultiReplace_->show();
}

#endif

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkRegex_toggled(bool checked) {


	bool searchRegex = checked;
	bool searchCaseSense = ui.checkCase->isChecked();

	// In sticky mode, restore the state of the Case Sensitive button 
	if (GetPrefStickyCaseSenseBtn()) {
		if (searchRegex) {
			lastLiteralCase_ = searchCaseSense;
			ui.checkCase->setChecked(lastRegexCase_);
		} else {
			lastRegexCase_ = searchCaseSense;
			ui.checkCase->setChecked(lastLiteralCase_);
		}
	}
	// make the Whole Word button insensitive for regex searches 
	ui.checkWord->setEnabled(!searchRegex);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkCase_toggled(bool checked) {

	bool searchCaseSense = checked;

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (ui.checkRegex->isChecked()) {
		lastRegexCase_ = searchCaseSense;
	} else {
		lastLiteralCase_ = searchCaseSense;
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::setTextField(Document *window, time_t time) {
	
	char *primary_selection = nullptr;
	auto selectionInfo = new SelectionInfo;

	// TODO(eteran): is there even a way to enable this setting?
	if (GetPrefFindReplaceUsesSelection()) {
		selectionInfo->done      = 0;
		selectionInfo->window    = window;
		selectionInfo->selection = nullptr;
		XtGetSelectionValue(window_->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)getSelectionCB, selectionInfo, time);

		XEvent nextEvent;
		while (selectionInfo->done == 0) {
			XtAppNextEvent(XtWidgetToApplicationContext(window_->textArea_), &nextEvent);
			ServerDispatchEvent(&nextEvent);
		}

		primary_selection = selectionInfo->selection;
	}

	if (primary_selection == nullptr) {
		primary_selection = XtNewStringEx("");
	}

	// Update the field 
	ui.textFind->setText(QLatin1String(primary_selection));

	XtFree(primary_selection);

	delete selectionInfo;
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
void DialogReplace::initToggleButtons(int searchType) {
	/* Set the initial search type and remember the corresponding case
	   sensitivity states in case sticky case sensitivity is required. */
	switch (searchType) {
	case SEARCH_LITERAL:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_CASE_SENSE:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_LITERAL_WORD:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(true);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_CASE_SENSE_WORD:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(true);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_REGEX:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(false);
		}
		break;
	case SEARCH_REGEX_NOCASE:
		lastLiteralCase_ = false;
		lastRegexCase_   = false;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(false);
		}
		break;
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::fUpdateActionButtons() {
	bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::UpdateReplaceActionButtons() {

#ifdef REPLACE_SCOPE
	// Is there any text in the search for field 
	bool searchText = !ui.textFind->text().isEmpty();

	switch (replaceScope_) {
	case REPL_SCOPE_WIN:
		// Enable all buttons, if there is any text in the search field. 
		rSetActionButtons(searchText, searchText, searchText, searchText);
		break;

	case REPL_SCOPE_SEL:
		// Only enable Replace All, if a selection exists and text in search field. 
		rSetActionButtons(false, false, false, searchText && window_->wasSelected_);
		break;

	case REPL_SCOPE_MULTI:
		// Only enable Replace All, if text in search field. 
		rSetActionButtons(false, false, false, searchText);
		break;
	}
#else
	// Is there any text in the search for field 
	bool searchText = !ui.textFind->text().isEmpty();
	rSetActionButtons(searchText, searchText, searchText, searchText, searchText && window_->wasSelected_, searchText && (countWritableWindows() > 1));
#endif
}

#ifdef REPLACE_SCOPE
void DialogReplace::rSetActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceAllBtn) {
	
	ui.buttonReplace->setEnabled(replaceBtn);
	ui.buttonFind->setEnabled(replaceFindBtn);
	ui.buttonReplaceFind->setEnabled(replaceAndFindBtn);
	ui.buttonAll->setEnabled(replaceAllBtn);
}
#else
void DialogReplace::rSetActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceInWinBtn, bool replaceInSelBtn, bool replaceAllBtn) {

	ui.buttonReplace->setEnabled(replaceBtn);
	ui.buttonFind->setEnabled(replaceFindBtn);
	ui.buttonReplaceFind->setEnabled(replaceAndFindBtn);
	ui.buttonWindow->setEnabled(replaceInWinBtn);
	ui.buttonSelection->setEnabled(replaceInSelBtn);
	ui.buttonMulti->setEnabled(replaceAllBtn); // all is multi here
}
#endif

/*
** Fetch and verify (particularly regular expression) search and replace
** strings and search type from the Replace dialog.  If the strings are ok,
** save a copy in the search history, copy them in to "searchString",
** "replaceString', which are assumed to be at least SEARCHMAX in length,
** return search type in "searchType", and return TRUE as the function
** value.  Otherwise, return FALSE.
*/
int DialogReplace::getReplaceDlogInfo(SearchDirection *direction, char *searchString, char *replaceString, int *searchType) {
	regexp *compiledRE = nullptr;

	/* Get the search and replace strings, search type, and direction
	   from the dialog */
	QString replaceText     = ui.textFind->text();
	QString replaceWithText = ui.textReplace->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
			*searchType = SEARCH_REGEX;
			regexDefault = REDFLT_STANDARD;
		} else {
			*searchType = SEARCH_REGEX_NOCASE;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			compiledRE = new regexp(replaceText.toLatin1().data(), regexDefault);
		} catch(const regex_error &e) {
			QMessageBox::warning(this, tr("Search String"), tr("Please respecify the search string:\n%1").arg(QLatin1String(e.what())));
			return FALSE;
		}
		delete compiledRE;
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked())
				*searchType = SEARCH_CASE_SENSE_WORD;
			else
				*searchType = SEARCH_CASE_SENSE;
		} else {
			if (ui.checkWord->isChecked())
				*searchType = SEARCH_LITERAL_WORD;
			else
				*searchType = SEARCH_LITERAL;
		}
	}

	*direction = ui.checkBackward->isChecked() ? SEARCH_BACKWARD : SEARCH_FORWARD;

	// Return strings 
	if (replaceText.size() >= SEARCHMAX) {
		QMessageBox::warning(this, tr("String too long"), tr("Search string too long."));
		return FALSE;
	}
	if (replaceWithText.size() >= SEARCHMAX) {
		QMessageBox::warning(this, tr("String too long"), tr("Replace string too long."));
		return FALSE;
	}
	strcpy(searchString, replaceText.toLatin1().data());
	strcpy(replaceString, replaceWithText.toLatin1().data());
	return TRUE;
}


/*
** Collects a list of writable windows (sorted by file name).
** The previous list, if any is freed first.
**/
void DialogReplace::collectWritableWindows() {
	int nWritable = countWritableWindows();
	int i = 0;
	Document **windows;

	delete [] window_->writableWindows_;

	// Make a sorted list of writable windows 
	windows = new Document*[nWritable];
	
	
	for(Document *w: WindowList) {
		if (!IS_ANY_LOCKED(w->lockReasons_)) {
			windows[i++] = w;
		}
	}

	std::sort(windows, windows + nWritable, [](const Document *lhs, const Document *rhs) {
		return lhs->filename_ < rhs->filename_;
	});

	window_->writableWindows_  = windows;
	window_->nWritableWindows_ = nWritable;
}


/*
 * Uploads the file items to the multi-file replament dialog list.
 * A boolean argument indicates whether the elements currently in the
 * list have to be replaced or not.
 * Depending on the state of the "Show path names" toggle button, either
 * the file names or the path names are listed.
 */
void DialogReplace::uploadFileListItems(bool replace) {

	int nWritable = window_->nWritableWindows_;

	QStringList names;

	bool usePathNames = dialogMultiReplace_->ui.checkShowPaths->isChecked();

	/* Note: the windows are sorted alphabetically by _file_ name. This
	         order is _not_ changed when we switch to path names. That
	         would be confusing for the user */

	for (int i = 0; i < nWritable; ++i) {
		Document *w = window_->writableWindows_[i];
		
		QString name;
		
		if (usePathNames && window_->filenameSet_) {
			name = tr("%1%2").arg(QString::fromStdString(w->path_)).arg(QString::fromStdString(w->filename_));
		} else {
			name = QString::fromStdString(w->filename_);
		}
		
		names.push_back(name);
	}


	// TODO(eteran): generally get the selection behavior to be more like the original
	//

	if (replace) {

		
		// TODO(eteran): what is the story with "maintaining the selection"
		//               the code *appeared* to record the positions of the current
		//               selections, replace ALL the names, then highlight the 
		//               elements at the positions noted (even though they are likely 
		//               different values...).
		dialogMultiReplace_->ui.listFiles->clear();
		dialogMultiReplace_->ui.listFiles->addItems(names);
	} else {
		dialogMultiReplace_->ui.listFiles->clear();
		dialogMultiReplace_->ui.listFiles->addItems(names);
		
		if(dialogMultiReplace_->ui.listFiles->selectedItems().isEmpty()) {
			dialogMultiReplace_->ui.listFiles->selectAll();
		}
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
bool DialogReplace::keepDialog() const {
	return ui.checkKeep->isChecked();
}
