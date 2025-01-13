
#ifndef TABWIDGET_20080118_H_
#define TABWIDGET_20080118_H_

#include <QTabWidget>

class TabWidget final : public QTabWidget {
	Q_OBJECT

public:
	explicit TabWidget(QWidget *parent = nullptr);

Q_SIGNALS:
	void tabCountChanged(int);

protected:
	void tabInserted(int index) override;
	void tabRemoved(int index) override;
};

#endif
