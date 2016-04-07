
#ifndef DIALOG_WRAP_MARGIN_H_
#define DIALOG_WRAP_MARGIN_H_

#include <QDialog>
#include "ui_DialogWrapMargin.h"

class Document;

class DialogWrapMargin : public QDialog {
	Q_OBJECT
public:
	DialogWrapMargin(Document *window, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogWrapMargin();

private Q_SLOTS:
	void on_checkWrapAndFill_toggled(bool checked);
	void on_buttonBox_accepted();

public:
	Ui::DialogWrapMargin ui;
	Document *window_;
};

#endif
