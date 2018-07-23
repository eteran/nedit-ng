
#ifndef DIALOG_WINDOW_SIZE_H_
#define DIALOG_WINDOW_SIZE_H_

#include "Dialog.h"
#include "ui_DialogWindowSize.h"

class DialogWindowSize final : public Dialog {
	Q_OBJECT
public:
    DialogWindowSize(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogWindowSize() noexcept override = default;

private Q_SLOTS:
	void on_buttonBox_accepted();
	
private:
	Ui::DialogWindowSize ui;
};

#endif
