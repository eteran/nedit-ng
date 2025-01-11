
#include "DialogRepeat.h"
#include "CommandRecorder.h"
#include "DocumentWidget.h"
#include "TextBuffer.h"
#include "macro.h"

#include <QIntValidator>
#include <QMessageBox>
#include <QTimer>

DialogRepeat::DialogRepeat(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document) {
	ui.setupUi(this);
	connectSlots();

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

	ui.lineEdit->setValidator(new QIntValidator(0, INT_MAX, this));

	const QString replayMacro = CommandRecorder::instance()->replayMacro();
	if (!replayMacro.isEmpty()) {
		ui.radioLearnReplay->setEnabled(true);
	}
}

void DialogRepeat::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogRepeat::buttonBox_accepted);
}

bool DialogRepeat::setCommand(const QString &command) {

	/* make a label for the Last command item of the dialog, which includes
	   the last executed action name */
	int index = command.indexOf(QLatin1Char('('));
	if (index == -1) {
		return false;
	}

	lastCommand_ = command;
	ui.radioLastCommand->setText(tr("Last &Command (%1)").arg(command.left(index)));
	return true;
}

void DialogRepeat::buttonBox_accepted() {
	if (!doRepeatDialogAction()) {
		return;
	}

	accept();
}

bool DialogRepeat::doRepeatDialogAction() {

	if (!document_) {
		return false;
	}

	int how;

	// Find out from the dialog how to repeat the command
	if (ui.radioInSelection->isChecked()) {
		if (!document_->buffer()->primary.hasSelection()) {
			QMessageBox::warning(this, tr("Repeat Macro"), tr("No selection in window to repeat within"));
			return false;
		}
		how = REPEAT_IN_SEL;
	} else if (ui.radioToEnd->isChecked()) {
		how = REPEAT_TO_END;
	} else {

		const QString strTimes = ui.lineEdit->text();
		if (strTimes.isEmpty()) {
			QMessageBox::warning(this, tr("Warning"), tr("Please supply a value for number of times"));
			return false;
		}

		bool ok;
		const int nTimes = strTimes.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in number of times").arg(strTimes));
			return false;
		}

		how = nTimes;
	}

	QString macro;
	const QString replayMacro = CommandRecorder::instance()->replayMacro();

	// Figure out which command user wants to repeat
	if (ui.radioLastCommand->isChecked()) {
		macro = lastCommand_;
	} else {
		if (replayMacro.isEmpty()) {
			return false;
		}
		macro = replayMacro;
	}

	// call the action routine repeat_macro to do the work
	document_->repeatMacro(macro, how);
	return true;
}
