
#ifndef DIALOG_SMART_INDENT_COMMON_H_
#define DIALOG_SMART_INDENT_COMMON_H_

#include <QDialog>
#include "ui_DialogSmartIndentCommon.h"

class DialogSmartIndentCommon : public QDialog {
	Q_OBJECT
public:
	DialogSmartIndentCommon(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogSmartIndentCommon();

private:
    virtual void showEvent(QShowEvent *event) override;

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
	Ui::DialogSmartIndentCommon ui;
};

#endif
