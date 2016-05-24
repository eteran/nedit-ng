
#ifndef ABOUT_H_
#define ABOUT_H_

#include <QDialog>
#include "ui_DialogAbout.h"

class DialogAbout : public QDialog {
	Q_OBJECT
public:
	DialogAbout(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogAbout() override;

private:
	Ui::DialogAbout ui;
};

#endif
