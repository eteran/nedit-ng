
#ifndef DIALOG_MACROS_H_
#define DIALOG_MACROS_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogMacros.h"

#include <memory>

struct MenuItem;
class MenuItemModel;

class DialogMacros : public Dialog {
	Q_OBJECT

public:
    DialogMacros(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogMacros() noexcept override = default;

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
	void on_buttonPasteLRMacro_clicked();
	void on_buttonCheck_clicked();
	void on_buttonApply_clicked();
	void on_buttonOK_clicked();
	
private:
    bool applyDialogChanges();
    bool validateFields(Verbosity verbosity);
    bool checkMacroText(const QString &macro, Verbosity verbosity);
    bool updateCurrentItem();
    bool updateCurrentItem(const QModelIndex &index);
    QString ensureNewline(const QString &string);
    std::unique_ptr<MenuItem> readFields(Verbosity verbosity);
    void updateButtonStates();
    void updateButtonStates(const QModelIndex &current);

private:
	Ui::DialogMacros ui;
    MenuItemModel *model_;
    QModelIndex deleted_;
};

#endif

