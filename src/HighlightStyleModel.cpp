
#include "HighlightStyleModel.h"
#include "Util/algorithm.h"

/**
 * @brief HighlightStyleModel::HighlightStyleModel
 * @param parent
 */
HighlightStyleModel::HighlightStyleModel(QObject *parent) : QAbstractItemModel(parent) {
}

/**
 * @brief HighlightStyleModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex HighlightStyleModel::index(int row, int column, const QModelIndex &parent) const {

	if(row >= rowCount(parent) || column >= columnCount(parent)) {
		return {};
	}

	if(row < 0) {
		return {};
	}

	return createIndex(row, column);
}

/**
 * @brief HighlightStyleModel::parent
 * @param index
 * @return
 */
QModelIndex HighlightStyleModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return {};
}

/**
 * @brief HighlightStyleModel::data
 * @param index
 * @param role
 * @return
 */
QVariant HighlightStyleModel::data(const QModelIndex &index, int role) const {
	if(index.isValid()) {

		const HighlightStyle &item = items_[index.row()];

		if(role == Qt::DisplayRole) {
			switch(index.column()) {
			case 0:
				return item.name;
			}
		} else if(role == Qt::UserRole) {
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief HighlightStyleModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
 */
QVariant HighlightStyleModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("Name");
		}
	}

	return QVariant();
}

/**
 * @brief HighlightStyleModel::columnCount
 * @param parent
 * @return
 */
int HighlightStyleModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 1;
}

/**
 * @brief HighlightStyleModel::rowCount
 * @param parent
 * @return
 */
int HighlightStyleModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return items_.size();
}

/**
 * @brief HighlightStyleModel::addItem
 * @param style
 */
void HighlightStyleModel::addItem(const HighlightStyle &style) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(style);
	endInsertRows();
}

/**
 * @brief HighlightStyleModel::clear
 */
void HighlightStyleModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief HighlightStyleModel::moveItemUp
 * @param index
 */
void HighlightStyleModel::moveItemUp(const QModelIndex &index) {
	if(index.isValid()) {
		int row = index.row();
		if(row > 0) {
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
			moveItem(items_, row, row - 1);
			endMoveRows();
		}
	}
}

/**
 * @brief HighlightStyleModel::moveItemDown
 * @param index
 */
void HighlightStyleModel::moveItemDown(const QModelIndex &index) {
	if(index.isValid()) {
		int row = index.row();
		if(row < rowCount() - 1) {
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
			moveItem(items_, row, row + 1);
			endMoveRows();
		}
	}
}

/**
 * @brief HighlightStyleModel::deleteItem
 * @param index
 */
void HighlightStyleModel::deleteItem(const QModelIndex &index) {
	if(index.isValid()) {
		int row = index.row();
		if(row < rowCount()) {
			beginRemoveRows(QModelIndex(), row, row);
			items_.remove(row);
			endRemoveRows();
		}
	}
}

/**
 * @brief HighlightStyleModel::updateItem
 * @param index
 * @param item
 * @return
 */
bool HighlightStyleModel::updateItem(const QModelIndex &index, const HighlightStyle &item) {
	if(index.isValid()) {
		int row = index.row();
		if(row < rowCount()) {
			items_[row] = item;
			static const QVector<int> roles = {Qt::DisplayRole};
			Q_EMIT dataChanged(index, index, roles);
			return true;
		}
	}

	return false;
}

/**
 * @brief HighlightStyleModel::itemFromIndex
 * @param index
 * @return
 */
const HighlightStyle *HighlightStyleModel::itemFromIndex(const QModelIndex &index) const {
	if(index.isValid()) {
		int row = index.row();
		if(row < rowCount()) {
			return &items_[row];
		}
	}

	return nullptr;
}

