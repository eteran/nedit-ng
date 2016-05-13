

#include <QApplication>
#include <QKeyEvent>
#include <QtDebug>
#include <QMessageBox>

#include "DialogFind.h"
#include "Document.h"
#include "search.h" // for the search type enum
#include "server.h"
#include "preferences.h"
#include "MotifHelper.h"
#include "regularExp.h"

namespace {

void getSelectionCB(Widget w, SelectionInfo *selectionInfo, Atom *selection, Atom *type, char *value, int *length, int *format) {

	Q_UNUSED(w);
	Q_UNUSED(selection);


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
DialogFind::DialogFind(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window) {
	ui.setupUi(this);
	
	lastRegexCase_   = true;
	lastLiteralCase_ = false;
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
DialogFind::~DialogFind() {
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
	ui.textFind->setFocus();
	
	// TODO(eteran): reset state on show?
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::keyPressEvent(QKeyEvent *event) {

	if(ui.textFind->hasFocus()) {
		int index = window_->fHistIndex_;

		// only process up and down arrow keys 
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			QDialog::keyPressEvent(event);
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
		const char *searchStr;
		int searchType;
		if (index == 0) {
			searchStr  = "";
			searchType = GetPrefSearch();
		} else {
			searchStr  = SearchHistory[historyIndex(index)];
			searchType = SearchTypeHistory[historyIndex(index)];
		}

		// Set the buttons and fields with the selected search type 
		initToggleButtons(searchType);

		ui.textFind->setText(QLatin1String(searchStr));

		// Set the state of the Find ... button 
		fUpdateActionButtons();

		window_->fHistIndex_ = index;
	}
	
	QDialog::keyPressEvent(event);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::on_checkBackward_toggled(bool checked) {
	Q_UNUSED(checked);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::on_checkKeep_toggled(bool checked) {
	if (checked) {
		setWindowTitle(tr("Find (in %1)").arg(QString::fromStdString(window_->filename_)));
	} else {
		setWindowTitle(tr("Find"));
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::fUpdateActionButtons() {
	bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::on_textFind_textChanged(const QString &text) {
	Q_UNUSED(text);
	fUpdateActionButtons();
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
void DialogFind::initToggleButtons(int searchType) {
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
void DialogFind::setTextField(Document *window, time_t time) {

	char *primary_selection = nullptr;
	auto selectionInfo = new SelectionInfo;

	// TODO(eteran): is there even a way to enable this setting?
	if (GetPrefFindReplaceUsesSelection()) {
		selectionInfo->done      = 0;
		selectionInfo->window    = window;
		selectionInfo->selection = nullptr;
		XtGetSelectionValue(window->textArea_, XA_PRIMARY, XA_STRING, (XtSelectionCallbackProc)getSelectionCB, selectionInfo, time);

		XEvent nextEvent;
		while (selectionInfo->done == 0) {
			XtAppNextEvent(XtWidgetToApplicationContext(window->textArea_), &nextEvent);
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

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::on_buttonFind_clicked() {

	SearchDirection direction;
	int searchType;
	
	// fetch find string, direction and type from the dialog 
	std::string searchString;
	if (!getFindDlogInfoEx(&direction, &searchString, &searchType))
		return;


	// Set the initial focus of the dialog back to the search string	
	resetFindTabGroup(window_);

	// find the text and mark it 
	windowNotToClose = window_;
#if 1
	const char *params[4];
	params[0] = searchString.c_str(); // TODO(eteran): is this OK?
	params[1] = directionArg(direction);
	params[2] = searchTypeArg(searchType);
	params[3] = searchWrapArg(GetPrefSearchWraps());
	XtCallActionProc(window_->lastFocus_, const_cast<char *>("find"), nullptr/* callData->event*/, const_cast<char **>(params), 4);
#else
	// TODO(eteran): replace this with signal/slots eventually
	//               the original code would end up calling menu.cpp:findAP
	//               which eventually calls search.cpp:SearchAndSelect
	//               I suppose this indirection implicitly handles searching
	//               being enabled or disabled (ever?), but we'll replace that 
	//               with a QAction eventually anyway
	//               this also is likely reusing the code used to support the scripting
	//               language
	SearchAndSelect(window_, direction, searchString.c_str(), searchType, GetPrefSearchWraps());
#endif
	windowNotToClose = nullptr;

	// pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** which is assumed to be at least SEARCHMAX in length, return search type
** in "searchType", and return TRUE as the function value.  Otherwise,
** return FALSE.
*/
int DialogFind::getFindDlogInfoEx(SearchDirection *direction, std::string *searchString, int *searchType) {


	regexp *compiledRE = nullptr;

	// Get the search string, search type, and direction from the dialog 
	QString findText = ui.textFind->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
			*searchType  = SEARCH_REGEX;
			regexDefault = REDFLT_STANDARD;
		} else {
			*searchType  = SEARCH_REGEX_NOCASE;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			compiledRE = new regexp(findText.toStdString(), regexDefault);
		} catch(const regex_error &e) {
			QMessageBox::warning(this, tr("Regex Error"), tr("Please respecify the search string:\n%1").arg(QLatin1String(e.what())));
			return FALSE;
		}
		delete compiledRE;
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked()) {
				*searchType = SEARCH_CASE_SENSE_WORD;
			} else {
				*searchType = SEARCH_CASE_SENSE;
			}
		} else {
			if (ui.checkWord->isChecked()) {
				*searchType = SEARCH_LITERAL_WORD;
			} else {
				*searchType = SEARCH_LITERAL;
			}
		}
	}

	*direction = ui.checkBackward->isChecked() ? SEARCH_BACKWARD : SEARCH_FORWARD;

	if (isRegexType(*searchType)) {
	}

	// Return the search string 
	if (findText.size() >= SEARCHMAX) {
		QMessageBox::warning(this, tr("String too long"), tr("Search string too long."));
		return FALSE;
	}
	
	*searchString = findText.toStdString();
	return TRUE;
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::on_checkRegex_toggled(bool checked) {

	bool searchRegex     = checked;
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
void DialogFind::on_checkCase_toggled(bool checked) {

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
bool DialogFind::keepDialog() const {
	return ui.checkKeep->isChecked();
}
