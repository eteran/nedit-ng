
#ifndef DIALOG_EXECUTE_COMMAND_H_
#define DIALOG_EXECUTE_COMMAND_H_

#include "Dialog.h"
#include <QStringList>
#include "ui_DialogExecuteCommand.h"

class DialogExecuteCommand : public Dialog {
	Q_OBJECT
public:
    DialogExecuteCommand(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogExecuteCommand() override = default;

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;

private Q_SLOTS:
	void on_buttonBox_accepted();

public:
	Ui::DialogExecuteCommand ui;
	QStringList history_;
	int historyIndex_;
};

#endif
