
#ifndef CALLTIP_H_
#define CALLTIP_H_

#include <QWidget>
#include "ui_CallTipWidget.h"

class CallTipWidget : public QWidget {
	Q_OBJECT
public:
    CallTipWidget(QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~CallTipWidget();
	
public:
	void setText(const QString &text);

public Q_SLOTS:
    void copyText();
	
private:
    Ui::CallTipWidget ui;
};

#endif
