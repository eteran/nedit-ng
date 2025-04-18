
#include "DialogFilter.h"

#include <QKeyEvent>

/**
 * @brief Constructor for DialogFilter class
 *
 * @param parent The parent widget, defaults to nullptr
 * @param f The window flags, defaults to Qt::WindowFlags()
 */
DialogFilter::DialogFilter(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);
	connectSlots();

	Dialog::shrinkToFit(this);

	// seed the history with a blank string, makes later logic simpler
	history_ << QLatin1String("");
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogFilter::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogFilter::buttonBox_accepted);
}

/**
 * @brief Handles key press events for the filter text input.
 * This method allows the user to navigate through filter history using the up and down arrow keys.
 * If the up arrow is pressed, it retrieves the previous filter from history.
 * If the down arrow is pressed, it retrieves the next filter in history.
 *
 * @param event The key event that was pressed.
 */
void DialogFilter::keyPressEvent(QKeyEvent *event) {
	if (ui.textFilter->hasFocus()) {
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

		ui.textFilter->setText(history_[index]);
		historyIndex_ = index;
	}

	QDialog::keyPressEvent(event);
}

/**
 * @brief Handles the show event for the dialog.
 * This method resets the filter text and history index to allow for a fresh start each time the dialog is shown.
 *
 * @param event The show event that is triggered when the dialog is displayed.
 */
void DialogFilter::showEvent(QShowEvent *event) {
	historyIndex_ = 0;
	ui.textFilter->setText(QString());
	Dialog::showEvent(event);
}

/**
 * @brief Handles the acceptance of the dialog.
 */
void DialogFilter::buttonBox_accepted() {

	const QString s = ui.textFilter->text();
	if (!s.isEmpty()) {
		history_ << s;
	}
}

/**
 * @brief Returns the current text from the filter input field.
 *
 * @return The text currently entered in the filter input field.
 */
QString DialogFilter::currentText() const {
	return ui.textFilter->text();
}
