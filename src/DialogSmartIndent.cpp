
#include "DialogSmartIndent.h"
#include "DialogLanguageModes.h"
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "Help.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "SmartIndent.h"
#include "SmartIndentEntry.h"
#include "interpret.h"
#include "macro.h"
#include "parse.h"

#include <QMessageBox>

namespace {

/*
** If "string" is not terminated with a newline character, return a
** string which does end in a newline.
**
** (The macro language requires newline terminators for statements, but the
** text widget doesn't force it like the NEdit text buffer does, so this might
** avoid some confusion.)
*/
QString ensureNewline(const QString &string) {

	if(string.isNull()) {
		return QString();
	}

	if(string.endsWith(QLatin1Char('\n'))) {
		return string;
	}

	return string + QLatin1Char('\n');
}

}


/**
 * @brief DialogSmartIndent::DialogSmartIndent
 * @param document
 * @param parent
 * @param f
 */
DialogSmartIndent::DialogSmartIndent(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);

	QString languageMode = Preferences::LanguageModeName((document->GetLanguageMode() == PLAIN_LANGUAGE_MODE) ? 0 : document->GetLanguageMode());

	updateLanguageModes();
	setLanguageMode(languageMode);

	// Fill in the dialog information for the selected language mode
	setSmartIndentDialogData(SmartIndent::findIndentSpec(languageMode_));
}

/**
 * @brief DialogSmartIndent::updateLanguageModes
 */
void DialogSmartIndent::updateLanguageModes() {

	const QString languageMode = languageMode_;
	ui.comboLanguageMode->clear();
	for(const LanguageMode &lang : Preferences::LanguageModes) {
		ui.comboLanguageMode->addItem(lang.name);
	}

	setLanguageMode(languageMode);
}

/**
 * @brief DialogSmartIndent::setLanguageMode
 * @param s
 */
