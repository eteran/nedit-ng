
#ifndef DIALOG_FIND_H_
#define DIALOG_FIND_H_

#include <QDialog>
#include "ui_DialogFind.h"
#include "SearchDirection.h"
#include "SearchType.h"
#include <ctime>


class MainWindow;
class DocumentWidget;

class DialogFind : public QDialog {
	Q_OBJECT
public:
    DialogFind(MainWindow *window, DocumentWidget *document, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogFind();
	
protected:
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;

public:
    void setDocument(DocumentWidget *document);
    void setTextField(DocumentWidget *document);
	void initToggleButtons(SearchType searchType);
	void fUpdateActionButtons();
	
private:
	int getFindDlogInfoEx(SearchDirection *direction, std::string *searchString, SearchType *searchType);
	
public:
	bool keepDialog() const;
	
private Q_SLOTS:
	void on_checkBackward_toggled(bool checked);
	void on_checkRegex_toggled(bool checked);
	void on_checkCase_toggled(bool checked);
	void on_checkKeep_toggled(bool checked);
	void on_textFind_textChanged(const QString &text);
	void on_buttonFind_clicked();
	
public:
    MainWindow *window_;
    DocumentWidget *document_;
	Ui::DialogFind ui;
	bool lastRegexCase_;        /* idem, for regex mode in find dialog */
	bool lastLiteralCase_;      /* idem, for literal mode */	
};


#endif
