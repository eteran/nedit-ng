
#ifndef DIALOG_MOVE_DOCUMENT_H_
#define DIALOG_MOVE_DOCUMENT_H_

#include <QDialog>
#include <QList>
#include "ui_DialogMoveDocument.h"

class MainWindow;

class DialogMoveDocument : public QDialog {
public:
	Q_OBJECT
public:
	DialogMoveDocument(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogMoveDocument();
	
public:
    void addItem(MainWindow *window);
	void resetSelection();
	void setLabel(const QString &label);
	void setMultipleDocuments(bool value);
	int selectionIndex() const;
	bool moveAllSelected() const;

private:
    virtual void showEvent(QShowEvent *event) override;
	
private Q_SLOTS:
	void on_buttonMove_clicked();

	
private:
	Ui::DialogMoveDocument ui;
    QList<MainWindow *>    windows_;
	
};

#endif
