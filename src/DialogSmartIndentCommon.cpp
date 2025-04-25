
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "LanguageMode.h"
#include "Preferences.h"
#include "SmartIndent.h"
#include "Util/String.h"
#include "macro.h"

#include <QMessageBox>

/**
 * @brief Constructor for the DialogSmartIndentCommon class.
 *
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogSmartIndentCommon::DialogSmartIndentCommon(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	const int tabStop = Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	ui.editCode->setTabStopDistance(tabStop * Font::characterWidth(ui.editCode->fontMetrics(), QLatin1Char(' ')));
#else
	ui.editCode->setTabStopWidth(tabStop * Font::characterWidth(ui.editCode->fontMetrics(), QLatin1Char(' ')));
#endif

	ui.editCode->setPlainText(SmartIndent::CommonMacros);
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogSmartIndentCommon::connectSlots() {
	connect(ui.buttonOK, &QPushButton::clicked, this, &DialogSmartIndentCommon::buttonOK_clicked);
	connect(ui.buttonApply, &QPushButton::clicked, this, &DialogSmartIndentCommon::buttonApply_clicked);
	connect(ui.buttonCheck, &QPushButton::clicked, this, &DialogSmartIndentCommon::buttonCheck_clicked);
	connect(ui.buttonRestore, &QPushButton::clicked, this, &DialogSmartIndentCommon::buttonRestore_clicked);
}

/**
 * @brief Handles the "OK" button click event in the dialog.
 */
void DialogSmartIndentCommon::buttonOK_clicked() {
	// change the macro
	if (updateSmartIndentCommonData()) {
		accept();
	}
}

/**
 * @brief Handles the "Apply" button click event in the dialog.
 */
void DialogSmartIndentCommon::buttonApply_clicked() {
	// change the macro
	updateSmartIndentCommonData();
}

/**
 * @brief Checks the smart indent macros for errors and displays a message.
 */
void DialogSmartIndentCommon::buttonCheck_clicked() {
	if (checkSmartIndentCommonDialogData()) {
		QMessageBox::information(this, tr("Macro compiled"), tr("Macros compiled without error"));
	}
}

/**
 * @brief Restores the common smart indent macros to their default values.
 */
void DialogSmartIndentCommon::buttonRestore_clicked() {
	const int resp = QMessageBox::question(
		this,
		tr("Discard Changes"),
		tr("Are you sure you want to discard all changes to common smart indent macros"),
		QMessageBox::Discard | QMessageBox::Cancel);

	if (resp == QMessageBox::Cancel) {
		return;
	}

	// replace common macros with default
	SmartIndent::CommonMacros = SmartIndent::LoadDefaultCommonMacros();

	// Update the dialog
	ui.editCode->setPlainText(SmartIndent::CommonMacros);
}

/**
 * @brief Checks the data in the smart indent common dialog for validity.
 *
 * @return `true` if the data is valid, `false` otherwise.
 */
bool DialogSmartIndentCommon::checkSmartIndentCommonDialogData() {

	const QString code = ui.editCode->toPlainText();

	if (!code.isEmpty()) {
		const QString widgetText = ensure_newline(code);
		int stoppedAt;
		if (!CheckMacroString(this, widgetText, tr("macros"), &stoppedAt)) {
			QTextCursor cursor = ui.editCode->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editCode->setTextCursor(cursor);
			ui.editCode->setFocus();
			return false;
		}
	}
	return true;
}

/**
 * @brief Update the smart indent macros being edited in the dialog
 * with the information that the dialog is currently displaying, and
 * apply changes to any window which is currently using the macros.
 *
 * @return `true` if the update was successful, `false` otherwise.
 */
bool DialogSmartIndentCommon::updateSmartIndentCommonData() {

	// Make sure the patterns are valid and compile
	if (!checkSmartIndentCommonDialogData()) {
		return false;
	}

	const QString code = ui.editCode->toPlainText();

	// Get the current data
	SmartIndent::CommonMacros = ensure_newline(code);

	/* Re-execute initialization macros (macros require a window to function,
	   since user could theoretically execute an action routine, but it
	   probably won't be referenced in a smart indent initialization) */
	std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
	if (!documents.empty()) {
		if (!documents[0]->readMacroString(SmartIndent::CommonMacros, tr("common macros"))) {
			return false;
		}
	}

	/* Find windows that are currently using smart indent and
	   re-initialize the smart indent macros (in case they have initialization
	   data which depends on common data) */
	for (DocumentWidget *document : documents) {
		if (document->autoIndentStyle() == IndentStyle::Smart && document->getLanguageMode() != PLAIN_LANGUAGE_MODE) {
			document->endSmartIndent();
			document->beginSmartIndent(Verbosity::Silent);
		}
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();

	return true;
}
