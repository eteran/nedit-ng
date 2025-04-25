
#include "TabWidget.h"

#include <QToolButton>

/**
 * @brief TabWidget constructor.
 *
 * @param parent The parent widget for this TabWidget.
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
 * @brief Handles the insertion of a new tab.
 * Emits a signal to notify that the tab count has changed.
 */
void TabWidget::tabInserted(int index) {
	Q_EMIT tabCountChanged(count());
	QTabWidget::tabInserted(index);
}

/**
 * @brief Handles the removal of a tab.
 * Emits a signal to notify that the tab count has changed.
 */
void TabWidget::tabRemoved(int index) {
	Q_EMIT tabCountChanged(count());
	QTabWidget::tabRemoved(index);
}
