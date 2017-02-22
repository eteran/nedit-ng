
#include "DialogFilter.h"
#include "preferences.h"
#include <QKeyEvent>
#include <QtDebug>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogFilter::DialogFilter(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), historyIndex_(0) {
	ui.setupUi(this);
	
	// seed the history with a blank string, makes later logic simpler
	history_ << QLatin1String("");
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogFilter::~DialogFilter() {
}
	
//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
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


//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogFilter::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
	historyIndex_ = 0;
	ui.textFilter->setText(QString());

    centerDialog(this);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogFilter::on_buttonBox_accepted() {

	QString s = ui.textFilter->text();
	if(!s.isEmpty()) {
		history_ << s;
	}
}
