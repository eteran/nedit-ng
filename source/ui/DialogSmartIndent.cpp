
#include "DialogSmartIndent.h"
#include "DialogSmartIndentEdit.h"
#include "DialogLanguageModes.h"
#include "SmartIndent.h"
#include "LanguageMode.h"

#include "smartIndent.h"
#include "Document.h"
#include "preferences.h"


//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndent::DialogSmartIndent(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	
	// fill the list
	for (int i = 0; i < NLanguageModes; i++) {
		ui.comboLanguageMode->addItem(QLatin1String(LanguageModes[i]->name));
	}

	const char *lmName = LanguageModeName(window->languageMode_ == PLAIN_LANGUAGE_MODE ? 0 : window->languageMode_);

	// Fill in the dialog information for the selected language mode 
	setSmartIndentDialogData(findIndentSpec(lmName));
	
	int index = ui.comboLanguageMode->findText(QLatin1String(lmName), Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if(index != -1) {
		ui.comboLanguageMode->setCurrentIndex(index);
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogSmartIndent::~DialogSmartIndent() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_comboLanguageMode_currentIndexChanged(const QString &text) {
	setSmartIndentDialogData(findIndentSpec(text.toLatin1().data()));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSmartIndent::on_buttonCommon_clicked() {
	auto dialog = new DialogSmartIndentEdit(this);
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
