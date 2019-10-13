
#ifndef DIALOG_WINDOW_SIZE_H_
#define DIALOG_WINDOW_SIZE_H_

#include "Dialog.h"
#include "ui_DialogWindowSize.h"

class DialogWindowSize final : public Dialog {
	Q_OBJECT
public:
	DialogWindowSize(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogWindowSize() override = default;

private:
	void buttonBox_accepted();
	void connectSlots();

private:
	Ui::DialogWindowSize ui;
};

#endif
