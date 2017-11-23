
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
    DialogMultiReplace(DialogReplace *replace, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogMultiReplace() override = default;

private Q_SLOTS:
	void on_checkShowPaths_toggled(bool checked);
	void on_buttonDeselectAll_clicked();
	void on_buttonSelectAll_clicked();
	void on_buttonReplace_clicked();
	
public:
	void uploadFileListItems(bool replace);
	
public:
	Ui::DialogMultiReplace ui;
	DialogReplace *replace_;

};

#endif
