
#ifndef DIALOG_OUTPUT_H_
#define DIALOG_OUTPUT_H_

#include "Dialog.h"
#include "ui_DialogOutput.h"

class DialogOutput : public Dialog {
	Q_OBJECT

public:
	DialogOutput(QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogOutput() = default;

public Q_SLOTS:
	void setText(const QString &text);

private:
	Ui::DialogOutput ui;
};

#endif
