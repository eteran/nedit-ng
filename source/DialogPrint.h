
#ifndef DIALOG_PRINT_H_
#define DIALOG_PRINT_H_

#include "Dialog.h"
#include "ui_DialogPrint.h"

class DialogPrint : public Dialog {
	Q_OBJECT
public:
    DialogPrint(const QString &PrintFileName, const QString &jobName, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogPrint() override = default;

private Q_SLOTS:
	void on_spinCopies_valueChanged(int n);
	void on_editQueue_textChanged(const QString &text);
	void on_editHost_textChanged(const QString &text);
	void on_editCommand_textEdited(const QString &text);
	void on_buttonPrint_clicked();

private:
	void updatePrintCmd();

protected:
    void showEvent(QShowEvent *event) override;

public:
    static void LoadPrintPreferencesEx(bool lookForFlpr);

public:
	Ui::DialogPrint ui;
	QString PrintFileName_;
	QString PrintJobName_;
};

#endif


