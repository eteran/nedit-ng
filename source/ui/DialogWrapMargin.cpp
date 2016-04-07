
#include "DialogWrapMargin.h"
#include <QMessageBox>
#include "preferences.h"
#include "Document.h"

DialogWrapMargin::DialogWrapMargin(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window) {
	ui.setupUi(this);
}

DialogWrapMargin::~DialogWrapMargin() {
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
	if(!window_) {
		SetPrefWrapMargin(margin);
	} else {
		char *params[1];
		char marginStr[25];
		snprintf(marginStr, sizeof(marginStr), "%d", margin);
		params[0] = marginStr;
		XtCallActionProc(window_->textArea_, "set_wrap_margin", nullptr, params, 1);
	}

	accept();
}
