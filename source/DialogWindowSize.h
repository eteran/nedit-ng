
#ifndef DIALOG_WINDOW_SIZE_H_
#define DIALOG_WINDOW_SIZE_H_

#include "Dialog.h"
#include "ui_DialogWindowSize.h"

class DialogWindowSize : public Dialog {
	Q_OBJECT
public:
    DialogWindowSize(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~DialogWindowSize() = default;

private Q_SLOTS:
	void on_buttonBox_accepted();
	
private:
	Ui::DialogWindowSize ui;
};

#endif
