
#include "TextArea.h"
#include "Document.h"
#include "preferences.h"

//------------------------------------------------------------------------------
// Name: TextArea
//------------------------------------------------------------------------------
TextArea::TextArea(QWidget *parent) : QAbstractScrollArea(parent) {
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

//------------------------------------------------------------------------------
// Name: ~TextArea
//------------------------------------------------------------------------------
TextArea::~TextArea() {
}
