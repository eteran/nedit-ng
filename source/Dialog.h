
#ifndef DIALOG_H_
#define DIALOG_H_

#include <QDialog>

class Dialog : public QDialog {
	Q_OBJECT
public:
	Dialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~Dialog() noexcept override = default;

public:
	void showEvent(QShowEvent *event) override;
};

#endif
