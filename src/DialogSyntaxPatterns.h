
#ifndef DIALOG_SYNTAX_PATTERNS_H_
#define DIALOG_SYNTAX_PATTERNS_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogSyntaxPatterns.h"

#include <QPointer>

#include <optional>
#include <memory>

class HighlightPattern;
class HighlightPatternModel;
class PatternSet;
class DialogDrawingStyles;
class DialogLanguageModes;

class DialogSyntaxPatterns final : public Dialog {
	Q_OBJECT

public:
	explicit DialogSyntaxPatterns(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogSyntaxPatterns() override = default;

public:
	void setLanguageName(const QString &name);
	void UpdateLanguageModeMenu();
	void updateHighlightStyleMenu();
	void SetLangModeMenu(const QString &name);
	void RenameHighlightPattern(const QString &oldName, const QString &newName);
	bool LMHasHighlightPatterns(const QString &languageMode);

Q_SIGNALS:
	void restore(const QModelIndex &selection);

private:
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);

private Q_SLOTS:
	void on_comboLanguageMode_currentIndexChanged(const QString &text);

private:
	void buttonLanguageMode_clicked();
	void buttonHighlightStyle_clicked();
	void buttonNew_clicked();
	void buttonDelete_clicked();
	void buttonCopy_clicked();
	void buttonUp_clicked();
	void buttonDown_clicked();
	void buttonOK_clicked();
	void buttonApply_clicked();
	void buttonCheck_clicked();
	void buttonDeletePattern_clicked();
	void buttonRestore_clicked();
	void buttonHelp_clicked();
	void connectSlots();

private:
	bool validateFields(Verbosity verbosity);
	bool checkHighlightDialogData();
	bool TestHighlightPatterns(const std::unique_ptr<PatternSet> &patSet);
	bool updateCurrentItem();
	bool updateCurrentItem(const QModelIndex &index);
	bool updatePatternSet();
	std::optional<HighlightPattern> readFields(Verbosity verbosity);
	std::unique_ptr<PatternSet> getDialogPatternSet();
	void setStyleMenu(const QString &name);
	void updateLabels();

private:
	Ui::DialogSyntaxPatterns ui;
	HighlightPatternModel *model_;
	QModelIndex deleted_;
	QString previousLanguage_;
	QPointer<DialogDrawingStyles> dialogDrawingStyles_;
	QPointer<DialogLanguageModes> dialogLanguageModes_;
};

#endif
