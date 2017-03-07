
#include "DialogTabs.h"
#include "DocumentWidget.h"
#include "TextBuffer.h"
#include "preferences.h"
#include <QMessageBox>
#include <QtDebug>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogTabs::DialogTabs(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), document_(document) {

	ui.setupUi(this);
	
	int emTabDist;
	int useTabs;
	int tabDist;

	// Set default values 
    if(!document) {
		emTabDist = GetPrefEmTabDist(PLAIN_LANGUAGE_MODE);
		useTabs   = GetPrefInsertTabs();
		tabDist   = GetPrefTabDist(PLAIN_LANGUAGE_MODE);
	} else {
		emTabDist = ui.editEmulatedTabSpacing->text().toInt();
        useTabs   = document->buffer_->useTabs_;
        tabDist   = document->buffer_->BufGetTabDistance();
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
	
	ui.editEmulatedTabSpacing->setValidator(new QIntValidator(0, INT_MAX, this));
	ui.editTabSpacing->setValidator(new QIntValidator(0, INT_MAX, this));

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

	if(ui.editTabSpacing->text().isEmpty()) {
		QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for tab spacing"));
	}
	
	if(!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in tab spacing").arg(ui.editTabSpacing->text()));
		return;
	}

	if (tabDist <= 0 || tabDist > MAX_EXP_CHAR_LEN) {
		QMessageBox::warning(this, tr("Tab Spacing"), tr("Tab spacing out of range"));
		return;
	}

	if (emulate) {	
		if(ui.editEmulatedTabSpacing->text().isEmpty()) {
			QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for emulated tab spacing"));
		}	
	
		emTabDist = ui.editEmulatedTabSpacing->text().toInt(&ok);
		if (!ok) {
			QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in emulated tab spacing").arg(ui.editEmulatedTabSpacing->text()));
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
    if(!document_) {
		SetPrefTabDist(tabDist);
		SetPrefEmTabDist(emTabDist);
		SetPrefInsertTabs(useTabs);
	} else {
        document_->SetTabDist(tabDist);
        document_->SetEmTabDist(emTabDist);
        document_->buffer_->useTabs_ = useTabs;
	}
	
	accept();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogTabs::on_buttonBox_helpRequested() {
#if 0
	Help(HELP_TABS_DIALOG);
#endif
}

void DialogTabs::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    centerDialog(this);
}
