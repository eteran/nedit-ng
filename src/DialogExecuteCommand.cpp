
#include "DialogExecuteCommand.h"

#include <QKeyEvent>

/**
 * @brief Constructor for DialogExecuteCommand class
 *
 * @param parent The parent widget, defaults to nullptr
 * @param f The window flags, defaults to Qt::WindowFlags()
 */
DialogExecuteCommand::DialogExecuteCommand(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);

	Dialog::shrinkToFit(this);

	// seed the history with a blank string, makes later logic simpler
	history_ << QStringLiteral("");
}

/**
 * @brief Handles key press events for the command text input.
 * This method allows the user to navigate through command history using the up and down arrow keys.
 * If the up arrow is pressed, it retrieves the previous command from history.
 * If the down arrow is pressed, it retrieves the next command in history.
 * If the key pressed is not an arrow key, it passes the event to the base class for default handling.
 *
 * @param event The key event that was pressed.
 */
void DialogExecuteCommand::keyPressEvent(QKeyEvent *event) {
	if (ui.textCommand->hasFocus()) {
		int index = historyIndex_;

		// only process up and down arrow keys
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			QDialog::keyPressEvent(event);
			return;
		}

		// increment or decrement the index depending on which arrow was pressed
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return
		if (index < 0 || index >= history_.size()) {
			QApplication::beep();
			return;
		}

		ui.textCommand->setText(history_[index]);
		historyIndex_ = index;
	}

	QDialog::keyPressEvent(event);
}

/**
 * @brief Handles the show event for the dialog.
 * This method resets the command history index to 0 and clears the command text input.
 *
 * @param event The show event that triggered this method.
 */
void DialogExecuteCommand::showEvent(QShowEvent *event) {
	Dialog::showEvent(event);
	historyIndex_ = 0;
	ui.textCommand->setText(QString());
	ui.textCommand->setFocus();
}

/**
 * @brief Adds a command to the history.
 *
 * @param string The command string to be added to the history.
 */
void DialogExecuteCommand::addHistoryItem(const QString &s) {
	if (!s.isEmpty()) {
		history_ << s;
	}
}

/**
 * @brief Returns the current text in the command input field.
 *
 * @return The current text in the command input field.
 */
QString DialogExecuteCommand::currentText() const {
	return ui.textCommand->text();
}
