
#include <QMessageBox>
#include "DialogSmartIndentEdit.h"
#include "IndentStyle.h"

#include "smartIndent.h"
#include "MotifHelper.h"
#include "macro.h"
#include "preferences.h"
#include "Document.h"


//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndentEdit::DialogSmartIndentEdit(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	
	ui.editCode->setPlainText(QLatin1String(CommonMacros));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndentEdit::~DialogSmartIndentEdit() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndentEdit::on_buttonOK_clicked() {
	// change the macro 
	if(updateSmartIndentCommonData()) {
		accept();
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndentEdit::on_buttonApply_clicked() {
	// change the macro 
	updateSmartIndentCommonData();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndentEdit::on_buttonCheck_clicked() {
	if (checkSmartIndentCommonDialogData()) {
		QMessageBox::information(this, tr("Macro compiled"), tr("Macros compiled without error"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndentEdit::on_buttonRestore_clicked() {
	int resp = QMessageBox::question(this, tr("Discard Changes"), tr("Are you sure you want to discard all\nchanges to common smart indent macros"), QMessageBox::Discard | QMessageBox::Cancel);
	if(resp == QMessageBox::Cancel) {
		return;
	}

	// replace common macros with default 
	XtFree(CommonMacros);
	CommonMacros = XtNewStringEx(DefaultCommonMacros);

	// Update the dialog 
	ui.editCode->setPlainText(QLatin1String(CommonMacros));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSmartIndentEdit::checkSmartIndentCommonDialogData() {

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
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
QString DialogSmartIndentEdit::ensureNewline(const QString &string) {

	if(string.isNull()) {
		return QString();
	}

	int length = string.size();
	if (length == 0 || string[length - 1] == QLatin1Char('\n')) {
		return string;
	}
	
	return string + QLatin1Char('\n');
}

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
bool DialogSmartIndentEdit::updateSmartIndentCommonData() {

	// Make sure the patterns are valid and compile 
	if (!checkSmartIndentCommonDialogData()) {
		return false;
	}


	QString code = ui.editCode->toPlainText();

	// Get the current data 
	CommonMacros = XtStringDup(ensureNewline(code).toLatin1().data());

	/* Re-execute initialization macros (macros require a window to function,
	   since user could theoretically execute an action routine, but it
	   probably won't be referenced in a smart indent initialization) */
	if (!ReadMacroString(WindowList, CommonMacros, "common macros")) {
		return false;
	}

	/* Find windows that are currently using smart indent and
	   re-initialize the smart indent macros (in case they have initialization
	   data which depends on common data) */
	for(Document *window: WindowList) {
		if (window->indentStyle_ == SMART_INDENT && window->languageMode_ != PLAIN_LANGUAGE_MODE) {
			EndSmartIndent(window);
			BeginSmartIndent(window, False);
		}
	}

	// Note that preferences have been changed 
	MarkPrefsChanged();

	return true;
}
