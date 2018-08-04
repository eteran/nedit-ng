
#include "DialogPrompt.h"
#include <QPushButton>

DialogPrompt::DialogPrompt(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	setWindowTitle(QLatin1String(" "));
}

void DialogPrompt::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPrompt::accept);
}

void DialogPrompt::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPrompt::accept);
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

	QList<QAbstractButton *> buttons = ui.buttonBox->buttons();

	for(int i = 0; i < buttons.size(); ++i) {
		if(button == buttons[i]) {
			result_ = (i + 1);
			break;
		}
	}
}
