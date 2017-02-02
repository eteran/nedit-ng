
#include "TabWidget.h"
#include <QTabBar>
#include <QDebug>
#include <QMouseEvent>

// TODO(eteran): when we switch to Qt5, leverage the tabBarAutoHide property
// instead of the manually rolled implementation we have here

//------------------------------------------------------------------------------
// Name: TabWidget
//------------------------------------------------------------------------------
TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent), hideSingleTab_(true) {
}

//------------------------------------------------------------------------------
// Name: hideSingleTab
//------------------------------------------------------------------------------
bool TabWidget::hideSingleTab() const {
    return hideSingleTab_;
}

//------------------------------------------------------------------------------
// Name: setHideSingleTab
//------------------------------------------------------------------------------
void TabWidget::setHideSingleTab(bool value) {
    hideSingleTab_ = value;
    if(hideSingleTab_) {
        tabBar()->setVisible(count() >= 2);
    } else {
        tabBar()->setVisible(true);
    }
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

QTabBar *TabWidget::getTabBar() const {
	return tabBar();
}
