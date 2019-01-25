
#ifndef DIALOG_COLORS_H_
#define DIALOG_COLORS_H_

#include "Dialog.h"
#include "ui_DialogColors.h"
#include <QColor>

class DocumentWidget;
class QLabel;
class QToolButton;

class DialogColors final : public Dialog {
	Q_OBJECT
public:
	DialogColors(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogColors() noexcept override = default;

private Q_SLOTS:
	void on_pushButtonFG_clicked();
	void on_pushButtonBG_clicked();
	void on_pushButtonSelectionFG_clicked();
	void on_pushButtonSelectionBG_clicked();
	void on_pushButtonMatchFG_clicked();
	void on_pushButtonMatchBG_clicked();
	void on_pushButtonLineNumbersFG_clicked();
	void on_pushButtonLineNumbersBG_clicked();
	void on_pushButtonCursor_clicked();

	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();

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
