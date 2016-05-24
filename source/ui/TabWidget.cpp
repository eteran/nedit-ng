
#include "TabWidget.h"
#include <QTabBar>
#include <QDebug>
#include <QMouseEvent>

//------------------------------------------------------------------------------
// Name: TabWidget
//------------------------------------------------------------------------------
TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent), hideSingleTab_(true) {
}

//------------------------------------------------------------------------------
// Name: tabInserted
//------------------------------------------------------------------------------
void TabWidget::tabInserted(int index) {

	Q_UNUSED(index);

	if(hideSingleTab_) {
		tabBar()->setVisible(count() >= 2);
	}

	Q_EMIT countChanged(count());
}

//------------------------------------------------------------------------------
// Name: tabRemoved
//------------------------------------------------------------------------------
void TabWidget::tabRemoved(int index) {

	Q_UNUSED(index);

	if(hideSingleTab_) {
		tabBar()->setVisible(count() >= 2);
	}

	Q_EMIT countChanged(count());
}

//------------------------------------------------------------------------------
// Name: mousePressEvent
//------------------------------------------------------------------------------
void TabWidget::mousePressEvent(QMouseEvent *event) {
	if(event->button() != Qt::RightButton) {
		return;
	}

	if(contextMenuPolicy() == Qt::CustomContextMenu) {
		const int tab = tabBar()->tabAt(event->pos());
		if(tab != -1) {
			Q_EMIT customContextMenuRequested(tab, event->pos());
		}
	}
}
