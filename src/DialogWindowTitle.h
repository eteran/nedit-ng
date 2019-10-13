
#ifndef DIALOG_WINDOW_TITLE_H_
#define DIALOG_WINDOW_TITLE_H_

#include "Dialog.h"
#include "LockReasons.h"
#include "ui_DialogWindowTitle.h"

class DocumentWidget;
class MainWindow;
struct UpdateState;

class DialogWindowTitle final : public Dialog {
	Q_OBJECT
public:
    explicit DialogWindowTitle(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogWindowTitle() override = default;

public:	
	static QString formatWindowTitle(DocumentWidget *document, const QString &clearCaseViewTag, const QString &serverName, bool isServer, const QString &format);

private:
	static QString formatWindowTitleInternal(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &format, UpdateState *state);

private:
	void connectSlots();
	void editDirectory_textChanged(const QString &text);
	void editFormat_textChanged(const QString &text);
	void buttonBox_clicked(QAbstractButton *button);
	void checkFileName_toggled(bool checked);
	void checkHostName_toggled(bool checked);
	void checkFileStatus_toggled(bool checked);
	void checkBrief_toggled(bool checked);
	void checkUserName_toggled(bool checked);
	void checkClearCase_toggled(bool checked);
	void checkServerName_toggled(bool checked);
	void checkDirectory_toggled(bool checked);
	void checkFileModified_toggled(bool checked);
	void checkFileReadOnly_toggled(bool checked);
	void checkFileLocked_toggled(bool checked);
	void checkServerNamePresent_toggled(bool checked);
	void checkClearCasePresent_toggled(bool checked);
	void checkServerEqualsCC_toggled(bool checked);
	void checkDirectoryPresent_toggled(bool checked);

private:
	void setToggleButtons();
	void formatChangedCB();
	QString formatWindowTitleAndUpdate(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat);
	void removeFromFormat(const QString &string);
	void appendToFormat(const QString &string);

private:
	Ui::DialogWindowTitle ui;

	QString filename_;
	QString path_;
	QString viewTag_;
	QString serverName_;
	LockReasons lockReasons_;
	bool isServer_;
	bool filenameSet_;
	bool fileChanged_;
	bool suppressFormatUpdate_;
};

#endif
