
#include "HighlightStyleModel.h"
#include "Util/algorithm.h"

/**
 * @brief Constructor for the HighlightStyleModel class.
 *
 * @param parent The parent object for this model.
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
 * @brief Returns the parent index of the specified index.
 *
 * @param index The index for which to retrieve the parent.
 * @return An invalid index if the item has no parent, or the parent index of the item.
 */
QModelIndex HighlightStyleModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief Returns the data for the specified index and role.
 *
 * @param index The index for which to retrieve the data.
 * @param role The role for which to retrieve the data. This can be Qt::DisplayRole, Qt::UserRole, etc.
 * @return The data for the specified index and role, or an invalid QVariant if the index is not valid or the role is not recognized.
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
 * @brief Returns the header data for the specified section and orientation.
 *
 * @param section The section for which to retrieve the header data. This is typically the column index for horizontal headers.
 * @param orientation The orientation of the header, either Qt::Horizontal or Qt::Vertical.
 * @param role The role for which to retrieve the header data. This can be Qt::DisplayRole, Qt::ToolTipRole, etc.
 * @return The header data for the specified section and orientation, or an invalid QVariant if the section or role is not recognized.
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
 * @brief Returns the number of columns in the model.
 *
 * @param parent The parent index for which to retrieve the column count.
 * @return The number of columns in the model, which is always 1 in this case.
 */
int HighlightStyleModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief Returns the number of rows in the model.
 *
 * @param parent The parent index for which to retrieve the row count.
 * @return The number of rows in the model.
 */
int HighlightStyleModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief Adds a new item to the model.
 *
 * @param style The HighlightStyle object to add to the model.
 */
void HighlightStyleModel::addItem(const HighlightStyle &style) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(style);
	endInsertRows();
}

/**
 * @brief Clears all items from the model.
 */
void HighlightStyleModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief Moves the item at the specified index up in the model.
 *
 * @param index The index of the item to move up.
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
 * @brief Moves the item at the specified index down in the model.
 *
 * @param index The index of the item to move down.
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
 * @brief Deletes the item at the specified index from the model.
 *
 * @param index The index of the item to delete.
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
 * @brief Updates the item at the specified index with the provided HighlightStyle object.
 *
 * @param index The index of the item to update.
 * @param item The HighlightStyle object containing the new data for the item.
 * @return `true` if the item was successfully updated, `false` otherwise.
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
 * @brief Returns a pointer to the HighlightStyle item at the specified index.
 *
 * @param index The index of the item to retrieve.
 * @return The HighlightStyle item at the specified index, or nullptr if the index is invalid.
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
