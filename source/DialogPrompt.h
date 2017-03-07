
#ifndef DIALOG_PROMPT_H_
#define DIALOG_PROMPT_H_

#include <QDialog>
#include "ui_DialogPrompt.h"

class DialogPrompt : public QDialog {
	Q_OBJECT
public:
	DialogPrompt(QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogPrompt() = default;

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
	virtual void showEvent(QShowEvent *event) override;

private:
	Ui::DialogPrompt ui;
	int result_;
};

#endif
