
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
 * @brief DialogSmartIndent::DialogSmartIndent
 * @param document
 * @param parent
 * @param f
 */
DialogSmartIndent::DialogSmartIndent(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	const int tabStop = Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE); // 4 characters
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	ui.editInit->setTabStopDistance(tabStop * Font::characterWidth(ui.editInit->fontMetrics(), QLatin1Char(' ')));
	ui.editNewline->setTabStopDistance(tabStop * Font::characterWidth(ui.editNewline->fontMetrics(), QLatin1Char(' ')));
	ui.editModMacro->setTabStopDistance(tabStop * Font::characterWidth(ui.editModMacro->fontMetrics(), QLatin1Char(' ')));
#else
	ui.editInit->setTabStopWidth(tabStop * Font::characterWidth(ui.editInit->fontMetrics(), QLatin1Char(' ')));
	ui.editNewline->setTabStopWidth(tabStop * Font::characterWidth(ui.editNewline->fontMetrics(), QLatin1Char(' ')));
	ui.editModMacro->setTabStopWidth(tabStop * Font::characterWidth(ui.editModMacro->fontMetrics(), QLatin1Char(' ')));
#endif

	QString languageMode = Preferences::LanguageModeName((document->getLanguageMode() == PLAIN_LANGUAGE_MODE) ? 0 : document->getLanguageMode());

	updateLanguageModes();
	setLanguageMode(languageMode);

	// Fill in the dialog information for the selected language mode
	setSmartIndentDialogData(SmartIndent::findIndentSpec(languageMode_));
}

/**
 * @brief DialogSmartIndent::connectSlots
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
 * @brief DialogSmartIndent::updateLanguageModes
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
 * @brief DialogSmartIndent::setLanguageMode
 * @param s
 */
void DialogSmartIndent::setLanguageMode(const QString &s) {
	languageMode_ = s;
	int index     = ui.comboLanguageMode->findText(languageMode_, Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if (index != -1) {
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
 * @brief DialogSmartIndent::buttonCommon_clicked
 */
void DialogSmartIndent::buttonCommon_clicked() {
	auto dialog = std::make_unique<DialogSmartIndentCommon>(this);
	dialog->exec();
}

/**
 * @brief DialogSmartIndent::buttonLanguageMode_clicked
 */
void DialogSmartIndent::buttonLanguageMode_clicked() {
	if (!dialogLanguageModes_) {
		dialogLanguageModes_ = new DialogLanguageModes(nullptr, this);
	}

	dialogLanguageModes_->show();
}

/**
 * @brief DialogSmartIndent::buttonOK_clicked
 */
void DialogSmartIndent::buttonOK_clicked() {
	// change the macro
	if (!updateSmartIndentData()) {
		return;
	}

	accept();
}

/**
 * @brief DialogSmartIndent::buttonApply_clicked
 */
void DialogSmartIndent::buttonApply_clicked() {
	updateSmartIndentData();
}

/**
 * @brief DialogSmartIndent::buttonCheck_clicked
 */
void DialogSmartIndent::buttonCheck_clicked() {
	if (checkSmartIndentDialogData()) {
		QMessageBox::information(this,
								 tr("Macro compiled"),
								 tr("Macros compiled without error"));
	}
}

/**
 * @brief DialogSmartIndent::buttonDelete_clicked
 */
void DialogSmartIndent::buttonDelete_clicked() {

	int resp = QMessageBox::question(
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
 * @brief DialogSmartIndent::buttonRestore_clicked
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

	int resp = QMessageBox::question(
		this,
		tr("Discard Changes"),
		tr("Are you sure you want to discard all changes to smart indent macros for language mode %1?").arg(languageMode_),
		QMessageBox::Discard | QMessageBox::Cancel);

	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the indent macros exist, replace them, if not, add a new one
	insert_or_replace(SmartIndent::SmartIndentSpecs, *spec, [spec](const SmartIndentEntry &entry) {
		return entry.language == spec->language;
	});

	// Update the dialog
	setSmartIndentDialogData(spec);
}

/**
 * @brief DialogSmartIndent::buttonHelp_clicked
 */
void DialogSmartIndent::buttonHelp_clicked() {
	Help::displayTopic(Help::Topic::SmartIndent);
}

/**
 * @brief DialogSmartIndent::setSmartIndentDialogData
 * @param is
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

	/* If it's a new language, add it at the end, otherwise replace the
	   existing macros */
	insert_or_replace(SmartIndent::SmartIndentSpecs, newMacros, [this](const SmartIndentEntry &entry) {
		return entry.language == languageMode_;
	});

	/* Find windows that are currently using this indent specification and
	   re-do the smart indent macros */
	for (DocumentWidget *document : DocumentWidget::allDocuments()) {

		QString lmName = Preferences::LanguageModeName(document->getLanguageMode());
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
		QString widgetText = ensure_newline(initText);
		int stoppedAt      = 0;
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
		QString widgetText = ensure_newline(newlineText);
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
	QString modMacroText = ui.editModMacro->toPlainText();
	if (!modMacroText.isEmpty()) {
		QString widgetText = ensure_newline(modMacroText);
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
 * @brief DialogSmartIndent::getSmartIndentDialogData
 * @return
 */
SmartIndentEntry DialogSmartIndent::getSmartIndentDialogData() const {

	SmartIndentEntry is;
	is.language     = languageMode_;
	is.initMacro    = ui.editInit->toPlainText().isEmpty() ? QString() : ensure_newline(ui.editInit->toPlainText());
	is.newlineMacro = ui.editNewline->toPlainText().isEmpty() ? QString() : ensure_newline(ui.editNewline->toPlainText());
	is.modMacro     = ui.editModMacro->toPlainText().isEmpty() ? QString() : ensure_newline(ui.editModMacro->toPlainText());
	return is;
}

QString DialogSmartIndent::languageMode() const {
	return languageMode_;
}
