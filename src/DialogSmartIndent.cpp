
#include "DialogSmartIndent.h"
#include "DialogLanguageModes.h"
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "Help.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "SmartIndent.h"
#include "SmartIndentEntry.h"
#include "Util/String.h"
#include "Util/algorithm.h"
#include "macro.h"
#include "parse.h"

#include <QMessageBox>

/**
 * @brief Constructor for the DialogSmartIndent class.
 *
 * @param document The DocumentWidget associated with this dialog, used to determine the language mode.
 * @param parent The parent widget for this dialog, typically the main window.
 * @param f Window flags for the dialog, allowing customization of its appearance and behavior.
 */
DialogSmartIndent::DialogSmartIndent(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	const int tabStop = Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	ui.editInit->setTabStopDistance(tabStop * Font::characterWidth(ui.editInit->fontMetrics(), QLatin1Char(' ')));
	ui.editNewline->setTabStopDistance(tabStop * Font::characterWidth(ui.editNewline->fontMetrics(), QLatin1Char(' ')));
	ui.editModMacro->setTabStopDistance(tabStop * Font::characterWidth(ui.editModMacro->fontMetrics(), QLatin1Char(' ')));
#else
	ui.editInit->setTabStopWidth(tabStop * Font::characterWidth(ui.editInit->fontMetrics(), QLatin1Char(' ')));
	ui.editNewline->setTabStopWidth(tabStop * Font::characterWidth(ui.editNewline->fontMetrics(), QLatin1Char(' ')));
	ui.editModMacro->setTabStopWidth(tabStop * Font::characterWidth(ui.editModMacro->fontMetrics(), QLatin1Char(' ')));
#endif

	const QString languageMode = Preferences::LanguageModeName((document->getLanguageMode() == PLAIN_LANGUAGE_MODE) ? 0 : document->getLanguageMode());

	updateLanguageModes();
	setLanguageMode(languageMode);

	// Fill in the dialog information for the selected language mode
	setSmartIndentDialogData(SmartIndent::findIndentSpec(languageMode_));
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogSmartIndent::connectSlots() {
	connect(ui.buttonCommon, &QPushButton::clicked, this, &DialogSmartIndent::buttonCommon_clicked);
	connect(ui.buttonLanguageMode, &QPushButton::clicked, this, &DialogSmartIndent::buttonLanguageMode_clicked);
	connect(ui.buttonOK, &QPushButton::clicked, this, &DialogSmartIndent::buttonOK_clicked);
	connect(ui.buttonApply, &QPushButton::clicked, this, &DialogSmartIndent::buttonApply_clicked);
	connect(ui.buttonCheck, &QPushButton::clicked, this, &DialogSmartIndent::buttonCheck_clicked);
	connect(ui.buttonDelete, &QPushButton::clicked, this, &DialogSmartIndent::buttonDelete_clicked);
	connect(ui.buttonRestore, &QPushButton::clicked, this, &DialogSmartIndent::buttonRestore_clicked);
	connect(ui.buttonHelp, &QPushButton::clicked, this, &DialogSmartIndent::buttonHelp_clicked);
}

/**
 * @brief Updates the list of available language modes in the combo box.
 */
void DialogSmartIndent::updateLanguageModes() {

	const QString languageMode = languageMode_;
	ui.comboLanguageMode->clear();
	for (const LanguageMode &lang : Preferences::LanguageModes) {
		ui.comboLanguageMode->addItem(lang.name);
	}

	setLanguageMode(languageMode);
}

/**
 * @brief Sets the current language mode for the dialog.
 *
 * @param s The name of the language mode to set.
 */
void DialogSmartIndent::setLanguageMode(const QString &s) {
	languageMode_   = s;
	const int index = ui.comboLanguageMode->findText(languageMode_, Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if (index != -1) {
		ui.comboLanguageMode->setCurrentIndex(index);
	}
}

/**
 * @brief Handles the change of the language mode in the combo box.
 *
 * @param text The text of the selected language mode in the combo box.
 */
void DialogSmartIndent::on_comboLanguageMode_currentIndexChanged(const QString &text) {
	languageMode_ = text;
	setSmartIndentDialogData(SmartIndent::findIndentSpec(text));
}

/**
 * @brief Opens the common smart indent dialog for editing shared macros.
 */
void DialogSmartIndent::buttonCommon_clicked() {
	auto dialog = std::make_unique<DialogSmartIndentCommon>(this);
	dialog->exec();
}

/**
 * @brief Opens the dialog for managing language modes.
 */
void DialogSmartIndent::buttonLanguageMode_clicked() {
	if (!dialogLanguageModes_) {
		dialogLanguageModes_ = new DialogLanguageModes(nullptr, this);
	}

	dialogLanguageModes_->show();
}

/**
 * @brief Handles the "OK" button click event.
 */
void DialogSmartIndent::buttonOK_clicked() {
	// change the macro
	if (!updateSmartIndentData()) {
		return;
	}

	accept();
}

/**
 * @brief Handles the "Apply" button click event.
 */
void DialogSmartIndent::buttonApply_clicked() {
	updateSmartIndentData();
}

/**
 * @brief Checks the smart indent macros for errors and displays a message.
 */
void DialogSmartIndent::buttonCheck_clicked() {
	if (checkSmartIndentDialogData()) {
		QMessageBox::information(this,
								 tr("Macro compiled"),
								 tr("Macros compiled without error"));
	}
}

/**
 * @brief Deletes the smart indent macros for the current language mode after user confirmation.
 */
void DialogSmartIndent::buttonDelete_clicked() {

	const int resp = QMessageBox::question(
		this,
		tr("Delete Macros"),
		tr("Are you sure you want to delete smart indent macros for language mode %1?").arg(languageMode_),
		QMessageBox::Yes | QMessageBox::Cancel);
	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, delete it from the list
	auto it = std::find_if(SmartIndent::SmartIndentSpecs.begin(), SmartIndent::SmartIndentSpecs.end(), [this](const SmartIndentEntry &entry) {
		return entry.language == languageMode_;
	});

	if (it != SmartIndent::SmartIndentSpecs.end()) {
		SmartIndent::SmartIndentSpecs.erase(it);
	}

	// Clear out the dialog
	setSmartIndentDialogData(nullptr);
}

/**
 * @brief Restores the default smart indent macros for the current language mode after user confirmation.
 */
void DialogSmartIndent::buttonRestore_clicked() {

	const SmartIndentEntry *spec = SmartIndent::findDefaultIndentSpec(languageMode_);
	if (!spec) {
		QMessageBox::warning(
			this,
			tr("Smart Indent"),
			tr("There are no default indent macros for language mode %1").arg(languageMode_));
		return;
	}

	const int resp = QMessageBox::question(
		this,
		tr("Discard Changes"),
		tr("Are you sure you want to discard all changes to smart indent macros for language mode %1?").arg(languageMode_),
		QMessageBox::Discard | QMessageBox::Cancel);

	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the indent macros exist, replace them, if not, add a new one
	Upsert(SmartIndent::SmartIndentSpecs, *spec, [spec](const SmartIndentEntry &entry) {
		return entry.language == spec->language;
	});

	// Update the dialog
	setSmartIndentDialogData(spec);
}

/**
 * @brief Displays the help topic for smart indent macros.
 */
void DialogSmartIndent::buttonHelp_clicked() {
	Help::displayTopic(Help::Topic::SmartIndent);
}

/**
 * @brief Sets the smart indent dialog data based on the provided SmartIndentEntry.
 *
 * @param is The SmartIndentEntry containing the smart indent macros.
 *           If null, the dialog fields will be cleared.
 */
void DialogSmartIndent::setSmartIndentDialogData(const SmartIndentEntry *is) {

	if (!is) {
		ui.editInit->setPlainText(QString());
		ui.editNewline->setPlainText(QString());
		ui.editModMacro->setPlainText(QString());
	} else {
		ui.editInit->setPlainText(is->initMacro);
		ui.editNewline->setPlainText(is->newlineMacro);
		ui.editModMacro->setPlainText(is->modMacro);
	}
}

/**
 * @brief Update the smart indent macros being edited in the dialog
 * with the information that the dialog is currently displaying, and
 * apply changes to any window which is currently using the macros.
 */
bool DialogSmartIndent::updateSmartIndentData() {

	// Make sure the patterns are valid and compile
	if (!checkSmartIndentDialogData()) {
		return false;
	}

	// Get the current data
	const SmartIndentEntry newMacros = getSmartIndentDialogData();

	/* If it's a new language, add it at the end, otherwise replace the
	   existing macros */
	Upsert(SmartIndent::SmartIndentSpecs, newMacros, [this](const SmartIndentEntry &entry) {
		return entry.language == languageMode_;
	});

	/* Find windows that are currently using this indent specification and
	   re-do the smart indent macros */
	for (DocumentWidget *document : DocumentWidget::allDocuments()) {

		const QString lmName = Preferences::LanguageModeName(document->getLanguageMode());
		if (!lmName.isNull()) {
			if (lmName == newMacros.language) {

				if (MainWindow *window = MainWindow::fromDocument(document)) {
					window->ui.action_Indent_Smart->setEnabled(true);
				}

				if (document->autoIndentStyle() == IndentStyle::Smart && document->getLanguageMode() != PLAIN_LANGUAGE_MODE) {
					document->endSmartIndent();
					document->beginSmartIndent(Verbosity::Silent);
				}
			}
		}
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief Checks the data in the smart indent dialog for validity.
 *
 * @return `true` if the data is valid, `false` otherwise.
 */
bool DialogSmartIndent::checkSmartIndentDialogData() {

	// NOTE(eteran): this throws up an error message to the user even if all
	// fields are blank (like nedit does). This is not very user friendly
	// since many language modes by default have no smart indent macros.
	// This means, that by default, simply opening up this dialog and clicking
	// "OK" will result in an error :-/. We should likely be tolerant when
	// the dialog is entirely blank.

	// Check the initialization macro
	const QString initText = ui.editInit->toPlainText();
	if (!initText.isEmpty()) {
		const QString widgetText = ensure_newline(initText);
		int stoppedAt            = 0;
		if (!CheckMacroString(this, widgetText, tr("initialization macro"), &stoppedAt)) {
			QTextCursor cursor = ui.editInit->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editInit->setTextCursor(cursor);
			ui.editInit->setFocus();
			return false;
		}
	}

	// Test compile the newline macro
	const QString newlineText = ui.editNewline->toPlainText();
	if (newlineText.isEmpty()) {
		QMessageBox::warning(this, tr("Smart Indent"), tr("Newline macro required"));
		return false;
	}

	{
		const QString widgetText = ensure_newline(newlineText);
		QString errMsg;
		int stoppedAt = 0;

		if (!isMacroValid(widgetText, &errMsg, &stoppedAt)) {
			Preferences::reportError(this, widgetText, stoppedAt, tr("newline macro"), errMsg);
			QTextCursor cursor = ui.editNewline->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editNewline->setTextCursor(cursor);
			ui.editNewline->setFocus();
			return false;
		}
	}

	// Test compile the modify macro
	const QString modMacroText = ui.editModMacro->toPlainText();
	if (!modMacroText.isEmpty()) {
		const QString widgetText = ensure_newline(modMacroText);
		QString errMsg;
		int stoppedAt = 0;

		if (!isMacroValid(widgetText, &errMsg, &stoppedAt)) {
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
 * @brief Retrieves the smart indent dialog data from the dialog fields.
 *
 * @return A SmartIndentEntry containing the language mode and macros.
 */
SmartIndentEntry DialogSmartIndent::getSmartIndentDialogData() const {

	SmartIndentEntry is;
	is.language     = languageMode_;
	is.initMacro    = ui.editInit->toPlainText().isEmpty() ? QString() : ensure_newline(ui.editInit->toPlainText());
	is.newlineMacro = ui.editNewline->toPlainText().isEmpty() ? QString() : ensure_newline(ui.editNewline->toPlainText());
	is.modMacro     = ui.editModMacro->toPlainText().isEmpty() ? QString() : ensure_newline(ui.editModMacro->toPlainText());
	return is;
}

/**
 * @brief Returns the current language mode set in the dialog.
 *
 * @return The name of the language mode as a QString.
 */
QString DialogSmartIndent::languageMode() const {
	return languageMode_;
}
