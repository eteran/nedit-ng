
#ifndef DIALOG_WINDOW_TITLE_H_
#define DIALOG_WINDOW_TITLE_H_

#include <QDialog>
#include <QString>
#include "LockReasons.h"
#include "ui_DialogWindowTitle.h"

class Document;
struct UpdateState;

class DialogWindowTitle : public QDialog {
	Q_OBJECT
public:
	DialogWindowTitle(Document *window, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogWindowTitle();

private Q_SLOTS:
	void on_checkFileName_toggled(bool checked);
	void on_checkHostName_toggled(bool checked);
	void on_checkFileStatus_toggled(bool checked);
	void on_checkBrief_toggled(bool checked);
	void on_checkUserName_toggled(bool checked);
	void on_checkClearCase_toggled(bool checked);
	void on_checkServerName_toggled(bool checked);
	void on_checkDirectory_toggled(bool checked);
	void on_checkFileModified_toggled(bool checked);
	void on_checkFileReadOnly_toggled(bool checked);
	void on_checkFileLocked_toggled(bool checked);
	void on_checkServerNamePresent_toggled(bool checked);
	void on_checkClearCasePresent_toggled(bool checked);
	void on_checkServerEqualsCC_toggled(bool checked);
	void on_checkDirectoryPresent_toggled(bool checked);
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_editDirectory_textChanged(const QString &text);
	void on_editFormat_textChanged(const QString &text);

public:
	static QString FormatWindowTitle(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat);

private:
	static QString compressWindowTitle(const QString &title);
	static QString FormatWindowTitleInternal(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat, UpdateState *state);

private:
	void setToggleButtons();
	void formatChangedCB();
	QString FormatWindowTitleEx(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat);
	void removeFromFormat(const QString &string);
	void appendToFormat(const QString &string);

private:
	Ui::DialogWindowTitle ui;
	
	QString filename_;
	QString path_;
	QString viewTag_;
	QString serverName_;
	bool isServer_;
	bool filenameSet_;
	LockReasons lockReasons_;
	bool fileChanged_;

	bool suppressFormatUpdate_;
	bool inConstructor_;
};

#endif
