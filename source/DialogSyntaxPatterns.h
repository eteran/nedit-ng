
#ifndef DIALOG_SYNTAX_PATTERNS_H_
#define DIALOG_SYNTAX_PATTERNS_H_

#include "Dialog.h"
#include "ui_DialogSyntaxPatterns.h"

#include <memory>

class HighlightPattern;
class HighlightPatternModel;
class MainWindow;
class PatternSet;

class DialogSyntaxPatterns : public Dialog {
	Q_OBJECT

private:
    enum class Mode {
        Silent,
        Verbose
    };

public:
    DialogSyntaxPatterns(MainWindow *window, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogSyntaxPatterns() noexcept override = default;

public:
	void setLanguageName(const QString &name);
	void UpdateLanguageModeMenu();
	void updateHighlightStyleMenu();
	void SetLangModeMenu(const QString &name);
	void RenameHighlightPattern(const QString &oldName, const QString &newName);
	bool LMHasHighlightPatterns(const QString &languageMode);

Q_SIGNALS:
    void restore(const QModelIndex &selection);

private Q_SLOTS:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous);
    void restoreSlot(const QModelIndex &index);

private Q_SLOTS:
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
    void on_comboLanguageMode_currentIndexChanged(const QString &text);

private:
    bool checkCurrentPattern(Mode mode);
    bool checkHighlightDialogData();
    bool TestHighlightPatterns(const std::unique_ptr<PatternSet> &patSet);
    bool updateCurrentItem();
    bool updateCurrentItem(const QModelIndex &index);
    bool updatePatternSet();
    std::unique_ptr<HighlightPattern> readDialogFields(Mode mode);
    std::unique_ptr<PatternSet> getDialogPatternSet();
    void setStyleMenu(const QString &name);
    void updateLabels();
    void updateButtonStates(const QModelIndex &current);
    void updateButtonStates();

private:
	Ui::DialogSyntaxPatterns ui;
    HighlightPatternModel *model_;
    QModelIndex deleted_;
	QString previousLanguage_;
    MainWindow *window_;
};

#endif
