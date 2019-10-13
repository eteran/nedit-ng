
#ifndef DIALOG_PROMPT_H_
#define DIALOG_PROMPT_H_

#include "Dialog.h"
#include "ui_DialogPrompt.h"

class DialogPrompt final : public Dialog {
	Q_OBJECT
public:
	explicit DialogPrompt(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPrompt() override = default;

public:
	int result() const {
		return result_;
	}

public:
	void addButton(const QString &text);
	void addButton(QDialogButtonBox::StandardButton button);
	void setMessage(const QString &text);

private:
	void connectSlots();
	void buttonBox_clicked(QAbstractButton *button);

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogPrompt ui;
	int result_ = 0;
};

#endif
