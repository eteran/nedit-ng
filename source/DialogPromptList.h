
#ifndef DIALOG_PROMPT_LIST_H_
#define DIALOG_PROMPT_LIST_H_

#include <QDialog>
#include "ui_DialogPromptList.h"

class DialogPromptList : public QDialog {
	Q_OBJECT
public:
	DialogPromptList(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogPromptList();

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
	virtual void showEvent(QShowEvent *event) override;

private:
	Ui::DialogPromptList ui;
	int result_;
	QString text_;
};

#endif
