
#include "DialogPrompt.h"

#include <QPushButton>

/**
 * @brief Constructor for DialogPrompt class.
 *
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
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
 * @brief Adds a button to the dialog's button box with the specified text.
 *
 * @param text The text to be displayed on the button.
 */
void DialogPrompt::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPrompt::accept);
}

/**
 * @brief Adds a standard button to the dialog's button box.
 *
 * @param button The standard button to be added, such as QDialogButtonBox::Ok, QDialogButtonBox::Cancel, etc.
 */
void DialogPrompt::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPrompt::accept);
}

/**
 * @brief Sets the message text in the dialog's message label.
 *
 * @param text The text to be displayed in the message label.
 */
void DialogPrompt::setMessage(const QString &text) {
	ui.message->setText(text);
}

/**
 * @brief Handles the show event of the dialog.
 * This method resets the result to 0 and adjusts the size of the dialog when it is shown.
 *
 * @param event The show event that is triggered when the dialog is displayed.
 */
void DialogPrompt::showEvent(QShowEvent *event) {

	adjustSize();
	result_ = 0;
	Dialog::showEvent(event);
}

/**
 * @brief Handles the button click event in the button box.
 *
 * @param button The button that was clicked in the button box.
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
