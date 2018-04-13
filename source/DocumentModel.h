
#ifndef DOCUMENT_MODEL_H_
#define DOCUMENT_MODEL_H_

#include "DocumentWidget.h"
#include <QAbstractItemModel>

class DocumentModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit DocumentModel(QObject *parent = nullptr) ;
    ~DocumentModel() override = default;

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
    void addItem(DocumentWidget *languageMode);
    void clear();
    void moveItemUp(const QModelIndex &index);
    void moveItemDown(const QModelIndex &index);
    void deleteItem(const QModelIndex &index);
    DocumentWidget *itemFromIndex(const QModelIndex &index);
    void setShowFullPath(bool showFullPath);

private:
    QVector<DocumentWidget *> items_;
    bool showFullPath_ = false;
};


#endif
