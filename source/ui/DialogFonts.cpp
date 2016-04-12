
#include <QPushButton>
#include <QtDebug>
#include "DialogFonts.h"
#include "Document.h"
#include "preferences.h"
#include "regularExp.h"
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
DialogFonts::DialogFonts(Document *window, bool forWindow, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window), forWindow_(forWindow) {
	ui.setupUi(this);

	if(!forWindow) {
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Apply));
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Close));
		ui.buttonBox->addButton(QDialogButtonBox::Cancel);
	}

	// Set initial values
	if (forWindow) {
		ui.editFontPrimary->setText(QString::fromStdString(window->fontName_));
		ui.editFontBold->setText(QLatin1String(window->boldFontName_));
		ui.editFontItalic->setText(QLatin1String(window->italicFontName_));
		ui.editFontBoldItalic->setText(QLatin1String(window->boldItalicFontName_));
	} else {
		ui.editFontPrimary->setText(QLatin1String(GetPrefFontName()));
		ui.editFontBold->setText(QLatin1String(GetPrefBoldFontName()));
		ui.editFontItalic->setText(QLatin1String(GetPrefItalicFontName()));
		ui.editFontBoldItalic->setText(QLatin1String(GetPrefBoldItalicFontName()));
	}
	
	// make sure we show the correct status values to start
	showFontStatus(ui.editFontItalic->text(),     ui.labelItalicError);
	showFontStatus(ui.editFontBold->text(),       ui.labelBoldError);
	showFontStatus(ui.editFontBoldItalic->text(), ui.labelBoldItalicError);	
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
DialogFonts::~DialogFonts() {
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonPrimaryFont_clicked() {
#if 0
	browseFont(fd->shell, fd->primaryW);
#endif
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFontItalic_clicked() {
#if 0
	browseFont(fd->shell, fd->italicW);
#endif
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFontBold_clicked() {
#if 0
	browseFont(fd->shell, fd->boldW);
#endif
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFontBoldItalic_clicked() {
#if 0
	browseFont(fd->shell, fd->boldItalicW);
#endif
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFonts::on_buttonFill_clicked() {

	// TODO(eteran): use QString::replace with QRegExp

	char modifiedFontName[MAX_FONT_LEN];
	const char *const searchString            = "(-[^-]*-[^-]*)-([^-]*)-([^-]*)-(.*)";
	
	const char *const italicReplaceString     = "\\1-\\2-o-\\4";
	const char *const boldReplaceString       = "\\1-bold-\\3-\\4";
	const char *const boldItalicReplaceString = "\\1-bold-o-\\4";
	regexp *compiledRE;

	/* Match the primary font agains RE pattern for font names.  If it
	   doesn't match, we can't generate highlight font names, so return */
	try {
		compiledRE = new regexp(searchString, REDFLT_STANDARD);
	} catch(const regex_error &e) {
		// NOTE(eteran): ignoring error?!
	}

	QString primaryName = ui.editFontPrimary->text();
	if (!compiledRE->execute(primaryName.toStdString())) {
		QApplication::beep();
		delete compiledRE;
		return;
	}

	// Make up names for new fonts based on RE replace patterns
	compiledRE->SubstituteRE(italicReplaceString, modifiedFontName, MAX_FONT_LEN);
	ui.editFontItalic->setText(QLatin1String(modifiedFontName));

	compiledRE->SubstituteRE(boldReplaceString, modifiedFontName, MAX_FONT_LEN);
	ui.editFontBold->setText(QLatin1String(modifiedFontName));

	compiledRE->SubstituteRE(boldItalicReplaceString, modifiedFontName, MAX_FONT_LEN);
	ui.editFontBoldItalic->setText(QLatin1String(modifiedFontName));

	delete compiledRE;
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
		QByteArray fontString       = fontName.toLatin1();
		QByteArray italicString     = italicName.toLatin1();
		QByteArray boldString       = boldName.toLatin1();
		QByteArray boldItalicString = boldItalicName.toLatin1();

		char *params[4];
		params[0] = fontString.data();
		params[1] = italicString.data();
		params[2] = boldString.data();
		params[3] = boldItalicString.data();
		XtCallActionProc(window_->textArea_, "set_fonts", nullptr, params, 4);

	} else {
		SetPrefFont(fontName.toLatin1().data());
		SetPrefItalicFont(italicName.toLatin1().data());
		SetPrefBoldFont(boldName.toLatin1().data());
		SetPrefBoldItalicFont(boldItalicName.toLatin1().data());
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
		msg = tr("");
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

	Display *display = XtDisplay(window_->shell_);

	/* Get width and height of the font to check.  Note the test for empty
	   name: X11R6 clients freak out X11R5 servers if they ask them to load
	   an empty font name, and kill the whole application! */
	QString testName = font;
	if (testName.isEmpty()) {
		return BAD_FONT;
	}
	
	XFontStruct *testFont = XLoadQueryFont(display, testName.toLatin1().data());
	if(!testFont) {
		return BAD_FONT;
	}

	const int testWidth  = testFont->min_bounds.width;
	const int testHeight = testFont->ascent + testFont->descent;
	XFreeFont(display, testFont);

	// Get width and height of the primary font 
	QString primaryName = ui.editFontPrimary->text();
	if (primaryName.isEmpty()) {
		return BAD_FONT;
	}
	
	XFontStruct *primaryFont = XLoadQueryFont(display, primaryName.toLatin1().data());
	if(!primaryFont) {
		return BAD_PRIMARY;
	}

	const int primaryWidth  = primaryFont->min_bounds.width;
	const int primaryHeight = primaryFont->ascent + primaryFont->descent;
	XFreeFont(display, primaryFont);


	// Compare font information 
	if (testWidth != primaryWidth) {
		return BAD_SPACING;
	}
		
	if (testHeight != primaryHeight) {
		return BAD_SIZE;
	}

	return GOOD_FONT;
}
