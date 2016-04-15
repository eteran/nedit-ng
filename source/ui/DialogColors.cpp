
#include "DialogColors.h"
#include <QColorDialog>

#include "preferences.h"
#include "Document.h"
#include "highlight.h"

// TODO(eteran): use QColor for all the validation and all that... eventually

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogColors::DialogColors(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window) {
	ui.setupUi(this);
	
    ui.labelErrorFG->setVisible(false);
    ui.labelErrorSelectionFG->setVisible(false);
    ui.labelErrorMatchFG->setVisible(false);
    ui.labelErrorLineNumbers->setVisible(false);
    ui.labelErrorBG->setVisible(false);
    ui.labelErrorSelectionBG->setVisible(false);
    ui.labelErrorMatchBG->setVisible(false);
    ui.labelErrorCursor->setVisible(false);

	
	ui.editFG->setText(QLatin1String(GetPrefColorName(TEXT_FG_COLOR)));
	ui.editBG->setText(QLatin1String(GetPrefColorName(TEXT_BG_COLOR)));
	ui.editSelectionFG->setText(QLatin1String(GetPrefColorName(SELECT_FG_COLOR)));
	ui.editSelectionBG->setText(QLatin1String(GetPrefColorName(SELECT_BG_COLOR)));
	ui.editMatchFG->setText(QLatin1String(GetPrefColorName(HILITE_FG_COLOR)));
	ui.editMatchBG->setText(QLatin1String(GetPrefColorName(HILITE_BG_COLOR)));
	ui.editLineNumbers->setText(QLatin1String(GetPrefColorName(LINENO_FG_COLOR)));
	ui.editCursor->setText(QLatin1String(GetPrefColorName(CURSOR_FG_COLOR)));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogColors::~DialogColors() {

}

//------------------------------------------------------------------------------
// Name: 
// Desc: Returns True if the color is valid, False if it's not 
//------------------------------------------------------------------------------
bool DialogColors::checkColorStatus(const QString &text) {
	Colormap cMap;
	XColor colorDef;
	Status status;
	Display *display = XtDisplay(window_->shell_);

	XtVaGetValues(window_->shell_, XtNcolormap, &cMap, nullptr);
	status = XParseColor(display, cMap, text.toLatin1().data(), &colorDef);

	return (status != 0);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::chooseColor(QLineEdit *edit) {

	QString name = edit->text();
	Color c;
	AllocColor(window_->shell_, name.toLatin1().data(), &c);

	QColor initColor(c.r / 256, c.g / 256, c.b / 256);
	QColor color = QColorDialog::getColor(initColor, this);
	if(color.isValid()) {
		edit->setText(tr("#%1").arg((color.rgb() & 0x00ffffff), 6, 16, QLatin1Char('0')));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::showColorStatus(const QString &text, QLabel *label) {
	/* Should set the OK/Apply button sensitivity here, instead
	   of leaving is sensitive and then complaining if an error. */
	
	label->setVisible(!checkColorStatus(text));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonFG_clicked() {
	chooseColor(ui.editFG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonBG_clicked() {
	chooseColor(ui.editBG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonSelectionFG_clicked() {
	chooseColor(ui.editSelectionFG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonSelectionBG_clicked() {
	chooseColor(ui.editSelectionBG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonMatchFG_clicked() {
	chooseColor(ui.editMatchFG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonMatchBG_clicked() {
	 chooseColor(ui.editMatchBG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonLineNumbers_clicked() {
	chooseColor(ui.editLineNumbers);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_buttonCursor_clicked() {
	chooseColor(ui.editCursor);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editFG_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorFG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editBG_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorBG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editSelectionFG_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorSelectionFG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editSelectionBG_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorSelectionBG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editMatchFG_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorMatchFG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editMatchBG_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorMatchBG);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editLineNumbers_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorLineNumbers);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogColors::on_editCursor_textChanged(const QString &text) {
	showColorStatus(text, ui.labelErrorCursor);
}

