
#ifndef DIALOG_H_
#define DIALOG_H_

#include <QDialog>

class Dialog : public QDialog {
	Q_OBJECT
public:
	explicit Dialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~Dialog() override = default;

public:
	void showEvent(QShowEvent *event) override;

protected:
	static void shrinkToFit(Dialog *dialog);
};

#endif
