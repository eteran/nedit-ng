
#include "DialogTabs.h"
#include "DocumentWidget.h"
#include "Help.h"
#include "LanguageMode.h"
#include "TextArea.h"
#include "preferences.h"
#include "TextBuffer.h"

#include <QMessageBox>

/**
 * @brief DialogTabs::DialogTabs
 * @param document
 * @param parent
 * @param f
 */
DialogTabs::DialogTabs(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), document_(document) {

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
        emTabDist = document->firstPane()->getEmulateTabs();
        useTabs   = document->buffer_->BufGetUseTabs();
        tabDist   = document->buffer_->BufGetTabDistance();
	}
	
	const bool emulate = emTabDist != 0;
	ui.editTabSpacing->setText(QString::number(tabDist));
	ui.checkEmulateTabs->setChecked(emulate);

	if (emulate) {
		ui.editEmulatedTabSpacing->setText(QString::number(emTabDist));
	}
	
	ui.checkUseTabsInPadding->setChecked(useTabs);
	ui.labelEmulatedTabSpacing->setEnabled(emulate);
	ui.editEmulatedTabSpacing->setEnabled(emulate);
	
	ui.editEmulatedTabSpacing->setValidator(new QIntValidator(0, INT_MAX, this));
	ui.editTabSpacing->setValidator(new QIntValidator(0, INT_MAX, this));

}

/**
 * @brief DialogTabs::on_checkEmulateTabs_toggled
 * @param checked
 */
void DialogTabs::on_checkEmulateTabs_toggled(bool checked) {
	ui.labelEmulatedTabSpacing->setEnabled(checked);
	ui.editEmulatedTabSpacing->setEnabled(checked);
}

/**
 * @brief DialogTabs::on_buttonBox_accepted
 */
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

    if (tabDist <= 0 || tabDist > TextBuffer::MAX_EXP_CHAR_LEN) {
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

        // NOTE(eteran): BUGCHECK, I think this should be emTabDist >= 1000
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
        document_->SetUseTabs(useTabs);
	}
	
	accept();
}

/**
 * @brief DialogTabs::on_buttonBox_helpRequested
 */
void DialogTabs::on_buttonBox_helpRequested() {
    Help::displayTopic(Help::Topic::HELP_TABS_DIALOG);
}
