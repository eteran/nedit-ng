
#ifndef DIALOG_FILTER_H_
#define DIALOG_FILTER_H_

#include <QDialog>
#include <QStringList>
#include "ui_DialogFilter.h"

class Document;

class DialogFilter : public QDialog {
	Q_OBJECT
public:
	DialogFilter(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogFilter();
	
protected:
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;
	
private Q_SLOTS:
	void on_buttonBox_accepted();

public:
	Ui::DialogFilter ui;
	QStringList history_;
	int historyIndex_;
};

#endif
