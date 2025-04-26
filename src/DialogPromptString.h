
#ifndef DIALOG_PROMPT_STRING_H_
#define DIALOG_PROMPT_STRING_H_

#include "Dialog.h"
#include "ui_DialogPromptString.h"

class DialogPromptString final : public Dialog {
	Q_OBJECT
public:
	explicit DialogPromptString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPromptString() override = default;

private:
	void showEvent(QShowEvent *event) override;

public:
	int result() const noexcept {
		return result_;
	}

	QString text() const {
		return text_;
	}

public:
	void addButton(const QString &text);
	void addButton(QDialogButtonBox::StandardButton button);
	void setMessage(const QString &text);

private:
	void connectSlots();
	void buttonBox_clicked(QAbstractButton *button);

private:
	Ui::DialogPromptString ui;
	QString text_;
	int result_ = 0;
};

#endif
