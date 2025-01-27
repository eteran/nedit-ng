
#include "DialogFonts.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "Preferences.h"
#include <QPushButton>
#include <QTimer>

/**
 * @brief DialogFonts::DialogFonts
 * @param document
 * @param parent
 * @param f
 */
DialogFonts::DialogFonts(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document) {

	ui.setupUi(this);
	connectSlots();

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

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
		for (int size : sizes) {
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
 * @brief DialogFonts::connectSlots
 */
void DialogFonts::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogFonts::buttonBox_clicked);
}

/**
 * @brief DialogFonts::buttonBox_clicked
 * @param button
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
 * Accept the changes in the dialog and set the font
 *
 * @brief DialogFonts::updateFont
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
