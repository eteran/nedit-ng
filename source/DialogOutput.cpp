
#include "DialogOutput.h"
#include "preferences.h"

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

void DialogOutput::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    centerDialog(this);
}
