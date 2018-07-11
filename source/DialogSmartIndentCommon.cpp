
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "LanguageMode.h"
#include "macro.h"
#include "preferences.h"
#include "SmartIndent.h"

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

    int length = string.size();
    if (length == 0 || string[length - 1] == QLatin1Char('\n')) {
        return string;
    }

    return string + QLatin1Char('\n');
}

}


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
		if (!CheckMacroStringEx(this, widgetText, tr("macros"), &stoppedAt)) {
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
        if (!documents[0]->ReadMacroStringEx(SmartIndent::CommonMacros, tr("common macros"))) {
			return false;
		}
	}

	/* Find windows that are currently using smart indent and
	   re-initialize the smart indent macros (in case they have initialization
	   data which depends on common data) */
    for(DocumentWidget *document : documents) {
        if (document->indentStyle_ == IndentStyle::Smart && document->GetLanguageMode() != PLAIN_LANGUAGE_MODE) {
            document->EndSmartIndent();
            document->BeginSmartIndentEx(/*warn=*/false);
		}
	}

	// Note that preferences have been changed 
    Preferences::MarkPrefsChanged();

	return true;
}
