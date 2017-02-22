
#ifndef DIALOG_ABOUT_H_
#define DIALOG_ABOUT_H_

#include <QDialog>
#include "ui_DialogAbout.h"

class DialogAbout : public QDialog {
	Q_OBJECT
public:
	DialogAbout(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogAbout() = default;

public:
    virtual void showEvent(QShowEvent *event) override;

public:
    static QString createInfoString();

private:
	Ui::DialogAbout ui;
};

#endif
