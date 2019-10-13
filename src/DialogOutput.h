
#ifndef DIALOG_OUTPUT_H_
#define DIALOG_OUTPUT_H_

#include "Dialog.h"
#include "ui_DialogOutput.h"

class DialogOutput : public Dialog {
	Q_OBJECT

public:
	explicit DialogOutput(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogOutput() override = default;

public:
	void setText(const QString &text);

private:
	Ui::DialogOutput ui;
};

#endif
