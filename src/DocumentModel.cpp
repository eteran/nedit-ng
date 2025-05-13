
#include "DocumentModel.h"
#include "Util/algorithm.h"

/**
 * @brief Constructor for DocumentModel.
 *
 * @param parent Parent object, defaults to nullptr.
 */
DocumentModel::DocumentModel(QObject *parent)
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
QModelIndex DocumentModel::index(int row, int column, const QModelIndex &parent) const {

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return {};
	}

	if (row < 0) {
		return {};
	}

	return createIndex(row, column);
}

/**
 * @brief Returns the parent of a given index.
 *
 * @param index The index for which to get the parent.
 * @return The parent of the given index.
 */
QModelIndex DocumentModel::parent(const QModelIndex &index) const {
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
QVariant DocumentModel::data(const QModelIndex &index, int role) const {
	if (index.isValid()) {

		const DocumentWidget *document = items_[index.row()];

		if (role == Qt::DisplayRole) {
			switch (index.column()) {
			case 0:
				return showFullPath_ ? document->fullPath() : document->filename();
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
 * @return The header data for the specified section and orientation.
 */
QVariant DocumentModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
 * @return The number of columns in the model.
 */
int DocumentModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief Returns the number of rows in the model.
 *
 * @param parent The parent index, defaults to an invalid index.
 * @return The number of rows in the model.
 */
int DocumentModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief Adds a new item to the model.
 *
 * @param document The DocumentWidget to be added.
 */
void DocumentModel::addItem(DocumentWidget *document) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(document);
	endInsertRows();
}

/**
 * @brief Sets whether to show the full path of documents in the model.
 *
 * @param showFullPath If `true`, the full path of documents will be shown; otherwise,
 * only the filename will be displayed.
 */
void DocumentModel::setShowFullPath(bool showFullPath) {
	showFullPath_ = showFullPath;

	static const QVector<int> roles = {Qt::DisplayRole};
	Q_EMIT dataChanged(index(0, 0), index(rowCount(), 0), roles);
}

/**
 * @brief Clears all items from the model.
 */
void DocumentModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief Moves an item up in the model.
 *
 * @param index The index of the item to move up.
 */
void DocumentModel::moveItemUp(const QModelIndex &index) {
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
 * @brief Moves an item down in the model.
 *
 * @param index The index of the item to move down.
 */
void DocumentModel::moveItemDown(const QModelIndex &index) {
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
 * @brief Deletes an item from the model at the specified index.
 *
 * @param index The index of the item to delete.
 */
void DocumentModel::deleteItem(const QModelIndex &index) {
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
 * @brief Returns the DocumentWidget associated with a given index.
 *
 * @param index The index for which to retrieve the DocumentWidget.
 * @return The DocumentWidget if the index is valid and within bounds; otherwise, returns nullptr.
 */
DocumentWidget *DocumentModel::itemFromIndex(const QModelIndex &index) {
	if (index.isValid()) {
		const int row = index.row();
		if (row < rowCount()) {
			return items_[row];
		}
	}

	return nullptr;
}
