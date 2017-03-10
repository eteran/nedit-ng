
#ifndef DIALOG_DRAWING_STYLES_H_
#define DIALOG_DRAWING_STYLES_H_

#include "Dialog.h"
#include <memory>
#include "ui_DialogDrawingStyles.h"

class HighlightStyle;

class DialogDrawingStyles : public Dialog {
	Q_OBJECT

private:
    enum class Mode {
        Silent,
        Verbose
    };

public:
	DialogDrawingStyles(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogDrawingStyles();

public Q_SLOTS:
	void setStyleByName(const QString &name);

private Q_SLOTS:
	void on_listItems_itemSelectionChanged();
	void on_buttonNew_clicked();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
	
private:
	bool updateHSList();
    bool checkCurrent(Mode mode);
    std::unique_ptr<HighlightStyle> readDialogFields(Mode mode);
	HighlightStyle *itemFromIndex(int i) const;
	bool updateCurrentItem();
	bool updateCurrentItem(QListWidgetItem *item);		

private:
	Ui::DialogDrawingStyles ui;
	QListWidgetItem *previous_;
};

#endif
