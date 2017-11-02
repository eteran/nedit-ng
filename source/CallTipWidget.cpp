
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

	connect(ui.buttonClose, SIGNAL(clicked()), this, SLOT(close()));
	connect(ui.buttonCopy, SIGNAL(clicked()), this, SLOT(copyText()));
}

/**
 * @brief CallTipWidget::setText
 * @param text
 */
void CallTipWidget::setText(const QString &text) {
	ui.labelTip->setText(text);
	ui.labelTip->adjustSize();

    resize(minimumSize());
}

/**
 * @brief CallTipWidget::copyText
 */
void CallTipWidget::copyText() {
	QApplication::clipboard()->setText(ui.labelTip->text());
}
