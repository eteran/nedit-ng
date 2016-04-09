
#ifndef DIALOG_EXECUTE_COMMAND_H_
#define DIALOG_EXECUTE_COMMAND_H_

#include <QDialog>
#include <QStringList>
#include "ui_DialogExecuteCommand.h"

class Document;

class DialogExecuteCommand : public QDialog {
	Q_OBJECT
public:
	DialogExecuteCommand(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogExecuteCommand();
	
protected:
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;
	
private Q_SLOTS:
	void on_buttonBox_accepted();

public:
	Ui::DialogExecuteCommand ui;
	QStringList history_;
	int historyIndex_;
};

#endif
