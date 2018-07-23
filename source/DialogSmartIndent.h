
#ifndef DIALOG_SMART_INDENT_H_
#define DIALOG_SMART_INDENT_H_

#include "Dialog.h"
#include "ui_DialogSmartIndent.h"

#include <boost/optional.hpp>

class DocumentWidget;
class SmartIndentEntry;

class DialogSmartIndent final : public Dialog {
	Q_OBJECT
public:
    DialogSmartIndent(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogSmartIndent() noexcept override = default;

public:
	void updateLanguageModes();
    void setLanguageMode(const QString &s);
    QString languageMode() const;

private:
    void setSmartIndentDialogData(const SmartIndentEntry *is);
	bool updateSmartIndentData();
	bool checkSmartIndentDialogData();
	boost::optional<SmartIndentEntry> getSmartIndentDialogData() const;
	
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
