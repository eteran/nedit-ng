
#ifndef DIALOG_LANGUAGE_MODES_H_
#define DIALOG_LANGUAGE_MODES_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogLanguageModes.h"

#include <memory>

class DialogSyntaxPatterns;
class LanguageMode;
class LanguageModeModel;

class DialogLanguageModes final : public Dialog {
public:
	Q_OBJECT

public:
    DialogLanguageModes(DialogSyntaxPatterns *dialogSyntaxPatterns, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogLanguageModes() noexcept override = default;

Q_SIGNALS:
    void restore(const QModelIndex &selection);

private Q_SLOTS:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous);
    void restoreSlot(const QModelIndex &index);

private Q_SLOTS:
	void on_buttonBox_accepted();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_buttonDelete_clicked();
	void on_buttonCopy_clicked();
	void on_buttonNew_clicked();
	
private:
    bool validateFields(Verbosity verbosity);
    bool updateCurrentItem();
    bool updateCurrentItem(const QModelIndex &index);
    bool updateLanguageList(Verbosity verbosity);
    bool updateLMList(Verbosity verbosity);
    std::unique_ptr<LanguageMode> readFields(Verbosity verbosity);
    void updateButtonStates();
    void updateButtonStates(const QModelIndex &current);
    int countLanguageModes(const QString &name) const;
    bool LMHasHighlightPatterns(const QString &name) const;

private:
	Ui::DialogLanguageModes ui;
    LanguageModeModel *model_;
    QModelIndex deleted_;
    DialogSyntaxPatterns *dialogSyntaxPatterns_;
};

#endif
