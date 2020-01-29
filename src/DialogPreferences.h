
#ifndef DIALOG_PREFERENCES_H_
#define DIALOG_PREFERENCES_H_

#include "ui_DialogPreferences.h"
#include <QAbstractButton>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QString>

class DialogPreferences final : public QDialog {
	Q_OBJECT
public:
	explicit DialogPreferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPreferences() override = default;

private:
	void setupFonts();
	void setupColors();
	void setupDisplay();
	void setupBackup();
	void setupMatching();
	void setupShell();
	void setupBehavior();
	void setupWrap();

private:
	bool validateFonts() { return true; }
	bool validateColors() { return true; }
	bool validateDisplay() { return true; }
	bool validateBackup() { return true; }
	bool validateMatching() { return true; }
	bool validateBehavior() { return true; }
	bool validateShell();
	bool validateWrap() { return true; }

private:
	void applyFonts();
	void applyColors();
	void applyDisplay();
	void applyBackup();
	void applyMatching();
	void applyShell();
	void applyBehavior();
	void applyWrap();

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
