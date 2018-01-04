
#ifndef DIALOG_MOVE_DOCUMENT_H_
#define DIALOG_MOVE_DOCUMENT_H_

#include "Dialog.h"
#include "ui_DialogMoveDocument.h"

#include <vector>

class MainWindow;

class DialogMoveDocument : public Dialog {
public:
	Q_OBJECT
public:
    DialogMoveDocument(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogMoveDocument() override = default;
	
public:
    void addItem(MainWindow *window);
	void resetSelection();
	void setLabel(const QString &label);
	void setMultipleDocuments(bool value);
	int selectionIndex() const;
	bool moveAllSelected() const;

private:
    Ui::DialogMoveDocument    ui;
    std::vector<MainWindow *> windows_;
	
};

#endif
