
#ifndef DIALOG_MACROS_H_
#define DIALOG_MACROS_H_

#include <QDialog>
#include <QList>
#include "ui_DialogMacros.h"

class MenuItem;

class DialogMacros : public QDialog {
	Q_OBJECT
public:
	DialogMacros(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogMacros();

public:
	void setPasteReplayEnabled(bool enabled);

private Q_SLOTS:
	void on_buttonNew_clicked();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_listItems_itemSelectionChanged();
	void on_buttonPasteLRMacro_clicked();
	void on_buttonCheck_clicked();
	void on_buttonApply_clicked();
	void on_buttonOK_clicked();
	
private:
	bool checkMacro(bool silent);
	MenuItem *readDialogFields(bool silent);
	bool checkMacroText(const QString &macro, bool silent);
	QString ensureNewline(const QString &string);
	bool applyDialogChanges();
	void updateButtons();
	
private:
	Ui::DialogMacros ui;
	QListWidgetItem *previous_;
};

#endif

