
#include "DialogRepeat.h"
#include "CommandRecorder.h"
#include "DocumentWidget.h"
#include "Macro.h"
#include "TextBuffer.h"

#include <QIntValidator>
#include <QMessageBox>

/**
 * @brief Constructor for DialogRepeat class.
 *
 * @param document The DocumentWidget instance associated with this dialog.
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
 */
DialogRepeat::DialogRepeat(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document) {
	ui.setupUi(this);
	connectSlots();

	Dialog::shrinkToFit(this);

	ui.lineEdit->setValidator(new QIntValidator(0, INT_MAX, this));

	const QString replayMacro = CommandRecorder::instance()->replayMacro();
	if (!replayMacro.isEmpty()) {
		ui.radioLearnReplay->setEnabled(true);
	}
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogRepeat::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogRepeat::buttonBox_accepted);
}

/**
 * @brief Sets the command for the dialog to repeat.
 *
 * @param command The command string that is the last executed action name.
 * @return `true` if the command was set successfully, `false` otherwise.
 */
bool DialogRepeat::setCommand(const QString &command) {

	/* make a label for the Last command item of the dialog, which includes
	   the last executed action name */
	const int index = command.indexOf(QLatin1Char('('));
	if (index == -1) {
		return false;
	}

	lastCommand_ = command;
	ui.radioLastCommand->setText(tr("Last &Command (%1)").arg(command.left(index)));
	return true;
}

/**
 * @brief Handles the "Accept" button click event in the dialog.
 */
void DialogRepeat::buttonBox_accepted() {
	if (!doRepeatDialogAction()) {
		return;
	}

	accept();
}

/**
 * @brief Handles the "Repeat" action in the dialog.
 *
 * @return true if the action was successfully performed, false otherwise.
 */
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
