
#ifndef DIALOG_DUPLICATE_TAGS_H_
#define DIALOG_DUPLICATE_TAGS_H_

#include "Dialog.h"
#include "ui_DialogDuplicateTags.h"

class DocumentWidget;
class TextArea;

class DialogDuplicateTags final : public Dialog {
	Q_OBJECT
public:
	DialogDuplicateTags(DocumentWidget *document, TextArea *area, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogDuplicateTags() override = default;

public:
	void setTag(const QString &tag);
	void addListItem(const QString &text, int id);

private:
	void buttonBox_accepted();
	void buttonBox_clicked(QAbstractButton *button);
	bool applySelection();
	void connectSlots();

private:
	Ui::DialogDuplicateTags ui;
	DocumentWidget *document_;
	TextArea *area_;
};

#endif
