
#include "DialogSmartIndent.h"
#include "DialogLanguageModes.h"
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "SmartIndentEntry.h"
#include "interpret.h"
#include "macro.h"
#include "preferences.h"
#include "smartIndent.h"
#include "Help.h"
#include <QMessageBox>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndent::DialogSmartIndent(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
		
    QString languageMode = LanguageModeName(document->languageMode_ == PLAIN_LANGUAGE_MODE ? 0 : document->languageMode_);

	updateLanguageModes();
	setLanguageMode(languageMode);

	// Fill in the dialog information for the selected language mode 
    setSmartIndentDialogData(findIndentSpec(languageMode_));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::updateLanguageModes() {

	const QString languageMode = languageMode_;
	ui.comboLanguageMode->clear();
    Q_FOREACH(const LanguageMode &lang, LanguageModes) {
        ui.comboLanguageMode->addItem(lang.name);
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
    setSmartIndentDialogData(findIndentSpec(text));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonCommon_clicked() {
	auto dialog = std::make_unique<DialogSmartIndentCommon>(this);
	dialog->exec();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonLanguageMode_clicked() {
	auto dialog = std::make_unique<DialogLanguageModes>(this);
	dialog->exec();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonOK_clicked() {
	// change the macro 
	if (!updateSmartIndentData()) {
		return;
	}
	
	accept();
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
	if (checkSmartIndentDialogData()) {
		QMessageBox::information(this, tr("Macro compiled"), tr("Macros compiled without error"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonDelete_clicked() {

	// NOTE(eteran): originally was "Yes, Delete"
	int resp = QMessageBox::question(this, tr("Delete Macros"), tr("Are you sure you want to delete smart indent macros for language mode %1?").arg(languageMode_), QMessageBox::Yes | QMessageBox::Cancel);
	if(resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, delete it from the list 
    for (auto it = SmartIndentSpecs.begin(); it != SmartIndentSpecs.end(); ++it) {
        if (languageMode_ == it->lmName) {
            SmartIndentSpecs.erase(it);
		}
	}

	// Clear out the dialog 
	setSmartIndentDialogData(nullptr);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonRestore_clicked() {

    const SmartIndentEntry *defaultIS = findDefaultIndentSpec(languageMode_);
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
    int i;
    for (i = 0; i < SmartIndentSpecs.size(); i++) {
        if (languageMode_ == SmartIndentSpecs[i].lmName) {
			break;
		}
	}
	
    if (i < SmartIndentSpecs.size()) {
        SmartIndentSpecs[i] = *defaultIS;
	} else {
        SmartIndentSpecs.push_back(*defaultIS);
	}

	// Update the dialog 
    setSmartIndentDialogData(defaultIS);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonHelp_clicked() {
    Help::displayTopic(Help::Topic::HELP_SMART_INDENT);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
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
	auto newMacros = getSmartIndentDialogData();

	// Find the original macros
	int i;
    for (i = 0; i < SmartIndentSpecs.size(); i++) {
        if (languageMode_ == SmartIndentSpecs[i].lmName) {
			break;
		}
	}

	/* If it's a new language, add it at the end, otherwise free the
	   existing macros and replace it */
    if (i == SmartIndentSpecs.size()) {
        SmartIndentSpecs.push_back(*newMacros);
	} else {
        SmartIndentSpecs[i] = *newMacros;
	}

	/* Find windows that are currently using this indent specification and
	   re-do the smart indent macros */
    Q_FOREACH(DocumentWidget *document, DocumentWidget::allDocuments()) {

        QString lmName = LanguageModeName(document->languageMode_);
		if(!lmName.isNull()) {
			if (lmName == newMacros->lmName) {

                if(auto window = MainWindow::fromDocument(document)) {
                    window->ui.action_Indent_Smart->setEnabled(true);
                }

                if (document->indentStyle_ == IndentStyle::Smart && document->languageMode_ != PLAIN_LANGUAGE_MODE) {
                    document->EndSmartIndentEx();
                    document->BeginSmartIndentEx(false);
				}
			}
		}
	}

	// Note that preferences have been changed 
	MarkPrefsChanged();
	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSmartIndent::checkSmartIndentDialogData() {

	// BUGFIX(eteran): make it not check if all fields are empty...

	// Check the initialization macro 
	QString initText = ui.editInit->toPlainText();
	if (!initText.isEmpty()) {
		QString widgetText = ensureNewline(initText);
		int stoppedAt = 0;
        if (!CheckMacroStringEx(this, widgetText, tr("initialization macro"), &stoppedAt)) {
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
	
	QString widgetText = ensureNewline(newlineText);
	QString errMsg;
	int stoppedAt = 0;
    Program *prog = ParseMacroEx(widgetText, &errMsg, &stoppedAt);
	if(!prog) {
		ParseErrorEx(this, widgetText, stoppedAt, tr("newline macro"), errMsg);	
		QTextCursor cursor = ui.editNewline->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editNewline->setTextCursor(cursor);
		ui.editNewline->setFocus();		
		return false;
	}

	FreeProgram(prog);


	// Test compile the modify macro 
	QString modMacroText = ui.editModMacro->toPlainText();
	if (!modMacroText.isEmpty()) {
		QString widgetText = ensureNewline(modMacroText);
		QString errMsg;
		int stoppedAt = 0;
        Program *prog = ParseMacroEx(widgetText, &errMsg, &stoppedAt);

		if(!prog) {
			ParseErrorEx(this, widgetText, stoppedAt, tr("modify macro"), errMsg);
			QTextCursor cursor = ui.editModMacro->textCursor();
			cursor.setPosition(stoppedAt);
			ui.editModMacro->setTextCursor(cursor);
			ui.editModMacro->setFocus();	
			return false;
		}
		
		FreeProgram(prog);
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
std::unique_ptr<SmartIndentEntry> DialogSmartIndent::getSmartIndentDialogData() {

	auto is = std::make_unique<SmartIndentEntry>();
	is->lmName       = languageMode_;
	is->initMacro    = ui.editInit->toPlainText().isEmpty()     ? QString() : ensureNewline(ui.editInit->toPlainText());
	is->newlineMacro = ui.editNewline->toPlainText().isEmpty()  ? QString() : ensureNewline(ui.editNewline->toPlainText());
	is->modMacro     = ui.editModMacro->toPlainText().isEmpty() ? QString() : ensureNewline(ui.editModMacro->toPlainText());
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
	
	if(string.endsWith(QLatin1Char('\n'))) {
		return string;	
	}
	
	return string + QLatin1Char('\n');	
}

bool DialogSmartIndent::hasSmartIndentMacros(const QString &languageMode) const {
	return languageMode_ == languageMode;
}
