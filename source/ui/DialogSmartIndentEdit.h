
#ifndef DIALOG_SMART_INDENT_EDIT_H_
#define DIALOG_SMART_INDENT_EDIT_H_

#include <QDialog>
#include "ui_DialogSmartIndentEdit.h"

class DialogSmartIndentEdit : public QDialog {
	Q_OBJECT
public:
	DialogSmartIndentEdit(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogSmartIndentEdit();

private:
	bool checkSmartIndentCommonDialogData();
	bool updateSmartIndentCommonData();
	QString ensureNewline(const QString &string);

private Q_SLOTS:
	void on_buttonOK_clicked();
	void on_buttonApply_clicked();
	void on_buttonCheck_clicked();
	void on_buttonRestore_clicked();

public:
	Ui::DialogSmartIndentEdit ui;
};

#endif
