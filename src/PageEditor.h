
#ifndef PAGE_EDITOR_H_
#define PAGE_EDITOR_H_

#include <QWidget>

#include "ui_PageEditor.h"

class PageEditor : public QWidget {
	Q_OBJECT
public:
	explicit PageEditor(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~PageEditor() override = default;

private:
	Ui::PageEditor ui;
};

#endif
