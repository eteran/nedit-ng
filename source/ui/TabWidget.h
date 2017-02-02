
#ifndef TABWIDGET_20080118_H_
#define TABWIDGET_20080118_H_

#include <QTabWidget>

class TabWidget : public QTabWidget {
	Q_OBJECT

public:
	explicit TabWidget(QWidget *parent = 0);

Q_SIGNALS:
	void countChanged(int count);
	void customContextMenuRequested(int, const QPoint &);

protected:
	virtual void tabInserted(int index) override;
	virtual void tabRemoved(int index) override;
	virtual void mousePressEvent(QMouseEvent *event) override;

public:
	QTabBar *getTabBar() const;
    bool hideSingleTab() const;
    void setHideSingleTab(bool value);

private:
	bool hideSingleTab_;
};

#endif

