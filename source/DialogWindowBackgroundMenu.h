
#ifndef DIALOG_WINDOW_BACKGROUND_MENU_H_
#define DIALOG_WINDOW_BACKGROUND_MENU_H_

#include "Dialog.h"
#include <QList>
#include "ui_DialogWindowBackgroundMenu.h"

class MenuItem;

class DialogWindowBackgroundMenu : public Dialog {
	Q_OBJECT
public:
	DialogWindowBackgroundMenu(QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogWindowBackgroundMenu() noexcept;

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
	MenuItem *itemFromIndex(int i) const;
	bool updateCurrentItem();
	bool updateCurrentItem(QListWidgetItem *item);	
	
private:
	Ui::DialogWindowBackgroundMenu ui;
	QListWidgetItem *previous_;
};

#endif

