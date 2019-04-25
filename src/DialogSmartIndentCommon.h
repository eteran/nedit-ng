
#ifndef DIALOG_SMART_INDENT_COMMON_H_
#define DIALOG_SMART_INDENT_COMMON_H_

#include "Dialog.h"
#include "ui_DialogSmartIndentCommon.h"

class DialogSmartIndentCommon final : public Dialog {
	Q_OBJECT
public:
	DialogSmartIndentCommon(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogSmartIndentCommon() override = default;

private:
	bool checkSmartIndentCommonDialogData();
	bool updateSmartIndentCommonData();

private:
	void buttonOK_clicked();
	void buttonApply_clicked();
	void buttonCheck_clicked();
	void buttonRestore_clicked();
	void connectSlots();

public:
	Ui::DialogSmartIndentCommon ui;
};

#endif
