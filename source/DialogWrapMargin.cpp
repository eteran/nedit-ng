
#include "DialogWrapMargin.h"
#include "DocumentWidget.h"
#include "preferences.h"

#include <QMessageBox>

DialogWrapMargin::DialogWrapMargin(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), document_(document) {
	ui.setupUi(this);
}

void DialogWrapMargin::on_checkWrapAndFill_toggled(bool checked) {
	ui.label->setEnabled(!checked);
	ui.spinWrapAndFill->setEnabled(!checked);
}

void DialogWrapMargin::on_buttonBox_accepted() {

	int margin;

	// get the values that the user entered and make sure they're ok 
	bool wrapAtWindow = ui.checkWrapAndFill->isChecked();
	if (wrapAtWindow) {
		margin = 0;
	} else {
		
		margin = ui.spinWrapAndFill->value();

		if (margin <= 0 || margin >= 1000) {
			QMessageBox::warning(this, tr("Wrap Margin"), tr("Wrap margin out of range"));
			return;
		}
	}

	// Set the value in either the requested window or default preferences 
    if(!document_) {
		SetPrefWrapMargin(margin);
	} else {
        document_->setWrapMargin(margin);
	}

	accept();
}

