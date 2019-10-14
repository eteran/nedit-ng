
#ifndef DIALOG_DRAWING_STYLES_H_
#define DIALOG_DRAWING_STYLES_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogDrawingStyles.h"

#include <boost/optional.hpp>

class DialogSyntaxPatterns;
class HighlightStyleModel;
struct HighlightStyle;

class DialogDrawingStyles final : public Dialog {
	Q_OBJECT

public:
	DialogDrawingStyles(DialogSyntaxPatterns *dialogSyntaxPatterns, std::vector<HighlightStyle> &highlightStyles, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogDrawingStyles() override = default;

public:
	void setStyleByName(const QString &name);

Q_SIGNALS:
	void restore(const QModelIndex &selection);

private:
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);
	void buttonBox_clicked(QAbstractButton *button);
	void buttonBox_accepted();
	void buttonNew_clicked();
	void buttonCopy_clicked();
	void buttonDelete_clicked();
	void buttonUp_clicked();
	void buttonDown_clicked();
	void buttonForeground_clicked();
	void buttonBackground_clicked();
	void connectSlots();

private:
	bool applyDialogChanges();
	bool validateFields(Verbosity verbosity);
	boost::optional<HighlightStyle> readFields(Verbosity verbosity);
	bool updateCurrentItem();
	bool updateCurrentItem(const QModelIndex &index);
	int countPlainEntries() const;
	void chooseColor(QLineEdit *edit);

private:
	Ui::DialogDrawingStyles ui;
	HighlightStyleModel *model_;
	std::vector<HighlightStyle> &highlightStyles_;
	QModelIndex deleted_;
	DialogSyntaxPatterns *dialogSyntaxPatterns_;
};

#endif
