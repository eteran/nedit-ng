

#include <QApplication>
#include <QKeyEvent>
#include <QtDebug>
#include <QMessageBox>
#include <QClipboard>
#include <QMimeData>

#include "DialogFind.h"
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "TextBuffer.h"
#include "TextArea.h"
#include "nedit.h"
#include "search.h" // for the search type enum
#include "server.h"
#include "preferences.h"
#include "regularExp.h"
#include <memory>

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
DialogFind::DialogFind(MainWindow *window, DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window), document_(document) {
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
		SearchType searchType;
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
        setWindowTitle(tr("Find (in %1)").arg(document_->filename_));
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
void DialogFind::initToggleButtons(SearchType searchType) {
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
	default:
		Q_ASSERT(0);
	}
}

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
void DialogFind::setTextField(DocumentWidget *document) {

    Q_UNUSED(document);

    QString initialText;

    if (GetPrefFindReplaceUsesSelection()) {
        const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
        if(mimeData->hasText()) {
            initialText = mimeData->text();
        }
    }


    // Update the field
    ui.textFind->setText(initialText);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogFind::on_buttonFind_clicked() {

	SearchDirection direction;
	SearchType searchType;
	
	// fetch find string, direction and type from the dialog 
	std::string searchString;
    if (!getFindDlogInfoEx(&direction, &searchString, &searchType)) {
		return;
    }

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// find the text and mark it 
    SearchAndSelectEx(window_, document_, window_->lastFocus_, direction, searchString.c_str(), searchType, GetPrefSearchWraps());

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
int DialogFind::getFindDlogInfoEx(SearchDirection *direction, std::string *searchString, SearchType *searchType) {

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
			auto compiledRE = std::make_unique<regexp>(findText.toStdString(), regexDefault);
		} catch(const regex_error &e) {
			QMessageBox::warning(this, tr("Regex Error"), tr("Please respecify the search string:\n%1").arg(QLatin1String(e.what())));
			return false;
		}
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

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
void DialogFind::setDocument(DocumentWidget *document) {
    document_ = document;
    if(keepDialog()) {
        setWindowTitle(tr("Find (in %1)").arg(document_->filename_));
    }
}
