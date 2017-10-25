
#ifndef DIALOG_MULTI_REPLACE_H_
#define DIALOG_MULTI_REPLACE_H_

#include "Dialog.h"
#include "ui_DialogMultiReplace.h"

class MainWindow;
class DocumentWidget;
class DialogReplace;

class DialogMultiReplace : public Dialog {
	Q_OBJECT
public:
    DialogMultiReplace(MainWindow *window, DocumentWidget *document, DialogReplace *replace, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Q_NULLPTR);
    virtual ~DialogMultiReplace() override = default;

private Q_SLOTS:
	void on_checkShowPaths_toggled(bool checked);
	void on_buttonDeselectAll_clicked();
	void on_buttonSelectAll_clicked();
	void on_buttonReplace_clicked();
	
public:
	void uploadFileListItems(bool replace);
	
public:
	Ui::DialogMultiReplace ui;
    MainWindow *window_;
    DocumentWidget *document_;
	DialogReplace *replace_;

};

#endif
