
#include "DialogSmartIndent.h"
#include "DialogSmartIndentCommon.h"
#include "DialogLanguageModes.h"
#include "SmartIndent.h"
#include "LanguageMode.h"

#include "smartIndent.h"
#include "Document.h"
#include "preferences.h"
#include "MotifHelper.h"


//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndent::DialogSmartIndent(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
		
	QString languageMode = QLatin1String(LanguageModeName(window->languageMode_ == PLAIN_LANGUAGE_MODE ? 0 : window->languageMode_));

	updateLanguageModes();
	setLanguageMode(languageMode);

	// Fill in the dialog information for the selected language mode 
	setSmartIndentDialogData(findIndentSpec(languageMode_.toLatin1().data()));
	

}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndent::~DialogSmartIndent() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::updateLanguageModes() {

	const QString languageMode = languageMode_;
	ui.comboLanguageMode->clear();
	for (int i = 0; i < NLanguageModes; i++) {
		ui.comboLanguageMode->addItem(QLatin1String(LanguageModes[i]->name));
	}
	
	setLanguageMode(languageMode);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::setLanguageMode(const QString &s) {
	languageMode_ = s;
	int index = ui.comboLanguageMode->findText(languageMode_, Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if(index != -1) {
		ui.comboLanguageMode->setCurrentIndex(index);
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_comboLanguageMode_currentIndexChanged(const QString &text) {
	languageMode_ = text;
	setSmartIndentDialogData(findIndentSpec(text.toLatin1().data()));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonCommon_clicked() {
	auto dialog = new DialogSmartIndentCommon(this);
	dialog->exec();
	delete dialog;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonLanguageMode_clicked() {
	auto dialog = new DialogLanguageModes(this);
	dialog->exec();
	delete dialog;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonOK_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonApply_clicked() {
	updateSmartIndentData();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonCheck_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonDelete_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonRestore_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonHelp_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::setSmartIndentDialogData(SmartIndent *is) {

	if(!is) {
		ui.editInit->setPlainText(QString());
		ui.editNewline->setPlainText(QString());
		ui.editTypeIn->setPlainText(QString());
	} else {
		if (!is->initMacro) {
			ui.editInit->setPlainText(QString());
		} else {
			ui.editInit->setPlainText(QLatin1String(is->initMacro));
		}
					
		ui.editNewline->setPlainText(QLatin1String(is->newlineMacro));

		if (!is->modMacro) {
			ui.editTypeIn->setPlainText(QString());
		} else {
			ui.editTypeIn->setPlainText(QLatin1String(is->modMacro));
		}
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
	SmartIndent *newMacros = getSmartIndentDialogData();
	(void)newMacros;
#if 0
	// Find the original macros
	int i;
	for (i = 0; i < NSmartIndentSpecs; i++) {
		if (!strcmp(SmartIndentDialog.langModeName, SmartIndentSpecs[i]->lmName)) {
			break;
		}
	}

	/* If it's a new language, add it at the end, otherwise free the
	   existing macros and replace it */
	if (i == NSmartIndentSpecs) {
		SmartIndentSpecs[NSmartIndentSpecs++] = newMacros;
	} else {
		freeIndentSpec(SmartIndentSpecs[i]);
		SmartIndentSpecs[i] = newMacros;
	}

	/* Find windows that are currently using this indent specification and
	   re-do the smart indent macros */
	for(Document *window: WindowList) {
		char *lmName = LanguageModeName(window->languageMode_);
		if (lmName != nullptr && !strcmp(lmName, newMacros->lmName)) {
			window->SetSensitive(window->smartIndentItem_, True);
			if (window->indentStyle_ == SMART_INDENT && window->languageMode_ != PLAIN_LANGUAGE_MODE) {
				EndSmartIndent(window);
				BeginSmartIndent(window, False);
			}
		}
	}
#endif
	// Note that preferences have been changed 
	MarkPrefsChanged();

	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSmartIndent::checkSmartIndentDialogData() {
#if 0
	char *widgetText;
	const char *errMsg;
	const char *stoppedAt;
	Program *prog;

	// Check the initialization macro 
	if (!TextWidgetIsBlank(SmartIndentDialog.initMacro)) {
		widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.initMacro));
		if (!CheckMacroString(SmartIndentDialog.shell, widgetText, "initialization macro", &stoppedAt)) {
			XmTextSetInsertionPosition(SmartIndentDialog.initMacro, stoppedAt - widgetText);
			XmProcessTraversal(SmartIndentDialog.initMacro, XmTRAVERSE_CURRENT);
			XtFree(widgetText);
			return False;
		}
		XtFree(widgetText);
	}

	// Test compile the newline macro 
	if (TextWidgetIsBlank(SmartIndentDialog.newlineMacro)) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Smart Indent"), QLatin1String("Newline macro required"));
		return False;
	}

	widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.newlineMacro));
	prog = ParseMacro(widgetText, &errMsg, &stoppedAt);
	if(!prog) {
		ParseError(SmartIndentDialog.shell, widgetText, stoppedAt, "newline macro", errMsg);
		XmTextSetInsertionPosition(SmartIndentDialog.newlineMacro, stoppedAt - widgetText);
		XmProcessTraversal(SmartIndentDialog.newlineMacro, XmTRAVERSE_CURRENT);
		XtFree(widgetText);
		return False;
	}
	XtFree(widgetText);
	FreeProgram(prog);

	// Test compile the modify macro 
	if (!TextWidgetIsBlank(SmartIndentDialog.modMacro)) {
		widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.modMacro));
		prog = ParseMacro(widgetText, &errMsg, &stoppedAt);
		if(!prog) {
			ParseError(SmartIndentDialog.shell, widgetText, stoppedAt, "modify macro", errMsg);
			XmTextSetInsertionPosition(SmartIndentDialog.modMacro, stoppedAt - widgetText);
			XmProcessTraversal(SmartIndentDialog.modMacro, XmTRAVERSE_CURRENT);
			XtFree(widgetText);
			return False;
		}
		XtFree(widgetText);
		FreeProgram(prog);
	}
#endif
	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
SmartIndent *DialogSmartIndent::getSmartIndentDialogData() {

	auto is = new SmartIndent;
	is->lmName       = XtNewStringEx(languageMode_.toStdString());
	is->initMacro    = ui.editInit->toPlainText().isNull()    ? nullptr : XtStringDup(ensureNewline(ui.editInit->toPlainText()).toStdString());
	is->newlineMacro = ui.editNewline->toPlainText().isNull() ? nullptr : XtStringDup(ensureNewline(ui.editNewline->toPlainText()).toStdString());
	is->modMacro     = ui.editTypeIn->toPlainText().isNull()  ? nullptr : XtStringDup(ensureNewline(ui.editTypeIn->toPlainText()).toStdString());
	return is;
}


/*
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
QString DialogSmartIndent::ensureNewline(const QString &string) {

	if(string.isNull()) {
		return QString();
	}

	int length = string.size();
	if (length == 0 || string[length - 1] == QLatin1Char('\n')) {
		return string;
	}
	
	return string + QLatin1Char('\n');
}