void DialogSmartIndent::setLanguageMode(const QString &s) {
	languageMode_ = s;
	int index = ui.comboLanguageMode->findText(languageMode_, Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if(index != -1) {
		ui.comboLanguageMode->setCurrentIndex(index);
	}
}

/**
 * @brief DialogSmartIndent::on_comboLanguageMode_currentIndexChanged
 * @param text
 */
void DialogSmartIndent::on_comboLanguageMode_currentIndexChanged(const QString &text) {
	languageMode_ = text;
	setSmartIndentDialogData(SmartIndent::findIndentSpec(text));
}

/**
 * @brief DialogSmartIndent::on_buttonCommon_clicked
 */
void DialogSmartIndent::on_buttonCommon_clicked() {
	auto dialog = std::make_unique<DialogSmartIndentCommon>(this);
	dialog->exec();
}

/**
 * @brief DialogSmartIndent::on_buttonLanguageMode_clicked
 */
void DialogSmartIndent::on_buttonLanguageMode_clicked() {
	auto dialog = std::make_unique<DialogLanguageModes>(nullptr, this);
	dialog->exec();
}

/**
 * @brief DialogSmartIndent::on_buttonOK_clicked
 */
void DialogSmartIndent::on_buttonOK_clicked() {
	// change the macro
	if (!updateSmartIndentData()) {
		return;
	}

	accept();
}

/**
 * @brief DialogSmartIndent::on_buttonApply_clicked
 */
void DialogSmartIndent::on_buttonApply_clicked() {
	updateSmartIndentData();
}

/**
 * @brief DialogSmartIndent::on_buttonCheck_clicked
 */
void DialogSmartIndent::on_buttonCheck_clicked() {
	if (checkSmartIndentDialogData()) {
		QMessageBox::information(this,
								 tr("Macro compiled"),
								 tr("Macros compiled without error"));
	}
}

/**
 * @brief DialogSmartIndent::on_buttonDelete_clicked
 */
void DialogSmartIndent::on_buttonDelete_clicked() {

	int resp = QMessageBox::question(
				this,
				tr("Delete Macros"),
				tr("Are you sure you want to delete smart indent macros for language mode %1?").arg(languageMode_),
				QMessageBox::Yes | QMessageBox::Cancel);
	if(resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, delete it from the list
	auto it = std::find_if(SmartIndent::SmartIndentSpecs.begin(), SmartIndent::SmartIndentSpecs.end(), [this](const SmartIndentEntry &entry) {
		return entry.languageMode == languageMode_;
	});

	if(it != SmartIndent::SmartIndentSpecs.end()) {
		SmartIndent::SmartIndentSpecs.erase(it);
	}

	// Clear out the dialog
	setSmartIndentDialogData(nullptr);
}

/**
 * @brief DialogSmartIndent::on_buttonRestore_clicked
 */
void DialogSmartIndent::on_buttonRestore_clicked() {

	const SmartIndentEntry *defaultIS = SmartIndent::findDefaultIndentSpec(languageMode_);
	if(!defaultIS) {
		QMessageBox::warning(
					this,
					tr("Smart Indent"),
					tr("There are no default indent macros for language mode %1").arg(languageMode_));
		return;
	}

	int resp = QMessageBox::question(
				this,
				tr("Discard Changes"),
				tr("Are you sure you want to discard all changes to smart indent macros for language mode %1?").arg(languageMode_),
				QMessageBox::Discard | QMessageBox::Cancel);

	if(resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the indent macros exist, replace them, if not, add a new one
	size_t i;
	for (i = 0; i < SmartIndent::SmartIndentSpecs.size(); i++) {
		if (languageMode_ == SmartIndent::SmartIndentSpecs[i].languageMode) {
			break;
		}
	}

	if (i < SmartIndent::SmartIndentSpecs.size()) {
		SmartIndent::SmartIndentSpecs[i] = *defaultIS;
	} else {
		SmartIndent::SmartIndentSpecs.push_back(*defaultIS);
	}

	// Update the dialog
	setSmartIndentDialogData(defaultIS);
}

/**
 * @brief DialogSmartIndent::on_buttonHelp_clicked
 */
void DialogSmartIndent::on_buttonHelp_clicked() {
	Help::displayTopic(this, Help::Topic::SmartIndent);
}

/**
 * @brief DialogSmartIndent::setSmartIndentDialogData
 * @param is
 */
void DialogSmartIndent::setSmartIndentDialogData(const SmartIndentEntry *is) {

	if(!is) {
		ui.editInit->setPlainText(QString());
		ui.editNewline->setPlainText(QString());
		ui.editModMacro->setPlainText(QString());
	} else {
		ui.editInit->setPlainText(is->initMacro);
		ui.editNewline->setPlainText(is->newlineMacro);
		ui.editModMacro->setPlainText(is->modMacro);
	}
}

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
bool DialogSmartIndent::updateSmartIndentData() {

	// Make sure the patterns are valid and compile
	if (!checkSmartIndentDialogData()) {
		return false;
	}

	// Get the current data
	SmartIndentEntry newMacros = getSmartIndentDialogData();

	// Find the original macros
	auto it = std::find_if(SmartIndent::SmartIndentSpecs.begin(), SmartIndent::SmartIndentSpecs.end(), [this](const SmartIndentEntry &entry) {
		return entry.languageMode == languageMode_;
	});

	/* If it's a new language, add it at the end, otherwise replace the
	   existing macros */
	if (it == SmartIndent::SmartIndentSpecs.end()) {
		SmartIndent::SmartIndentSpecs.push_back(newMacros);
	} else {
		*it = newMacros;
	}

	/* Find windows that are currently using this indent specification and
	   re-do the smart indent macros */
	for(DocumentWidget *document : DocumentWidget::allDocuments()) {

		QString lmName = Preferences::LanguageModeName(document->GetLanguageMode());
		if(!lmName.isNull()) {
			if (lmName == newMacros.languageMode) {

				if(auto window = MainWindow::fromDocument(document)) {
					window->ui.action_Indent_Smart->setEnabled(true);
				}

				if (document->autoIndentStyle() == IndentStyle::Smart && document->GetLanguageMode() != PLAIN_LANGUAGE_MODE) {
					document->endSmartIndent();
					document->beginSmartIndent(/*warn=*/false);
				}
			}
		}
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief DialogSmartIndent::checkSmartIndentDialogData
 * @return
 */
bool DialogSmartIndent::checkSmartIndentDialogData() {

	// NOTE(eteran): this throws up an error message to the user even if all
	// fields are blank (like nedit does). This is not very user friendly
	// since many language modes by default have no smart indent macros.
	// This means, that by default, simply opening up this dialog and clicking
	// "OK" will result in an error :-/. We should likely be tolerant when
	// the dialog is entirely blank.

	// Check the initialization macro
	QString initText = ui.editInit->toPlainText();
	if (!initText.isEmpty()) {
		QString widgetText = ensureNewline(initText);
		int stoppedAt = 0;
		if (!CheckMacroString(this, widgetText, tr("initialization macro"), &stoppedAt)) {
			QTextCursor cursor = ui.editInit->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editInit->setTextCursor(cursor);
			ui.editInit->setFocus();
			return false;
		}
	}

	// Test compile the newline macro
	QString newlineText = ui.editNewline->toPlainText();
	if (newlineText.isEmpty()) {
		QMessageBox::warning(this, tr("Smart Indent"), tr("Newline macro required"));
		return false;
	}

	{
		QString widgetText = ensureNewline(newlineText);
		QString errMsg;
		int stoppedAt = 0;

		if(!isMacroValid(widgetText, &errMsg, &stoppedAt)) {
			Preferences::reportError(this, widgetText, stoppedAt, tr("newline macro"), errMsg);
			QTextCursor cursor = ui.editNewline->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editNewline->setTextCursor(cursor);
			ui.editNewline->setFocus();
			return false;
		}
	}


	// Test compile the modify macro
	QString modMacroText = ui.editModMacro->toPlainText();
	if (!modMacroText.isEmpty()) {
		QString widgetText = ensureNewline(modMacroText);
		QString errMsg;
		int stoppedAt = 0;

		if(!isMacroValid(widgetText, &errMsg, &stoppedAt)) {
			Preferences::reportError(this, widgetText, stoppedAt, tr("modify macro"), errMsg);
			QTextCursor cursor = ui.editModMacro->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editModMacro->setTextCursor(cursor);
			ui.editModMacro->setFocus();
			return false;
		}
	}

	return true;
}

/**
 * @brief DialogSmartIndent::getSmartIndentDialogData
 * @return
 */
SmartIndentEntry DialogSmartIndent::getSmartIndentDialogData() const {

	SmartIndentEntry is;
	is.languageMode = languageMode_;
	is.initMacro    = ui.editInit->toPlainText().isEmpty()     ? QString() : ensureNewline(ui.editInit->toPlainText());
	is.newlineMacro = ui.editNewline->toPlainText().isEmpty()  ? QString() : ensureNewline(ui.editNewline->toPlainText());
	is.modMacro     = ui.editModMacro->toPlainText().isEmpty() ? QString() : ensureNewline(ui.editModMacro->toPlainText());
	return is;
}


QString DialogSmartIndent::languageMode() const {
	return languageMode_;
}
