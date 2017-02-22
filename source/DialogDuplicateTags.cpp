
#include "DialogDuplicateTags.h"
#include "tags.h"
#include "preferences.h"
#include <QPushButton>

//------------------------------------------------------------------------------
// Name: DialogDuplicateTags
//------------------------------------------------------------------------------
DialogDuplicateTags::DialogDuplicateTags(DocumentWidget *document, TextArea *area, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), document_(document), area_(area) {
	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: setTag
//------------------------------------------------------------------------------
void DialogDuplicateTags::setTag(const QString &tag) {
	ui.label->setText(tr("Select File With TAG: %1").arg(tag));
}

//------------------------------------------------------------------------------
// Name: addListItem
//------------------------------------------------------------------------------
void DialogDuplicateTags::addListItem(const QString &item) {
	ui.listWidget->addItem(item);
}


void DialogDuplicateTags::on_buttonBox_accepted() {
	if(applySelection()) {
		accept();
	}
}

void DialogDuplicateTags::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applySelection();
	}
}


bool DialogDuplicateTags::applySelection() {
	QListWidgetItem *item = ui.listWidget->currentItem();
	if(!item) {
		QApplication::beep();
		return false;
	}


	// TODO(eteran): replace with data embeded in the item telling us which index it is
	std::string eptr = item->text().toStdString();
	try {
		int i = stoi(eptr) - 1;

		if (searchMode == TAG) {
            editTaggedLocationEx(area_, i); // Open the file with the definition
		} else {
            showMatchingCalltipEx(area_, i);
		}

		return true;

	} catch(const std::exception &) {
		QApplication::beep();
	}

	return false;
}

void DialogDuplicateTags::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    centerDialog(this);
}
