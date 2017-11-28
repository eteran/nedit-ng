
#include "DialogDuplicateTags.h"
#include "DocumentWidget.h"

/**
 * @brief DialogDuplicateTags::DialogDuplicateTags
 * @param document
 * @param area
 * @param f
 */
DialogDuplicateTags::DialogDuplicateTags(DocumentWidget *document, TextArea *area, Qt::WindowFlags f) : Dialog(document, f), document_(document), area_(area) {
	ui.setupUi(this);
}

/**
 * @brief DialogDuplicateTags::setTag
 * @param tag
 */
void DialogDuplicateTags::setTag(const QString &tag) {
	ui.label->setText(tr("Select File With TAG: %1").arg(tag));
}

/**
 * @brief DialogDuplicateTags::addListItem
 * @param text
 * @param id
 */
void DialogDuplicateTags::addListItem(const QString &text, int id) {
    auto item = new QListWidgetItem(text, ui.listWidget);
    item->setData(Qt::UserRole, id);
}

/**
 * @brief DialogDuplicateTags::on_buttonBox_accepted
 */
void DialogDuplicateTags::on_buttonBox_accepted() {
	if(applySelection()) {
		accept();
	}
}

/**
 * @brief DialogDuplicateTags::on_buttonBox_clicked
 * @param button
 */
void DialogDuplicateTags::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applySelection();
	}
}

/**
 * @brief DialogDuplicateTags::applySelection
 * @return
 */
bool DialogDuplicateTags::applySelection() {
	QListWidgetItem *item = ui.listWidget->currentItem();
	if(!item) {
		QApplication::beep();
		return false;
	}

    int i = item->data(Qt::UserRole).toInt();

    if (searchMode == TagSearchMode::TAG) {
        editTaggedLocationEx(area_, i); // Open the file with the definition
    } else {
        showMatchingCalltipEx(area_, i);
    }

    return true;


}
