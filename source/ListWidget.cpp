
#include "ListWidget.h"
#include <QMouseEvent>

ListWidget::ListWidget(QWidget *parent) : QListWidget(parent) {
}

ListWidget::~ListWidget() {
}

void ListWidget::mousePressEvent(QMouseEvent *event) {
	QModelIndex item = indexAt(event->pos());
	bool selected = selectionModel()->isSelected(indexAt(event->pos()));
	QListWidget::mousePressEvent(event);
	if ((item.row() == -1 && item.column() == -1) || selected) {
		clearSelection();
		const QModelIndex index;
		selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
	}
}
