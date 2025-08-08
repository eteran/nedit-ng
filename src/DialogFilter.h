
#ifndef DIALOG_FILTER_H_
#define DIALOG_FILTER_H_

#include "Dialog.h"
#include "ui_DialogFilter.h"

#include <QStringList>

class DialogFilter final : public Dialog {
	Q_OBJECT
public:
	explicit DialogFilter(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogFilter() override = default;

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;

public:
	QString currentText() const;
	void addHistoryItem(const QString &s);

private:
	Ui::DialogFilter ui;
	QStringList history_;
	int historyIndex_ = 0;
};

#endif
