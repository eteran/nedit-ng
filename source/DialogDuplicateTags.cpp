
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
void DialogDuplicateTags::addListItem(const QString &text, int id) {
    auto item = new QListWidgetItem(text, ui.listWidget);
    item->setData(Qt::UserRole, id);
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

    int i = item->data(Qt::UserRole).toInt();

    if (searchMode == TAG) {
        editTaggedLocationEx(area_, i); // Open the file with the definition
    } else {
        showMatchingCalltipEx(area_, i);
    }

    return true;


}

void DialogDuplicateTags::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    centerDialog(this);
}
