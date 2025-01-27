
#include "DialogFind.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Regex.h"
#include "Search.h"
#include "Util/regex.h"

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
DialogFind::DialogFind(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f)
	: Dialog(window, f), window_(window), document_(document) {
	ui.setupUi(this);
	connectSlots();

	ui.textFind->installEventFilter(this);

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});
}

/**
 * @brief DialogFind::connectSlots
 */
void DialogFind::connectSlots() {
	connect(ui.buttonFind, &QPushButton::clicked, this, &DialogFind::buttonFind_clicked);
	connect(ui.checkRegex, &QCheckBox::toggled, this, &DialogFind::checkRegex_toggled);
	connect(ui.checkCase, &QCheckBox::toggled, this, &DialogFind::checkCase_toggled);
	connect(ui.checkKeep, &QCheckBox::toggled, this, &DialogFind::checkKeep_toggled);
	connect(ui.textFind, &QLineEdit::textChanged, this, &DialogFind::textFind_textChanged);
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
 * @brief DialogFind::eventFilter
 * @param obj
 * @param ev
 * @return
 */
bool DialogFind::eventFilter(QObject *obj, QEvent *ev) {
	if (obj == ui.textFind && ev->type() == QEvent::KeyPress) {
		auto event = static_cast<QKeyEvent *>(ev);

		int index = window_->fHistIndex_;

		// only process up and down arrow keys
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			return false;
		}

		// increment or decrement the index depending on which arrow was pressed
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return
		if (index != 0 && Search::historyIndex(index) == -1) {
			QApplication::beep();
			return true;
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

		return true;
	}

	return false;
}

/**
 * @brief DialogFind::checkKeep_toggled
 * @param checked
 */
void DialogFind::checkKeep_toggled(bool checked) {
	if (checked && document_) {
		setWindowTitle(tr("Find (in %1)").arg(document_->filename()));
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
 * @brief DialogFind::textFind_textChanged
 * @param text
 */
void DialogFind::textFind_textChanged(const QString &text) {
	Q_UNUSED(text)
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
		initialText = document->getAnySelection();
	}

	// Update the field
	ui.textFind->setText(initialText);
}

/**
 * @brief DialogFind::buttonFind_clicked
 */
void DialogFind::buttonFind_clicked() {

	if (!document_) {
		return;
	}

	// fetch find string, direction and type from the dialog
	std::optional<Fields> fields = readFields();
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
std::optional<DialogFind::Fields> DialogFind::readFields() {

	Fields fields;

	// Get the search string, search type, and direction from the dialog
	const QString findText = ui.textFind->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
			fields.searchType = SearchType::Regex;
			regexDefault      = RE_DEFAULT_STANDARD;
		} else {
			fields.searchType = SearchType::RegexNoCase;
			regexDefault      = RE_DEFAULT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			auto compiledRE = make_regex(findText, regexDefault);
		} catch (const RegexError &e) {
			QMessageBox::warning(
				this,
				tr("Regex Error"),
				tr("Please re-specify the search string:\n%1").arg(QString::fromLatin1(e.what())));
			return {};
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
 * @brief DialogFind::checkRegex_toggled
 * @param checked
 */
void DialogFind::checkRegex_toggled(bool checked) {

	const bool searchRegex     = checked;
	const bool searchCaseSense = ui.checkCase->isChecked();

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
 * @brief DialogFind::checkCase_toggled
 * @param checked
 */
void DialogFind::checkCase_toggled(bool checked) {

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
	Q_ASSERT(document);
	document_ = document;

	if (keepDialog()) {
		setWindowTitle(tr("Find (in %1)").arg(document_->filename()));
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
