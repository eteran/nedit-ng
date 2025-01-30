
#ifndef MENU_ITEM_MODEL_H_
#define MENU_ITEM_MODEL_H_

#include "MenuItem.h"

#include <QAbstractItemModel>

class MenuItemModel final : public QAbstractItemModel {
	Q_OBJECT

public:
	explicit MenuItemModel(QObject *parent = nullptr);
	~MenuItemModel() override = default;

public:
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	void addItem(const MenuItem &style);
	void clear();
	void moveItemUp(const QModelIndex &index);
	void moveItemDown(const QModelIndex &index);
	void deleteItem(const QModelIndex &index);
	bool updateItem(const QModelIndex &index, const MenuItem &item);
	const MenuItem *itemFromIndex(const QModelIndex &index) const;

private:
	QVector<MenuItem> items_;
};

#endif
