
#ifndef DIALOG_REPEAT_H_
#define DIALOG_REPEAT_H_

#include "Dialog.h"
#include "ui_DialogRepeat.h"

class DocumentWidget;

class DialogRepeat : public Dialog {
	Q_OBJECT
public:
    DialogRepeat(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogRepeat() override = default;

public:
    void setCommand(const QString &command);

private Q_SLOTS:
	void on_buttonBox_accepted();
	
private:
	bool doRepeatDialogAction();

private:
	Ui::DialogRepeat ui;
	QString lastCommand_;
    DocumentWidget *document_;
};

#endif
