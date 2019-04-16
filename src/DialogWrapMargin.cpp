
#include "DialogWrapMargin.h"
#include "DocumentWidget.h"
#include "Preferences.h"
#include "TextArea.h"

#include <QMessageBox>
#include <QTimer>

DialogWrapMargin::DialogWrapMargin(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), document_(document) {
	ui.setupUi(this);

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

	const int margin = document ?
				document->firstPane()->getWrapMargin() :
				Preferences::GetPrefWrapMargin();

	ui.checkWrapAndFill->setChecked(margin == 0);
	ui.spinWrapAndFill->setValue(margin);
}

void DialogWrapMargin::on_checkWrapAndFill_toggled(bool checked) {
	ui.label->setEnabled(!checked);
	ui.spinWrapAndFill->setEnabled(!checked);
}

void DialogWrapMargin::on_buttonBox_accepted() {

	int margin;
	if (ui.checkWrapAndFill->isChecked()) {
		margin = 0;
	} else {
		margin = ui.spinWrapAndFill->value();
	}

	// Set the value in either the requested window or default preferences
	if(!document_) {
		Preferences::SetPrefWrapMargin(margin);
	} else {
		document_->setWrapMargin(margin);
	}

	accept();
}

