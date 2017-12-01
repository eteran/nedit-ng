
#include "DialogFonts.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "preferences.h"

#include <QFontDialog>

/**
 * @brief DialogFonts::DialogFonts
 * @param document
 * @param parent
 * @param f
 */
DialogFonts::DialogFonts(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), document_(document) {
	ui.setupUi(this);

    if(!document_) {
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Apply));
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Close));
		ui.buttonBox->addButton(QDialogButtonBox::Cancel);
	}

	// Set initial values
    if (document_) {
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

/**
 * @brief DialogFonts::on_buttonPrimaryFont_clicked
 */
void DialogFonts::on_buttonPrimaryFont_clicked() {
	browseFont(ui.editFontPrimary);
}

/**
 * @brief DialogFonts::on_buttonFontItalic_clicked
 */
void DialogFonts::on_buttonFontItalic_clicked() {
	browseFont(ui.editFontItalic);
}

/**
 * @brief DialogFonts::on_buttonFontBold_clicked
 */
void DialogFonts::on_buttonFontBold_clicked() {
	browseFont(ui.editFontBold);
}

/**
 * @brief DialogFonts::on_buttonFontBoldItalic_clicked
 */
void DialogFonts::on_buttonFontBoldItalic_clicked() {
	browseFont(ui.editFontBoldItalic);
}

/**
 * @brief DialogFonts::on_buttonFill_clicked
 */
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

/**
 * @brief DialogFonts::on_buttonBox_clicked
 * @param button
 */
void DialogFonts::on_buttonBox_clicked(QAbstractButton *button) {

	if(button == ui.buttonBox->button(QDialogButtonBox::Ok)) {
		updateFonts();
		accept();
	} else if(button == ui.buttonBox->button(QDialogButtonBox::Apply)) {
		updateFonts();
	} else {
		reject();
	}
}


/**
 * Accept the changes in the dialog and set the fonts regardless of errors
 *
 * @brief DialogFonts::updateFonts
 */
void DialogFonts::updateFonts() {

	QString fontName       = ui.editFontPrimary->text();
	QString italicName     = ui.editFontItalic->text();
	QString boldName       = ui.editFontBold->text();
	QString boldItalicName = ui.editFontBoldItalic->text();

    if (document_) {
        document_->action_Set_Fonts(fontName, italicName, boldName, boldItalicName);
	} else {
        SetPrefFont(fontName);
        SetPrefItalicFont(italicName);
        SetPrefBoldFont(boldName);
        SetPrefBoldItalicFont(boldItalicName);
	}
}

/**
 * @brief DialogFonts::on_editFontPrimary_textChanged
 * @param text
 */
void DialogFonts::on_editFontPrimary_textChanged(const QString &text) {
	Q_UNUSED(text);

	showFontStatus(ui.editFontItalic->text(),     ui.labelItalicError);
	showFontStatus(ui.editFontBold->text(),       ui.labelBoldError);
	showFontStatus(ui.editFontBoldItalic->text(), ui.labelBoldItalicError);

}

/**
 * @brief DialogFonts::on_editFontItalic_textChanged
 * @param text
 */
void DialogFonts::on_editFontItalic_textChanged(const QString &text) {
	showFontStatus(text, ui.labelItalicError);
}

/**
 * @brief DialogFonts::on_editFontBold_textChanged
 * @param text
 */
void DialogFonts::on_editFontBold_textChanged(const QString &text) {
	showFontStatus(text, ui.labelBoldError);
}

/**
 * @brief DialogFonts::on_editFontBoldItalic_textChanged
 * @param text
 */
void DialogFonts::on_editFontBoldItalic_textChanged(const QString &text) {
	showFontStatus(text, ui.labelBoldItalicError);
}

/**
 * Update the error label for a font text field to reflect its validity and
 * degree of agreement with the currently selected primary font
 *
 * @brief DialogFonts::showFontStatus
 * @param font
 * @param errorLabel
 * @return
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


/**
 * Check over a font name in a text field to make sure it agrees with the
 * primary font in height and spacing.
 *
 * @brief DialogFonts::checkFontStatus
 * @param font
 * @return
 */
DialogFonts::FontStatus DialogFonts::checkFontStatus(const QString &font) {

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

    bool ok;
    auto currFont = Font::fromString(lineEdit->text());
    QFont newFont = QFontDialog::getFont(&ok, currFont, this);

    if(ok) {
        newFont.setStyleName(QString());
        lineEdit->setText(newFont.toString());
    }
}
