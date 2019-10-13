
#ifndef DIALOG_MULTI_REPLACE_H_
#define DIALOG_MULTI_REPLACE_H_

#include "Dialog.h"
#include "ui_DialogMultiReplace.h"

class DialogReplace;
class DocumentModel;
class DocumentWidget;
class MainWindow;

class DialogMultiReplace : public Dialog {
	Q_OBJECT
public:
    explicit DialogMultiReplace(DialogReplace *replace, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogMultiReplace() override = default;

private:
	void checkShowPaths_toggled(bool checked);
	void buttonDeselectAll_clicked();
	void buttonSelectAll_clicked();
	void buttonReplace_clicked();
	void connectSlots();

public:
	void uploadFileListItems(const std::vector<DocumentWidget *> &writeableDocuments);

public:
	Ui::DialogMultiReplace ui;
	DialogReplace *replace_;
	DocumentModel *model_;

};

#endif
