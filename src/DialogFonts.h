
#ifndef DIALOG_FONTS_H_
#define DIALOG_FONTS_H_

#include "Dialog.h"
#include "ui_DialogFonts.h"

class DocumentWidget;

class DialogFonts final : public Dialog {
	Q_OBJECT

public:
	DialogFonts(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogFonts() override = default;

private:
	void buttonBox_clicked(QAbstractButton *button);
	void connectSlots();
	void updateFont();

private:
	Ui::DialogFonts ui;
	DocumentWidget *document_;
};

#endif
