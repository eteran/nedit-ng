
#ifndef DIALOG_FIND_H_
#define DIALOG_FIND_H_

#include "Dialog.h"
#include "Direction.h"
#include "SearchType.h"

#include <boost/optional.hpp>

#include "ui_DialogFind.h"

class DocumentWidget;
class MainWindow;

class DialogFind : public Dialog {
	Q_OBJECT

public:
	struct Fields {
		Direction direction;
		QString searchString;
		SearchType searchType;
	};

public:
	DialogFind(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogFind() override = default;

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;

public:
	void initToggleButtons(SearchType searchType);
	bool keepDialog() const;
	void setDirection(Direction direction);
	void setDocument(DocumentWidget *document);
	void setKeepDialog(bool keep);
	void setTextFieldFromDocument(DocumentWidget *document);
	void updateFindButton();

private:
	boost::optional<Fields> readFields();

private:
	void textFind_textChanged(const QString &text);
	void buttonFind_clicked();
	void checkRegex_toggled(bool checked);
	void checkCase_toggled(bool checked);
	void checkKeep_toggled(bool checked);
	void connectSlots();

private:
	Ui::DialogFind ui;
	MainWindow *window_;
	DocumentWidget *document_;
	bool lastRegexCase_   = true;
	bool lastLiteralCase_ = false;
};

#endif
