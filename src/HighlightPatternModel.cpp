
#include "HighlightPatternModel.h"
#include "Util/algorithm.h"

/**
 * @brief HighlightPatternModel::HighlightPatternModel
 * @param parent
 */
HighlightPatternModel::HighlightPatternModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief HighlightPatternModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex HighlightPatternModel::index(int row, int column, const QModelIndex &parent) const {

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return {};
	}

	if (row < 0) {
		return {};
	}

	return createIndex(row, column);
}

/**
 * @brief HighlightPatternModel::parent
 * @param index
 * @return
 */
QModelIndex HighlightPatternModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief HighlightPatternModel::data
 * @param index
 * @param role
 * @return
 */
QVariant HighlightPatternModel::data(const QModelIndex &index, int role) const {
	if (index.isValid()) {

		const HighlightPattern &item = items_[index.row()];

		if (role == Qt::DisplayRole) {
			switch (index.column()) {
			case 0:
				return item.name;
			default:
				return QVariant();
			}
		} else if (role == Qt::UserRole) {
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief HighlightPatternModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
 */
QVariant HighlightPatternModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
 * @brief HighlightPatternModel::columnCount
 * @param parent
 * @return
 */
int HighlightPatternModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief HighlightPatternModel::rowCount
 * @param parent
 * @return
 */
int HighlightPatternModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief HighlightPatternModel::addItem
 * @param style
 */
void HighlightPatternModel::addItem(const HighlightPattern &style) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(style);
	endInsertRows();
}

/**
 * @brief HighlightPatternModel::clear
 */
void HighlightPatternModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief HighlightPatternModel::moveItemUp
 * @param index
 */
void HighlightPatternModel::moveItemUp(const QModelIndex &index) {
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
 * @brief HighlightPatternModel::moveItemDown
 * @param index
 */
void HighlightPatternModel::moveItemDown(const QModelIndex &index) {
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
 * @brief HighlightPatternModel::deleteItem
 * @param index
 */
void HighlightPatternModel::deleteItem(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			beginRemoveRows(QModelIndex(), row, row);
			items_.remove(row);
			endRemoveRows();
		}
	}
}

/**
 * @brief HighlightPatternModel::updateItem
 * @param index
 * @param item
 * @return
 */
bool HighlightPatternModel::updateItem(const QModelIndex &index, const HighlightPattern &item) {
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

/**
 * @brief HighlightPatternModel::itemFromIndex
 * @param index
 * @return
 */
const HighlightPattern *HighlightPatternModel::itemFromIndex(const QModelIndex &index) const {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			return &items_[row];
		}
	}

	return nullptr;
}
