
#include "Calltip.h"

Calltip::Calltip(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
	ui.setupUi(this);
	connect(ui.buttonClose, SIGNAL(clicked()), this, SLOT(close()));
}

Calltip::~Calltip() {
}

void Calltip::setTitle(const QString &title) {
	ui.labelTitle->setText(title);
}

void Calltip::setText(const QString &text) {
	ui.labelTip->setText(text);
}
