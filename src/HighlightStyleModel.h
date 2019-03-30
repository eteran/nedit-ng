
#ifndef HIGHLIGHT_STYLE_MODEL_H_
#define HIGHLIGHT_STYLE_MODEL_H_

#include "HighlightStyle.h"
#include <QAbstractItemModel>

class HighlightStyleModel final : public QAbstractItemModel {
	Q_OBJECT

public:
	explicit HighlightStyleModel(QObject *parent = nullptr) ;
	~HighlightStyleModel() noexcept override = default;

public:
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	void addItem(const HighlightStyle &style);
	void clear();
	void moveItemUp(const QModelIndex &index);
	void moveItemDown(const QModelIndex &index);
	void deleteItem(const QModelIndex &index);
	bool updateItem(const QModelIndex &index, const HighlightStyle &item);
	const HighlightStyle *itemFromIndex(const QModelIndex &index) const;

private:
	QVector<HighlightStyle> items_;
};

#endif
