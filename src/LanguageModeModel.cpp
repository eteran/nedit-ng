
#include "LanguageModeModel.h"
#include "Util/algorithm.h"

/**
 * @brief LanguageModeModel::LanguageModeModel
 * @param parent
 */
LanguageModeModel::LanguageModeModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief LanguageModeModel::index
 * @param row
 * @param column
 * @param parent
 * @return
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
 * @brief LanguageModeModel::parent
 * @param index
 * @return
 */
QModelIndex LanguageModeModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return {};
}

/**
 * @brief LanguageModeModel::data
 * @param index
 * @param role
 * @return
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
 * @brief LanguageModeModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
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
 * @brief LanguageModeModel::columnCount
 * @param parent
 * @return
 */
int LanguageModeModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 1;
}

/**
 * @brief LanguageModeModel::rowCount
 * @param parent
 * @return
 */
int LanguageModeModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

/**
 * @brief LanguageModeModel::addItem
 * @param style
 */
void LanguageModeModel::addItem(const LanguageMode &languageMode) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	items_.push_back(languageMode);
	endInsertRows();
}

/**
 * @brief LanguageModeModel::clear
 */
void LanguageModeModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}

/**
 * @brief LanguageModeModel::moveItemUp
 * @param index
 */
void LanguageModeModel::moveItemUp(const QModelIndex &index) {
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
 * @brief LanguageModeModel::moveItemDown
 * @param index
 */
void LanguageModeModel::moveItemDown(const QModelIndex &index) {
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
 * @brief LanguageModeModel::deleteItem
 * @param index
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
 * @brief LanguageModeModel::itemFromIndex
 * @param index
 * @return
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
