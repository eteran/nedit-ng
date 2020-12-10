
#ifndef DIALOG_REPLACE_H_
#define DIALOG_REPLACE_H_

#include "Dialog.h"
#include "Direction.h"
#include "SearchType.h"

#include <QPointer>
#include <boost/optional.hpp>

#include "ui_DialogReplace.h"

class DialogMultiReplace;
class DocumentWidget;
class MainWindow;

class DialogReplace : public Dialog {
	Q_OBJECT
	friend class DialogMultiReplace;

public:
	struct Fields {
		Direction direction;
		QString searchString;
		QString replaceString;
		SearchType searchType;
	};

public:
	DialogReplace(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogReplace() override = default;

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *ev) override;

public:
	bool keepDialog() const;
	void UpdateReplaceActionButtons();
	void initToggleButtons(SearchType searchType);
	void setActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceInWinBtn, bool replaceInSelBtn, bool replaceAllBtn);
	void setDirection(Direction direction);
	void setDocument(DocumentWidget *document);
	void setKeepDialog(bool keep);
	void setReplaceText(const QString &text);
	void setTextFieldFromDocument(DocumentWidget *document);
	void updateFindButton();

private:
	boost::optional<Fields> readFields();

private:
	void textFind_textChanged(const QString &text);
	void checkRegex_toggled(bool checked);
	void checkCase_toggled(bool checked);
	void checkKeep_toggled(bool checked);
	void buttonFind_clicked();
	void buttonReplace_clicked();
	void buttonReplaceFind_clicked();
	void buttonWindow_clicked();
	void buttonSelection_clicked();
	void buttonMulti_clicked();
	void connectSlots();

private:
	Ui::DialogReplace ui;
	MainWindow *window_;
	DocumentWidget *document_;
	bool lastRegexCase_   = true;  // idem, for regex mode in replace dialog
	bool lastLiteralCase_ = false; // idem, for literal mode
};

#endif
