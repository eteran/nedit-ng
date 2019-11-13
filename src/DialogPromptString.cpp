
#include "DialogPromptString.h"
#include <QPushButton>

DialogPromptString::DialogPromptString(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	connectSlots();
}

void DialogPromptString::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogPromptString::buttonBox_clicked);
}

void DialogPromptString::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, &QPushButton::clicked, this, &DialogPromptString::accept);
}

void DialogPromptString::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, &QPushButton::clicked, this, &DialogPromptString::accept);
}

void DialogPromptString::setMessage(const QString &text) {
	ui.labelMessage->setText(text);
}

void DialogPromptString::showEvent(QShowEvent *event) {
	adjustSize();
	result_ = 0;
	text_   = QString();
	Dialog::showEvent(event);
}

void DialogPromptString::buttonBox_clicked(QAbstractButton *button) {

	QList<QAbstractButton *> buttons = ui.buttonBox->buttons();

	for (int i = 0; i < buttons.size(); ++i) {
		if (button == buttons[i]) {
			result_ = (i + 1);
			text_   = ui.lineEdit->text();
			break;
		}
	}
}
