
#ifndef DIALOG_SMART_INDENT_H_
#define DIALOG_SMART_INDENT_H_

#include <QDialog>
#include "ui_DialogSmartIndent.h"

class DialogSmartIndent : public QDialog {
	Q_OBJECT
public:
	DialogSmartIndent(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogSmartIndent();

public:
	Ui::DialogSmartIndent ui;
};

#endif
