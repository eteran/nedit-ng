
#include "CallTipWidget.h"

#include <QClipboard>
#include <QFontDatabase>
#include <QToolTip>

/**
 * @brief Constructor for CallTipWidget
 *
 * @param parent the parent widget
 * @param f the window flags
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
 * @brief Sets the text for the call tip widget.
 *
 * @param text the text to display in the call tip
 */
void CallTipWidget::setText(const QString &text) {
	ui.labelTip->setText(text);
	ui.labelTip->adjustSize();
}

/**
 * @brief Handles the show event for the call tip widget.
 * This resizes the widget to its minimum size when shown.
 *
 * @param event the show event
 *
 */
void CallTipWidget::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	resize(minimumSize());
}
