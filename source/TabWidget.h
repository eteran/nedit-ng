
#ifndef TABWIDGET_20080118_H_
#define TABWIDGET_20080118_H_

#include <QTabWidget>

class TabWidget : public QTabWidget {
	Q_OBJECT
	
public:
    explicit TabWidget(QWidget *parent = nullptr);

Q_SIGNALS:
	void tabCountChanged(int);

protected:
    virtual void tabInserted(int index);
    virtual void tabRemoved(int index);
};

#endif

