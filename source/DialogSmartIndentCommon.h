
#ifndef DIALOG_SMART_INDENT_COMMON_H_
#define DIALOG_SMART_INDENT_COMMON_H_

#include "Dialog.h"
#include "ui_DialogSmartIndentCommon.h"

class DialogSmartIndentCommon final : public Dialog {
	Q_OBJECT
public:
    DialogSmartIndentCommon(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogSmartIndentCommon() noexcept override = default;

private:
	bool checkSmartIndentCommonDialogData();
	bool updateSmartIndentCommonData();

private Q_SLOTS:
	void on_buttonOK_clicked();
	void on_buttonApply_clicked();
	void on_buttonCheck_clicked();
	void on_buttonRestore_clicked();

public:
	Ui::DialogSmartIndentCommon ui;
};

#endif
