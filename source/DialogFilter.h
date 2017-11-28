
#ifndef DIALOG_FILTER_H_
#define DIALOG_FILTER_H_

#include "Dialog.h"
#include "ui_DialogFilter.h"

#include <QStringList>

class DialogFilter : public Dialog {
	Q_OBJECT
public:
    DialogFilter(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogFilter() override = default;
	
protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;
	
private Q_SLOTS:
	void on_buttonBox_accepted();

public:
	Ui::DialogFilter ui;
	QStringList history_;
	int historyIndex_;
};

#endif
