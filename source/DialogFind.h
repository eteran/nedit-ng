
#ifndef DIALOG_FIND_H_
#define DIALOG_FIND_H_

#include "SearchDirection.h"
#include "SearchType.h"

#include "Dialog.h"
#include <ctime>

#include "ui_DialogFind.h"

class MainWindow;
class DocumentWidget;

class DialogFind : public Dialog {
	Q_OBJECT
public:
    DialogFind(MainWindow *window, DocumentWidget *document, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Q_NULLPTR);
    virtual ~DialogFind() override = default;
	
protected:
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;

public:
    void setDocument(DocumentWidget *document);
    void setTextField(DocumentWidget *document);
	void initToggleButtons(SearchType searchType);
	void fUpdateActionButtons();
	
private:
    bool getFindDlogInfoEx(SearchDirection *direction, QString *searchString, SearchType *searchType);
	
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
