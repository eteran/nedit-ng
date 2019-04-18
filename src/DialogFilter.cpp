
#include "DialogFilter.h"
#include <QKeyEvent>
#include <QTimer>

/**
 * @brief DialogFilter::DialogFilter
 * @param parent
 * @param f
 */
DialogFilter::DialogFilter(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

	// seed the history with a blank string, makes later logic simpler
	history_ << QLatin1String("");
}

/**
 * @brief DialogFilter::keyPressEvent
 * @param event
 */
void DialogFilter::keyPressEvent(QKeyEvent *event) {
	if(ui.textFilter->hasFocus()) {
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
 * @brief DialogFilter::showEvent
 * @param event
 */
void DialogFilter::showEvent(QShowEvent *event) {
	historyIndex_ = 0;
	ui.textFilter->setText(QString());
	Dialog::showEvent(event);
}

/**
 * @brief DialogFilter::on_buttonBox_accepted
 */
void DialogFilter::on_buttonBox_accepted() {

	QString s = ui.textFilter->text();
	if(!s.isEmpty()) {
		history_ << s;
	}
}

/**
 * @brief DialogFilter::currentText
 * @return
 */
QString DialogFilter::currentText() const {
	return ui.textFilter->text();
}
