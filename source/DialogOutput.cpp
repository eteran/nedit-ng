
#include "DialogOutput.h"
#include "preferences.h"

//------------------------------------------------------------------------------
// Name: DialogOutput
//------------------------------------------------------------------------------
DialogOutput::DialogOutput(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: ~DialogOutput
//------------------------------------------------------------------------------
void DialogOutput::setText(const QString &text) {
	ui.plainTextEdit->setPlainText(text);
}
