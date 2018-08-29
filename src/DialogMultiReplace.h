
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
	DialogMultiReplace(DialogReplace *replace, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogMultiReplace() noexcept override = default;

private Q_SLOTS:
	void on_checkShowPaths_toggled(bool checked);
	void on_buttonDeselectAll_clicked();
	void on_buttonSelectAll_clicked();
	void on_buttonReplace_clicked();

public:
	void uploadFileListItems(const std::vector<DocumentWidget *> &writeableDocuments);

public:
	Ui::DialogMultiReplace ui;
	DialogReplace *replace_;
	DocumentModel *model_;

};

#endif
