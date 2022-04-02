
#ifndef DIALOG_TABS_H_
#define DIALOG_TABS_H_

#include "Dialog.h"
#include "ui_DialogTabs.h"
#include <QPointer>

class DocumentWidget;

class DialogTabs final : public Dialog {
public:
	Q_OBJECT
public:
	explicit DialogTabs(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogTabs() override = default;

private:
	void buttonBox_accepted();
	void buttonBox_helpRequested();
	void checkEmulateTabs_toggled(bool checked);
	void connectSlots();

private:
	Ui::DialogTabs ui;
	QPointer<DocumentWidget> document_;
};

#endif
