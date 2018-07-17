
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
        Direction  direction;
        QString    searchString;
        QString    replaceString;
        SearchType searchType;
    };

public:
    DialogReplace(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogReplace() override = default;
	
protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;
	
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
    boost::optional<Fields> getFields();

private Q_SLOTS:
	void on_checkRegex_toggled(bool checked);
	void on_checkCase_toggled(bool checked);
	void on_checkKeep_toggled(bool checked);
	void on_textFind_textChanged(const QString &text);
	void on_buttonFind_clicked();
	void on_buttonReplace_clicked();
	void on_buttonReplaceFind_clicked();
	void on_buttonWindow_clicked();
	void on_buttonSelection_clicked();
	void on_buttonMulti_clicked();
	
private:
    Ui::DialogReplace ui;
    MainWindow *window_;
    DocumentWidget *document_;
    bool lastRegexCase_   = true;  // idem, for regex mode in replace dialog
    bool lastLiteralCase_ = false; // idem, for literal mode
};

#endif
