
#ifndef DIALOG_MULTI_REPLACE_H_
#define DIALOG_MULTI_REPLACE_H_

#include <QDialog>
#include "ui_DialogMultiReplace.h"

class Document;
class DialogReplace;

class DialogMultiReplace : public QDialog {
	Q_OBJECT
public:
	DialogMultiReplace(Document *window, DialogReplace *replace, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogMultiReplace();
	
public Q_SLOTS:
	void on_checkShowPaths_toggled(bool checked);
	void on_buttonDeselectAll_clicked();
	void on_buttonSelectAll_clicked();
	void on_buttonReplace_clicked();
	
public:
	Ui::DialogMultiReplace ui;
	Document *window_;
	DialogReplace *replace_;

};

#endif
