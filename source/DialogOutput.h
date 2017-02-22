
#ifndef DIALOG_OUTPUT_H_
#define DIALOG_OUTPUT_H_

#include <QDialog>
#include "ui_DialogOutput.h"

class DialogOutput : public QDialog {
	Q_OBJECT

public:
	DialogOutput(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogOutput() override;

private:
    virtual void showEvent(QShowEvent *event) override;

public Q_SLOTS:
	void setText(const QString &text);

private:
	Ui::DialogOutput ui;
};

#endif
