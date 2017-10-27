
#ifndef CALLTIP_H_
#define CALLTIP_H_

#include <QWidget>
#include "ui_CallTipWidget.h"

class CallTipWidget : public QWidget {
	Q_OBJECT
public:
    CallTipWidget(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~CallTipWidget() override = default;
	
public:
	void setText(const QString &text);

public Q_SLOTS:
    void copyText();

private:
    Ui::CallTipWidget ui;
};

#endif
