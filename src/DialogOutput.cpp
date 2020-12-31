
#include "DialogOutput.h"

/**
 * @brief DialogOutput::DialogOutput
 * @param parent
 * @param f
 */
DialogOutput::DialogOutput(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);

	ui.plainTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

/**
 * @brief DialogOutput::setText
 * @param text
 */
void DialogOutput::setText(const QString &text) {
	ui.plainTextEdit->setPlainText(text);
}
