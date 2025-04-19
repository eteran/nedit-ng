
#include "TabWidget.h"

#include <QToolButton>

/**
 * @brief
 *
 * @param parent
 */
TabWidget::TabWidget(QWidget *parent)
	: QTabWidget(parent) {

	// NOTE(eteran): fix for issue #51
	auto allToolButtons = findChildren<QToolButton *>();
	for (QToolButton *button : allToolButtons) {
		button->setFocusPolicy(Qt::NoFocus);
	}
}

/**
 * @brief
 */
void TabWidget::tabInserted(int index) {
	Q_EMIT tabCountChanged(count());
	QTabWidget::tabInserted(index);
}

/**
 * @brief
 */
void TabWidget::tabRemoved(int index) {
	Q_EMIT tabCountChanged(count());
	QTabWidget::tabRemoved(index);
}
