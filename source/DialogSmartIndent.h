
#ifndef DIALOG_SMART_INDENT_H_
#define DIALOG_SMART_INDENT_H_

#include "Dialog.h"
#include <memory>
#include "ui_DialogSmartIndent.h"

class DocumentWidget;
class SmartIndentEntry;

class DialogSmartIndent : public Dialog {
	Q_OBJECT
public:
    DialogSmartIndent(DocumentWidget *document, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Q_NULLPTR);
    virtual ~DialogSmartIndent() override = default;

public:
	void updateLanguageModes();
	bool hasSmartIndentMacros(const QString &languageMode) const;
	
private:
    void setSmartIndentDialogData(const SmartIndentEntry *is);
	bool updateSmartIndentData();
	bool checkSmartIndentDialogData();
	std::unique_ptr<SmartIndentEntry>  getSmartIndentDialogData();
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
