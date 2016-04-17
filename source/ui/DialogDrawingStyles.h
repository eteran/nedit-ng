
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
	
private Q_SLOTS:
	void on_listStyles_itemSelectionChanged();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	
private:
	Ui::DialogDrawingStyles ui;
	QList<HighlightStyle *>  styles_;
};

#endif
