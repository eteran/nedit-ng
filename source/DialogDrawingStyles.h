
#ifndef DIALOG_DRAWING_STYLES_H_
#define DIALOG_DRAWING_STYLES_H_

#include "Dialog.h"
#include "HighlightStyle.h"
#include "ui_DialogDrawingStyles.h"

#include <memory>

class HighlightStyleModel;

class DialogDrawingStyles : public Dialog {
	Q_OBJECT

private:
    enum class Mode {
        Silent,
        Verbose
    };

public:
    DialogDrawingStyles(std::vector<HighlightStyle> &highlightStyles, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
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
	
private:
    void updateButtonStates(const QModelIndex &current);
    void updateButtonStates();
    bool applyDialogChanges();
    bool checkCurrent(Mode mode);
    std::unique_ptr<HighlightStyle> readDialogFields(Mode mode);
	bool updateCurrentItem();
    bool updateCurrentItem(const QModelIndex &index);
    int countPlainEntries() const;

private:
	Ui::DialogDrawingStyles ui;
    HighlightStyleModel *model_;
    std::vector<HighlightStyle> &highlightStyles_;
    QModelIndex deleted_;
};

#endif
