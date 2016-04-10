
#include <QMessageBox>
#include <QtDebug>
#include "DialogTabs.h"
#include "Document.h"
#include "preferences.h"

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogTabs::DialogTabs(Document *forWindow, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), forWindow_(forWindow) {

	ui.setupUi(this);
	
	int emTabDist;
	int useTabs;
	int tabDist;


	// Set default values 
	if(!forWindow) {
		emTabDist = GetPrefEmTabDist(PLAIN_LANGUAGE_MODE);
		useTabs   = GetPrefInsertTabs();
		tabDist   = GetPrefTabDist(PLAIN_LANGUAGE_MODE);
	} else {
		emTabDist = ui.editEmulatedTabSpacing->text().toInt();
		useTabs   = forWindow->buffer_->useTabs_;
		tabDist   = forWindow->buffer_->BufGetTabDistance();
	}
	
	const bool emulate = emTabDist != 0;
	ui.editTabSpacing->setText(tr("%1").arg(tabDist));
	ui.checkEmulateTabs->setChecked(emulate);

	if (emulate) {
		ui.editEmulatedTabSpacing->setText(tr("%1").arg(emTabDist));
	}
	
	ui.checkUseTabsInPadding->setChecked(useTabs);
	ui.labelEmulatedTabSpacing->setEnabled(emulate);
	ui.editEmulatedTabSpacing->setEnabled(emulate);

}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogTabs::~DialogTabs() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogTabs::on_checkEmulateTabs_toggled(bool checked) {
	ui.labelEmulatedTabSpacing->setEnabled(checked);
	ui.editEmulatedTabSpacing->setEnabled(checked);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogTabs::on_buttonBox_accepted() {

	// get the values that the user entered and make sure they're ok 
	bool ok;
	int emTabDist;
	bool emulate = ui.checkEmulateTabs->isChecked();
	bool useTabs = ui.checkUseTabsInPadding->isChecked();	
	int tabDist  = ui.editTabSpacing->text().toInt(&ok);
	
	if(!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in tab spacing").arg(ui.editTabSpacing->text()));
		return;
	}

	if (tabDist <= 0 || tabDist > MAX_EXP_CHAR_LEN) {
		QMessageBox::warning(this, tr("Tab Spacing"), tr("Tab spacing out of range"));
		return;
	}

	if (emulate) {
		emTabDist = ui.editEmulatedTabSpacing->text().toInt(&ok);
		if (!ok) {
			QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in emulated tab spacing").arg(ui.editTabSpacing->text()));
			return;
		}

		if (emTabDist <= 0 || tabDist >= 1000) {
			QMessageBox::warning(this, tr("Tab Spacing"), tr("Emulated tab spacing out of range"));
			return;
		}
	} else {
		emTabDist = 0;
	}


	// Set the value in either the requested window or default preferences 
	if(!forWindow_) {
		SetPrefTabDist(tabDist);
		SetPrefEmTabDist(emTabDist);
		SetPrefInsertTabs(useTabs);
	} else {
		char *params[1];
		char numStr[25];

		params[0] = numStr;
		sprintf(numStr, "%d", tabDist);
		XtCallActionProc(forWindow_->textArea_, "set_tab_dist", nullptr, params, 1);
		
		params[0] = numStr;
		sprintf(numStr, "%d", emTabDist);
		XtCallActionProc(forWindow_->textArea_, "set_em_tab_dist", nullptr, params, 1);
		
		params[0] = numStr;
		sprintf(numStr, "%d", useTabs);
		XtCallActionProc(forWindow_->textArea_, "set_use_tabs", nullptr, params, 1);
	}
	
	accept();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogTabs::on_buttonBox_helpRequested() {
	qDebug() << "TODO(eteran): implement this";
#if 0
	Help(HELP_TABS_DIALOG);
#endif
}
