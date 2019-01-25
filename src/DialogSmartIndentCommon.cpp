
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "LanguageMode.h"
#include "Preferences.h"
#include "SmartIndent.h"
#include "macro.h"
#include "Util/String.h"

#include <QMessageBox>

/**
 * @brief DialogSmartIndentCommon::DialogSmartIndentCommon
 * @param parent
 * @param f
 */
DialogSmartIndentCommon::DialogSmartIndentCommon(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);

	ui.editCode->setPlainText(SmartIndent::CommonMacros);
}

/**
 * @brief DialogSmartIndentCommon::on_buttonOK_clicked
 */
void DialogSmartIndentCommon::on_buttonOK_clicked() {
	// change the macro
	if(updateSmartIndentCommonData()) {
		accept();
	}
}

/**
 * @brief DialogSmartIndentCommon::on_buttonApply_clicked
 */
void DialogSmartIndentCommon::on_buttonApply_clicked() {
	// change the macro
	updateSmartIndentCommonData();
}

/**
 * @brief DialogSmartIndentCommon::on_buttonCheck_clicked
 */
void DialogSmartIndentCommon::on_buttonCheck_clicked() {
	if (checkSmartIndentCommonDialogData()) {
		QMessageBox::information(this, tr("Macro compiled"), tr("Macros compiled without error"));
	}
}

/**
 * @brief DialogSmartIndentCommon::on_buttonRestore_clicked
 */
void DialogSmartIndentCommon::on_buttonRestore_clicked() {
	int resp = QMessageBox::question(
				this,
				tr("Discard Changes"),
				tr("Are you sure you want to discard all changes to common smart indent macros"),
				QMessageBox::Discard | QMessageBox::Cancel);

	if(resp == QMessageBox::Cancel) {
		return;
	}

	// replace common macros with default
	SmartIndent::CommonMacros = QString::fromLatin1(SmartIndent::defaultCommonMacros());

	// Update the dialog
	ui.editCode->setPlainText(SmartIndent::CommonMacros);
}

/**
 * @brief DialogSmartIndentCommon::checkSmartIndentCommonDialogData
 * @return
 */
bool DialogSmartIndentCommon::checkSmartIndentCommonDialogData() {

	QString code = ui.editCode->toPlainText();

	if(!code.isEmpty()) {
		QString widgetText = ensureNewline(code);
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

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
bool DialogSmartIndentCommon::updateSmartIndentCommonData() {

	// Make sure the patterns are valid and compile
	if (!checkSmartIndentCommonDialogData()) {
		return false;
	}

	QString code = ui.editCode->toPlainText();

	// Get the current data
	SmartIndent::CommonMacros = ensureNewline(code);

	/* Re-execute initialization macros (macros require a window to function,
	   since user could theoretically execute an action routine, but it
	   probably won't be referenced in a smart indent initialization) */
	std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
	if(!documents.empty()) {
		if (!documents[0]->ReadMacroString(SmartIndent::CommonMacros, tr("common macros"))) {
			return false;
		}
	}

	/* Find windows that are currently using smart indent and
	   re-initialize the smart indent macros (in case they have initialization
	   data which depends on common data) */
	for(DocumentWidget *document : documents) {
		if (document->autoIndentStyle() == IndentStyle::Smart && document->GetLanguageMode() != PLAIN_LANGUAGE_MODE) {
			document->endSmartIndent();
			document->beginSmartIndent(/*warn=*/false);
		}
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();

	return true;
}
