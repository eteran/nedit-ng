
#ifndef DIALOG_DRAWING_STYLES_H_
#define DIALOG_DRAWING_STYLES_H_

#include <QDialog>
#include "ui_DialogDrawingStyles.h"

class HighlightStyle;

class DialogDrawingStyles : public QDialog {
	Q_OBJECT
public:
	DialogDrawingStyles(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogDrawingStyles();

public Q_SLOTS:
	void setStyleByName(const QString &name);

private Q_SLOTS:
	void on_listStyles_itemSelectionChanged();
	void on_buttonNew_clicked();
	void on_buttonCopy_clicked();
	void on_buttonDelete_clicked();
	void on_buttonUp_clicked();
	void on_buttonDown_clicked();

	void on_buttonBox_clicked(QAbstractButton *button);
	void on_buttonBox_accepted();
	
private:
	bool updateHSList();
	bool checkCurrent(bool silent);
	HighlightStyle *readDialogFields(bool silent);
	HighlightStyle *itemFromIndex(int i) const;

private:
	Ui::DialogDrawingStyles ui;
	QListWidgetItem *previous_;
	QString                 styleName_;
};

#endif
