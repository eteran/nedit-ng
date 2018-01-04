
#ifndef DIALOG_SHELL_MENU_H_
#define DIALOG_SHELL_MENU_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogShellMenu.h"

#include <memory>

struct MenuItem;
class MenuItemModel;

class DialogShellMenu : public Dialog {
	Q_OBJECT

public:
    DialogShellMenu(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogShellMenu() noexcept override = default;

Q_SIGNALS:
    void restore(const QModelIndex &selection);

private Q_SLOTS:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous);
    void restoreSlot(const QModelIndex &index);

private Q_SLOTS:
	void on_buttonNew_clicked();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
	void on_radioToSameDocument_toggled(bool checked);
	
private:
    bool applyDialogChanges();
    bool validateFields(Verbosity verbosity);
    bool updateCurrentItem();
    bool updateCurrentItem(const QModelIndex &index);
    QString ensureNewline(const QString &string);
    std::unique_ptr<MenuItem> readFields(Verbosity verbosity);
    void updateButtonStates();
    void updateButtonStates(const QModelIndex &current);

private:
	Ui::DialogShellMenu ui;
    MenuItemModel *model_;
    QModelIndex deleted_;
};

#endif

