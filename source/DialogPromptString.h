
#ifndef DIALOG_PROMPT_STRING_H_
#define DIALOG_PROMPT_STRING_H_

#include "Dialog.h"
#include "ui_DialogPromptString.h"

class DialogPromptString : public Dialog {
	Q_OBJECT
public:
	DialogPromptString(QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogPromptString() = default;

private:
    virtual void showEvent(QShowEvent *event) override;

public:
	int result() const {
		return result_;
	}
	
	QString text() const {
		return text_;
	}

public Q_SLOTS:
	void addButton(const QString &text);
	void addButton(QDialogButtonBox::StandardButton button);
	void setMessage(const QString &text);
	
private Q_SLOTS:
	void on_buttonBox_clicked(QAbstractButton *button);
	
private:
	Ui::DialogPromptString ui;
	int result_;
	QString text_;
};

#endif
