#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>

class ElidedLabel : public QLabel {
	Q_OBJECT
public:
    ElidedLabel(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ElidedLabel(const QString &text, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ElidedLabel(const QString &text, Qt::TextElideMode elideMode, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~ElidedLabel() override = default;

public Q_SLOTS:
	void setText(const QString &text);
	void setElideMode(Qt::TextElideMode elideMode);

public:
	Qt::TextElideMode elideMode() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
	Qt::TextElideMode elideMode_;
	QString           realString_;
};

#endif
