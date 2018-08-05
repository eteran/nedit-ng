
#include "DialogFonts.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "preferences.h"
#include <QPushButton>

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

	const QFont font = document ?
				Font::fromString(document->fontName_) :
				Font::fromString(Preferences::GetPrefFontName());


	for(int size : QFontDatabase::standardSizes()) {
		ui.fontSize->addItem(tr("%1").arg(size), size);
	}

	ui.fontCombo->setCurrentFont(font);

	const int n = ui.fontSize->findData(font.pointSize());
	if(n != -1) {
		ui.fontSize->setCurrentIndex(n);
	}
}

/**
 * @brief DialogFonts::on_buttonBox_clicked
 * @param button
 */
void DialogFonts::on_buttonBox_clicked(QAbstractButton *button) {

	switch(ui.buttonBox->standardButton(button)) {
	case QDialogButtonBox::Ok:
		updateFont();
		accept();
		break;
	case QDialogButtonBox::Apply:
		updateFont();
		break;
	default:
		reject();
		break;
	}
}


/**
 * Accept the changes in the dialog and set the font
 *
 * @brief DialogFonts::updateFont
 */
void DialogFonts::updateFont() {

	QFont font = ui.fontCombo->currentFont();
	font.setPointSize(ui.fontSize->currentData().toInt());
	QString fontName = font.toString();

	if (document_) {
		document_->action_Set_Fonts(fontName);
	} else {
		Preferences::SetPrefFont(fontName);
	}
}
