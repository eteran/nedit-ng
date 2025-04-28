
#include "HighlightPatternModel.h"
#include "Util/algorithm.h"

/**
 * @brief Constructor for the HighlightPatternModel class.
 *
 * @param parent
 */
HighlightPatternModel::HighlightPatternModel(QObject *parent)
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
 * @brief Returns the parent index of the specified index in the model.
 *
 * @param index The index for which to retrieve the parent index. If the index has no parent, this should be an invalid index.
 * @return The parent index of the specified index, or an invalid index if the index has no parent.
 */
QModelIndex HighlightPatternModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief Returns the data for the specified index and role in the model.
 *
 * @param index The index for which to retrieve the data.
 * @param role The role for which to retrieve the data.
 * @return The data for the specified index and role, or an invalid QVariant if the index is not valid or the role is not supported.
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
 * @brief Returns the header data for the specified section, orientation, and role in the model.
 *
 * @param section The section for which to retrieve the header data. This is typically the column index for horizontal headers.
 * @param orientation The orientation of the header, either horizontal or vertical.
 * @param role The role for which to retrieve the header data. This is typically Qt::DisplayRole for display text.
 * @return The header data for the specified section, orientation, and role, or an invalid QVariant if the section or role is not supported.
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
 * @brief Returns the number of columns in the model.
 *
 * @param parent The parent index for which to retrieve the column count. This is typically an invalid index for top-level models.
 * @return The number of columns in the model, which is always 1 for this model.
 */
int HighlightPatternModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief Returns the number of rows in the model.
 *
 * @param parent The parent index for which to retrieve the row count. This is typically an invalid index for top-level models.
 * @return The number of rows in the model.
 */
int HighlightPatternModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief Adds a new item to the model.
 *
 * @param pattern The HighlightPattern to add to the model.
 */
void HighlightPatternModel::addItem(const HighlightPattern &pattern) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(pattern);
	endInsertRows();
}

/**
 * @brief Clears all items from the model.
 */
void HighlightPatternModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief Moves the item at the specified index up in the model.
 *
 * @param index The index of the item to move up. If the index is invalid or the item is already at the top, no action is taken.
 */
void HighlightPatternModel::moveItemUp(const QModelIndex &index) {
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
 * @brief Moves the item at the specified index down in the model.
 *
 * @param index The index of the item to move down. If the index is invalid or the item is already at the bottom, no action is taken.
 */
void HighlightPatternModel::moveItemDown(const QModelIndex &index) {
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
 * @brief Deletes the item at the specified index from the model.
 *
 * @param index The index of the item to delete. If the index is invalid or out of bounds, no action is taken.
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
 * @brief Updates the item at the specified index with the provided HighlightPattern.
 *
 * @param index The index of the item to update. If the index is invalid or out of bounds, no action is taken.
 * @param item The HighlightPattern to update the item with. This replaces the existing item at the specified index.
 * @return `true` if the item was successfully updated, `false` if the index was invalid or out of bounds.
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
 * @brief Returns the HighlightPattern item associated with the specified index.
 *
 * @param index The index for which to retrieve the HighlightPattern item. This should be a valid index within the model.
 * @return The HighlightPattern item associated with the specified index, or `nullptr` if the index is invalid or out of bounds.
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
