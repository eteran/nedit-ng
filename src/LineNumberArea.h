
#ifndef LINE_NUMBER_AREA_H_
#define LINE_NUMBER_AREA_H_

#include <QWidget>

class TextArea;

class LineNumberArea : public QWidget {
	Q_OBJECT
public:
	static constexpr int Padding = 5;

public:
	explicit LineNumberArea(TextArea *area);
	~LineNumberArea() override = default;

public:
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	TextArea *area_;
};

#endif
