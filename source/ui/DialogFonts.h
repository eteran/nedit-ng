
#ifndef DIALOG_FONTS_H_
#define DIALOG_FONTS_H_

#include <QDialog>
#include "ui_DialogFonts.h"

class DocumentWidget;

class DialogFonts : public QDialog {
	Q_OBJECT
private:
	// Return values for checkFontStatus 
	enum FontStatus {
		GOOD_FONT,
		BAD_PRIMARY,
		BAD_FONT,
		BAD_SIZE,
		BAD_SPACING
	};
	
public:
    DialogFonts(DocumentWidget *window, bool forWindow, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogFonts();

private Q_SLOTS:
	void on_buttonPrimaryFont_clicked();
	void on_buttonFontItalic_clicked();
	void on_buttonFontBold_clicked();
	void on_buttonFontBoldItalic_clicked();
	void on_buttonFill_clicked();
	void on_buttonBox_clicked(QAbstractButton *button);
	void on_editFontPrimary_textChanged(const QString &text);
	void on_editFontItalic_textChanged(const QString &text);
	void on_editFontBold_textChanged(const QString &text);
	void on_editFontBoldItalic_textChanged(const QString &text);

private:
	void updateFonts();
	FontStatus checkFontStatus(const QString &font);
	FontStatus showFontStatus(const QString &font, QLabel *errorLabel);
	void browseFont(QLineEdit *lineEdit);

private:
	Ui::DialogFonts ui;
    DocumentWidget *document_;
	int forWindow_;
};

#endif
