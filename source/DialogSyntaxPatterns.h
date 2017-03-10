
#ifndef DIALOG_SYNTAX_PATTERNS_H_
#define DIALOG_SYNTAX_PATTERNS_H_

#include "Dialog.h"
#include <memory>
#include "ui_DialogSyntaxPatterns.h"

class PatternSet;
class HighlightPattern;

class DialogSyntaxPatterns : public Dialog {
	Q_OBJECT

private:
    enum class Mode {
        Silent,
        Verbose
    };

public:
	DialogSyntaxPatterns(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogSyntaxPatterns();

public:
	void setLanguageName(const QString &name);
	void UpdateLanguageModeMenu();
	void updateHighlightStyleMenu();
	void SetLangModeMenu(const QString &name);
	void RenameHighlightPattern(const QString &oldName, const QString &newName);
	bool LMHasHighlightPatterns(const QString &languageMode);

public Q_SLOTS:
	void on_buttonLanguageMode_clicked();
	void on_buttonHighlightStyle_clicked();
	void on_buttonNew_clicked();
	void on_buttonDelete_clicked();
	void on_buttonCopy_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_buttonOK_clicked();
	void on_buttonApply_clicked();
	void on_buttonCheck_clicked();
	void on_buttonDeletePattern_clicked();
	void on_buttonRestore_clicked();
	void on_buttonHelp_clicked();
	void on_listItems_itemSelectionChanged();
	void on_comboLanguageMode_currentIndexChanged(const QString &currentText);

private Q_SLOTS:
	void updateLabels();

private:
	bool updatePatternSet();
	bool checkHighlightDialogData();
	void setStyleMenu(const QString &name);
	void setLanguageMenu(const QString &name);
    PatternSet *getDialogPatternSet();
	HighlightPattern *itemFromIndex(int i) const;
    HighlightPattern *readDialogFields(Mode mode);
    bool checkCurrentPattern(Mode mode);
	bool updateCurrentItem();
	bool updateCurrentItem(QListWidgetItem *item);
    bool TestHighlightPatterns(PatternSet *patSet);

private:
	Ui::DialogSyntaxPatterns ui;
	QListWidgetItem *previous_;
	QString previousLanguage_;
};

#endif
