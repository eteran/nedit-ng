
#ifndef DIALOG_WRAP_MARGIN_H_
#define DIALOG_WRAP_MARGIN_H_

#include "Dialog.h"
#include "ui_DialogWrapMargin.h"

class DocumentWidget;

class DialogWrapMargin : public Dialog {
	Q_OBJECT
public:
    DialogWrapMargin(DocumentWidget *document, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogWrapMargin() = default;

private Q_SLOTS:
	void on_checkWrapAndFill_toggled(bool checked);
	void on_buttonBox_accepted();

public:
	Ui::DialogWrapMargin ui;
    DocumentWidget *document_;
};

#endif
