
#ifndef DIALOG_H_
#define DIALOG_H_

#include <QDialog>

class Dialog : public QDialog {
	Q_OBJECT
public:
	Dialog(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~Dialog() = default;

public:
	virtual void showEvent(QShowEvent *event) override;
};

#endif
