
#ifndef DIALOG_EXECUTE_COMMAND_H_
#define DIALOG_EXECUTE_COMMAND_H_

#include "Dialog.h"
#include "ui_DialogExecuteCommand.h"

#include <QStringList>

class DialogExecuteCommand : public Dialog {
	Q_OBJECT
public:
    DialogExecuteCommand(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogExecuteCommand() noexcept override = default;

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;

private Q_SLOTS:
	void on_buttonBox_accepted();

public:
    QString currentText() const;

private:
	Ui::DialogExecuteCommand ui;
    QStringList              history_;
    int                      historyIndex_ = 0;
};

#endif
