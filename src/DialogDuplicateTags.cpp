
#include "DialogDuplicateTags.h"
#include "DocumentWidget.h"

#include <QAbstractButton>
#include <QFontDatabase>

/**
 * @brief
 *
 * @param document
 * @param area
 * @param f
 */
DialogDuplicateTags::DialogDuplicateTags(DocumentWidget *document, TextArea *area, Qt::WindowFlags f)
	: Dialog(document, f), document_(document), area_(area) {

	ui.setupUi(this);
	connectSlots();
}

/**
 * @brief
 */
void DialogDuplicateTags::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogDuplicateTags::buttonBox_clicked);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogDuplicateTags::buttonBox_accepted);
}

/**
 * @brief
 *
 * @param tag
 */
void DialogDuplicateTags::setTag(const QString &tag) {
	ui.label->setText(tr("Select File With TAG: %1").arg(tag));
}

/**
 * @brief
 *
 * @param text
 * @param id
 */
void DialogDuplicateTags::addListItem(const QString &text, int id) {
	auto item = new QListWidgetItem(text, ui.listWidget);
	item->setData(Qt::UserRole, id);
}

/**
 * @brief
 */
void DialogDuplicateTags::buttonBox_accepted() {
	if (applySelection()) {
		accept();
	}
}

/**
 * @brief
 *
 * @param button
 */
void DialogDuplicateTags::buttonBox_clicked(QAbstractButton *button) {
	if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applySelection();
	}
}

/**
 * @brief
 *
 * @return
 */
bool DialogDuplicateTags::applySelection() {
	QListWidgetItem *item = ui.listWidget->currentItem();
	if (!item) {
		QApplication::beep();
		return false;
	}

	if (!document_) {
		QApplication::beep();
		return false;
	}

	const int id = item->data(Qt::UserRole).toInt();

	if (Tags::searchMode == Tags::SearchMode::TAG) {
		document_->editTaggedLocation(area_, id); // Open the file with the definition
	} else {
		Tags::showMatchingCalltip(this, area_, id);
	}

	return true;
}
