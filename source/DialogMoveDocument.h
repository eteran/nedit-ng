
#ifndef DIALOG_MOVE_DOCUMENT_H_
#define DIALOG_MOVE_DOCUMENT_H_

#include "Dialog.h"
#include <QList>
#include "ui_DialogMoveDocument.h"

class MainWindow;

class DialogMoveDocument : public Dialog {
public:
	Q_OBJECT
public:
    DialogMoveDocument(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Q_NULLPTR);
    virtual ~DialogMoveDocument() override = default;
	
public:
    void addItem(MainWindow *window);
	void resetSelection();
	void setLabel(const QString &label);
	void setMultipleDocuments(bool value);
	int selectionIndex() const;
	bool moveAllSelected() const;

private:
	Ui::DialogMoveDocument ui;
    QList<MainWindow *>    windows_;
	
};

#endif
