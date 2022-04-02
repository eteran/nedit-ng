
#ifndef DIALOG_WRAP_MARGIN_H_
#define DIALOG_WRAP_MARGIN_H_

#include "Dialog.h"
#include "ui_DialogWrapMargin.h"
#include <QPointer>

class DocumentWidget;

class DialogWrapMargin final : public Dialog {
	Q_OBJECT
public:
	explicit DialogWrapMargin(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogWrapMargin() override = default;

private:
	void buttonBox_accepted();
	void checkWrapAndFill_toggled(bool checked);
	void connectSlots();

private:
	Ui::DialogWrapMargin ui;
	QPointer<DocumentWidget> document_;
};

#endif
