
#include "DocumentModel.h"
#include "Util/algorithm.h"

/**
 * @brief DocumentModel::LanguageModeModel
 * @param parent
 */
DocumentModel::DocumentModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief DocumentModel::index
 * @param row
 * @param column
 * @param parent
 * @return
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
 * @brief DocumentModel::parent
 * @param index
 * @return
 */
QModelIndex DocumentModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief DocumentModel::data
 * @param index
 * @param role
 * @return
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
 * @brief DocumentModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
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
 * @brief DocumentModel::columnCount
 * @param parent
 * @return
 */
int DocumentModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief DocumentModel::rowCount
 * @param parent
 * @return
 */
int DocumentModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief DocumentModel::addItem
 * @param style
 */
void DocumentModel::addItem(DocumentWidget *languageMode) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(languageMode);
	endInsertRows();
}

/**
 * @brief DocumentModel::setShowFullPath
 * @param showFullPath
 */
void DocumentModel::setShowFullPath(bool showFullPath) {
	showFullPath_ = showFullPath;

	static const QVector<int> roles = {Qt::DisplayRole};
	Q_EMIT dataChanged(index(0, 0), index(rowCount(), 0), roles);
}

/**
 * @brief DocumentModel::clear
 */
void DocumentModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief DocumentModel::moveItemUp
 * @param index
 */
void DocumentModel::moveItemUp(const QModelIndex &index) {
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
 * @brief DocumentModel::moveItemDown
 * @param index
 */
void DocumentModel::moveItemDown(const QModelIndex &index) {
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
 * @brief DocumentModel::deleteItem
 * @param index
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
 * @brief LanguageModeModel::itemFromIndex
 * @param index
 * @return
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
