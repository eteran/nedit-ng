
#include "DialogOutput.h"

/**
 * @brief
 *
 * @param parent
 * @param f
 */
DialogOutput::DialogOutput(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
}

/**
 * @brief
 *
 * @param text
 */
void DialogOutput::setText(const QString &text) {
	ui.plainTextEdit->setPlainText(text);
}
