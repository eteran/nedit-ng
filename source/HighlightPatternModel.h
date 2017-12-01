
#ifndef HIGHLIGHT_PATTERN_MODEL_H_
#define HIGHLIGHT_PATTERN_MODEL_H_

#include "HighlightPattern.h"
#include <QAbstractItemModel>

class HighlightPatternModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit HighlightPatternModel(QObject *parent = nullptr) ;
    ~HighlightPatternModel() override = default;

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
    void addItem(const HighlightPattern &style);
    void clear();
    void moveItemUp(const QModelIndex &index);
    void moveItemDown(const QModelIndex &index);
    void deleteItem(const QModelIndex &index);
    bool updateItem(const QModelIndex &index, const HighlightPattern &item);
    const HighlightPattern *itemFromIndex(const QModelIndex &index) const;

private:
    QVector<HighlightPattern> items_;
};

#endif
