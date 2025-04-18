
#include "DialogPrompt.h"

#include <QPushButton>

/**
 * @brief
 *
 * @param parent
 * @param f
 */
DialogPrompt::DialogPrompt(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	connectSlots();
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogPrompt::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogPrompt::buttonBox_clicked);
}

/**
 * @brief
 *
 * @param text
 */
void DialogPrompt::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPrompt::accept);
}

/**
 * @brief
 *
 * @param button
 */
void DialogPrompt::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPrompt::accept);
}

/**
 * @brief
 *
 * @param text
 */
void DialogPrompt::setMessage(const QString &text) {
	ui.message->setText(text);
}

/**
 * @brief
 *
 * @param event
 */
void DialogPrompt::showEvent(QShowEvent *event) {

	adjustSize();
	result_ = 0;
	Dialog::showEvent(event);
}

/**
 * @brief
 *
 * @param button
 */
void DialogPrompt::buttonBox_clicked(QAbstractButton *button) {

	QList<QAbstractButton *> buttons = ui.buttonBox->buttons();

	for (int i = 0; i < buttons.size(); ++i) {
		if (button == buttons[i]) {
			result_ = (i + 1);
			break;
		}
	}
}
