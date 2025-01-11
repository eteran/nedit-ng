
#ifndef DIALOG_WINDOW_BACKGROUND_MENU_H_
#define DIALOG_WINDOW_BACKGROUND_MENU_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogWindowBackgroundMenu.h"

#include <optional>

struct MenuItem;
class MenuItemModel;

class DialogWindowBackgroundMenu final : public Dialog {
	Q_OBJECT

public:
	explicit DialogWindowBackgroundMenu(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogWindowBackgroundMenu() override = default;

public:
	void setPasteReplayEnabled(bool enabled);

Q_SIGNALS:
	void restore(const QModelIndex &selection);

private:
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
	void buttonNew_clicked();
	void buttonCopy_clicked();
	void buttonDelete_clicked();
	void buttonUp_clicked();
	void buttonDown_clicked();
	void buttonPasteLRMacro_clicked();
	void buttonCheck_clicked();
	void buttonApply_clicked();
	void buttonOK_clicked();
	void connectSlots();

private:
	bool applyDialogChanges();
	bool validateFields(Verbosity verbosity);
	bool checkMacroText(const QString &macro, Verbosity verbosity);
	bool updateCurrentItem();
	bool updateCurrentItem(const QModelIndex &index);
	std::optional<MenuItem> readFields(Verbosity verbosity);

private:
	Ui::DialogWindowBackgroundMenu ui;
	MenuItemModel *model_;
	QModelIndex deleted_;
};

#endif
