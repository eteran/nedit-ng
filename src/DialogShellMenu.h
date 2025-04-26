
#ifndef DIALOG_SHELL_MENU_H_
#define DIALOG_SHELL_MENU_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogShellMenu.h"

#include <optional>

struct MenuItem;
class MenuItemModel;

class DialogShellMenu final : public Dialog {
	Q_OBJECT

public:
	explicit DialogShellMenu(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogShellMenu() override = default;

Q_SIGNALS:
	void restore(const QModelIndex &selection);

private:
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
	void radioToSameDocument_toggled(bool checked);
	void buttonBox_clicked(QAbstractButton *button);
	void buttonBox_accepted();
	void buttonNew_clicked();
	void buttonCopy_clicked();
	void buttonDelete_clicked();
	void buttonUp_clicked();
	void buttonDown_clicked();
	void connectSlots();

private:
	bool applyDialogChanges();
	bool validateFields(Verbosity verbosity);
	bool updateCurrentItem();
	bool updateItem(const QModelIndex &index);
	std::optional<MenuItem> readFields(Verbosity verbosity);

private:
	Ui::DialogShellMenu ui;
	MenuItemModel *model_;
	QModelIndex deleted_;
};

#endif
