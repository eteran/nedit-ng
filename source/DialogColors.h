
#ifndef DIALOG_COLORS_H_
#define DIALOG_COLORS_H_

#include "Dialog.h"
#include "ui_DialogColors.h"

class DocumentWidget;
class QLabel;

class DialogColors : public Dialog {
	Q_OBJECT
public:
    DialogColors(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogColors() override = default;

private Q_SLOTS:
	void on_buttonFG_clicked();
	void on_buttonBG_clicked();
	void on_buttonSelectionFG_clicked();
	void on_buttonSelectionBG_clicked();
	void on_buttonMatchFG_clicked();
	void on_buttonMatchBG_clicked();
	void on_buttonLineNumbers_clicked();
	void on_buttonCursor_clicked();

	void on_editFG_textChanged(const QString &text);
	void on_editBG_textChanged(const QString &text);
	void on_editSelectionFG_textChanged(const QString &text);
	void on_editSelectionBG_textChanged(const QString &text);
	void on_editMatchFG_textChanged(const QString &text);
	void on_editMatchBG_textChanged(const QString &text);
	void on_editLineNumbers_textChanged(const QString &text);
	void on_editCursor_textChanged(const QString &text);

	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
  
private:	
	void chooseColor(QLineEdit *edit);
	bool verifyAllColors();
	void updateColors();

private:
	Ui::DialogColors ui;
};

#endif
