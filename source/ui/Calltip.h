
#ifndef CALLTIP_H_
#define CALLTIP_H_

#include <QWidget>
#include "ui_Calltip.h"

class Calltip : public QWidget {
	Q_OBJECT
public:
	Calltip(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~Calltip();
	
public:
	void setTitle(const QString &title);
	void setText(const QString &text);
	
private:
	Ui::Calltip ui;
};

#endif
