
#ifndef DIALOG_DRAWING_STYLES_H_
#define DIALOG_DRAWING_STYLES_H_

#include <QDialog>
#include <QList>
#include "ui_DialogDrawingStyles.h"

class HighlightStyle;

class DialogDrawingStyles : public QDialog {
	Q_OBJECT
public:
	DialogDrawingStyles(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogDrawingStyles();

public Q_SLOTS:
	void setStyleByName(const QString &name);

private Q_SLOTS:
	void on_listStyles_itemSelectionChanged();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_editName_textChanged(const QString &text);
	void on_editColorFG_textChanged(const QString &text);
	void on_editColorBG_textChanged(const QString &text);
	void on_radioPlain_toggled(bool checked);
	void on_radioBold_toggled(bool checked);
	void on_radioItalic_toggled(bool checked);
	void on_radioBoldItalic_toggled(bool checked);
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
	
private:
	bool updateHSList();

private:
	Ui::DialogDrawingStyles ui;
	QList<HighlightStyle *>  styles_;
};

#endif
