
#ifndef DIALOG_SHELL_MENU_H_
#define DIALOG_SHELL_MENU_H_

#include <QDialog>
#include <QList>
#include "ui_DialogShellMenu.h"

class MenuItem;

class DialogShellMenu : public QDialog {
	Q_OBJECT
public:
	DialogShellMenu(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogShellMenu();

private Q_SLOTS:
	void on_buttonNew_clicked();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_listItems_itemSelectionChanged();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
	void on_radioToSameDocument_toggled(bool checked);
	
private:
	MenuItem *readDialogFields(bool silent);
	QString ensureNewline(const QString &string);
	bool applyDialogChanges();
	void updateButtons();
	MenuItem *itemFromIndex(int i) const;
	
private:
	Ui::DialogShellMenu ui;
	QListWidgetItem *previous_;
};

#endif

