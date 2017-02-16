
#include "DialogOutput.h"

//------------------------------------------------------------------------------
// Name: DialogOutput
//------------------------------------------------------------------------------
DialogOutput::DialogOutput(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: ~DialogOutput
//------------------------------------------------------------------------------
DialogOutput::~DialogOutput() {
}

//------------------------------------------------------------------------------
// Name: ~DialogOutput
//------------------------------------------------------------------------------
void DialogOutput::setText(const QString &text) {
	ui.plainTextEdit->setPlainText(text);
}
