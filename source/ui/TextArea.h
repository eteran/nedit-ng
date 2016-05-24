
#ifndef TEXT_AREA_H_
#define TEXT_AREA_H_

#include <QAbstractScrollArea>

class TextArea : public QAbstractScrollArea {
	Q_OBJECT
	
public:
	explicit TextArea(QWidget *parent = 0);
	virtual ~TextArea();
};

#endif
