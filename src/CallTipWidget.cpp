
#include "CallTipWidget.h"

#include <QClipboard>
#include <QFontDatabase>
#include <QToolTip>

/**
 * @brief
 *
 * @param parent
 * @param f
 */
CallTipWidget::CallTipWidget(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);

	setAttribute(Qt::WA_DeleteOnClose, true);
	ui.labelTip->setPalette(QToolTip::palette());

	connect(ui.buttonClose, &QAbstractButton::clicked, this, &CallTipWidget::close);

	connect(ui.buttonCopy, &QAbstractButton::clicked, this, [this]() {
		QApplication::clipboard()->setText(ui.labelTip->text());
	});
}

/**
 * @brief
 *
 * @param text
 */
void CallTipWidget::setText(const QString &text) {
	ui.labelTip->setText(text);
	ui.labelTip->adjustSize();
}

/**
 * @brief
 *
 * @param event
 */
void CallTipWidget::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	resize(minimumSize());
}
