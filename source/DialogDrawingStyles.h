
#ifndef DIALOG_DRAWING_STYLES_H_
#define DIALOG_DRAWING_STYLES_H_

#include "Dialog.h"
#include "Verbosity.h"
#include "ui_DialogDrawingStyles.h"

#include <memory>

class DialogSyntaxPatterns;
class HighlightStyleModel;
struct HighlightStyle;

class DialogDrawingStyles final : public Dialog {
	Q_OBJECT

public:
    DialogDrawingStyles(DialogSyntaxPatterns *dialogSyntaxPatterns, std::vector<HighlightStyle> &highlightStyles, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogDrawingStyles() noexcept override = default;

public:
	void setStyleByName(const QString &name);

Q_SIGNALS:
    void restore(const QModelIndex &selection);

private Q_SLOTS:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous);
    void restoreSlot(const QModelIndex &index);

private Q_SLOTS:
	void on_buttonNew_clicked();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
    void on_buttonForeground_clicked();
    void on_buttonBackground_clicked();
	
private:
    void updateButtonStates(const QModelIndex &current);
    void updateButtonStates();
    bool applyDialogChanges();
    bool validateFields(Verbosity verbosity);
    std::unique_ptr<HighlightStyle> readFields(Verbosity verbosity);
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
