
#ifndef DIALOG_WINDOW_SIZE_H_
#define DIALOG_WINDOW_SIZE_H_

#include <QDialog>
#include "ui_DialogWindowSize.h"

class DialogWindowSize : public QDialog {
	Q_OBJECT
public:
	DialogWindowSize(QWidget *parent, Qt::WindowFlags f = 0);
    virtual ~DialogWindowSize() = default;

private:
    virtual void showEvent(QShowEvent *event) override;

private Q_SLOTS:
	void on_buttonBox_accepted();
	
private:
	Ui::DialogWindowSize ui;
};

#endif
