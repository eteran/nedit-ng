
#ifndef DIALOG_ABOUT_H_
#define DIALOG_ABOUT_H_

#include <QDialog>
#include "ui_DialogDuplicateTags.h"
class DocumentWidget;
class TextArea;

class DialogDuplicateTags : public QDialog {
	Q_OBJECT
public:
    DialogDuplicateTags(DocumentWidget *document, TextArea *area, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogDuplicateTags() = default;
	
public:
	void setTag(const QString &tag);
	void addListItem(const QString &item);
	
private Q_SLOTS:
	void on_buttonBox_accepted();
	void on_buttonBox_clicked(QAbstractButton *button);
	
private:
	bool applySelection();

private:
	Ui::DialogDuplicateTags ui;
    DocumentWidget *document_;
    TextArea *area_;
};

#endif
