
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
	bool hideSingleTab() const        { return hideSingleTab_; }
	void setHideSingleTab(bool value) { hideSingleTab_ = value; }

private:
	bool hideSingleTab_;
};

#endif

