
#include "DialogPromptList.h"

#include <QPushButton>

// NOTE(eteran): maybe we want to present an option to have this be a combo box instead?

/**
 * @brief
 *
 * @param parent
 * @param f
 */
DialogPromptList::DialogPromptList(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	connectSlots();
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogPromptList::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogPromptList::buttonBox_clicked);
}

void DialogPromptList::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPromptList::accept);
}

void DialogPromptList::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPromptList::accept);
}

void DialogPromptList::setMessage(const QString &text) {
	ui.labelMessage->setText(text);
}

void DialogPromptList::setList(const QString &string) {
	const QStringList items = string.split(QLatin1Char('\n'));
	ui.listWidget->clear();
	ui.listWidget->addItems(items);
}

void DialogPromptList::showEvent(QShowEvent *event) {
	adjustSize();
	result_ = 0;
	text_   = QString();
	Dialog::showEvent(event);
}

void DialogPromptList::buttonBox_clicked(QAbstractButton *button) {

	QList<QAbstractButton *> buttons = ui.buttonBox->buttons();

	for (int i = 0; i < buttons.size(); ++i) {
		if (button == buttons[i]) {
			result_ = (i + 1);

			QList<QListWidgetItem *> items = ui.listWidget->selectedItems();
			if (items.size() == 1) {
				QListWidgetItem *item = items[0];
				text_                 = item->text();
			}
			break;
		}
	}
}
