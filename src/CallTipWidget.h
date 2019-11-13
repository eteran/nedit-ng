
#ifndef CALLTIP_WIDGET_H_
#define CALLTIP_WIDGET_H_

#include "ui_CallTipWidget.h"
#include <QWidget>

class CallTipWidget final : public QWidget {
	Q_OBJECT
public:
	explicit CallTipWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~CallTipWidget() override = default;

public:
	void setText(const QString &text);

public:
	void showEvent(QShowEvent *event) override;

private:
	Ui::CallTipWidget ui;
};

#endif
