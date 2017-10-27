
#ifndef DIALOG_PROMPT_H_
#define DIALOG_PROMPT_H_

#include "Dialog.h"
#include "ui_DialogPrompt.h"

class DialogPrompt : public Dialog {
	Q_OBJECT
public:
    DialogPrompt(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogPrompt() override = default;

public:
	int result() const {
		return result_;
	}

public Q_SLOTS:
	void addButton(const QString &text);
	void addButton(QDialogButtonBox::StandardButton button);
	void setMessage(const QString &text);
	
private Q_SLOTS:
	void on_buttonBox_clicked(QAbstractButton *button);
	
private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogPrompt ui;
	int result_;
};

#endif
