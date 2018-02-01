
#ifndef DIALOG_FONTS_H_
#define DIALOG_FONTS_H_

#include "Dialog.h"
#include "ui_DialogFonts.h"

class DocumentWidget;

class DialogFonts : public Dialog {
	Q_OBJECT
private:
    // Return values for checkFontStatus
    enum FontStatus {
        GOOD_FONT,
        BAD_SIZE,
        BAD_SPACING
    };

public:
    DialogFonts(DocumentWidget *document, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~DialogFonts() override = default;

private Q_SLOTS:
    void on_buttonFill_clicked();
	void on_buttonBox_clicked(QAbstractButton *button);
    void on_fontPrimary_currentFontChanged(const QFont &font);
    void on_fontItalic_currentFontChanged(const QFont &font);
    void on_fontBold_currentFontChanged(const QFont &font);
    void on_fontBoldItalic_currentFontChanged(const QFont &font);

private:
	void updateFonts();
    FontStatus checkFontStatus(const QFont &font);
    void showFontStatus(const QFont &font, QLabel *errorLabel);

private:
	Ui::DialogFonts ui;
    DocumentWidget *document_;
};

#endif
