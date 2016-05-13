
#include "DialogPromptString.h"
#include <QPushButton>

DialogPromptString::DialogPromptString(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), result_(0) {
	setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
	ui.setupUi(this);
	setWindowTitle(QLatin1String(" "));
}

DialogPromptString::~DialogPromptString() {
}

void DialogPromptString::addButton(const QString &text) {
	QPushButton *btn = ui.buttonBox->addButton(text, QDialogButtonBox::AcceptRole);
	connect(btn, SIGNAL(clicked()), this, SLOT(accept()));
}

void DialogPromptString::addButton(QDialogButtonBox::StandardButton button) {
	QPushButton *btn = ui.buttonBox->addButton(button);
	connect(btn, SIGNAL(clicked()), this, SLOT(accept()));
}

void DialogPromptString::setMessage(const QString &text) {
	ui.labelMessage->setText(text);
}

void DialogPromptString::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
	resize(minimumSize());
	result_ = 0;
	text_ = QString();
}

void DialogPromptString::on_buttonBox_clicked(QAbstractButton *button) {
	for(int i = 0; i < ui.buttonBox->buttons().size(); ++i) {
		if(button == ui.buttonBox->buttons()[i]) {
			result_ = (i + 1);
			text_ = ui.lineEdit->text();
			break;
		}
	}
}
