
#ifndef DIALOG_TABS_H_
#define DIALOG_TABS_H_

#include <QDialog>
#include "ui_DialogTabs.h"

class DocumentWidget;

/*
** Present the user a dialog for setting tab related preferences, either as
** defaults, or for a specific window (pass "forWindow" as nullptr to set default
** preference, or a window to set preferences for the specific window.
*/
class DialogTabs : public QDialog {
public:
	Q_OBJECT
public:
    DialogTabs(DocumentWidget *document, QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogTabs() = default;

private:
    virtual void showEvent(QShowEvent *event) override;

private Q_SLOTS:
	void on_checkEmulateTabs_toggled(bool checked);
	void on_buttonBox_accepted();
	void on_buttonBox_helpRequested();
	
private:
	Ui::DialogTabs ui;
    DocumentWidget *document_;
};

#endif
