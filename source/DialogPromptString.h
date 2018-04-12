
#ifndef DIALOG_PROMPT_STRING_H_
#define DIALOG_PROMPT_STRING_H_

#include "Dialog.h"
#include "ui_DialogPromptString.h"

class DialogPromptString : public Dialog {
	Q_OBJECT
public:
    DialogPromptString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogPromptString() override = default;

private:
    void showEvent(QShowEvent *event) override;

public:
	int result() const {
		return result_;
	}
	
	QString text() const {
		return text_;
	}

public:
	void addButton(const QString &text);
	void addButton(QDialogButtonBox::StandardButton button);
	void setMessage(const QString &text);
	
private Q_SLOTS:
	void on_buttonBox_clicked(QAbstractButton *button);
	
private:
	Ui::DialogPromptString ui;
    int result_ = 0;
	QString text_;
};

#endif
