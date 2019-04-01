
#ifndef DIALOG_PRINT_H_
#define DIALOG_PRINT_H_

#include "Dialog.h"
#include "ui_DialogPrint.h"

class DocumentWidget;
class QPrinter;

class DialogPrint final : public Dialog {
	Q_OBJECT
public:
	DialogPrint(QString contents, QString jobname, DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPrint() noexcept override = default;

private Q_SLOTS:
	void on_printers_currentIndexChanged(int index);

private:
	void buttonPrint_clicked();
	void connectSlots();

private:
	void print(QPrinter *printer);

protected:
	void showEvent(QShowEvent *event) override;

public:
	Ui::DialogPrint ui;
	DocumentWidget *document_;
	QString         contents_;
	QString         jobname_;
};

#endif


