
#ifndef DIALOG_H_
#define DIALOG_H_

#include <QDialog>

class Dialog : public QDialog {
	Q_OBJECT
public:
    Dialog(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Q_NULLPTR);
    virtual ~Dialog() override = default;

public:
	virtual void showEvent(QShowEvent *event) override;
};

#endif
