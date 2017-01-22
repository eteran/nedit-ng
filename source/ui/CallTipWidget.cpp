
#include "CallTipWidget.h"
#include <QToolTip>
#include <QClipboard>

CallTipWidget::CallTipWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
	ui.setupUi(this);

    ui.labelTip->setPalette(QToolTip::palette());

	connect(ui.buttonClose, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui.buttonCopy, SIGNAL(clicked()), this, SLOT(copyText()));
}

CallTipWidget::~CallTipWidget() {
}

void CallTipWidget::setText(const QString &text) {
	ui.labelTip->setText(text);
    ui.labelTip->adjustSize();
    adjustSize();
}

void CallTipWidget::copyText() {
    QApplication::clipboard()->setText(ui.labelTip->text());
}
