
#ifndef DIALOG_FONT_SELECTOR_H_
#define DIALOG_FONT_SELECTOR_H_

#include <QDialog>
#include "ui_DialogFontSelector.h"

class DialogFontSelector : public QDialog {
public:
	DialogFontSelector(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogFontSelector();

private:
	Ui::DialogFontSelector ui;
};

#endif
