
#include "DialogFind.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "preferences.h"
#include "Regex.h"
#include "Util/regex.h"
#include "Search.h"

#include <QClipboard>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QTimer>

/**
 * @brief DialogFind::DialogFind
 * @param window
 * @param document
 * @param f
 */
DialogFind::DialogFind(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f) : Dialog(window, f), window_(window), document_(document) {
    ui.setupUi(this);
}

/**
 * @brief DialogFind::showEvent
 * @param event
 */
void DialogFind::showEvent(QShowEvent *event) {    
    Dialog::showEvent(event);
    ui.textFind->setFocus();
}

/**
 * @brief DialogFind::keyPressEvent
 * @param event
 */
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
        if (index != 0 && Search::historyIndex(index) == -1) {
			QApplication::beep();
			return;
		}

		// determine the strings and button settings to use 
        QString searchStr;
		SearchType searchType;
		if (index == 0) {
            searchStr  = QString();
            searchType = Preferences::GetPrefSearch();
        } else {
            const Search::HistoryEntry *entry = Search::HistoryByIndex(index);
            Q_ASSERT(entry);
            searchStr  = entry->search;
            searchType = entry->type;
		}

		// Set the buttons and fields with the selected search type 
		initToggleButtons(searchType);

        ui.textFind->setText(searchStr);

		// Set the state of the Find ... button 
        updateFindButton();

		window_->fHistIndex_ = index;
	}
	
	QDialog::keyPressEvent(event);
}

/**
 * @brief DialogFind::on_checkKeep_toggled
 * @param checked
 */
void DialogFind::on_checkKeep_toggled(bool checked) {
	if (checked) {
        setWindowTitle(tr("Find (in %1)").arg(document_->FileName()));
	} else {
		setWindowTitle(tr("Find"));
	}
}

/**
 * @brief DialogFind::updateFindButton
 */
void DialogFind::updateFindButton() {
	bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

/**
 * @brief DialogFind::on_textFind_textChanged
 * @param text
 */
void DialogFind::on_textFind_textChanged(const QString &text) {
	Q_UNUSED(text);
    updateFindButton();
}

/*
** initialize the state of the regex/case/word toggle buttons, and the sticky
** case sensitivity states.
*/
void DialogFind::initToggleButtons(SearchType searchType) {
	/* Set the initial search type and remember the corresponding case
	   sensitivity states in case sticky case sensitivity is required. */
	switch (searchType) {
    case SearchType::Literal:
        lastLiteralCase_ = false;
        lastRegexCase_   = true;
        ui.checkRegex->setChecked(false);
        ui.checkCase->setChecked(false);
        ui.checkWord->setChecked(false);
        ui.checkWord->setEnabled(true);
		break;
    case SearchType::CaseSense:
        lastLiteralCase_ = true;
        lastRegexCase_   = true;
        ui.checkRegex->setChecked(false);
        ui.checkCase->setChecked(true);
        ui.checkWord->setChecked(false);
        ui.checkWord->setEnabled(true);
		break;
    case SearchType::LiteralWord:
        lastLiteralCase_ = false;
        lastRegexCase_   = true;
        ui.checkRegex->setChecked(false);
        ui.checkCase->setChecked(false);
        ui.checkWord->setChecked(true);
        ui.checkWord->setEnabled(true);
		break;
    case SearchType::CaseSenseWord:
        lastLiteralCase_ = true;
        lastRegexCase_   = true;
        ui.checkRegex->setChecked(false);
        ui.checkCase->setChecked(true);
        ui.checkWord->setChecked(true);
        ui.checkWord->setEnabled(true);
		break;
    case SearchType::Regex:
        lastLiteralCase_ = false;
        lastRegexCase_   = true;
        ui.checkRegex->setChecked(true);
        ui.checkCase->setChecked(true);
        ui.checkWord->setChecked(false);
        ui.checkWord->setEnabled(false);
		break;
    case SearchType::RegexNoCase:
        lastLiteralCase_ = false;
        lastRegexCase_   = false;
        ui.checkRegex->setChecked(true);
        ui.checkCase->setChecked(false);
        ui.checkWord->setChecked(false);
        ui.checkWord->setEnabled(false);
		break;
	}
}

/**
 * @brief DialogFind::setTextFieldFromDocument
 * @param document
 */
void DialogFind::setTextFieldFromDocument(DocumentWidget *document) {

    QString initialText;

    if (Preferences::GetPrefFindReplaceUsesSelection()) {
        initialText = document->GetAnySelection(/*beep_on_error=*/false);
    }

    // Update the field
    ui.textFind->setText(initialText);
}

/**
 * @brief DialogFind::on_buttonFind_clicked
 */
void DialogFind::on_buttonFind_clicked() {

	// fetch find string, direction and type from the dialog     
    boost::optional<Fields> fields = getFields();
    if (!fields) {
		return;
    }

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// find the text and mark it 
    window_->action_Find(
                document_,
                fields->searchString,
                fields->direction,
                fields->searchType,
                Preferences::GetPrefSearchWraps());

	if (!keepDialog()) {
		hide();
	}
}

/*
** Fetch and verify (particularly regular expression) search and replace
** strings and search type from the Find dialog.  If the strings are ok,
** save a copy in the search history, and return the fields
*/
boost::optional<DialogFind::Fields> DialogFind::getFields() {

    Fields fields;

	// Get the search string, search type, and direction from the dialog 
	QString findText = ui.textFind->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
            fields.searchType = SearchType::Regex;
            regexDefault      = REDFLT_STANDARD;
		} else {
            fields.searchType = SearchType::RegexNoCase;
            regexDefault      = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
            auto compiledRE = make_regex(findText, regexDefault);
		} catch(const RegexError &e) {
            QMessageBox::warning(this, tr("Regex Error"), tr("Please respecify the search string:\n%1").arg(QString::fromLatin1(e.what())));
            return boost::none;
		}
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked()) {
                fields.searchType = SearchType::CaseSenseWord;
			} else {
                fields.searchType = SearchType::CaseSense;
			}
		} else {
			if (ui.checkWord->isChecked()) {
                fields.searchType = SearchType::LiteralWord;
			} else {
                fields.searchType = SearchType::Literal;
			}
		}
	}

    fields.direction    = ui.checkBackward->isChecked() ? Direction::Backward : Direction::Forward;
    fields.searchString = findText;
    return fields;
}

/**
 * @brief DialogFind::on_checkRegex_toggled
 * @param checked
 */
void DialogFind::on_checkRegex_toggled(bool checked) {

	bool searchRegex     = checked;
	bool searchCaseSense = ui.checkCase->isChecked();

	// In sticky mode, restore the state of the Case Sensitive button 
    if (Preferences::GetPrefStickyCaseSenseBtn()) {
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

/**
 * @brief DialogFind::on_checkCase_toggled
 * @param checked
 */
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

/**
 * @brief DialogFind::keepDialog
 * @return
 */
bool DialogFind::keepDialog() const {
	return ui.checkKeep->isChecked();
}

/**
 * @brief DialogFind::setDocument
 * @param document
 */
void DialogFind::setDocument(DocumentWidget *document) {
    document_ = document;
    if(keepDialog()) {
        setWindowTitle(tr("Find (in %1)").arg(document_->FileName()));
    }
}

/**
 * @brief DialogFind::setDirection
 * @param direction
 */
void DialogFind::setDirection(Direction direction) {
    ui.checkBackward->setChecked(direction == Direction::Backward);
}

/**
 * @brief DialogReplace::setKeepDialog
 * @param keep
 */
void DialogFind::setKeepDialog(bool keep) {
    ui.checkKeep->setChecked(keep);
}
