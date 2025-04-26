
#include "DialogFonts.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "Preferences.h"

#include <QPushButton>

/**
 * @brief Constructor for DialogFonts class.
 *
 * @param document The DocumentWidget where the font settings will be applied.
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogFonts::DialogFonts(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document) {

	ui.setupUi(this);
	connectSlots();

	Dialog::shrinkToFit(this);

	if (!document_) {
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Apply));
		ui.buttonBox->removeButton(ui.buttonBox->button(QDialogButtonBox::Close));
		ui.buttonBox->addButton(QDialogButtonBox::Cancel);
	}

	const QFont font = document ? Font::fromString(document->fontName_) : Font::fromString(Preferences::GetPrefFontName());

	// whenever the font changes, update the available sizes too
	connect(ui.fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
		const QList<int> sizes = Font::pointSizes(font);

		const QVariant currentSize = ui.fontSize->currentData();
		const int current          = currentSize.isValid() ? currentSize.toInt() : font.pointSize();

		ui.fontSize->clear();
		for (const int size : sizes) {
			ui.fontSize->addItem(QStringLiteral("%1").arg(size), size);
		}

		const int n = ui.fontSize->findData(current);
		if (n != -1) {
			ui.fontSize->setCurrentIndex(n);
		}
	});

	ui.fontCombo->setCurrentFont(font);

	if (!document) {
		ui.checkApplyAll->setVisible(false);
	}
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogFonts::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogFonts::buttonBox_clicked);
}

/**
 * @brief Handles the button box click event.
 *
 * @param button The button that was clicked in the button box.
 */
void DialogFonts::buttonBox_clicked(QAbstractButton *button) {

	switch (ui.buttonBox->standardButton(button)) {
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
 * @brief Accept the changes in the dialog and set the font.
 */
void DialogFonts::updateFont() {

	QFont font = ui.fontCombo->currentFont();
	font.setPointSize(ui.fontSize->currentData().toInt());
	const QString fontName = font.toString();

	if (document_) {
		document_->action_Set_Fonts(fontName);
		if (ui.checkApplyAll->isChecked()) {
			for (DocumentWidget *document : DocumentWidget::allDocuments()) {
				document->action_Set_Fonts(fontName);
			}
		}
	} else {
		Preferences::SetPrefFont(fontName);
	}
}
