
#include "DialogExecuteCommand.h"
#include <QKeyEvent>
#include <QtDebug>

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
DialogExecuteCommand::DialogExecuteCommand(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), historyIndex_(0) {
	ui.setupUi(this);

	// seed the history with a blank string, makes later logic simpler
	history_ << QLatin1String("");
}

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
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


//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
void DialogExecuteCommand::showEvent(QShowEvent *event) {
	historyIndex_ = 0;
	ui.textCommand->setText(QString());
    Dialog::showEvent(event);
}

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
void DialogExecuteCommand::on_buttonBox_accepted() {

	QString s = ui.textCommand->text();
	if(!s.isEmpty()) {
		history_ << s;
	}
}
