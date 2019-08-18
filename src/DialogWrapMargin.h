
#ifndef DIALOG_WRAP_MARGIN_H_
#define DIALOG_WRAP_MARGIN_H_

#include "Dialog.h"
#include "ui_DialogWrapMargin.h"

class DocumentWidget;

class DialogWrapMargin final : public Dialog {
	Q_OBJECT
public:
	explicit DialogWrapMargin(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogWrapMargin() override = default;

private Q_SLOTS:
	void on_checkWrapAndFill_toggled(bool checked);
	void on_buttonBox_accepted();

private:
	Ui::DialogWrapMargin ui;
	DocumentWidget *document_;
};

#endif
