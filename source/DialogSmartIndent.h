
#ifndef DIALOG_SMART_INDENT_H_
#define DIALOG_SMART_INDENT_H_

#include <QDialog>
#include "ui_DialogSmartIndent.h"

class DocumentWidget;
class SmartIndent;

class DialogSmartIndent : public QDialog {
	Q_OBJECT
public:
    DialogSmartIndent(DocumentWidget *document, QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~DialogSmartIndent() = default;

private:
    virtual void showEvent(QShowEvent *event) override;

public:
	void updateLanguageModes();
	bool hasSmartIndentMacros(const QString &languageMode) const;
	
private:
    void setSmartIndentDialogData(const SmartIndent *is);
	bool updateSmartIndentData();
	bool checkSmartIndentDialogData();
	SmartIndent *getSmartIndentDialogData();
	QString ensureNewline(const QString &string);
	

public Q_SLOTS:
	void setLanguageMode(const QString &s);

private Q_SLOTS:
	void on_buttonCommon_clicked();
	void on_buttonLanguageMode_clicked();
	void on_buttonOK_clicked();
	void on_buttonApply_clicked();
	void on_buttonCheck_clicked();
	void on_buttonDelete_clicked();
	void on_buttonRestore_clicked();
	void on_buttonHelp_clicked();
	void on_comboLanguageMode_currentIndexChanged(const QString &text);

public:
	Ui::DialogSmartIndent ui;
	QString               languageMode_;
};

#endif
