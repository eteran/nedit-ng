
#include "DialogWrapMargin.h"
#include "DocumentWidget.h"
#include "Preferences.h"
#include "TextArea.h"

#include <QMessageBox>

/**
 * @brief Constructor for the DialogWrapMargin class.
 *
 * @param document The DocumentWidget associated with this dialog, which provides the context for the wrap margin setting.
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
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
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogWrapMargin::connectSlots() {
	connect(ui.checkWrapAndFill, &QCheckBox::toggled, this, &DialogWrapMargin::checkWrapAndFill_toggled);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogWrapMargin::buttonBox_accepted);
}

/**
 * @brief Handles the toggling of the "Wrap and Fill" checkbox.
 *
 * @param checked Indicates whether the "Wrap and Fill" checkbox is checked.
 */
void DialogWrapMargin::checkWrapAndFill_toggled(bool checked) {
	ui.label->setEnabled(!checked);
	ui.spinWrapAndFill->setEnabled(!checked);
}

/**
 * @brief Handles the acceptance of the dialog when the "OK" button is clicked.
 */
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
