#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>

class ElidedLabel : public QLabel {
	Q_OBJECT
public:
	ElidedLabel(QWidget *parent = 0, Qt::WindowFlags f = 0);
	ElidedLabel(const QString &text, QWidget *parent = 0, Qt::WindowFlags f = 0);
	ElidedLabel(const QString &text, Qt::TextElideMode elideMode, QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~ElidedLabel() = default;

public Q_SLOTS:
	void setText(const QString &text);
	void setElideMode(Qt::TextElideMode elideMode);
	Qt::TextElideMode elideMode() const;

protected:
	virtual void resizeEvent(QResizeEvent *event);

private:
	Qt::TextElideMode elideMode_;
	QString           realString_;
};

#endif
