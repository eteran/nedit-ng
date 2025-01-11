
#ifndef DIALOG_SMART_INDENT_H_
#define DIALOG_SMART_INDENT_H_

#include "Dialog.h"
#include "ui_DialogSmartIndent.h"

#include <QPointer>

class DocumentWidget;
class SmartIndentEntry;
class DialogLanguageModes;

class DialogSmartIndent final : public Dialog {
	Q_OBJECT
public:
	explicit DialogSmartIndent(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogSmartIndent() override = default;

public:
	void updateLanguageModes();
	void setLanguageMode(const QString &s);
	QString languageMode() const;

private:
	void setSmartIndentDialogData(const SmartIndentEntry *is);
	bool updateSmartIndentData();
	bool checkSmartIndentDialogData();
	SmartIndentEntry getSmartIndentDialogData() const;

private Q_SLOTS:
	void on_comboLanguageMode_currentIndexChanged(const QString &text);

private:
	void buttonCommon_clicked();
	void buttonLanguageMode_clicked();
	void buttonOK_clicked();
	void buttonApply_clicked();
	void buttonCheck_clicked();
	void buttonDelete_clicked();
	void buttonRestore_clicked();
	void buttonHelp_clicked();
	void connectSlots();

private:
	Ui::DialogSmartIndent ui;
	QString languageMode_;
	QPointer<DialogLanguageModes> dialogLanguageModes_;
};

#endif
