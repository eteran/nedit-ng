
#include "CallTipWidget.h"
#include <QToolTip>
#include <QClipboard>

CallTipWidget::CallTipWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
	ui.setupUi(this);

    ui.labelTip->setPalette(QToolTip::palette());

    connect(ui.buttonClose, SIGNAL(clicked()), this, SLOT(deleteThis()));
    connect(ui.buttonCopy, SIGNAL(clicked()), this, SLOT(copyText()));
}

void CallTipWidget::setText(const QString &text) {
	ui.labelTip->setText(text);
    ui.labelTip->adjustSize();
    adjustSize();
}

void CallTipWidget::copyText() {
    QApplication::clipboard()->setText(ui.labelTip->text());
}

void CallTipWidget::deleteThis() {
    delete this;
}
