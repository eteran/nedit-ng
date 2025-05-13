
#include "HighlightStyleModel.h"
#include "Util/algorithm.h"

/**
 * @brief
 *
 * @param parent
 */
HighlightStyleModel::HighlightStyleModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief Returns the index of the item at the specified row and column in the model.
 *
 * @param row The row of the item to retrieve.
 * @param column The column of the item to retrieve.
 * @param parent The parent index of the item to retrieve. If the item has no parent, this should be an invalid index.
 * @return The index of the item at the specified row and column, or an invalid index if the row or column is out of bounds.
 */
QModelIndex HighlightStyleModel::index(int row, int column, const QModelIndex &parent) const {

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return {};
	}

	if (row < 0) {
		return {};
	}

	return createIndex(row, column);
}

/**
 * @brief
 *
 * @param index
 * @return
 */
QModelIndex HighlightStyleModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief
 *
 * @param index
 * @param role
 * @return
 */
QVariant HighlightStyleModel::data(const QModelIndex &index, int role) const {
	if (index.isValid()) {

		const HighlightStyle &item = items_[index.row()];

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
 * @brief
 *
 * @param section
 * @param orientation
 * @param role
 * @return
 */
QVariant HighlightStyleModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
 * @brief
 *
 * @param parent
 * @return
 */
int HighlightStyleModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief
 *
 * @param parent
 * @return
 */
int HighlightStyleModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief
 *
 * @param style
 */
void HighlightStyleModel::addItem(const HighlightStyle &style) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(style);
	endInsertRows();
}

/**
 * @brief
 */
void HighlightStyleModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief
 *
 * @param index
 */
void HighlightStyleModel::moveItemUp(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row > 0) {
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
			MoveItem(items_, row, row - 1);
			endMoveRows();
		}
	}
}

/**
 * @brief
 *
 * @param index
 */
void HighlightStyleModel::moveItemDown(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount() - 1) {
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
			MoveItem(items_, row, row + 1);
			endMoveRows();
		}
	}
}

/**
 * @brief
 *
 * @param index
 */
void HighlightStyleModel::deleteItem(const QModelIndex &index) {
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
 * @brief
 *
 * @param index
 * @param item
 * @return
 */
bool HighlightStyleModel::updateItem(const QModelIndex &index, const HighlightStyle &item) {
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
 * @brief
 *
 * @param index
 * @return
 */
const HighlightStyle *HighlightStyleModel::itemFromIndex(const QModelIndex &index) const {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			return &items_[row];
		}
	}

	return nullptr;
}
