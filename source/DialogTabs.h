
#ifndef DIALOG_TABS_H_
#define DIALOG_TABS_H_

#include "Dialog.h"
#include "ui_DialogTabs.h"

class DocumentWidget;

class DialogTabs final : public Dialog {
public:
	Q_OBJECT
public:
    DialogTabs(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogTabs() noexcept override = default;

private Q_SLOTS:
	void on_checkEmulateTabs_toggled(bool checked);
	void on_buttonBox_accepted();
	void on_buttonBox_helpRequested();
	
private:
	Ui::DialogTabs ui;
    DocumentWidget *document_;
};

#endif
