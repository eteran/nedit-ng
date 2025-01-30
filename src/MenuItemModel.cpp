
#include "MenuItemModel.h"
#include "Util/algorithm.h"

/**
 * @brief MenuItemModel::MenuItemModel
 * @param parent
 */
MenuItemModel::MenuItemModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief MenuItemModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex MenuItemModel::index(int row, int column, const QModelIndex &parent) const {

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return {};
	}

	if (row < 0) {
		return {};
	}

	return createIndex(row, column);
}

/**
 * @brief MenuItemModel::parent
 * @param index
 * @return
 */
QModelIndex MenuItemModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief MenuItemModel::data
 * @param index
 * @param role
 * @return
 */
QVariant MenuItemModel::data(const QModelIndex &index, int role) const {
	if (index.isValid()) {

		const MenuItem &item = items_[index.row()];

		if (role == Qt::DisplayRole) {
			switch (index.column()) {
			case 0:
				return item.name;
			default:
				return QVariant();
			}
		}
	}

	return QVariant();
}

/**
 * @brief MenuItemModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
 */
QVariant MenuItemModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Name");
		default:
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief MenuItemModel::columnCount
 * @param parent
 * @return
 */
int MenuItemModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief MenuItemModel::rowCount
 * @param parent
 * @return
 */
int MenuItemModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief MenuItemModel::addItem
 * @param style
 */
void MenuItemModel::addItem(const MenuItem &style) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(style);
	endInsertRows();
}

/**
 * @brief MenuItemModel::clear
 */
void MenuItemModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief MenuItemModel::moveItemUp
 * @param index
 */
void MenuItemModel::moveItemUp(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row > 0) {
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
			moveItem(items_, row, row - 1);
			endMoveRows();
		}
	}
}

/**
 * @brief MenuItemModel::moveItemDown
 * @param index
 */
void MenuItemModel::moveItemDown(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount() - 1) {
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
			moveItem(items_, row, row + 1);
			endMoveRows();
		}
	}
}

/**
 * @brief MenuItemModel::deleteItem
 * @param index
 */
void MenuItemModel::deleteItem(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			beginRemoveRows(QModelIndex(), row, row);
			items_.remove(row);
			endRemoveRows();
		}
	}
}

bool MenuItemModel::updateItem(const QModelIndex &index, const MenuItem &item) {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			items_[row]                     = item;
			static const QVector<int> roles = {Qt::DisplayRole};
			Q_EMIT dataChanged(index, index, roles);
			return true;
		}
	}

	return false;
}

const MenuItem *MenuItemModel::itemFromIndex(const QModelIndex &index) const {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			return &items_[row];
		}
	}

	return nullptr;
}
