
#ifndef DIALOG_REPEAT_H_
#define DIALOG_REPEAT_H_

#include <QDialog>
#include "ui_DialogRepeat.h"

class DialogRepeat : public QDialog {
	Q_OBJECT
public:
	DialogRepeat(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogRepeat();

private:
	Ui::DialogRepeat ui;
};

#endif
