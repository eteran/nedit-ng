
#include "DialogTabs.h"
#include "DocumentWidget.h"
#include "Help.h"
#include "LanguageMode.h"
#include "Preferences.h"
#include "TextArea.h"
#include "TextBuffer.h"

#include <QIntValidator>
#include <QMessageBox>
#include <QTimer>

/**
 * @brief DialogTabs::DialogTabs
 * @param document
 * @param parent
 * @param f
 */
DialogTabs::DialogTabs(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document) {

	ui.setupUi(this);
	connectSlots();

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

	int emTabDist;
	int useTabs;
	int tabDist;

	// Set default values
	if (!document) {
		emTabDist = Preferences::GetPrefEmTabDist(PLAIN_LANGUAGE_MODE);
		useTabs   = Preferences::GetPrefInsertTabs();
		tabDist   = Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE);
	} else {
		emTabDist = document->firstPane()->getEmulateTabs();
		useTabs   = document->buffer()->BufGetUseTabs();
		tabDist   = document->buffer()->BufGetTabDistance();
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
 * @brief DialogTabs::connectSlots
 */
void DialogTabs::connectSlots() {
	connect(ui.checkEmulateTabs, &QCheckBox::toggled, this, &DialogTabs::checkEmulateTabs_toggled);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogTabs::buttonBox_accepted);
	connect(ui.buttonBox, &QDialogButtonBox::helpRequested, this, &DialogTabs::buttonBox_helpRequested);
}

/**
 * @brief DialogTabs::checkEmulateTabs_toggled
 * @param checked
 */
void DialogTabs::checkEmulateTabs_toggled(bool checked) {
	ui.labelEmulatedTabSpacing->setEnabled(checked);
	ui.editEmulatedTabSpacing->setEnabled(checked);
}

/**
 * @brief DialogTabs::buttonBox_accepted
 */
void DialogTabs::buttonBox_accepted() {

	const bool emulate = ui.checkEmulateTabs->isChecked();
	const bool useTabs = ui.checkUseTabsInPadding->isChecked();

	if (ui.editTabSpacing->text().isEmpty()) {
		QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for tab spacing"));
	}

	bool ok;
	int tabDist = ui.editTabSpacing->text().toInt(&ok);
	if (!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in tab spacing").arg(ui.editTabSpacing->text()));
		return;
	}

	if (tabDist <= 0 || tabDist > TextBuffer::MAX_EXP_CHAR_LEN) {
		QMessageBox::warning(this, tr("Tab Spacing"), tr("Tab spacing out of range"));
		return;
	}

	int emTabDist = 0;
	if (emulate) {
		if (ui.editEmulatedTabSpacing->text().isEmpty()) {
			QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for emulated tab spacing"));
			return;
		}

		emTabDist = ui.editEmulatedTabSpacing->text().toInt(&ok);
		if (!ok) {
			QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in emulated tab spacing").arg(ui.editEmulatedTabSpacing->text()));
			return;
		}

		// BUGCHECK(eteran): this used to have tabDist >= 1000, but I think that's a C&P bug
		if (emTabDist <= 0 || emTabDist >= 1000) {
			QMessageBox::warning(this, tr("Tab Spacing"), tr("Emulated tab spacing out of range"));
			return;
		}
	}

	// Set the value in either the requested window or default preferences
	if (!document_) {
		Preferences::SetPrefTabDist(tabDist);
		Preferences::SetPrefEmTabDist(emTabDist);
		Preferences::SetPrefInsertTabs(useTabs);
	} else {
		document_->setTabDistance(tabDist);
		document_->setEmTabDistance(emTabDist);
		document_->setUseTabs(useTabs);
	}

	accept();
}

/**
 * @brief DialogTabs::buttonBox_helpRequested
 */
void DialogTabs::buttonBox_helpRequested() {
	Help::displayTopic(Help::Topic::TabsDialog);
}
