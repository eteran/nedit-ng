
#ifndef DIALOG_COLORS_H_
#define DIALOG_COLORS_H_

#include "Dialog.h"
#include "ui_DialogColors.h"
#include <QColor>

class DialogColors final : public Dialog {
	Q_OBJECT
public:
	explicit DialogColors(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogColors() override = default;

private:
	QColor chooseColor(QPushButton *button, const QColor &currentColor);
	void updateColors();

private:
	Ui::DialogColors ui;
	QColor textFG_;
	QColor textBG_;
	QColor selectionFG_;
	QColor selectionBG_;
	QColor matchFG_;
	QColor matchBG_;
	QColor lineNumbersFG_;
	QColor lineNumbersBG_;
	QColor cursorFG_;
};

#endif
