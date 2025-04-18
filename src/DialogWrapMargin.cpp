
#include "DialogWrapMargin.h"
#include "DocumentWidget.h"
#include "Preferences.h"
#include "TextArea.h"

#include <QMessageBox>

DialogWrapMargin::DialogWrapMargin(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document) {
	ui.setupUi(this);
	connectSlots();

	Dialog::shrinkToFit(this);

	const int margin = document ? document->firstPane()->getWrapMargin() : Preferences::GetPrefWrapMargin();

	ui.checkWrapAndFill->setChecked(margin == 0);
	ui.spinWrapAndFill->setValue(margin);
}

/**
 * @brief
 */
void DialogWrapMargin::connectSlots() {
	connect(ui.checkWrapAndFill, &QCheckBox::toggled, this, &DialogWrapMargin::checkWrapAndFill_toggled);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogWrapMargin::buttonBox_accepted);
}

void DialogWrapMargin::checkWrapAndFill_toggled(bool checked) {
	ui.label->setEnabled(!checked);
	ui.spinWrapAndFill->setEnabled(!checked);
}

void DialogWrapMargin::buttonBox_accepted() {

	int margin;
	if (ui.checkWrapAndFill->isChecked()) {
		margin = 0;
	} else {
		margin = ui.spinWrapAndFill->value();
	}

	// Set the value in either the requested window or default preferences
	if (!document_) {
		Preferences::SetPrefWrapMargin(margin);
	} else {
		document_->setWrapMargin(margin);
	}

	accept();
}
