
#ifndef CALLTIP_WIDGET_H_
#define CALLTIP_WIDGET_H_

#include <QWidget>
#include "ui_CallTipWidget.h"

class CallTipWidget : public QWidget {
	Q_OBJECT
public:
	CallTipWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~CallTipWidget() noexcept override = default;

public:
	void setText(const QString &text);

public:
	void showEvent(QShowEvent *event) override;

private:
	Ui::CallTipWidget ui;
};

#endif
