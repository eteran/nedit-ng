
#include "TextArea.h"

TextArea::TextArea(QWidget *parent) : QAbstractScrollArea(parent) {
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

TextArea::~TextArea() {
}
