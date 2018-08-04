
#include "TabWidget.h"
#include <QTabBar>

/**
 * @brief TabWidget::TabWidget
 * @param parent
 */
TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent) {
}

/**
 * @brief TabWidget::tabInserted
 */
void TabWidget::tabInserted(int index) {
	Q_EMIT tabCountChanged(count());
	QTabWidget::tabInserted(index);
}

/**
 * @brief TabWidget::tabRemoved
 */
void TabWidget::tabRemoved(int index) {
	Q_EMIT tabCountChanged(count());
	QTabWidget::tabRemoved(index);
}


