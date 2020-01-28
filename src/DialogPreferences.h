
#ifndef DIALOG_PREFERENCES_H_
#define DIALOG_PREFERENCES_H_

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QColor>
#include <QString>
#include "ui_DialogPreferences.h"

class DialogPreferences : public QDialog {
	Q_OBJECT
public:
	explicit DialogPreferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPreferences() override = default;

private:
	void setupFonts();
	void setupColors();
	void setupDisplay();
	void setupBackup();

private:
	void applyFonts();
	void applyColors();
	void applyDisplay();
	void applyBackup();

private:
	void buttonBoxClicked(QAbstractButton *button);
	void applySettings();
	void acceptDialog();
	QColor chooseColor(QPushButton *button, const QColor &currentColor);

private:
	QColor textFG_;
	QColor textBG_;
	QColor selectionFG_;
	QColor selectionBG_;
	QColor matchFG_;
	QColor matchBG_;
	QColor lineNumbersFG_;
	QColor lineNumbersBG_;
	QColor cursorFG_;
	Ui::DialogPreferences ui;
};

#endif
