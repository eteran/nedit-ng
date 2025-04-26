
#ifndef DIALOG_PROMPT_LIST_H_
#define DIALOG_PROMPT_LIST_H_

#include "Dialog.h"
#include "ui_DialogPromptList.h"

class DialogPromptList final : public Dialog {
	Q_OBJECT
public:
	explicit DialogPromptList(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPromptList() override = default;

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
	void setList(const QString &string);

private:
	void connectSlots();
	void buttonBox_clicked(QAbstractButton *button);

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogPromptList ui;
	QString text_;
	int result_ = 0;
};

#endif
