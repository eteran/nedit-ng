
#ifndef DIALOG_PRINT_H_
#define DIALOG_PRINT_H_

#include "Dialog.h"
#include "ui_DialogPrint.h"

#include <QPointer>

class DocumentWidget;
class QPrinter;

class DialogPrint final : public Dialog {
	Q_OBJECT
public:
	DialogPrint(QString contents, QString jobName, DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPrint() override = default;

private Q_SLOTS:
	void on_printers_currentIndexChanged(int index);

private:
	void buttonPrint_clicked();
	void connectSlots();

private:
	void print(QPrinter *printer) const;

protected:
	void showEvent(QShowEvent *event) override;

public:
	Ui::DialogPrint ui;
	QPointer<DocumentWidget> document_;
	QString contents_;
	QString jobName_;
};

#endif
