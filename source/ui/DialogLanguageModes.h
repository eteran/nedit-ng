
#ifndef DIALOG_LANGUAGE_MODES_H_
#define DIALOG_LANGUAGE_MODES_H_

#include <QDialog>
#include "ui_DialogLanguageModes.h"

class LanguageMode;

class DialogLanguageModes : public QDialog {
public:
	Q_OBJECT
public:
	DialogLanguageModes(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogLanguageModes();
	
private Q_SLOTS:
	void on_buttonBox_accepted();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_buttonDelete_clicked();
	void on_buttonCopy_clicked();
	void on_buttonNew_clicked();
	void on_listItems_itemSelectionChanged();
	
private:
	bool updateLMList(bool silent);
	bool updateLanguageList(bool silent);
	LanguageMode *readLMDialogFields(bool silent);
	LanguageMode *itemFromIndex(int i) const;
	bool updateCurrentItem();
	bool updateCurrentItem(QListWidgetItem *item);		
	
private:
	Ui::DialogLanguageModes ui;
	QListWidgetItem *previous_;
};

#endif
