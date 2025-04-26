
#include "DialogPromptList.h"

#include <QPushButton>

// NOTE(eteran): maybe we want to present an option to have this be a combo box instead?

/**
 * @brief Constructor for DialogPromptList class.
 *
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
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

/**
 * @brief Adds a button to the dialog's button box with the specified text.
 *
 * @param text The text to be displayed on the button.
 */
void DialogPromptList::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPromptList::accept);
}

/**
 * @brief Adds a standard button to the dialog's button box.
 *
 * @param button The standard button to be added, such as QDialogButtonBox::Ok, QDialogButtonBox::Cancel, etc.
 */
void DialogPromptList::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPromptList::accept);
}

/**
 * @brief Sets the message text in the dialog's message label.
 *
 * @param text The text to be displayed in the message label.
 */
void DialogPromptList::setMessage(const QString &text) {
	ui.labelMessage->setText(text);
}

/**
 * @brief Sets the list of items in the dialog's list widget from a string.
 *
 * @param string The string containing the items to be displayed in the list.
 *
 * @note The string should contain items separated by newline characters.
 */
void DialogPromptList::setList(const QString &string) {
	const QStringList items = string.split(QLatin1Char('\n'));
	ui.listWidget->clear();
	ui.listWidget->addItems(items);
}

/**
 * @brief Handles the show event of the dialog.
 * This method resets the result to 0, clears the result text, and adjusts
 * the size of the dialog when it is shown.
 *
 * @param event The show event that is triggered when the dialog is displayed.
 */
void DialogPromptList::showEvent(QShowEvent *event) {
	adjustSize();
	result_ = 0;
	text_   = QString();
	Dialog::showEvent(event);
}

/**
 * @brief Handles the button click event in the button box.
 *
 * @param button The button that was clicked in the button box.
 */
void DialogPromptList::buttonBox_clicked(QAbstractButton *button) {

	QList<QAbstractButton *> buttons = ui.buttonBox->buttons();

	for (int i = 0; i < buttons.size(); ++i) {
		if (button == buttons[i]) {
			result_ = (i + 1);

			QList<QListWidgetItem *> items = ui.listWidget->selectedItems();
			if (items.size() == 1) {
				text_ = items[0]->text();
			}
			break;
		}
	}
}
