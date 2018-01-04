
#ifndef DIALOG_ABOUT_H_
#define DIALOG_ABOUT_H_

#include "Dialog.h"
#include "ui_DialogAbout.h"

class DialogAbout : public Dialog {
	Q_OBJECT
public:
    DialogAbout(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogAbout() override = default;

public:
    static QString createInfoString();

private:
	Ui::DialogAbout ui;
};

#endif
