
#ifndef DIALOG_REPEAT_H_
#define DIALOG_REPEAT_H_

#include "Dialog.h"
#include "ui_DialogRepeat.h"
#include <QPointer>

class DocumentWidget;

class DialogRepeat final : public Dialog {
	Q_OBJECT
public:
	explicit DialogRepeat(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogRepeat() override = default;

public:
	bool setCommand(const QString &command);

private:
	void buttonBox_accepted();
	void connectSlots();

private:
	bool doRepeatDialogAction();

private:
	Ui::DialogRepeat ui;
	QString lastCommand_;
	QPointer<DocumentWidget> document_;
};

#endif
