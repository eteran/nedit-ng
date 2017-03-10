
#include "DialogPrompt.h"
#include "preferences.h"
#include <QPushButton>

DialogPrompt::DialogPrompt(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), result_(0) {
	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	setWindowTitle(QLatin1String(" "));
}

void DialogPrompt::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, SIGNAL(clicked()), this, SLOT(accept()));
}

void DialogPrompt::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, SIGNAL(clicked()), this, SLOT(accept()));
}

void DialogPrompt::setMessage(const QString &text) {
	ui.labelMessage->setText(text);
}

void DialogPrompt::showEvent(QShowEvent *event) {
	resize(minimumSize());
	result_ = 0;
    Dialog::showEvent(event);
}

void DialogPrompt::on_buttonBox_clicked(QAbstractButton *button) {
	for(int i = 0; i < ui.buttonBox->buttons().size(); ++i) {
		if(button == ui.buttonBox->buttons()[i]) {
			result_ = (i + 1);
			break;
		}
	}
}
