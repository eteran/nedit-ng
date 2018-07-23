
#ifndef LANGUAGE_MODE_MODEL_H_
#define LANGUAGE_MODE_MODEL_H_

#include "LanguageMode.h"
#include <QAbstractItemModel>

class LanguageModeModel final : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit LanguageModeModel(QObject *parent = nullptr) ;
    ~LanguageModeModel() noexcept override = default;

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
    void addItem(const LanguageMode &languageMode);
    void clear();
    void moveItemUp(const QModelIndex &index);
    void moveItemDown(const QModelIndex &index);
    void deleteItem(const QModelIndex &index);
    bool updateItem(const QModelIndex &index, const LanguageMode &item);
    const LanguageMode *itemFromIndex(const QModelIndex &index) const;

private:
    QVector<LanguageMode> items_;
};


#endif
