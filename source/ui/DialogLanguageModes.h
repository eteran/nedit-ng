
#ifndef DIALOG_LANGUAGE_MODES_H_
#define DIALOG_LANGUAGE_MODES_H_

#include <QDialog>
#include <QVector>
#include "ui_DialogLanguageModes.h"

class languageModeRec;

class DialogLanguageModes : public QDialog {
public:
	Q_OBJECT
public:
	DialogLanguageModes(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogLanguageModes();
	
private Q_SLOTS:
	void on_buttonBox_accepted();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_listLanguages_itemSelectionChanged();
	
private:
	Ui::DialogLanguageModes    ui;
	QVector<languageModeRec *> languageModes_;
};

#endif
