
#include "LanguageModeModel.h"
#include "Util/algorithm.h"

/**
 * @brief Constructor for LanguageModeModel.
 *
 * @param parent The parent object for this model.
 */
LanguageModeModel::LanguageModeModel(QObject *parent)
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
QModelIndex LanguageModeModel::index(int row, int column, const QModelIndex &parent) const {

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
 * @param index The index for which to retrieve the parent index.
 * @return The parent index of the specified index, or an invalid index if the index has no parent.
 */
QModelIndex LanguageModeModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief Returns the data for a given index and role.
 *
 * @param index The index for which to retrieve data.
 * @param role The role for which to retrieve data, such as Qt::DisplayRole or Qt::UserRole.
 * @return The data for the specified index and role.
 */
QVariant LanguageModeModel::data(const QModelIndex &index, int role) const {
	if (index.isValid()) {

		const LanguageMode &item = items_[index.row()];

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
 * @brief Returns the header data for a specific section and orientation.
 *
 * @param section The section number for which to retrieve header data.
 * @param orientation The orientation of the header, either Qt::Horizontal or Qt::Vertical.
 * @param role The role for which to retrieve header data, such as Qt::DisplayRole.
 * @return The header data for the specified section and orientation, or an invalid QVariant if the section or role is not supported.
 */
QVariant LanguageModeModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
 * @param parent The parent index, defaults to an invalid index.
 * @return The number of columns in the model, which is always 1 for this model.
 */
int LanguageModeModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief Returns the number of rows in the model.
 *
 * @param parent The parent index, defaults to an invalid index.
 * @return The number of rows in the model.
 */
int LanguageModeModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief Adds a new item to the model.
 *
 * @param languageMode The LanguageMode item to add to the model.
 */
void LanguageModeModel::addItem(const LanguageMode &languageMode) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(languageMode);
	endInsertRows();
}

/**
 * @brief Clears all items from the model.
 */
void LanguageModeModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief Moves the item at the specified index up in the model.
 *
 * @param index The index of the item to move up. If the index is invalid or the item is already at the top, no action is taken.
 */
void LanguageModeModel::moveItemUp(const QModelIndex &index) {
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
void LanguageModeModel::moveItemDown(const QModelIndex &index) {
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
 * @param index The index of the item to delete. If the index is invalid or out of range, no action is taken.
 */
void LanguageModeModel::deleteItem(const QModelIndex &index) {
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
 * @brief Updates the item at the specified index with a new LanguageMode item.
 *
 * @param index The index of the item to update.
 * @param item The new LanguageMode item to set at the specified index.
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool LanguageModeModel::updateItem(const QModelIndex &index, const LanguageMode &item) {
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
 * @brief Returns the LanguageMode item at the specified index.
 *
 * @param index The index for which to retrieve the LanguageMode item.
 * @return A pointer to the LanguageMode item at the specified index, or `nullptr` if the index is invalid or out of range.
 */
const LanguageMode *LanguageModeModel::itemFromIndex(const QModelIndex &index) const {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			return &items_[row];
		}
	}

	return nullptr;
}
