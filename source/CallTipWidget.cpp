
#include "CallTipWidget.h"
#include <QClipboard>
#include <QToolTip>

/**
 * @brief CallTipWidget::CallTipWidget
 * @param parent
 * @param f
 */
CallTipWidget::CallTipWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
	ui.setupUi(this);

	setAttribute(Qt::WA_DeleteOnClose, true);

	ui.labelTip->setPalette(QToolTip::palette());

    connect(ui.buttonClose, &QAbstractButton::clicked, this, &CallTipWidget::close);
    connect(ui.buttonCopy,  &QAbstractButton::clicked, this, &CallTipWidget::copyText);
}

/**
 * @brief CallTipWidget::setText
 * @param text
 */
void CallTipWidget::setText(const QString &text) {
	ui.labelTip->setText(text);
	ui.labelTip->adjustSize();    
}

/**
 * @brief CallTipWidget::copyText
 */
void CallTipWidget::copyText() {
	QApplication::clipboard()->setText(ui.labelTip->text());
}

/**
 * @brief CallTipWidget::showEvent
 * @param event
 */
void CallTipWidget::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    resize(minimumSize());
}
