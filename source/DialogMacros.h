
#ifndef DIALOG_MACROS_H_
#define DIALOG_MACROS_H_

#include <QDialog>
#include <QList>
#include <memory>
#include "ui_DialogMacros.h"

class MenuItem;

class DialogMacros : public QDialog {
	Q_OBJECT
private:
    enum class Mode {
        Silent,
        Verbose
    };

public:
	DialogMacros(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogMacros();

private:
    virtual void showEvent(QShowEvent *event) override;

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
    bool checkMacro(Mode mode);
    std::unique_ptr<MenuItem> readDialogFields(Mode mode);
    bool checkMacroText(const QString &macro, Mode mode);
	QString ensureNewline(const QString &string);
	bool applyDialogChanges();
	void updateButtons();
	MenuItem *itemFromIndex(int i) const;
	bool updateCurrentItem();
	bool updateCurrentItem(QListWidgetItem *item);		
	
private:
	Ui::DialogMacros ui;
	QListWidgetItem *previous_;
};

#endif

