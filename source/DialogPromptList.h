
#ifndef DIALOG_PROMPT_LIST_H_
#define DIALOG_PROMPT_LIST_H_

#include "Dialog.h"
#include "ui_DialogPromptList.h"

class DialogPromptList : public Dialog {
	Q_OBJECT
public:
    DialogPromptList(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogPromptList() override = default;

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
	void setList(const QString &string);
	
private Q_SLOTS:
	void on_buttonBox_clicked(QAbstractButton *button);
	
private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogPromptList ui;
	int result_;
	QString text_;
};

#endif
