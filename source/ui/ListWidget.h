
#ifndef LIST_WIDGET_H_
#define LIST_WIDGET_H_

#include <QListWidget>

class ListWidget : public QListWidget {
	Q_OBJECT

public:
	ListWidget(QWidget *parent = 0);
	virtual ~ListWidget();
	
protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
};


#endif
