
#include "MenuItemModel.h"

/**
 * @brief MenuItemModel::MenuItemModel
 * @param parent
 */
MenuItemModel::MenuItemModel(QObject *parent) : QAbstractItemModel(parent) {
}

/**
 * @brief MenuItemModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex MenuItemModel::index(int row, int column, const QModelIndex &parent) const {
    Q_UNUSED(parent);

    if(row >= rowCount(parent) || column >= columnCount(parent)) {
        return QModelIndex();
    }

    if(row >= 0) {
        return createIndex(row, column, const_cast<MenuItem *>(&items_[row]));
    } else {
        return createIndex(row, column);
    }
}

/**
 * @brief MenuItemModel::parent
 * @param index
 * @return
 */
QModelIndex MenuItemModel::parent(const QModelIndex &index) const {
    Q_UNUSED(index);
    return QModelIndex();
}

/**
 * @brief MenuItemModel::data
 * @param index
 * @param role
 * @return
 */
QVariant MenuItemModel::data(const QModelIndex &index, int role) const {
    if(index.isValid()) {

        const MenuItem &item = items_[index.row()];

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
 * @brief MenuItemModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
 */
QVariant MenuItemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch(section) {
        case 0:
            return tr("Name");
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
    Q_UNUSED(parent);
    return 1;
}

/**
 * @brief MenuItemModel::rowCount
 * @param parent
 * @return
 */
int MenuItemModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
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
    beginRemoveRows(QModelIndex(), 0, rowCount());
    items_.clear();
    endRemoveRows();
}

/**
 * @brief MenuItemModel::moveItemUp
 * @param index
 */
void MenuItemModel::moveItemUp(const QModelIndex &index) {
    if(index.isValid()) {
        int row = index.row();
        if(row > 0) {
            beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
            items_.move(row, row - 1);
            endMoveRows();
        }
    }
}

/**
 * @brief MenuItemModel::moveItemDown
 * @param index
 */
void MenuItemModel::moveItemDown(const QModelIndex &index) {
    if(index.isValid()) {
        int row = index.row();
        if(row < rowCount() - 1) {
            beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
            items_.move(row, row + 1);
            endMoveRows();
        }
    }
}

/**
 * @brief MenuItemModel::deleteItem
 * @param index
 */
void MenuItemModel::deleteItem(const QModelIndex &index) {
    if(index.isValid()) {
        int row = index.row();
        if(row < rowCount()) {
            beginRemoveRows(QModelIndex(), row, row);
            items_.remove(row);
            endRemoveRows();
        }
    }
}
