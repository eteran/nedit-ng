
#include <QMessageBox>
#include <QIntValidator>
#include "DialogRepeat.h"
#include "Document.h"
#include "MotifHelper.h"
#include "macro.h"

DialogRepeat::DialogRepeat(Document *forWindow, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), forWindow_(forWindow) {
	ui.setupUi(this);
	ui.lineEdit->setValidator(new QIntValidator(0, INT_MAX, this));
}

DialogRepeat::~DialogRepeat() {
}

void DialogRepeat::setCommand(const QString &command) {
	
	
	/* make a label for the Last command item of the dialog, which includes
	   the last executed action name */
	QByteArray str = command.toLatin1();
	char *LastCommand = str.data();	
	char *parenChar = strchr(LastCommand, '(');
	if(!parenChar) {
		return;
	}
	
	int cmdNameLen = parenChar - LastCommand;
	
	lastCommand_ = command;
	ui.radioLastCommand->setText(tr("Last &Command (%1)").arg(QString::fromLatin1(LastCommand, cmdNameLen)));
}

void DialogRepeat::on_buttonBox_accepted() {
	if(!doRepeatDialogAction()) {
		return;
	}
	
	accept();
}

bool DialogRepeat::doRepeatDialogAction() {

	char nTimesStr[TYPE_INT_STR_SIZE(int)];
	const char *params[2];

	// Find out from the dialog how to repeat the command 
	if (ui.radioInSelection->isChecked()) {
		if (!forWindow_->buffer_->primary_.selected) {
			QMessageBox::warning(this, tr("Repeat Macro"), tr("No selection in window to repeat within"));
			return False;
		}
		params[0] = "in_selection";
	} else if (ui.radioToEnd->isChecked()) {
		params[0] = "to_end";
	} else {
	
		QString strTimes = ui.lineEdit->text();
		bool ok;
		int nTimes = strTimes.toInt(&ok);
		if(!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in number of times").arg(strTimes));
			return false;
		}
	
		sprintf(nTimesStr, "%d", nTimes);
		params[0] = nTimesStr;
	}

	// Figure out which command user wants to repeat 
	if (ui.radioLastCommand->isChecked()) {
		params[1] = XtNewStringEx(lastCommand_);
	} else {
		if (ReplayMacro.empty()) {
			return false;
		}
		params[1] = XtNewStringEx(ReplayMacro);
	}

	// call the action routine repeat_macro to do the work 
	XtCallActionProc(forWindow_->lastFocus_, "repeat_macro", nullptr, const_cast<char **>(params), 2);
	XtFree((char *)params[1]);
	return true;
}
