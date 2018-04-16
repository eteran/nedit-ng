
#include "DialogFonts.h"
#include "DocumentWidget.h"
#include "preferences.h"

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
        ui.fontPrimary->setCurrentFont(document->fontName_);
        ui.fontBold->setCurrentFont(document->boldFontName_);
        ui.fontItalic->setCurrentFont(document->italicFontName_);
        ui.fontBoldItalic->setCurrentFont(document->boldItalicFontName_);
	} else {
        ui.fontPrimary->setCurrentFont(Preferences::GetPrefFontName());
        ui.fontBold->setCurrentFont(Preferences::GetPrefBoldFontName());
        ui.fontItalic->setCurrentFont(Preferences::GetPrefItalicFontName());
        ui.fontBoldItalic->setCurrentFont(Preferences::GetPrefBoldItalicFontName());
    }
}

/**
 * @brief DialogFonts::on_buttonFill_clicked
 */
void DialogFonts::on_buttonFill_clicked() {

    QFont font = ui.fontPrimary->currentFont();

    QFont boldFont(font);
    boldFont.setBold(true);
    boldFont.setItalic(false);
    ui.fontBold->setCurrentFont(boldFont);

    QFont italicFont(font);
    italicFont.setItalic(true);
    italicFont.setBold(false);
    ui.fontItalic->setCurrentFont(italicFont);

    QFont boldItalicFont(font);
    boldItalicFont.setBold(true);
    boldItalicFont.setItalic(true);
    ui.fontBoldItalic->setCurrentFont(boldItalicFont);
}

/**
 * @brief DialogFonts::on_buttonBox_clicked
 * @param button
 */
void DialogFonts::on_buttonBox_clicked(QAbstractButton *button) {

    switch(ui.buttonBox->standardButton(button)) {
    case QDialogButtonBox::Ok:
        updateFonts();
        accept();
        break;
    case QDialogButtonBox::Apply:
        updateFonts();
        break;
    default:
        reject();
        break;
    }
}


/**
 * Accept the changes in the dialog and set the fonts regardless of errors
 *
 * @brief DialogFonts::updateFonts
 */
void DialogFonts::updateFonts() {

    QString fontName       = ui.fontPrimary->currentFont().toString();
    QString italicName     = ui.fontItalic->currentFont().toString();
    QString boldName       = ui.fontBold->currentFont().toString();
    QString boldItalicName = ui.fontBoldItalic->currentFont().toString();

    if (document_) {
        document_->action_Set_Fonts(fontName, italicName, boldName, boldItalicName);
	} else {
        Preferences::SetPrefFont(fontName);
        Preferences::SetPrefItalicFont(italicName);
        Preferences::SetPrefBoldFont(boldName);
        Preferences::SetPrefBoldItalicFont(boldItalicName);
	}
}

/**
 * @brief DialogFonts::on_fontPrimary_textChanged
 * @param text
 */
void DialogFonts::on_fontPrimary_currentFontChanged(const QFont &font) {
    Q_UNUSED(font);

    showFontStatus(ui.fontItalic->currentFont(),     ui.labelItalicError);
    showFontStatus(ui.fontBold->currentFont(),       ui.labelBoldError);
    showFontStatus(ui.fontBoldItalic->currentFont(), ui.labelBoldItalicError);

}

/**
 * @brief DialogFonts::on_fontItalic_textChanged
 * @param text
 */
void DialogFonts::on_fontItalic_currentFontChanged(const QFont &font) {
    showFontStatus(font, ui.labelItalicError);
}

/**
 * @brief DialogFonts::on_fontBold_textChanged
 * @param text
 */
void DialogFonts::on_fontBold_currentFontChanged(const QFont &font) {
    showFontStatus(font, ui.labelBoldError);
}

/**
 * @brief DialogFonts::on_fontBoldItalic_textChanged
 * @param text
 */
void DialogFonts::on_fontBoldItalic_currentFontChanged(const QFont &font) {
    showFontStatus(font, ui.labelBoldItalicError);
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
void DialogFonts::showFontStatus(const QFont &font, QLabel *errorLabel) {

    switch(checkFontStatus(font)) {
    case BAD_SIZE:
        errorLabel->setText(tr("(height of font below does not match primary)"));
        break;
    case BAD_SPACING:
        errorLabel->setText(tr("(spacing of font below does not match primary)"));
        break;
    default:
        errorLabel->setText(QString());
        break;
    }
}


/**
 * Check over a font name in a text field to make sure it agrees with the
 * primary font in height and spacing.
 *
 * @brief DialogFonts::checkFontStatus
 * @param font
 * @return
 */
DialogFonts::FontStatus DialogFonts::checkFontStatus(const QFont &font) {

    QFontMetrics fm(font);

    const int testWidth  = fm.width(QLatin1Char('i')); // NOTE(eteran): min-width?
    const int testHeight = fm.lineSpacing();

    // Get width and height of the primary font
    QFont primaryFont = ui.fontPrimary->currentFont();
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

