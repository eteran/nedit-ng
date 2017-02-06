
#ifndef TABWIDGET_20080118_H_
#define TABWIDGET_20080118_H_

#include <QTabWidget>

class TabWidget : public QTabWidget {
	Q_OBJECT

public:
	explicit TabWidget(QWidget *parent = 0);

Q_SIGNALS:
	void customContextMenuRequested(int, const QPoint &);

protected:
	virtual void mousePressEvent(QMouseEvent *event) override;
};

#endif

