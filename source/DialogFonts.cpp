
#include "DialogFonts.h"
#include "DocumentWidget.h"
#include "TextArea.h"
#include "nedit.h"
#include "Font.h"
#include "preferences.h"
#include <QFontDialog>
#include <QPushButton>

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
DialogFonts::DialogFonts(DocumentWidget *document, bool forWindow, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), document_(document), forWindow_(forWindow) {
	ui.setupUi(this);

	if(!forWindow) {
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Apply));
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Close));
		ui.buttonBox->addButton(QDialogButtonBox::Cancel);
	}

	// Set initial values
	if (forWindow) {
        ui.editFontPrimary->setText(document->fontName_);
        ui.editFontBold->setText(document->boldFontName_);
        ui.editFontItalic->setText(document->italicFontName_);
        ui.editFontBoldItalic->setText(document->boldItalicFontName_);
	} else {
		ui.editFontPrimary->setText(GetPrefFontName());
		ui.editFontBold->setText(GetPrefBoldFontName());
		ui.editFontItalic->setText(GetPrefItalicFontName());
		ui.editFontBoldItalic->setText(GetPrefBoldItalicFontName());
	}	
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonPrimaryFont_clicked() {
	browseFont(ui.editFontPrimary);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFontItalic_clicked() {
	browseFont(ui.editFontItalic);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFontBold_clicked() {
	browseFont(ui.editFontBold);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFontBoldItalic_clicked() {
	browseFont(ui.editFontBoldItalic);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFill_clicked() {

    QFont font;
    font = Font::fromString(ui.editFontPrimary->text());

    QFont boldFont(font);
    boldFont.setBold(true);
    boldFont.setItalic(false);
    ui.editFontBold->setText(boldFont.toString());

    QFont italicFont(font);
    italicFont.setItalic(true);
    italicFont.setBold(false);
    ui.editFontItalic->setText(italicFont.toString());

    QFont boldItalicFont(font);
    boldItalicFont.setBold(true);
    boldItalicFont.setItalic(true);
    ui.editFontBoldItalic->setText(boldItalicFont.toString());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonBox_clicked(QAbstractButton *button) {
	Q_UNUSED(button);

	if(button == ui.buttonBox->button(QDialogButtonBox::Ok)) {
		updateFonts();
		accept();
	} else if(button == ui.buttonBox->button(QDialogButtonBox::Apply)) {
		updateFonts();
	} else {
		reject();
	}
}


/*
** Accept the changes in the dialog and set the fonts regardless of errors
*/
void DialogFonts::updateFonts() {

	QString fontName       = ui.editFontPrimary->text();
	QString italicName     = ui.editFontItalic->text();
	QString boldName       = ui.editFontBold->text();
	QString boldItalicName = ui.editFontBoldItalic->text();

	if (forWindow_) {
        document_->SetFonts(fontName, italicName, boldName, boldItalicName);
	} else {
        SetPrefFont(fontName);
        SetPrefItalicFont(italicName);
        SetPrefBoldFont(boldName);
        SetPrefBoldItalicFont(boldItalicName);
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_editFontPrimary_textChanged(const QString &text) {
	Q_UNUSED(text);

	showFontStatus(ui.editFontItalic->text(),     ui.labelItalicError);
	showFontStatus(ui.editFontBold->text(),       ui.labelBoldError);
	showFontStatus(ui.editFontBoldItalic->text(), ui.labelBoldItalicError);

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_editFontItalic_textChanged(const QString &text) {
	showFontStatus(text, ui.labelItalicError);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_editFontBold_textChanged(const QString &text) {
	showFontStatus(text, ui.labelBoldError);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_editFontBoldItalic_textChanged(const QString &text) {
	showFontStatus(text, ui.labelBoldItalicError);
}

/*
** Update the error label for a font text field to reflect its validity and degree
** of agreement with the currently selected primary font
*/
DialogFonts::FontStatus DialogFonts::showFontStatus(const QString &font, QLabel *errorLabel) {

	QString msg;

	FontStatus status = checkFontStatus(font);
	switch(status) {
	case BAD_PRIMARY:
		msg = tr("(font below may not match primary font)");
		break;
	case BAD_FONT:
		msg = tr("(xxx font below is invalid xxx)");
		break;
	case BAD_SIZE:
		msg = tr("(height of font below does not match primary)");
		break;
	case BAD_SPACING:
		msg = tr("(spacing of font below does not match primary)");
		break;
	default:
		msg = QString();
		break;
	}

	errorLabel->setText(msg);
	return status;
}

/*
** Check over a font name in a text field to make sure it agrees with the
** primary font in height and spacing.
*/
DialogFonts::FontStatus DialogFonts::checkFontStatus(const QString &font) {


	/* Get width and height of the font to check.  Note the test for empty
	   name: X11R6 clients freak out X11R5 servers if they ask them to load
	   an empty font name, and kill the whole application! */
	QString testName = font;
	if (testName.isEmpty()) {
		return BAD_FONT;
	}

    QFont f = Font::fromString(testName);
    QFontMetrics fm(f);

    const int testWidth  = fm.width(QLatin1Char('i')); // NOTE(eteran): min-width?
    const int testHeight = fm.lineSpacing();


	// Get width and height of the primary font 
	QString primaryName = ui.editFontPrimary->text();
	if (primaryName.isEmpty()) {
		return BAD_FONT;
    }

    QFont primaryFont = Font::fromString(primaryName);	
    QFontMetrics primaryFm(primaryFont);

    const int primaryWidth  = primaryFm.width(QLatin1Char('i')); // NOTE(eteran): min-width?
    const int primaryHeight = primaryFm.lineSpacing();

	// Compare font information 
	if (testWidth != primaryWidth) {
		return BAD_SPACING;
	}
		
	if (testHeight != primaryHeight) {
		return BAD_SIZE;
	}

	return GOOD_FONT;
}

/*
** Put up a font selector panel to set the font name in the text widget "fontTextW"
*/
void DialogFonts::browseFont(QLineEdit *lineEdit) {

    // NOTE(eteran): there is a bug in pre Qt-5.8 where a font with no "normal"
    //               style, (ex. sometimes they call it "regular") will default
    //               to the first style in the list
    bool ok;
    auto currFont = Font::fromString(lineEdit->text());
    QFont newFont = QFontDialog::getFont(&ok, currFont, this);

    if(ok) {
        lineEdit->setText(newFont.toString());
    }
}
