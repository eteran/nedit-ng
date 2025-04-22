
#include "DialogPromptString.h"

#include <QPushButton>

/**
 * @brief Constructor for DialogPromptString class.
 *
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
 */
DialogPromptString::DialogPromptString(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	connectSlots();
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogPromptString::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogPromptString::buttonBox_clicked);
}

/**
 * @brief Adds a button to the dialog's button box with the specified text.
 *
 * @param text The text to be displayed on the button.
 */
void DialogPromptString::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPromptString::accept);
}

/**
 * @brief Adds a standard button to the dialog's button box.
 *
 * @param button The standard button to be added, such as QDialogButtonBox::Ok, QDialogButtonBox::Cancel, etc.
 */
void DialogPromptString::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPromptString::accept);
}

/**
 * @brief Sets the message text in the dialog's message label.
 *
 * @param text The text to be displayed in the message label.
 */
void DialogPromptString::setMessage(const QString &text) {
	ui.labelMessage->setText(text);
}

/**
 * @brief Handles the show event of the dialog.
 * This method resets the result to 0, clears the result text, and adjusts
 * the size of the dialog when it is shown.
 *
 * @param event The show event that is triggered when the dialog is displayed.
 */
void DialogPromptString::showEvent(QShowEvent *event) {
	adjustSize();
	result_ = 0;
	text_   = QString();
	Dialog::showEvent(event);
}

/**
 * @brief Handles the button click event in the button box.
 * This method determines which button was clicked and sets the result and text accordingly.
 *
 * @param button The button that was clicked.
 */
void DialogPromptString::buttonBox_clicked(QAbstractButton *button) {

	QList<QAbstractButton *> buttons = ui.buttonBox->buttons();

	for (int i = 0; i < buttons.size(); ++i) {
		if (button == buttons[i]) {
			result_ = (i + 1);
			text_   = ui.lineEdit->text();
			break;
		}
	}
}
