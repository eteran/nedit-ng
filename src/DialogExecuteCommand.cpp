
#include "DialogExecuteCommand.h"
#include <QKeyEvent>
#include <QTimer>

/**
 * @brief DialogExecuteCommand::DialogExecuteCommand
 * @param parent
 * @param f
 */
DialogExecuteCommand::DialogExecuteCommand(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

	// seed the history with a blank string, makes later logic simpler
	history_ << QLatin1String("");
}

/**
 * @brief DialogExecuteCommand::keyPressEvent
 * @param event
 */
void DialogExecuteCommand::keyPressEvent(QKeyEvent *event) {
	if(ui.textCommand->hasFocus()) {
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
 * @brief DialogExecuteCommand::showEvent
 * @param event
 */
void DialogExecuteCommand::showEvent(QShowEvent *event) {
	Dialog::showEvent(event);
	historyIndex_ = 0;
	ui.textCommand->setText(QString());
	ui.textCommand->setFocus();
}

/**
 * @brief DialogExecuteCommand::addHistoryItem
 * @param string
 */
void DialogExecuteCommand::addHistoryItem(const QString &s) {
	if(!s.isEmpty()) {
		history_ << s;
	}
}

/**
 * @brief DialogExecuteCommand::currentText
 * @return
 */
QString DialogExecuteCommand::currentText() const {
	return ui.textCommand->text();
}
