
#ifndef DIALOG_MULTI_REPLACE_H_
#define DIALOG_MULTI_REPLACE_H_

#include <QDialog>
#include "ui_DialogMultiReplace.h"

class MainWindow;
class DocumentWidget;
class DialogReplace;

class DialogMultiReplace : public QDialog {
	Q_OBJECT
public:
    DialogMultiReplace(MainWindow *window, DocumentWidget *document, DialogReplace *replace, QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogMultiReplace() = default;

public:
    virtual void showEvent(QShowEvent *event) override;

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
