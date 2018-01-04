
#ifndef DIALOG_TABS_H_
#define DIALOG_TABS_H_

#include "Dialog.h"
#include "ui_DialogTabs.h"

class DocumentWidget;

/*
** Present the user a dialog for setting tab related preferences, either as
** defaults, or for a specific window (pass "forWindow" as nullptr to set default
** preference, or a window to set preferences for the specific window.
*/
class DialogTabs : public Dialog {
public:
	Q_OBJECT
public:
    DialogTabs(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogTabs() override = default;

private Q_SLOTS:
	void on_checkEmulateTabs_toggled(bool checked);
	void on_buttonBox_accepted();
	void on_buttonBox_helpRequested();
	
private:
	Ui::DialogTabs ui;
    DocumentWidget *document_;
};

#endif
