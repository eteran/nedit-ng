
#ifndef DIALOG_LANGUAGE_MODES_H_
#define DIALOG_LANGUAGE_MODES_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogLanguageModes.h"

#include <QPointer>

#include <optional>

class DialogSyntaxPatterns;
class LanguageMode;
class LanguageModeModel;

class DialogLanguageModes final : public Dialog {
public:
	Q_OBJECT

public:
	explicit DialogLanguageModes(DialogSyntaxPatterns *dialogSyntaxPatterns, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogLanguageModes() override = default;

Q_SIGNALS:
	void restore(const QModelIndex &selection);

private:
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
	void buttonBox_clicked(QAbstractButton *button);
	void buttonBox_accepted();
	void buttonUp_clicked();
	void buttonDown_clicked();
	void buttonDelete_clicked();
	void buttonCopy_clicked();
	void buttonNew_clicked();
	void connectSlots();

private:
	bool validateFields(Verbosity verbosity);
	bool updateCurrentItem();
	bool updateCurrentItem(const QModelIndex &index);
	bool updateLanguageList(Verbosity verbosity);
	bool updateLMList(Verbosity verbosity);
	std::optional<LanguageMode> readFields(Verbosity verbosity);
	int countLanguageModes(const QString &name) const;
	bool LMHasHighlightPatterns(const QString &name) const;

private:
	Ui::DialogLanguageModes ui;
	LanguageModeModel *model_;
	QModelIndex deleted_;
	QPointer<DialogSyntaxPatterns> dialogSyntaxPatterns_;
};

#endif
