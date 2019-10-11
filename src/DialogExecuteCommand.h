
#ifndef DIALOG_EXECUTE_COMMAND_H_
#define DIALOG_EXECUTE_COMMAND_H_

#include "Dialog.h"
#include "ui_DialogExecuteCommand.h"

#include <QStringList>

class DialogExecuteCommand final : public Dialog {
	Q_OBJECT
public:
	DialogExecuteCommand(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogExecuteCommand() override = default;

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;

public:
	QString currentText() const;
	void addHistoryItem(const QString &s);

private:
	Ui::DialogExecuteCommand ui;
	QStringList              history_;
	int                      historyIndex_ = 0;
};

#endif
