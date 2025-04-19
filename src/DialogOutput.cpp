
#include "DialogOutput.h"

/**
 * @brief Constructor for DialogOutput class
 *
 * @param parent The parent widget, defaults to nullptr
 * @param f The window flags, defaults to Qt::WindowFlags()
 */
DialogOutput::DialogOutput(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
}

/**
 * @brief Sets the text in the dialog's plain text edit area.
 *
 * @param text The text to be set in the plain text edit area.
 */
void DialogOutput::setText(const QString &text) {
	ui.plainTextEdit->setPlainText(text);
}
